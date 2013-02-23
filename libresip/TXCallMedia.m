//
//  TXCallMedia.m
//  Texr
//
//  Created by Ilya Petrov on 9/28/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXCallMedia.h"
#import "TXSipCall.h"
#include "ajitter.h"
#include "txsip_private.h"

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
        printf("invalid base64 key len %ld\n", key.l);
        return false;
    }

    base64_decode(key.p, key.l, srtp_in_key, &klen);
    if(klen!=30) {
        printf("invalid key len %ld\n", klen);
        return false;
    }

    return true;

}

static void rtcp_recv_io(const struct sa *src, struct rtcp_msg *msg,
			   void *arg)
{
    re_printf("rtcp handler %J\n", src);
}

static bool candidate_handler(struct le *le, void *arg)
{
	re_printf("candidate %p %H\n", arg, ice_cand_encode, le->data);
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

    re_printf("interface %s %J\n", ifname, sa);

    lprio = 0;
    err = icem_cand_add(icem, 1, lprio, ifname, sa);
    err = icem_cand_add(icem, 2, lprio, ifname, sa);

    return false;
}

static void dns_handler(int err, const struct sa *srv, void *arg)
{
   TXCallMedia *media = (__bridge TXCallMedia*)arg;

   re_printf("dns handler %d %J\n", err, srv);
   if(err)
       return;

   [media stun: srv];
}

static void gather_handler(int err, uint16_t scode, const char *reason,
			   void *arg)
{
   TXCallMedia *media = (__bridge TXCallMedia*)arg;
   [media gather: scode err:err reason:reason];
}
static void conncheck_handler(int err, bool update, void *arg)
{
   TXCallMedia *media = (__bridge TXCallMedia*)arg;
   [media conn_check: update err:err];
}

@implementation TXCallMedia
- (id) initWithUAC: (struct uac*)pUac dir:(call_dir_t)pDir
{

    self = [super init];
    if(!self) return self;

    uac = pUac;
    laddr = &uac->laddr;

    [self setup: pDir];

    return self;
}

- (void) setup:(call_dir_t)pDir {

    int err;
    media = NULL;

    rtp_listen(&rtp, IPPROTO_UDP, laddr, 6000, 7000, true,
            rtp_recv_io, rtcp_recv_io, &recv_io_arg);

    laddr = (struct sa*)rtp_local(rtp);

    err = ice_alloc(&ice, ICE_MODE_FULL, pDir == CALL_OUT);
    err |= icem_alloc(&icem, ice, IPPROTO_UDP, 0,
			gather_handler, conncheck_handler,
                        (__bridge void*)self);

    icem_comp_add(icem, 1, rtp_sock(rtp));
    icem_comp_add(icem, 2, rtcp_sock(rtp));

    /* create SDP session */
    err |= sdp_session_alloc(&sdp, laddr);
    err |= sdp_media_add(&sdp_media_s, sdp, "audio", sa_port(laddr), "RTP/SAVP");
    err |= sdp_media_add(&sdp_media, sdp, "audio", sa_port(laddr), "RTP/AVP");
    err |= sdp_media_add(&sdp_media_sf, sdp, "audio", sa_port(laddr), "RTP/SAVPF");

    rand_bytes(srtp_out_key, 30);
    char crypto_line[] = "1 AES_CM_128_HMAC_SHA1_80 inline:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX      ";
    size_t klen = 40;
    base64_encode(srtp_out_key, 30, &crypto_line[33], &klen);
    sdp_media_set_lattr(sdp_media_s, true, "crypto", crypto_line);
    sdp_media_set_lattr(sdp_media_sf, true, "crypto", crypto_line);

#define FMT_CH(pt, name, srate, ch) {\
    err |= sdp_format_add(NULL, sdp_media, true,\
	pt, name, srate, ch,\
	 NULL, NULL, NULL, false, NULL);\
    err |= sdp_format_add(NULL, sdp_media_s, true,\
	pt, name, srate, ch,\
	 NULL, NULL, NULL, false, NULL);\
    err |= sdp_format_add(NULL, sdp_media_sf, false,\
	pt, name, srate, ch,\
	 NULL, NULL, NULL, false, NULL);}

#define FMT(pt, name) FMT_CH(pt, name, 8000, 1)
    sdp_media_set_laddr_rtcp(sdp_media, rtcp_sock(rtp));
    sdp_media_set_laddr_rtcp(sdp_media_s, rtcp_sock(rtp));
    sdp_media_set_laddr_rtcp(sdp_media_sf, rtcp_sock(rtp));

    FMT("97", "speex");
    FMT("0", "PCMU");
    FMT_CH("101", "opus", 4800, 2);

    // dump local
    const struct sdp_format *lfmt;

    lfmt = sdp_media_lformat(sdp_media, 97);
    if (!lfmt) {
        re_printf("no local media format found\n");
        return;
    }

    stun_server_discover(&stun_dns, uac->dnsc,
	stun_usage_binding,  stun_proto_udp,
	AF_INET, stun_srv, stun_port,
	dns_handler, (__bridge void*)self);

    [self gencname];
    [self set_ssrc];

}

- (void)gencname
{
    unsigned char rbytes[20];
    rand_bytes(rbytes, 20);
    size_t i=9, olen=12;
    base64_encode(rbytes, i, cname, &olen);
    cname[olen] = '\0';

    rand_bytes(rbytes, 20); 
    olen=31;
    i=20;
    base64_encode(rbytes, i, msid, &olen); 
    msid[olen] = '\0';
    re_printf("cname: %s, msid(%ld) %s\n", cname, olen, msid);
}

- (void)set_ssrc
{
    uint32_t ssrc;
    sdp_session_set_lattr(sdp, true, "msid-semantic", "WMS %s", msid);
    
    ssrc = rtp_sess_ssrc(rtp);
    sdp_media_set_lattr(sdp_media_sf, true, "ssrc", "%u cname:%s", ssrc, cname);

    sdp_media_set_lattr(sdp_media_sf, false, "ssrc", "%u msid:%s mic0", ssrc, msid);
    sdp_media_set_lattr(sdp_media_sf, false, "ssrc", "%u mslabel:%s", ssrc, msid);
    sdp_media_set_lattr(sdp_media_sf, false, "ssrc", "%u label:mic0", ssrc);

}

- (void) gather: (uint16_t)scode err:(int)err reason:(const char*)reason
{
    re_printf("gather %J\n",
            icem_cand_default(icem, 1),
            icem_cand_default(icem, 2)
    );
    [self update_media];
    [iceCB response: @"done"];
    iceCB = nil;
}
- (void) conn_check: (bool)update err:(int)err
{
    re_printf("conncheck complete %d\n", err);

    re_printf("check %J, %J\n",
            icem_selected_laddr(icem, 1),
            icem_selected_laddr(icem, 2));

    if(err==0) {
        [self change_dst];
    }

}

- (void)update_media
{
    sdp_media_del_lattr(sdp_media_sf, ice_attr_cand);

    list_apply(icem_lcandl(icem), true, candidate_handler, sdp_media_sf);

    if (ice_remotecands_avail(icem)) {
        sdp_media_set_lattr(sdp_media_sf, true,
		   ice_attr_remote_cand, "%H",
		   ice_remotecands_encode, icem);
    }

    sdp_media_set_lattr(sdp_media_sf, true,
				   ice_attr_ufrag, ice_ufrag(ice));
    sdp_media_set_lattr(sdp_media_sf, true,
				   ice_attr_pwd, ice_pwd(ice));
}

- (void)stun:(const struct sa*)srv
{
    int err;

    net_if_apply(if_handler, icem);

    err = icem_gather_srflx(icem, srv);
    re_printf("media start %d %J\n", err, srv);;
}

- (void)setIceCb:(Callback*)pCB
{
    iceCB = pCB;
}

- (void) setupSRTP
{
    int err;

    srtp_policy_t in_policy, out_policy;

    crypto_policy_set_rtp_default(&in_policy.rtp);
    crypto_policy_set_rtcp_default(&in_policy.rtcp);

    in_policy.key = (uint8_t*)srtp_in_key;
    in_policy.next = NULL;
    in_policy.ssrc.type  = ssrc_any_inbound;
    in_policy.rtp.sec_serv = sec_serv_conf_and_auth;
    in_policy.rtcp.sec_serv = sec_serv_none;

    err = srtp_create(&srtp_in, &in_policy);
    printf("srtp create %d\n", err);

    crypto_policy_set_rtp_default(&out_policy.rtp);
    crypto_policy_set_rtcp_default(&out_policy.rtcp);

    out_policy.key = (uint8_t*)srtp_out_key;
    out_policy.next = NULL;
    out_policy.ssrc.type  = ssrc_any_outbound;
    out_policy.rtp.sec_serv = sec_serv_conf_and_auth;
    out_policy.rtcp.sec_serv = sec_serv_none;

    err = srtp_create(&srtp_out, &out_policy);
    printf("srtp create %d\n", err);

}

- (void) stop {

    if(rtp)
        rtp = mem_deref(rtp);

    if(ice)
        ice = mem_deref(ice);

    if(send_io_ctx) {
        rtp_send_stop(send_io_ctx);
        send_io_ctx = NULL;
    }

    if(media) {
        media_snd_stream_stop(media);
	media_snd_stream_close(media);
	media = NULL;
    }


    if(recv_io_arg.ctx) {
	rtp_recv_stop(recv_io_arg.ctx);
	recv_io_arg.ctx = NULL;
    }

    /* XXX: free sdp session and both sdp medias */
 
    if(srtp_in)
        srtp_dealloc(srtp_in);

    if(srtp_out)
        srtp_dealloc(srtp_out);

}

- (void) open {

    // XXX: use audio, video, chat flags
    int ok = media_snd_open(DIR_BI, 8000, O_LIM, &media);
    if(ok!=0) {
        NSLog(@"media faled to open %d", ok);
        return;
    }

}

- (bool) start {

    int ok;

    icem_update(icem);
    ice_conncheck_start(ice);

    if(!media)
        [self open];

    if(!media)
        return YES;

    rtp_send_ctx *send_ctx = rtp_send_init(fmt);
    send_ctx->record_jitter = media->record_jitter;
    send_ctx->rtp = rtp;
    send_ctx->pt = pt;
    send_ctx->srtp_out = srtp_out;
    send_ctx->dst = dst;

    send_io_ctx = send_ctx;

    // set recv side
    rtp_recv_ctx * recv_ctx = rtp_recv_init(fmt);
    recv_ctx->srtp_in = srtp_in;
    recv_ctx->play_jitter = media->play_jitter;

    recv_io_arg.ctx = recv_ctx;
    recv_io_arg.handler = rtp_recv_func(fmt);

    ok = media_snd_stream_start(media);
    rtp_send_start(send_io_ctx);

    return NO;
}

- (void)change_dst
{
    const struct sa *ice_dst1, *ice_dst2;

    ice_dst1 = icem_selected_raddr(icem, 1);
    ice_dst2 = icem_selected_raddr(icem, 2);

    sa_cpy(dst, ice_dst1);

    re_printf("change dst ice dst %J (%J) and %J\n", ice_dst1, dst, ice_dst2);

    rtcp_start(rtp, cname, ice_dst2);
}


- (int) offer: (struct mbuf*)offer ret:(struct mbuf **)ret {

    int err;

    /*
     * Workarround here
     *
     * for some reason libre sets mb->end to the
     * offset, pointing to payload start.
     *
     * Settind mb->end to mb->size breaks connection buffer,
     * so we just copy payload to separate mb
     *
     * */
    struct mbuf *mb = mbuf_alloc(0);
    mbuf_write_mem(mb, mbuf_buf(offer), mbuf_get_space(offer));
    mb->pos = 0;

    NSLog(@"processing offer in call media");
    err = sdp_decode(sdp, mb, true);
    if(err) {
        NSLog(@"cant decode offer %d\n", err);
        goto out;
    }

    [self sdpFormats];

    err = sdp_encode(ret, sdp, false);
out:
    mem_deref(mb);
    return err;
}
- (int) offer: (struct mbuf **)ret {
    return sdp_encode(ret, sdp, true); 
}

- (int) answer: (struct mbuf*)offer {
    int err;
    NSLog(@"processing answer");

    err = sdp_decode(sdp, offer, false);
    if(err) {
        NSLog(@"cant decode answer %d", err);
        goto out;
    }

    [self sdpFormats];

out:
    return err;
}

- (void) sdpFormats {

    const struct sdp_format *rfmt = NULL;
    const struct sdp_media *md;
    const struct list *list;
    struct le *le;

    list = sdp_session_medial(sdp, false);

    for(le = list->head; le ; le = le->next) {
        md = le->data;
        rfmt = sdp_media_rformat(md, NULL);
    }

    if (!rfmt) {
        re_printf("no common media format found\n");
        return;
    }

    if(md == sdp_media_s || md == sdp_media_sf) {
        (void)sdp_media_rattr_apply(md, "crypto", sdp_crypto, srtp_in_key);
        [self setupSRTP];
    }

    sdp_media_rattr_apply(md, NULL, media_attr_handler, icem);

    dst = (struct sa*)sdp_media_raddr(md);
    icem_verify_support(icem, 1, dst);
    re_printf("dest %J\n", dst);

    sdp_media_raddr_rtcp(md, &rtcp_dst);
    re_printf("rtcp dest %J\n", &rtcp_dst);
    icem_verify_support(icem, 2, &rtcp_dst);

    [self set_format: rfmt->name];
    pt = rfmt->pt;
}

- (void) set_format:(char*)fmt_name
{
    if(!strcmp(fmt_name, "speex"))
        fmt = FMT_SPEEX;
    if(!strcmp(fmt_name, "PCMU"))
        fmt = FMT_PCMU;
    if(!strcmp(fmt_name, "opus"))
	fmt = FMT_OPUS;
}

@end
