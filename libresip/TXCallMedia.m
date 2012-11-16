//
//  TXCallMedia.m
//  Texr
//
//  Created by Ilya Petrov on 9/28/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXCallMedia.h"
#include "ajitter.h"

typedef struct {
    int magic;
    struct pjmedia_snd_stream * media;
    int frame_size;
    int read_off;
    SpeexBits *enc_bits;
    void *enc_state;
    struct mbuf *mb;
    struct rtp_sock *rtp;
    int pt;
    int ts;
    srtp_t srtp_out;
    struct sa *dst;
    struct tmr *tmr;
} rtp_send_ctx;

void rtp_send_io(void *varg)
{
    int len = 0, err=0;
    rtp_send_ctx * arg = varg;
    if(arg->magic != 0x1ee1F00D)
        return;

    struct mbuf *mb = arg->mb;
    char *obuf;
restart:
    obuf = ajitter_get_chunk(arg->media->record_jitter, arg->frame_size, &arg->ts);
    if(!obuf)
        goto timer;

    len = speex_encode_int(arg->enc_state, (spx_int16_t*)obuf, arg->enc_bits);

    mb->pos = RTP_HEADER_SIZE;
    len = speex_bits_write(arg->enc_bits, mbuf_buf(mb), 200); // XXX: constant
    len += RTP_HEADER_SIZE;
    mb->end = len;
    mbuf_advance(mb, -RTP_HEADER_SIZE);

    err = rtp_encode(arg->rtp, 0, arg->pt, arg->ts, mb);
    mb->pos = 0;

    if(arg->srtp_out) {
        err = srtp_protect(arg->srtp_out, mbuf_buf(mb), &len);
        if(err)
            printf("srtp failed %d\n", err);
        mb->end = len;
    }

    udp_send(rtp_sock(arg->rtp), arg->dst, mb);

    speex_bits_reset(arg->enc_bits);
    goto restart;

timer:
    tmr_start(arg->tmr, 4, rtp_send_io, varg);
}


void rtp_io (const struct sa *src, const struct rtp_header *hdr,
        struct mbuf *mb, void *arg)
{
    TXCallMedia *media = (__bridge TXCallMedia*)arg;
    [media rtpData: mb header:hdr];
}

@implementation TXCallMedia
- (id) initWithLaddr: (struct sa*)pLaddr {

    self = [super init];
    if(!self) return self;

    laddr = pLaddr;

    [self setup];

    return self;
}

- (void) setup {

    int err, port;
    media = NULL;

    enc_state = speex_encoder_init(&speex_nb_mode);
    dec_state = speex_decoder_init(&speex_nb_mode);
    speex_bits_init(&enc_bits);
    speex_bits_init(&dec_bits);

    speex_bits_reset(&enc_bits);

    read_off = 0;
    write_off = 0;

    speex_decoder_ctl(dec_state, SPEEX_GET_FRAME_SIZE, &frame_size); 
    //speex_jitter_init(&jitter, dec_state, 800);
    frame_size *= 2;

    /* create SDP session */

    rtp_listen(&rtp, IPPROTO_UDP, laddr, 6000, 7000, false,
            rtp_io, NULL, (__bridge void*)self);

    laddr = rtp_local(rtp);

    err = sdp_session_alloc(&sdp, laddr);
    err = sdp_media_add(&sdp_media_s, sdp, "audio", sa_port(laddr), "RTP/SAVP");
    err = sdp_media_add(&sdp_media, sdp, "audio", sa_port(laddr), "RTP/AVP");
    /* XXX broken shit */
    sdp_media_set_lattr(sdp_media_s, true, "crypto", "1 AES_CM_128_HMAC_SHA1_80 inline:Si/+CaPuPa+Wq2ByfeDG1/yWufg4OPCl6D2W9xs5");

    err = sdp_format_add(NULL, sdp_media, true,
	"97", "speex", 8000, 1,
	 NULL, NULL, NULL, false, NULL);

    err = sdp_format_add(NULL, sdp_media_s, true,
	"97", "speex", 8000, 1,
	 NULL, NULL, NULL, false, NULL);

    err = sdp_format_add(NULL, sdp_media, false,
	"0", "PCMU", 8000, 1,
	 NULL, NULL, NULL, false, NULL);

    err = sdp_format_add(NULL, sdp_media_s, false,
	"0", "PCMU", 8000, 1,
	 NULL, NULL, NULL, false, NULL);

    // dump local
    const struct sdp_format *fmt;

    fmt = sdp_media_lformat(sdp_media, 97);
    if (!fmt) {
        re_printf("no local media format found\n");
        return;
    }

    re_printf("local format: %s/%u/%u (payload type: %u)\n",
		  fmt->name, fmt->srate, fmt->ch, fmt->pt);

    memset(srtp_in_key, 0xFC, 64);
    memset(srtp_out_key, 0xFC, 64);

    ts = 0;
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
    tmr_cancel(&rtp_tmr);

    if(rtp) {
        mem_deref(rtp);
	rtp = NULL;
    }

    if(send_io_ctx) {
        ((rtp_send_ctx*)send_io_ctx)->magic = 0;
        mem_deref(((rtp_send_ctx*)send_io_ctx)->mb);
        free(send_io_ctx);
    }

    if(media) {
        media_snd_stream_stop(media);
	media_snd_stream_close(media);
	media = NULL;
    }


    if(dec_state) {
	speex_decoder_destroy(dec_state);
	dec_state = NULL;
    }

    if(enc_state) {
	speex_encoder_destroy(enc_state);
	enc_state = NULL;
    }

    /* XXX: free sdp session and both sdp medias */

    speex_bits_reset(&enc_bits);
    speex_bits_reset(&dec_bits);

    speex_bits_destroy(&enc_bits);
    speex_bits_destroy(&dec_bits);

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

    if(!media)
        [self open];

    if(!media)
        return -1;

    tmr_init(&rtp_tmr);
    rtp_send_ctx *send_ctx = malloc(sizeof(rtp_send_ctx));
    send_ctx->media = media;
    send_ctx->frame_size = frame_size;
    send_ctx->read_off = 0;
    send_ctx->enc_bits = &enc_bits;
    send_ctx->enc_state = enc_state;
    send_ctx->mb = mbuf_alloc(200 + RTP_HEADER_SIZE);
    send_ctx->rtp = rtp;
    send_ctx->pt = pt;
    send_ctx->ts = ts;
    send_ctx->srtp_out = srtp_out;
    send_ctx->dst = dst;
    send_ctx->tmr = &rtp_tmr;
    send_ctx->magic = 0x1ee1F00D;

    send_io_ctx = send_ctx;
    tmr_start(&rtp_tmr, 10, rtp_send_io, send_io_ctx);

    ok = media_snd_stream_start(media);

    return (bool)ok;
}

- (void) rtpData: (struct mbuf*)mb header:(const struct rtp_header *)hdr
{

    if(!media)
        return;

    int err, len;
    if(srtp_in) {
        mbuf_advance(mb, -RTP_HEADER_SIZE);
        len = mbuf_get_left(mb);
        err = srtp_unprotect(srtp_in, mbuf_buf(mb), &len);
        if(err) {
            printf("srtp unprotect fail %d\n", err);
            return;
        }
        mbuf_advance(mb, RTP_HEADER_SIZE);
        len -= RTP_HEADER_SIZE;
    } else {
        len = mbuf_get_left(mb);
    }

    speex_bits_read_from(&dec_bits, mbuf_buf(mb), len);

    ajitter_packet * ajp;
    ajp = ajitter_put_ptr(media->play_jitter);
    speex_decode_int(dec_state, &dec_bits, (spx_int16_t*)ajp->data);
    ajp->left = frame_size;
    ajp->off = 0;

    ajitter_put_done(media->play_jitter, ajp->idx, (double)hdr->seq);

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

    struct mbuf *mb = mbuf_alloc(0);
    mbuf_write_mem(mb, mbuf_buf(offer), mbuf_get_space(offer));
    mb->pos = 0;

    err = sdp_decode(sdp, mb, false);
    if(err) {
        NSLog(@"cant decode answer");
        goto out;
    }

    [self sdpFormats];

out:
    mem_deref(mb);
    return err;
}

- (void) sdpFormats {

    const struct sdp_format *fmt = NULL;
    const struct sdp_media *md;
    const struct list *list;
    struct le *le;

    list = sdp_session_medial(sdp, false);

    for(le = list->head; le && !fmt; le = le->next) {
        md = le->data;
        fmt = sdp_media_rformat(md, NULL);
    }

    if (!fmt) {
        re_printf("no common media format found\n");
        return;
    }

    struct pl key, crypt_n, crypt_s, key_m, key_param;
    char *crypt;
    size_t klen = 64;

    if(md == sdp_media_s) {
        crypt = sdp_media_rattr(md, "crypto");
        if(!crypt) {
            printf("SAVP without crypto param? wtf\n");
            return;
        }
        re_regex(crypt, strlen(crypt), "[0-9]+ [a-zA-Z0-9_]+ [a-zA-Z]+:[^]*[|]*[^]*",
                &crypt_n, &crypt_s, &key_m, &key, &key_param);

        if(!key.l)
            return;

        re_printf("n:%r s:%r m:%r key:%r\n", &crypt_n, &crypt_s, &key_m, &key);
        base64_decode(key.p, key.l, srtp_in_key, &klen);
        printf("key len %d\n", klen);

        [self setupSRTP];
    }

    re_printf("SDP peer address: %J\n", sdp_media_raddr(md));

    re_printf("SDP media format: %s/%u/%u (payload type: %u)\n",
		  fmt->name, fmt->srate, fmt->ch, fmt->pt);

    re_printf("SDP crypt %s\n", sdp_media_rattr(md, "crypto"));

    dst = sdp_media_raddr(md);
    pt = fmt->pt;
}

@end
