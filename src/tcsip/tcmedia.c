#include <srtp/srtp.h>
#include <sys/time.h>
#include <string.h>

#include "re.h"
#include "jitter/ajitter.h"
#include "txsip_private.h"
#include "rtp/rtp_io.h"

#include "tcsipcall.h"
#include "tcmedia.h"

#include "sound/sound_macro.h"

#define DEBUG_MODULE "tcmedia"
#define DEBUG_LEVEL 5
#include <re_dbg.h>


struct tcmedia {
    struct uac *uac;
    struct sa *laddr;

    struct sound_t *sound;
    struct rtp_sock *rtp;
    struct ice* ice;
    struct icem* icem;
    struct stun_dns *stun_dns;

    struct tmr ka_tmr;

    void *send_io_ctx;
    rtp_recv_arg recv_io_arg;

    srtp_t srtp_in;
    srtp_t srtp_out;
    unsigned char srtp_in_key[64];
    unsigned char srtp_out_key[64];
    char cname[14];
    char msid[32];

    struct sdp_session *sdp;
    struct sdp_media *sdp_media;
    struct sdp_media *sdp_media_s;
    struct sdp_media *sdp_media_sf;

    struct sa *dst;
    struct sa rtcp_dst;
    int pt;
    int fmt;

    media_h *media_h;
    void *marg;
};

void tcmedia_stun(struct tcmedia*media, const struct sa* srv);
void tcmedia_conn_check(struct tcmedia*media, bool update, int err);
void tcmedia_upd(struct tcmedia*media);
void tcmedia_chdst(struct tcmedia*media);
int tcmedia_formats(struct tcmedia*media);
int tcmedia_setup_srtp(struct tcmedia*media);
void tcmedia_set_format(struct tcmedia*media, char* fmt_name);
static void gather_handler(int err, uint16_t scode, const char *reason, void *arg);

void media_de(void*arg)
{
    struct tcmedia *media = arg;
    mem_deref(media->uac);

    if(media->stun_dns) media->stun_dns = mem_deref(media->stun_dns);

    if(media->sdp) media->sdp = mem_deref(media->sdp);

}


static char stun_srv[] = "stun.l.google.com";
static int stun_port = 19302;

bool sdp_crypto(const char *name, const char *value, void *arg)
{
    unsigned char *srtp_in_key = arg;

    struct pl key, crypt_n, crypt_s, key_m;
    size_t klen = 64;

    re_regex(value, strlen(value), "[0-9]+ [a-zA-Z0-9_]+ [a-zA-Z]+:[A-Za-z0-9+/]+",
            &crypt_n, &crypt_s, &key_m, &key);

    if(pl_strcmp(&crypt_s, "AES_CM_128_HMAC_SHA1_80"))
        return false;

    if(key.l!=40) {
        DEBUG_WARNING("invalid base64 key len %ld\n", key.l);
        return false;
    }

    base64_decode(key.p, key.l, srtp_in_key, &klen);
    if(klen!=30) {
        DEBUG_WARNING("invalid key len %ld\n", klen);
        return false;
    }

    return true;

}

static bool candidate_handler(struct le *le, void *arg)
{
	DEBUG_INFO("candidate %p %H\n", arg, ice_cand_encode, le->data);
	return 0 != sdp_media_set_lattr(arg, false, ice_attr_cand, "%H",
					ice_cand_encode, le->data);
}

static bool media_attr_handler(const char *name, const char *value, void *arg)
{
	struct icem *icem = arg;
	return 0 != icem_sdp_decode(icem, name, value);
}

static bool if_handler(const char *ifname, const struct sa *sa, void *arg)
{

    struct icem *icem = arg;
    int err;
    uint16_t lprio;

    /* Skip loopback and link-local addresses */
    if (sa_is_loopback(sa) || sa_is_linklocal(sa))
            return false;

    DEBUG_INFO("interface %s %J\n", ifname, sa);

    lprio = 0;
    err = icem_cand_add(icem, 1, lprio, ifname, sa);
    err = icem_cand_add(icem, 2, lprio, ifname, sa);

    return false;
}

static void dns_handler(int err, const struct sa *srv, void *arg)
{
   struct tcmedia *media = arg;

   DEBUG_INFO("dns handler %d %J\n", err, srv);
   if(err)
       return;

   tcmedia_stun(media, srv);
}

static void conncheck_handler(int err, bool update, void *arg)
{
   struct tcmedia *media = arg;
   tcmedia_conn_check(media, update, err);
}

void tcmedia_gencname(struct tcmedia*media)
{
    unsigned char rbytes[20];
    rand_bytes(rbytes, 20);
    size_t i=9, olen=12;
    base64_encode(rbytes, i, media->cname, &olen);
    media->cname[olen] = '\0';

    rand_bytes(rbytes, 20); 
    olen=31;
    i=20;
    base64_encode(rbytes, i, media->msid, &olen); 
    media->msid[olen] = '\0';
    DEBUG_INFO("cname: %s, msid(%ld) %s\n", media->cname, olen, media->msid);
}

void tcmedia_genssrc(struct tcmedia*media)
{
    uint32_t ssrc;
    sdp_session_set_lattr(media->sdp, true, "msid-semantic", "WMS %s", media->msid);
    
    ssrc = rtp_sess_ssrc(media->rtp);
    sdp_media_set_lattr(media->sdp_media_sf, true, "ssrc", "%u cname:%s", ssrc, media->cname);

    sdp_media_set_lattr(media->sdp_media_sf, false, "ssrc", "%u msid:%s mic0", ssrc, media->msid);
    sdp_media_set_lattr(media->sdp_media_sf, false, "ssrc", "%u mslabel:%s", ssrc, media->msid);
    sdp_media_set_lattr(media->sdp_media_sf, false, "ssrc", "%u label:mic0", ssrc);

}

void tcmedia_handler(struct tcmedia* media, media_h hdl, void*arg)
{
    if(!media) return;

    media->media_h = hdl;
    media->marg = arg; 
}

int tcmedia_setup(struct tcmedia*media, call_dir_t dir)
{
    int err;

    rtp_listen(&media->rtp, IPPROTO_UDP, media->laddr, 6000, 7000, true,
            rtp_recv_io, rtcp_recv_io, &media->recv_io_arg);

    media->laddr = (struct sa*)rtp_local(media->rtp);

    err = ice_alloc(&media->ice, ICE_MODE_FULL, dir == CALL_OUT);
    err |= icem_alloc(&media->icem, media->ice, IPPROTO_UDP, 0,
			gather_handler, conncheck_handler,
			media);

    icem_comp_add(media->icem, 1, rtp_sock(media->rtp));
    icem_comp_add(media->icem, 2, rtcp_sock(media->rtp));

    /* create SDP session */
    err |= sdp_session_alloc(&media->sdp, media->laddr);
    err |= sdp_media_add(&media->sdp_media_s, media->sdp, "audio", sa_port(media->laddr), "RTP/SAVP");
    err |= sdp_media_add(&media->sdp_media, media->sdp, "audio", sa_port(media->laddr), "RTP/AVP");
    err |= sdp_media_add(&media->sdp_media_sf, media->sdp, "audio", sa_port(media->laddr), "RTP/SAVPF");

    rand_bytes(media->srtp_out_key, 30);
    char crypto_line[] = "1 AES_CM_128_HMAC_SHA1_80 inline:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX      ";
    size_t klen = 40;
    base64_encode(media->srtp_out_key, 30, &crypto_line[33], &klen);
    sdp_media_set_lattr(media->sdp_media_s, true, "crypto", crypto_line);
    sdp_media_set_lattr(media->sdp_media_sf, true, "crypto", crypto_line);

#define FMT_X(pt, name, srate, ch, p) {\
    err |= sdp_format_add(NULL, media->sdp_media, false,\
	pt, name, srate, ch,\
	 NULL, NULL, NULL, false, p);\
    err |= sdp_format_add(NULL, media->sdp_media_s, false,\
	pt, name, srate, ch,\
	 NULL, NULL, NULL, false, p);\
    err |= sdp_format_add(NULL, media->sdp_media_sf, false,\
	pt, name, srate, ch,\
	 NULL, NULL, NULL, false, p);}

#define FMT(pt, name) FMT_X(pt, name, 8000, 1, NULL)
#define FMT_CH(pt, name, srate, ch) FMT_X(pt, name, srate, ch, NULL)

    sdp_media_set_laddr_rtcp(media->sdp_media, rtcp_sock(media->rtp));
    sdp_media_set_laddr_rtcp(media->sdp_media_s, rtcp_sock(media->rtp));
    sdp_media_set_laddr_rtcp(media->sdp_media_sf, rtcp_sock(media->rtp));

    FMT_X("101", "opus", 48000, 2, "minptime=10");
    FMT("97", "speex");
    FMT("0", "PCMU");

    stun_server_discover(&media->stun_dns, media->uac->dnsc,
	stun_usage_binding,  stun_proto_udp,
	AF_INET, stun_srv, stun_port,
	dns_handler, media);

    tcmedia_gencname(media);
    tcmedia_genssrc(media);

    return err;
}

static void gather_handler(int err, uint16_t scode, const char *reason,
			   void *arg)
{
    struct tcmedia *media = arg;

    DEBUG_INFO("gather %J\n",
            icem_cand_default(media->icem, 1),
            icem_cand_default(media->icem, 2)
    );
    tcmedia_upd(media);
    if(media->media_h) {
        media->media_h(media, MEDIA_ICE_OK, 0, media->marg);
    }
}

void tcmedia_conn_check(struct tcmedia*media, bool update, int err)
{
    if(err==0) {
	tcmedia_chdst(media);
    }


}

void tcmedia_upd(struct tcmedia*media)
{
    sdp_media_del_lattr(media->sdp_media_sf, ice_attr_cand);

    list_apply(icem_lcandl(media->icem), true, candidate_handler, media->sdp_media_sf);

    if (ice_remotecands_avail(media->icem)) {
        sdp_media_set_lattr(media->sdp_media_sf, true,
		   ice_attr_remote_cand, "%H",
		   ice_remotecands_encode, media->icem);
    }

    sdp_media_set_lattr(media->sdp_media_sf, true,
				   ice_attr_ufrag, ice_ufrag(media->ice));
    sdp_media_set_lattr(media->sdp_media_sf, true,
				   ice_attr_pwd, ice_pwd(media->ice));
}

void tcmedia_stun(struct tcmedia*media, const struct sa* srv)
{
    int err;

    net_if_apply(if_handler, media->icem);

    err = icem_gather_srflx(media->icem, srv);
}

int tcmedia_setup_srtp(struct tcmedia*media)
{
    int err;

    srtp_policy_t in_policy, out_policy;

    crypto_policy_set_rtp_default(&in_policy.rtp);
    crypto_policy_set_rtcp_default(&in_policy.rtcp);

    in_policy.key = (uint8_t*)media->srtp_in_key;
    in_policy.next = NULL;
    in_policy.ssrc.type  = ssrc_any_inbound;
    in_policy.rtp.sec_serv = sec_serv_conf_and_auth;
    in_policy.rtcp.sec_serv = sec_serv_none;

    err = srtp_create(&media->srtp_in, &in_policy);
    if(err) {
        DEBUG_WARNING("strtp create failed %d\n", err);
	goto fail;
    }

    crypto_policy_set_rtp_default(&out_policy.rtp);
    crypto_policy_set_rtcp_default(&out_policy.rtcp);

    out_policy.key = (uint8_t*)media->srtp_out_key;
    out_policy.next = NULL;
    out_policy.ssrc.type  = ssrc_any_outbound;
    out_policy.rtp.sec_serv = sec_serv_conf_and_auth;
    out_policy.rtcp.sec_serv = sec_serv_none;

    err = srtp_create(&media->srtp_out, &out_policy);

fail:
    return err;
}

int tcmedia_alloc(struct tcmedia** rp, struct uac*uac, call_dir_t cdir)
{
    struct tcmedia *media;
    int err;
    media = mem_zalloc(sizeof(struct tcmedia), media_de);
    if(!media) {
        err = -ENOMEM;
	goto fail;
    }
    media->uac = mem_ref(uac);
    media->laddr = &uac->laddr;
    err = tcmedia_setup(media, cdir);
    if(err) {
	mem_deref(media);
	goto fail;
    }

    *rp = media;
    return 0;
fail:
    return err;
}

int tcmedia_offer(struct tcmedia*media, struct mbuf *offer, struct mbuf**ret)
{
    int err;

    err = sdp_decode(media->sdp, offer, true);
    if(err) {
        goto out;
    }

    err |= tcmedia_formats(media);

    err |= sdp_encode(ret, media->sdp, false);
out:
    return err;
}

int tcmedia_get_offer(struct tcmedia*media, struct mbuf**ret)
{
    return sdp_encode(ret, media->sdp, true); 
}

int tcmedia_answer(struct tcmedia*media, struct mbuf *offer)
{
    int err;

    err = sdp_decode(media->sdp, offer, false);
    if(err) {
        goto out;
    }

    tcmedia_formats(media);

out:
    return err;
}

int tcmedia_formats(struct tcmedia*media)
{
    int err = 0;
    const struct sdp_format *rfmt = NULL, *_rfmt = NULL;
    const struct sdp_media *md = NULL;
    const struct list *list;
    struct le *le;

    list = sdp_session_medial(media->sdp, false);

    for(le = list->head; le ; le = le->next) {
        _rfmt = sdp_media_rformat(le->data, NULL);
        if(_rfmt) {
            rfmt = _rfmt;
            md = le->data;
        }
    }

    if (!rfmt) {
        DEBUG_INFO("no common media format found\n");
	err = -EINVAL;
	goto fail;
    }

    if(md == media->sdp_media_s || md == media->sdp_media_sf) {
        (void)sdp_media_rattr_apply(md, "crypto", sdp_crypto, media->srtp_in_key);
	err = tcmedia_setup_srtp(media);
	if(err) goto fail;
    }

    sdp_media_rattr_apply(md, NULL, media_attr_handler, media->icem);

    media->dst = (struct sa*)sdp_media_raddr(md);
    icem_verify_support(media->icem, 1, media->dst);

    sdp_media_raddr_rtcp(md, &media->rtcp_dst);
    DEBUG_INFO("rtcp dest %J\n", &media->rtcp_dst);
    icem_verify_support(media->icem, 2, &media->rtcp_dst);

    tcmedia_set_format(media, rfmt->name);
    media->pt = rfmt->pt;

    return 0;
fail:
    return err;
}


void tcmedia_set_format(struct tcmedia*media, char* fmt_name)
{
    if(!strcmp(fmt_name, "speex"))
        media->fmt = FMT_SPEEX;
    if(!strcmp(fmt_name, "PCMU"))
        media->fmt = FMT_PCMU;
    if(!strcmp(fmt_name, "opus"))
	media->fmt = FMT_OPUS;
}

void tcmedia_chdst(struct tcmedia*media)
{
    const struct sa *ice_dst1, *ice_dst2;

    ice_dst1 = icem_selected_raddr(media->icem, 1);
    ice_dst2 = icem_selected_raddr(media->icem, 2);

    sa_cpy(media->dst, ice_dst1);

    DEBUG_INFO("change dst ice dst %J (%J) and %J\n", ice_dst1, media->dst, ice_dst2);

    rtcp_start(media->rtp, media->cname, ice_dst2);
}

static void ka_handler(void *arg)
{
    struct tcmedia *media = arg;
    struct timeval tv;
    int ret, delt;
    rtp_recv_ctx *rctx = media->recv_io_arg.ctx;
    if(!rctx) goto timer;

    ret = gettimeofday(&tv, NULL);
    if(ret) goto timer;

    delt = (int)tv.tv_sec - rctx->last_read;
    if(delt < 5)
	goto timer;
        
    media->media_h(media, (delt<15) ? MEDIA_KA_WARN : MEDIA_KA_FAIL, delt, media->marg);

timer:
    tmr_start(&media->ka_tmr, 5000, ka_handler, media);
}

int tcmedia_start(struct tcmedia*media)
{

    int ok;

    icem_update(media->icem);
    ice_conncheck_start(media->ice);

    if(!media->sound) {
        // XXX: use audio, video, chat flags
	ok = snd_open(&media->sound);
        if(ok!=0)
            return -1;
    }

    rtp_send_ctx *send_ctx = rtp_send_init(media->fmt);
    send_ctx->record_jitter = media->sound->record_jitter;
    send_ctx->rtp = media->rtp;
    send_ctx->pt = media->pt;
    send_ctx->srtp_out = media->srtp_out;
    send_ctx->dst = media->dst;

    media->send_io_ctx = send_ctx;

    // set recv side
    rtp_recv_ctx * recv_ctx = rtp_recv_init(media->fmt);
    recv_ctx->srtp_in = media->srtp_in;
    recv_ctx->play_jitter = (void*)media->sound->play_jitter;

    media->recv_io_arg.ctx = recv_ctx;
    media->recv_io_arg.handler = rtp_recv_func(media->fmt);

    ok = snd_start(media->sound);

    rtp_send_start(media->send_io_ctx);

    tmr_start(&media->ka_tmr, 5000, ka_handler, media);

    return 0;
}

void tcmedia_stop(struct tcmedia *media)
{

    if(media->rtp)
        media->rtp = mem_deref(media->rtp);

    if(media->ice)
        media->ice = mem_deref(media->ice);

    if(media->send_io_ctx) {
        rtp_send_stop(media->send_io_ctx);
        media->send_io_ctx = NULL;
    }

    if(media->sound) {
        snd_close(media->sound);
	media->sound = NULL;
    }

    if(media->recv_io_arg.ctx) {
	rtp_recv_stop(media->recv_io_arg.ctx);
	media->recv_io_arg.ctx = NULL;
    }

    if(media->srtp_in)
        srtp_dealloc(media->srtp_in);

    if(media->srtp_out)
        srtp_dealloc(media->srtp_out);

    tmr_cancel(&media->ka_tmr);

}
