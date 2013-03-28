#include "g711.h"
#include "rtp_io.h"
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
    int frame_size;
} rtp_send_pcmu_ctx;

typedef struct {
    int magic;
    srtp_t srtp_in;
    ajitter *play_jitter;
    fmt_t fmt;
    int last_read;
    // codec
    int frame_size;
} rtp_recv_pcmu_ctx;

rtp_recv_ctx * rtp_recv_pcmu_init()
{
    rtp_recv_pcmu_ctx *ctx = malloc(sizeof(rtp_recv_pcmu_ctx));
    ctx->srtp_in = NULL;
    ctx->play_jitter = NULL;

    ctx->frame_size = 320;
    ctx->magic = 0x1ab1D00F;
    ctx->fmt = FMT_PCMU;
    ctx->last_read = 0;
    return (rtp_recv_ctx*)ctx;
}

void rtp_send_pcmu(void *varg)
{
    int len = 0, err=0;
    rtp_send_pcmu_ctx * arg = varg;
    if(arg->magic != 0x1ee1F00D)
        return;

    struct mbuf *mb = arg->mb;
    short *ibuf;
    char *obuf;
restart:
    ibuf = (short*)ajitter_get_chunk(arg->record_jitter, arg->frame_size, &arg->ts);

    if(!ibuf)
        goto timer;

    mb->pos = RTP_HEADER_SIZE;
    obuf = (char*)mbuf_buf(mb);

    len = arg->frame_size / 2;
    mb->pos = 0;
    mb->end = len + RTP_HEADER_SIZE;

    int i;
    for(i=0;i<len;i++) {
	*obuf = MuLaw_Encode(*ibuf);

	obuf++;
	ibuf++;
    }

    err = rtp_encode(arg->rtp, 0, arg->pt, arg->ts, mb);
    mb->pos = 0;

    rtp_p(arg->srtp_out, mb);

    udp_send(rtp_sock(arg->rtp), arg->dst, mb);

    goto restart;

timer:
    tmr_start(&arg->tmr, 4, rtp_send_pcmu, varg);
}

void rtp_recv_pcmu(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *varg)
{
    rtp_recv_pcmu_ctx * arg = varg;

    int len;
    len = rtp_un(arg->srtp_in, mb);
    if(len<0)
	    return;

    if(len > arg->play_jitter->csize)
	    return;

    int i;
    ajitter_packet * ajp;
    ajp = ajitter_put_ptr(arg->play_jitter);
    short *obuf = (short*)ajp->data;
    unsigned char *ibuf = mbuf_buf(mb);
    for(i=0; i< len; i++) {
	*obuf = MuLaw_Decode(*ibuf);
	obuf++;
	ibuf++;
    }

    ajp->left = len * 2;
    ajp->off = 0;

    ajitter_put_done(arg->play_jitter, ajp->idx, (double)hdr->seq);

    struct timeval tv;
    if(!gettimeofday(&tv, NULL))
        arg->last_read = (int)tv.tv_sec;
}

rtp_send_ctx* rtp_send_pcmu_init() {

    rtp_send_pcmu_ctx *send_ctx = malloc(sizeof(rtp_send_pcmu_ctx));
    tmr_init(&send_ctx->tmr);
    send_ctx->frame_size = 320;
    send_ctx->mb = mbuf_alloc(400 + RTP_HEADER_SIZE);
    send_ctx->fmt = FMT_PCMU;
    send_ctx->ts = 0;

    send_ctx->magic = 0x1ee1F00D;
    return (rtp_send_ctx*)send_ctx;
}
