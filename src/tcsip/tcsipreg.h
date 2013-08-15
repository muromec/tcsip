#ifndef TCSIPREG_H
#define TCSIPREG_H
struct tcsipreg;
struct sip_addr;
struct uac;

enum reg_state {
    REG_NONE,
    REG_START=1,
    REG_AUTH=2,
    REG_ONLINE=4,
    REG_TRY=5,
};

enum reg_cmd {
    REG_OFF,
    REG_BG,
    REG_FG
};

typedef enum reg_state reg_state_t;
struct list;
struct mbuf;

typedef void(tcsipreg_h)(enum reg_state state, void*arg);
typedef void(uplink_h)(struct list*, void*arg);

int tcsipreg_alloc(struct tcsipreg**rp, struct uac *uac);

void tcop_users(struct tcsipreg *op, struct sip_addr *lo, struct sip_addr *re);
void tcop_lr(struct tcsipreg *op, struct sip_addr **lo, struct sip_addr **re);

void tcsreg_state(struct tcsipreg *reg, enum reg_cmd state);
void tcsreg_handler(struct tcsipreg *reg, tcsipreg_h rh, void*arg);
void tcsreg_uhandler(struct tcsipreg *reg, uplink_h uh, void*arg);

void tcsreg_set_instance(struct tcsipreg *reg, const char* instance_id);
void tcsreg_set_instance_pl(struct tcsipreg *reg, struct pl* instance_id);
void tcsreg_token(struct tcsipreg *reg, struct mbuf *data);

#endif
