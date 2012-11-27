#include "rtp_io.h"

typedef struct {
    int magic;
    srtp_t srtp_out;
    ajitter *record_jitter;
    struct rtp_sock *rtp;
    struct sa *dst;
    int pt;
    /// specific
    struct tmr tmr;
    fmt_t fmt;
    struct mbuf *mb;
    int ts;
    // codec
} rtp_send_opus_ctx;

typedef struct {
    int magic;
    srtp_t srtp_in;
    ajitter *play_jitter;
    fmt_t fmt;
    // codec
} rtp_recv_opus_ctx;

void rtp_recv_opus(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *varg)
{
    printf("recv opus...\n");
}

rtp_send_ctx* rtp_send_opus_init() {

    rtp_send_ctx *send_ctx = malloc(sizeof(rtp_send_ctx));
    tmr_init(&send_ctx->tmr);
    send_ctx->mb = mbuf_alloc(400 + RTP_HEADER_SIZE);
    send_ctx->fmt = FMT_OPUS;

    send_ctx->magic = 0x1ee1F00D;
    return send_ctx;
}

rtp_recv_ctx * rtp_recv_opus_init()
{
    rtp_recv_ctx *ctx = malloc(sizeof(rtp_recv_ctx));
    ctx->srtp_in = NULL;
    ctx->play_jitter = NULL;
    ctx->fmt = FMT_OPUS;

    ctx->magic = 0x1ab1D00F;
    return ctx;
}
