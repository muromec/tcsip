#include <srtp.h>
#include <speex/speex.h>

#include "re.h"
#include "ajitter.h"

typedef enum {
	FMT_NONE,
	FMT_SPEEX,
	FMT_PCMU,
} fmt_t;

typedef struct {
    int magic;
    ajitter *record_jitter;
    int frame_size;
    SpeexBits enc_bits;
    void *enc_state;
    struct mbuf *mb;
    struct rtp_sock *rtp;
    int pt;
    int ts;
    srtp_t srtp_out;
    struct sa *dst;
    struct tmr tmr;
    fmt_t fmt;
} rtp_send_ctx;

typedef struct {
    int magic;
    srtp_t srtp_in;
    SpeexBits dec_bits;
    ajitter *play_jitter;
    void *dec_state;
    int frame_size;
    fmt_t fmt;
} rtp_recv_ctx;

typedef struct {
    void *ctx;
} rtp_recv_arg;

void rtp_send_io(void *varg);
rtp_send_ctx* rtp_send_init(fmt_t fmt);
void rtp_send_start(rtp_send_ctx* ctx);
void rtp_send_stop(rtp_send_ctx* ctx);

void rtp_recv_io (const struct sa *src, const struct rtp_header *hdr,
        struct mbuf *mb, void *varg);

rtp_recv_ctx* rtp_recv_init(fmt_t fmt);
void rtp_recv_stop(rtp_recv_ctx* ctx);

