#ifndef TCREPORT_H
#define TCREPORT_H
enum reg_state;
struct tcsipcall;
struct uplink;
struct pl;

void report_reg(enum reg_state state, void*arg);
void report_call_change(struct tcsipcall* call, void *arg);
void report_call(struct tcsipcall* call, void *arg);
void report_up(struct uplink *up, int op, void*arg);
void report_cert(int err, struct pl*name, void*arg);

#define push_str(__s) {\
    msgpack_pack_raw(pk, [__s length]);\
    msgpack_pack_raw_body(pk, _byte(__s), [__s length]);}
#define push_cstr(__c) {\
    msgpack_pack_raw(pk, sizeof(__c)-1);\
    msgpack_pack_raw_body(pk, __c, sizeof(__c)-1);}
#define push_pl(__c) {\
    msgpack_pack_raw(pk, __c.l);\
    msgpack_pack_raw_body(pk, __c.p, __c.l);}

#endif
