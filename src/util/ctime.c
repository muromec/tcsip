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
	char *ret;
	struct tm tm;
    struct timeval *tv = arg;
	struct pl tmp;
	pl_dup(&tmp, &hdr->val);
	ret = strptime(tmp.p, "%a, %d %b %Y %H:%M:%S GMT", &tm);

    if(ret) {
        tv->tv_usec = 0;
    } else {
        ret = strptime(tmp.p, "%a, %d %b %Y %H:%M:%S.", &tm);
        if(ret) {
            sscanf(ret, "%06u GMT", &tv->tv_usec);
        }
    }

	mem_deref((void*)tmp.p);

	if(ret) {
	    tv->tv_sec = timegm(&tm);
	    return false;
	}

	return true;
}

void sipmsg_parse_date(const struct sip_msg *msg, struct timeval *tv)
{
    tv->tv_sec = 0;
    sip_msg_hdr_apply(msg, true, SIP_HDR_DATE, find_date, tv);
    if(tv->tv_sec <= 0) {
        gettimeofday(tv, NULL);
    }
}
