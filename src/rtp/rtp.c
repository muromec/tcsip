#include "re.h"
#include "rtp_io.h"

struct rtp {
    rtp_send_ctx *sender;
    fmt_t fmt;
    rtp_send_h *send_h;
};

static void destruct(void *arg)
{
    struct rtp *rtp = arg;
    rtp_send_stop(rtp->sender);
}

int rtp_io_alloc(struct rtp **rp, fmt_t fmt)
{
    struct rtp *rtp;

    rtp = mem_zalloc(sizeof(struct rtp), destruct);
    if(!rtp)
        return -ENOMEM;

    rtp->fmt = fmt;
    rtp->send_h = rtp_send_func(fmt);
    rtp->sender = rtp_send_init(fmt);

    *rp = rtp;

    return 0;
}

size_t rtp_encode_send(struct rtp *rtp, const uint8_t *data, size_t sz)
{
    rtp->send_h(rtp->sender, data, sz);
    return sz;
}
