#ifndef TCMEDIA_H
#define TCMEDIA_H

struct tcmedia;
struct mbuf;

int tcmedia_answer(struct tcmedia*media, struct mbuf *offer);
int tcmedia_offer(struct tcmedia*media, struct mbuf *offer, struct mbuf**ret);
int tcmedia_get_offer(struct tcmedia*media, struct mbuf*ret);

int tcmedia_alloc(struct tcmedia** rp, struct uac*uac, call_dir_t cdir);

#endif
