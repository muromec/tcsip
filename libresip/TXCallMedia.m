//
//  TXCallMedia.m
//  Texr
//
//  Created by Ilya Petrov on 9/28/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXCallMedia.h"
#include "ajitter.h"

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

    rtp_listen(&rtp, IPPROTO_UDP, laddr, 6000, 7000, false,
            rtp_recv_io, NULL, &recv_io_arg);

    laddr = rtp_local(rtp);

    /* create SDP session */
    err = sdp_session_alloc(&sdp, laddr);
    err = sdp_media_add(&sdp_media_s, sdp, "audio", sa_port(laddr), "RTP/SAVP");
    err = sdp_media_add(&sdp_media, sdp, "audio", sa_port(laddr), "RTP/AVP");

    rand_bytes(srtp_out_key, 30);
    char crypto_line[] = "1 AES_CM_128_HMAC_SHA1_80 inline:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX      ";
    size_t klen = 40;
    base64_encode(srtp_out_key, 30, &crypto_line[33], &klen);
    sdp_media_set_lattr(sdp_media_s, true, "crypto", crypto_line);

    err = sdp_format_add(NULL, sdp_media, true,
	"97", "speex", 8000, 1,
	 NULL, NULL, NULL, false, NULL);

    err = sdp_format_add(NULL, sdp_media_s, true,
	"97", "speex", 8000, 1,
	 NULL, NULL, NULL, false, NULL);

    err = sdp_format_add(NULL, sdp_media_s, false,
	"101", "opus", 48000, 2,
	 NULL, NULL, NULL, false, NULL);

    err = sdp_format_add(NULL, sdp_media, false,
	"101", "opus", 48000, 2,
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

    if(rtp) {
        mem_deref(rtp);
	rtp = NULL;
    }

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

    if(!media)
        [self open];

    if(!media)
        return -1;

    rtp_send_ctx *send_ctx = rtp_send_init(fmt);
    send_ctx->record_jitter = media->record_jitter;
    send_ctx->rtp = rtp;
    send_ctx->pt = pt;
    send_ctx->srtp_out = srtp_out;
    send_ctx->dst = dst;

    send_io_ctx = send_ctx;

    ok = media_snd_stream_start(media);
    rtp_send_start(send_ctx);

    // set recv side
    rtp_recv_ctx * recv_ctx = rtp_recv_init(fmt);
    recv_ctx->srtp_in = srtp_in;
    recv_ctx->play_jitter = media->play_jitter;

    recv_io_arg.ctx = recv_ctx;
    recv_io_arg.handler = rtp_recv_func(fmt);

    return (bool)ok;
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
        re_regex(crypt, strlen(crypt), "[0-9]+ [a-zA-Z0-9_]+ [a-zA-Z]+:[A-Za-z0-9+/]*|*[^]*",
                &crypt_n, &crypt_s, &key_m, &key, &key_param);

        if(key.l!=40) {
            printf("invalid base64 key len %d\n", key.l);
            return;
        }

        base64_decode(key.p, key.l, srtp_in_key, &klen);
        if(klen!=30) {
            printf("invalid key len %d\n", klen);
            return;
        }

        [self setupSRTP];
    }

    re_printf("SDP peer address: %J\n", sdp_media_raddr(md));

    re_printf("SDP media format: %s/%u/%u (payload type: %u)\n",
		  fmt->name, fmt->srate, fmt->ch, fmt->pt);

    re_printf("SDP crypt %s\n", sdp_media_rattr(md, "crypto"));

    dst = sdp_media_raddr(md);

    [self set_format: fmt->name];
    pt = fmt->pt;
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
