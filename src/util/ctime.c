#include "re.h"
#include <sys/time.h>
#include <time.h>
#include "util/ctime.h"

#if ANDROID
#define CHAR_BIT 8
#include <time64.h>
time_t timegm(struct tm * const t) {
    static const time_t kTimeMax = ~(1 << (sizeof (time_t) * CHAR_BIT - 1));
    static const time_t kTimeMin = (1 << (sizeof (time_t) * CHAR_BIT - 1));
    time64_t result = timegm64(t);
    if (result < kTimeMin || result > kTimeMax)
        return -1;
    return result;
}
#endif

bool find_date(const struct sip_hdr *hdr, const struct sip_msg *msg, void *arg)
{
	int *stamp = arg;
	char *ret;
	struct tm tv;
	struct pl tmp;
	pl_dup(&tmp, &hdr->val);
	ret = strptime(tmp.p, "%a, %d %b %Y %H:%M:%S GMT", &tv);

	mem_deref((void*)tmp.p);

	if(ret) {
	    *stamp = (int)timegm(&tv);
	    return false;
	}

	return true;
}

time_t sipmsg_parse_date(const struct sip_msg *msg)
{
    time_t ts = 0;
    struct timeval now;

    sip_msg_hdr_apply(msg, true, SIP_HDR_DATE, find_date, &ts);
    if(ts <= 0) {
        gettimeofday(&now, NULL);
        ts = now.tv_sec;
    }
    return ts;
}
