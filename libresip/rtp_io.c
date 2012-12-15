#include "rtp_io.h"

void rtp_recv_io (const struct sa *src, const struct rtp_header *hdr,
        struct mbuf *mb, void *varg)
{

    rtp_recv_arg * _arg = varg;

    _arg->handler(src, hdr, mb, _arg->ctx);
}

void rtp_p(srtp_t srtp, struct mbuf *mb)
{
    int err, len;
    if(srtp) {
	len = (int)mbuf_get_left(mb);
        err = srtp_protect(srtp, mbuf_buf(mb), &len);
        if(err)
            printf("srtp failed %d\n", err);
        mb->end = len;
    }
}

int rtp_un(srtp_t srtp, struct mbuf *mb)
{
    int err, len;
    if(srtp) {
        mbuf_advance(mb, -RTP_HEADER_SIZE);
        len = (int)mbuf_get_left(mb);
        err = srtp_unprotect(srtp, mbuf_buf(mb), &len);
        if(err) {
            printf("srtp unprotect fail %d\n", err);
            return -1;
        }
        mbuf_advance(mb, RTP_HEADER_SIZE);
        len -= RTP_HEADER_SIZE;
    } else {
        len = (int)mbuf_get_left(mb);
    }
    return len;
}


rtp_recv_h * rtp_recv_func(fmt_t fmt)
{
    switch(fmt) {
    case FMT_SPEEX:
	    return rtp_recv_speex;
    case FMT_PCMU:
	    return rtp_recv_pcmu;
    case FMT_OPUS:
	    return rtp_recv_opus;
    }
}

rtp_send_h * rtp_send_func(fmt_t fmt)
{
    switch(fmt) {
    case FMT_SPEEX:
	    return rtp_send_io;
    case FMT_PCMU:
	    return rtp_send_pcmu;
    case FMT_OPUS:
	    return rtp_send_opus;
    }
}

rtp_send_ctx* rtp_send_init(fmt_t fmt) {
    switch(fmt) {
    case FMT_SPEEX:
	    return rtp_send_speex_init();
    case FMT_PCMU:
	    return rtp_send_pcmu_init();
    case FMT_OPUS:
	    return rtp_send_opus_init();
    }
}



rtp_recv_ctx* rtp_recv_init(fmt_t fmt)
{

    switch(fmt) {
    case FMT_SPEEX:
	    return rtp_recv_speex_init();
    case FMT_PCMU:
	    return rtp_recv_pcmu_init();
    case FMT_OPUS:
	    return rtp_recv_opus_init();
    }
}


void rtp_send_start(rtp_send_ctx* ctx) {
    tmr_start(&ctx->tmr, 10, rtp_send_func(ctx->fmt), ctx);
}


void rtp_send_stop(rtp_send_ctx* ctx) {
    tmr_cancel(&ctx->tmr);
    ctx->magic = 0;

    switch(ctx->fmt) {
    case FMT_SPEEX:
	    rtp_send_speex_stop(ctx);
	    break;
    case FMT_OPUS:
    case FMT_PCMU:
	    break;
    }

    mem_deref(ctx->mb);
    free(ctx);
}

void rtp_recv_stop(rtp_recv_ctx* ctx) {
    switch(ctx->fmt) {
    case FMT_SPEEX:
            rtp_recv_speex_stop(ctx);
	    break;
    case FMT_OPUS:
    case FMT_PCMU:
	    break;
    }

    free(ctx);
}