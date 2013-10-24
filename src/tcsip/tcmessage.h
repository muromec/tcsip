#ifndef TCSIPMESSAGE_H
#define TCSIPMESSAGE_H

struct uac;
struct tcmessages;
struct sip_addr;
struct store_client;

int tcmessage(struct tcmessages *tcmsg, struct sip_addr *to, char *text);
int tcmessage_alloc(struct tcmessages **rp, struct uac *uac, struct sip_addr* local_user, struct store_client *stc);
void tcmessage_handler(struct tcmessages *tcmsg, void *fn, void *arg);

#endif
