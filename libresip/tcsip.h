#ifndef TCSIP_H
#define TCSIP_H

struct tcsip;
struct pl;
struct sip_addr;

void tcsip_set_online(struct tcsip *sip, int state);
void tcsip_apns(struct tcsip *sip, const char*data, size_t length);
void tcsip_uuid(struct tcsip *sip, struct pl *uuid);
void tcsip_local(struct tcsip* sip, struct pl* login);

int tcsip_alloc(struct tcsip**rp, int mode, void *rarg);
void tcsip_start_call(struct tcsip* sip, struct sip_addr*udest);
void tcsip_call_control(struct tcsip*sip, struct pl* ckey, int op);
void tcsip_xdns(struct tcsip* sip, void *arg);
void tcsip_close(struct tcsip*sip);

#endif
