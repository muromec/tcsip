#ifndef TCMEDIA_H
#define TCMEDIA_H

struct tcmedia;
struct mbuf;
struct uac;

typedef void(icehandler)(struct tcmedia*, void*arg);

int tcmedia_answer(struct tcmedia*media, struct mbuf *offer);
int tcmedia_offer(struct tcmedia*media, struct mbuf *offer, struct mbuf**ret);
int tcmedia_get_offer(struct tcmedia*media, struct mbuf**ret);

int tcmedia_start(struct tcmedia*media);
void tcmedia_stop(struct tcmedia *media);

int tcmedia_alloc(struct tcmedia** rp, struct uac*uac, call_dir_t cdir);

void tcmedia_iceok(struct tcmedia*, icehandler ih, void*arg);

#endif
