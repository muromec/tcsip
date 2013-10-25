#ifndef TCSIP_H
#define TCSIP_H

struct tcsip;
struct pl;
struct sip_addr;
struct list;

enum reg_state;
struct tcsipcall;
struct uplink;

struct hist_el;

struct sip_handlers {
    void *arg;
    void(*reg_h)(enum reg_state, void*arg);
    void(*call_ch)(struct tcsipcall* call, void *arg);
    void(*call_h)(struct tcsipcall* call, void *arg);
    void(*up_h)(struct uplink *up, int op, void*arg);
    void(*msg_h)(time_t, char *idx, const struct sip_addr *, struct mbuf*, int state, void *arg);
    void(*cert_h)(int err, struct pl*name, struct pl*uri, void*arg);
    void(*lp_h)(int err, struct pl*token, void*arg);
    void(*signup_h)(int code, struct list*, void *arg);
    void(*hist_h)(int err, char*idx, struct list*hlist, void*arg);
    void(*histel_h)(int err, int op, struct hist_el*el, void *arg);
    void(*ctlist_h)(int err, struct list*ctlist, void *arg);
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
int tcsip_hist_fetch(struct tcsip* sip, char **pidx, struct list **);
void tcsip_hist_ipc(struct tcsip* sip, int flag);
void tcsip_contacts_ipc(struct tcsip* sip);
int tcsip_report_cert(struct tcsip*sip, int code);
int tcsip_report_login(struct tcsip*sip, int code, struct pl *token);
int tcsip_report_signup(struct tcsip*sip, int code, struct list*);
int tcsip_report_message(struct tcsip*sip, time_t ts, char *idx, const struct sip_addr *from, struct mbuf* data, int state);

void tcsip_login_phone(struct tcsip* sip, struct pl *phone);
void tcsip_signup(struct tcsip* sip, struct pl *token, struct pl *otp, struct pl*login, struct pl* name);
void tcsip_message(struct tcsip*, struct sip_addr*, struct pl*);


#endif
