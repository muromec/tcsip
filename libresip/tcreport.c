#include "tcreport.h"

#include "strmacro.h"
#include "re.h"
#include "tcsipreg.h"
#include "tcsipcall.h"
#include "txsip_private.h"
#include "store/history.h"
#include <msgpack.h>

void report_call_change(struct tcsipcall* call, void *arg) {
    int cstate, reason;
    struct pl *ckey = tcsipcall_ckey(call);

    msgpack_packer *pk = arg;

    tcsipcall_dirs(call, NULL, &cstate, &reason, NULL);

    if((cstate & CSTATE_ALIVE) == 0) {
        tcsipcall_remove(call); // wrong place for this

        msgpack_pack_array(pk, 3);
        push_cstr("sip.call.del");
        push_pl((*ckey));
        msgpack_pack_int(pk, reason);

	return;
    }

    if(cstate & CSTATE_EST) {
        msgpack_pack_array(pk, 2);
        push_cstr("sip.call.est");
        push_pl((*ckey));
 
	return;
    }

}

void report_call(struct tcsipcall* call, void *arg) {
    int cdir, cstate, ts;
    struct sip_addr *remote;
    struct pl *ckey = tcsipcall_ckey(call);

    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, 7);
    push_cstr("sip.call.add");

    push_pl((*ckey));

    tcsipcall_dirs(call, &cdir, &cstate, NULL, &ts);

    msgpack_pack_int(pk, cdir);
    msgpack_pack_int(pk, cstate);
    msgpack_pack_int(pk, ts);

    tcop_lr((void*)call, NULL, &remote);
    push_pl(remote->dname);
    push_pl(remote->uri.user);

}

void report_reg(enum reg_state state, void*arg) {
    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, 2);
    push_cstr("sip.reg");
    msgpack_pack_int(pk, state);
}

void report_up(struct uplink *up, int op, void*arg) {
    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, op ? 4 : 3);
    push_cstr("sip.up");
    msgpack_pack_int(pk, op);
    push_pl(up->uri);
    if(op)
	msgpack_pack_int(pk, up->ok);
}

void report_cert(int err, struct pl*name, void*arg)
{
    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, err ? 2 : 3);
    push_cstr("cert.ok");
    msgpack_pack_int(pk, err);
    if(err==0)
        push_pl((*name));
}

static bool history_el(struct le *le, void *arg)
{
    msgpack_packer *pk = arg;
    struct hist_el *hel = le->data;

    msgpack_pack_array(pk, 5);
    msgpack_pack_int(pk, hel->event);
    msgpack_pack_int(pk, hel->time);
    push_pl(hel->key);
    push_pl(hel->login);
    push_pl(hel->name);

    return false;
}


void report_hist(int err, struct pl *idx, struct list*hlist, void*arg)
{
    int cnt;
    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, err ? 2 : 4);
    push_cstr("hist.res");
    msgpack_pack_int(pk, err);
    if(err) return;

    push_pl((*idx));
    cnt = list_count(hlist);
    msgpack_pack_array(pk, cnt);

    list_apply(hlist, true, history_el, arg);
}
