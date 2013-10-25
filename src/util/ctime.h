#ifndef TCSIP_UTIL_TIME_H
#define TCSIP_UTIL_TIME_H

struct tm;
struct sip_hdr;
struct sip_msg;
struct timeval;

#if ANDROID
time_t timegm(struct tm * const t);
#endif

bool find_date(const struct sip_hdr *hdr, const struct sip_msg *msg, void *arg);
void sipmsg_parse_date(const struct sip_msg *msg, struct timeval *tv);
#endif
