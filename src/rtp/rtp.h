#ifndef TCSIP_RTP_H
#define TCSIP_RTP_H 1

struct rtp;
enum fmt;

int rtp_io_alloc(struct rtp **rp, enum fmt fmt);
size_t rtp_encode_send(struct rtp *rtp, const uint8_t *data, size_t sz);

#endif
