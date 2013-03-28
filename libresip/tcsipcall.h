#ifndef TCSIPCALL_H
#define TCSIPCALL_H

typedef enum {
    CSTATE_STOP=0,
    CSTATE_IN_RING=1,
    CSTATE_OUT_RING=(1<<1),
    CSTATE_RING=(CSTATE_IN_RING|CSTATE_OUT_RING),
    CSTATE_FLOW=(1<<2),
    CSTATE_MEDIA=(1<<3),
    CSTATE_EST=(1<<4),
    CSTATE_ERR=(1<<5),
    CSTATE_ICE=(1<<6),
    CSTATE_ALIVE=(CSTATE_EST|CSTATE_RING),
} call_state_t;

typedef enum {
    CEND_OK=1,
    CEND_CANCEL,
    CEND_HANG,
} call_end_t;

typedef enum {
    CALL_ACCEPT,
    CALL_REJECT,
    CALL_BYE,
}
call_action_t;

typedef enum call_dir_t {
    CALL_IN,
    CALL_OUT,
} call_dir_t;

// bitmath helpers
#define DROP(x, bit) x&=x^bit
#define TEST(x, bit) ((x&(bit))==(bit))


struct tcsipcall;
struct uac;
struct list;
struct sip_msg;

int tcsipcall_alloc(struct tcsipcall**rp, struct uac *uac);
void tcsipcall_append(struct tcsipcall*call, struct list* cl);
void tcsipcall_remove(struct tcsipcall*call);

typedef void(tcsipcall_h)(struct tcsipcall*call, void*arg);

void tcsipcall_out(struct tcsipcall*call);
int tcsipcall_incomfing(struct tcsipcall*call, const struct sip_msg* msg);

void tcsipcall_accept(struct tcsipcall*call);

void tcsipcall_handler(struct tcsipcall*call, tcsipcall_h ch, void*arg);

void tcsipcall_dirs(struct tcsipcall*call, int *dir, int *state, int *reason, int *ts);
struct pl* tcsipcall_ckey(struct tcsipcall*call);
void tcsipcall_control(struct tcsipcall*call, int action);

#endif
