#ifndef TCMEDIA_H
#define TCMEDIA_H

struct tcmedia;
struct mbuf;
struct uac;

enum media_e {
    MEDIA_ICE_OK,
    MEDIA_KA_WARN,
    MEDIA_KA_FAIL
};

typedef void(media_h)(struct tcmedia*, enum media_e event, int earg, void*arg);

int tcmedia_answer(struct tcmedia*media, struct mbuf *offer);
int tcmedia_offer(struct tcmedia*media, struct mbuf *offer, struct mbuf**ret);
int tcmedia_get_offer(struct tcmedia*media, struct mbuf**ret);

int tcmedia_start(struct tcmedia*media);
void tcmedia_stop(struct tcmedia *media);

int tcmedia_alloc(struct tcmedia** rp, struct uac*uac, call_dir_t cdir);

void tcmedia_handler(struct tcmedia*, media_h, void*arg);

#endif
