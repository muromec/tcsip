#ifndef TCSIP_H
#define TCSIP_H

struct tcsip;
struct pl;
struct sip_addr;

enum reg_state;
struct tcsipcall;
struct uplink;

struct sip_handlers {
    void *arg;
    void(*reg_h)(enum reg_state, void*arg);
    void(*call_ch)(struct tcsipcall* call, void *arg);
    void(*call_h)(struct tcsipcall* call, void *arg);
    void(*up_h)(struct uplink *up, int op, void*arg);
    void(*cert_h)(int err, struct pl*name, void*arg);
};

void tcsip_set_online(struct tcsip *sip, int state);
void tcsip_apns(struct tcsip *sip, const char*data, size_t length);
void tcsip_uuid(struct tcsip *sip, struct pl *uuid);
int tcsip_local(struct tcsip* sip, struct pl* login);
int tcsip_get_cert(struct tcsip* sip, struct pl* login, struct pl*password);

int tcsip_alloc(struct tcsip**rp, int mode, void *rarg);
void tcsip_start_call(struct tcsip* sip, struct sip_addr*udest);
void tcsip_call_control(struct tcsip*sip, struct pl* ckey, int op);
void tcsip_xdns(struct tcsip* sip, void *arg);
void tcsip_close(struct tcsip*sip);
struct sip_addr *tcsip_user(struct tcsip*sip);

#endif
