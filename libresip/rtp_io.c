#include "rtp_io.h"

void rtp_send_io(void *varg)
{
    int len = 0, err=0;
    rtp_send_ctx * arg = varg;
    if(arg->magic != 0x1ee1F00D)
        return;

    struct mbuf *mb = arg->mb;
    char *obuf;
restart:
    obuf = ajitter_get_chunk(arg->record_jitter, arg->frame_size, &arg->ts);
    if(!obuf)
        goto timer;

    len = speex_encode_int(arg->enc_state, (spx_int16_t*)obuf, &arg->enc_bits);

    mb->pos = RTP_HEADER_SIZE;
    len = speex_bits_write(&arg->enc_bits, mbuf_buf(mb), 200); // XXX: constant
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

    speex_bits_reset(&arg->enc_bits);
    goto restart;

timer:
    tmr_start(&arg->tmr, 4, rtp_send_io, varg);
}

void rtp_recv_io (const struct sa *src, const struct rtp_header *hdr,
        struct mbuf *mb, void *varg)
{
    rtp_recv_ctx * arg = varg;

    int err, len;
    if(arg->srtp_in) {
        mbuf_advance(mb, -RTP_HEADER_SIZE);
        len = mbuf_get_left(mb);
        err = srtp_unprotect(arg->srtp_in, mbuf_buf(mb), &len);
        if(err) {
            printf("srtp unprotect fail %d\n", err);
            return;
        }
        mbuf_advance(mb, RTP_HEADER_SIZE);
        len -= RTP_HEADER_SIZE;
    } else {
        len = mbuf_get_left(mb);
    }

    speex_bits_read_from(&arg->dec_bits, mbuf_buf(mb), len);

    ajitter_packet * ajp;
    ajp = ajitter_put_ptr(arg->play_jitter);
    speex_decode_int(arg->dec_state, &arg->dec_bits, (spx_int16_t*)ajp->data);
    ajp->left = arg->frame_size;
    ajp->off = 0;

    ajitter_put_done(arg->play_jitter, ajp->idx, (double)hdr->seq);
}

rtp_send_ctx* rtp_send_init(fmt_t fmt) {
    rtp_send_ctx *send_ctx = malloc(sizeof(rtp_send_ctx));
    tmr_init(&send_ctx->tmr);
    speex_bits_init(&send_ctx->enc_bits);
    speex_bits_reset(&send_ctx->enc_bits);
    send_ctx->enc_state = speex_encoder_init(&speex_nb_mode);
    speex_decoder_ctl(send_ctx->enc_state, SPEEX_GET_FRAME_SIZE, &send_ctx->frame_size); 
    send_ctx->frame_size *= 2;
    send_ctx->mb = mbuf_alloc(200 + RTP_HEADER_SIZE);
    send_ctx->fmt = fmt;
    send_ctx->ts = 0;

    send_ctx->magic = 0x1ee1F00D;
    return send_ctx;
}

rtp_recv_ctx* rtp_recv_init() {
    rtp_recv_ctx *ctx = malloc(sizeof(rtp_recv_ctx));

    ctx->dec_state = speex_decoder_init(&speex_nb_mode);
    speex_bits_init(&ctx->dec_bits);

    speex_decoder_ctl(ctx->dec_state, SPEEX_GET_FRAME_SIZE, &ctx->frame_size); 
    ctx->frame_size *= 2;

    ctx->magic = 0x1ab1D00F;
    return ctx;
}

void rtp_send_start(rtp_send_ctx* ctx) {
    tmr_start(&ctx->tmr, 10, rtp_send_io, ctx);
}


void rtp_send_stop(rtp_send_ctx* ctx) {
    tmr_cancel(&ctx->tmr);
    ctx->magic = 0;
    speex_bits_reset(&ctx->enc_bits);
    speex_bits_destroy(&ctx->enc_bits);
    speex_encoder_destroy(ctx->enc_state);

    mem_deref(ctx->mb);
    free(ctx);
}

void rtp_recv_stop(rtp_recv_ctx* ctx) {
    speex_bits_reset(&ctx->dec_bits);
    speex_bits_destroy(&ctx->dec_bits);

    speex_decoder_destroy(ctx->dec_state);

    free(ctx);
}
