#include "rtp_io.h"
#include <opus/opus.h>
#include <sys/time.h>

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
    OpusEncoder *enc;
    int frame_size;
} rtp_send_opus_ctx;

typedef struct {
    int magic;
    srtp_t srtp_in;
    ajitter *play_jitter;
    fmt_t fmt;
    int last_read;
    // codec
    OpusDecoder *dec;
} rtp_recv_opus_ctx;

void rtp_recv_opus(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *varg)
{
    rtp_recv_opus_ctx * arg = varg;

    int len;
    len = rtp_un(arg->srtp_in, mb);
    if(len<0)
	    return;

    ajitter_packet * ajp;

    ajp = ajitter_put_ptr(arg->play_jitter);

    unsigned char *inb = mbuf_buf(mb);
    len = opus_decode(arg->dec, inb, len, (short*)ajp->data, 160, 0);
    ajp->left = len * 2;
    ajp->off = 0;

    ajitter_put_done(arg->play_jitter, ajp->idx, (double)hdr->seq);

    struct timeval tv;
    if(!gettimeofday(&tv, NULL))
        arg->last_read = (int)tv.tv_sec;
}

void rtp_send_opus(void *varg)
{
    int len = 0, err=0;
    rtp_send_opus_ctx * arg = varg;
    if(arg->magic != 0x1ee1F00D)
        return;

    struct mbuf *mb = arg->mb;
    unsigned char *obuf;
    short *ibuf;
restart:
    ibuf = (short*)ajitter_get_chunk(arg->record_jitter, arg->frame_size, &arg->ts);

    if(!ibuf)
        goto timer;

    mb->pos = RTP_HEADER_SIZE;
    obuf = mbuf_buf(mb);

    len = arg->frame_size / 2;
    mb->pos = 0;

    len = opus_encode(arg->enc, ibuf, len, obuf, (int)mb->size - RTP_HEADER_SIZE);
    mb->end = len + RTP_HEADER_SIZE;

    err = rtp_encode(arg->rtp, 0, arg->pt, arg->ts, mb);
    mb->pos = 0;

    rtp_p(arg->srtp_out, mb);

    udp_send(rtp_sock(arg->rtp), arg->dst, mb);

    goto restart;

timer:
    send_tmr(rtp_send_opus);
}

rtp_send_ctx* rtp_send_opus_init() {

    int err;
    rtp_send_opus_ctx *send_ctx = malloc(sizeof(rtp_send_opus_ctx));
    tmr_init(&send_ctx->tmr);
    send_ctx->mb = mbuf_alloc(400 + RTP_HEADER_SIZE);
    send_ctx->fmt = FMT_OPUS;
    send_ctx->frame_size = 320;
    send_ctx->enc = opus_encoder_create(8000, 1, OPUS_APPLICATION_VOIP, &err);

    send_ctx->magic = 0x1ee1F00D;
    return (rtp_send_ctx*)send_ctx;
}

rtp_recv_ctx * rtp_recv_opus_init()
{
    int err, size;
    rtp_recv_opus_ctx *ctx = malloc(sizeof(rtp_recv_opus_ctx));
    ctx->srtp_in = NULL;
    ctx->play_jitter = NULL;
    ctx->fmt = FMT_OPUS;
    ctx->last_read = 0;
    size = opus_decoder_get_size(1);
    ctx->dec = malloc(size);
    err = opus_decoder_init(ctx->dec, 8000, 1);

    ctx->magic = 0x1ab1D00F;
    return (rtp_recv_ctx*)ctx;
}
