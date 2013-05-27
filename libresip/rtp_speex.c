#include "rtp_io.h"
#include <sys/time.h>

typedef struct _rtp_send_speex_ctx {
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
    int frame_size;
    SpeexBits enc_bits;
    void *enc_state;
} rtp_send_speex_ctx;

typedef struct _rtp_recv_speex_ctx {
    int magic;
    srtp_t srtp_in;
    ajitter *play_jitter;
    fmt_t fmt;
    int last_read;
    // codec
    SpeexBits dec_bits;
    void *dec_state;
    int frame_size;
} rtp_recv_speex_ctx;

void send_stats(int rbytes, int nbytes){
    static int cc = 0, rb = 0, nb = 0;
    cc++;
    rb += rbytes;
    nb += nbytes;
    if((cc%100)==0) fprintf(stderr, "send[%d] by %d/%d bytes %d/%d\n", cc, rbytes, nbytes, rb, nb);
}

#if RTP_DEBUG
#define dsend_stats(__r, __n) send_stats(__r, __n)
#else
#define dsend_stats(__r, __n) {}
#endif

void rtp_recv_speex(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *varg)
{
    rtp_recv_speex_ctx * arg = varg;

    int len;
    len = rtp_un(arg->srtp_in, mb);
    if(len<0)
	    return;

    speex_bits_read_from(&arg->dec_bits, (char*)mbuf_buf(mb), len);

    ajitter_packet * ajp;

    ajp = ajitter_put_ptr(arg->play_jitter);
    int err = speex_decode_int(arg->dec_state, &arg->dec_bits, (spx_int16_t*)ajp->data);
    ajp->left = arg->frame_size;
    ajp->off = 0;
    if(err!=0) printf("decode err %d\n", err);

    ajitter_put_done(arg->play_jitter, ajp->idx, (double)hdr->seq);

    struct timeval tv;
    if(!gettimeofday(&tv, NULL))
        arg->last_read = (int)tv.tv_sec;
}

void rtp_send_io(void *varg)
{
    int len = 0, err=0;
    rtp_send_speex_ctx * arg = varg;
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
    len = speex_bits_write(&arg->enc_bits, (char*)mbuf_buf(mb), 200); // XXX: constant
    len += RTP_HEADER_SIZE;
    mb->end = len;
    mbuf_advance(mb, -RTP_HEADER_SIZE);

    err = rtp_encode(arg->rtp, 0, arg->pt, arg->ts, mb);
    mb->pos = 0;

    rtp_p(arg->srtp_out, mb);

    dsend_stats(arg->frame_size, len);

    udp_send(rtp_sock(arg->rtp), arg->dst, mb);

    speex_bits_reset(&arg->enc_bits);
    goto restart;

timer:
#if __APPLE__
    tmr_start(&arg->tmr, 4, rtp_send_io, varg);
#else
    (void*)1;
#endif
}

rtp_send_ctx* rtp_send_speex_init() {

    rtp_send_speex_ctx *send_ctx = malloc(sizeof(rtp_send_speex_ctx));
    tmr_init(&send_ctx->tmr);
    speex_bits_init(&send_ctx->enc_bits);
    speex_bits_reset(&send_ctx->enc_bits);
    send_ctx->enc_state = speex_encoder_init(&speex_nb_mode);
    speex_encoder_ctl(send_ctx->enc_state, SPEEX_GET_FRAME_SIZE, &send_ctx->frame_size);
    send_ctx->frame_size *= 2;
    send_ctx->mb = mbuf_alloc(400 + RTP_HEADER_SIZE);
    send_ctx->fmt = FMT_SPEEX;
    send_ctx->ts = 0;

    send_ctx->magic = 0x1ee1F00D;
    return (rtp_send_ctx*)send_ctx;
}

rtp_recv_ctx * rtp_recv_speex_init()
{
    rtp_recv_speex_ctx *ctx = malloc(sizeof(rtp_recv_speex_ctx));
    ctx->srtp_in = NULL;
    ctx->play_jitter = NULL;

    ctx->dec_state = speex_decoder_init(&speex_nb_mode);
    speex_bits_init(&ctx->dec_bits);

    speex_decoder_ctl(ctx->dec_state, SPEEX_GET_FRAME_SIZE, &ctx->frame_size); 
    ctx->frame_size *= 2;
    ctx->fmt = FMT_SPEEX;
    ctx->last_read = 0;

    ctx->magic = 0x1ab1D00F;
    return (rtp_recv_ctx*)ctx;
}

void rtp_send_speex_stop(rtp_send_ctx *_ctx)
{
    rtp_send_speex_ctx *ctx = (rtp_send_speex_ctx*)_ctx;
    speex_bits_reset(&ctx->enc_bits);
    speex_bits_destroy(&ctx->enc_bits);
    speex_encoder_destroy(ctx->enc_state);
}

void rtp_recv_speex_stop(rtp_recv_ctx *_ctx)
{
    rtp_recv_speex_ctx *ctx = (rtp_recv_speex_ctx*)_ctx;
    speex_bits_reset(&ctx->dec_bits);
    speex_bits_destroy(&ctx->dec_bits);

    speex_decoder_destroy(ctx->dec_state);
}

