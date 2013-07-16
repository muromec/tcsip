#include <srtp/srtp.h>
#include <speex/speex.h>

#include "re.h"
#include "ajitter.h"

#if __APPLE__
#define send_tmr(_f) tmr_start(&arg->tmr, 4, _f, varg);
#else
#define send_tmr(_f) (void*)1;
#endif

typedef enum {
	FMT_SPEEX,
	FMT_PCMU,
	FMT_OPUS,
} fmt_t;

typedef struct {
    void *ctx;
    rtp_recv_h *handler;

} rtp_recv_arg;

typedef struct {
    int magic;
    srtp_t srtp_out;
    ajitter *record_jitter;
    struct rtp_sock *rtp;
    struct sa *dst;
    int pt;
    struct tmr tmr;
    fmt_t fmt;
    struct mbuf *mb;
} rtp_send_ctx;

typedef struct {
    int magic;
    srtp_t srtp_in;
    ajitter *play_jitter;
    fmt_t fmt;
    int last_read;
} rtp_recv_ctx;

rtp_send_ctx* rtp_send_init(fmt_t fmt);
void rtp_send_start(rtp_send_ctx* ctx);
void rtp_send_stop(rtp_send_ctx* ctx);


typedef void (rtp_send_h)(void *varg);

void rtp_recv_io (const struct sa *src, const struct rtp_header *hdr,
        struct mbuf *mb, void *varg);

void rtcp_recv_io(const struct sa *src, struct rtcp_msg *msg,
			   void *arg);

rtp_recv_ctx* rtp_recv_init(fmt_t fmt);
void rtp_recv_stop(rtp_recv_ctx* ctx);

void rtp_recv_speex(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *varg);

void rtp_recv_pcmu(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *varg);

rtp_recv_h * rtp_recv_func(fmt_t fmt);
rtp_send_h * rtp_send_func(fmt_t fmt);


void rtp_p(srtp_t srtp, struct mbuf *mb);
int rtp_un(srtp_t srtp, struct mbuf *mb);

// implementations
struct _rtp_send_speex_ctx;

void rtp_recv_speex(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *varg);
void rtp_send_io(void *varg);
void rtp_send_speex_stop(rtp_send_ctx *ctx);
void rtp_recv_speex_stop(rtp_recv_ctx *ctx);
rtp_send_ctx* rtp_send_speex_init();
rtp_recv_ctx * rtp_recv_speex_init();

void rtp_recv_pcmu(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *varg);
void rtp_send_pcmu(void *varg);
rtp_send_ctx* rtp_send_pcmu_init();
rtp_recv_ctx * rtp_recv_pcmu_init();

void rtp_recv_opus(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *varg);
rtp_send_ctx* rtp_send_opus_init();
rtp_recv_ctx * rtp_recv_opus_init();
void rtp_send_opus(void *varg);

