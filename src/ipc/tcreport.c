
#include "re.h"

#include "strmacro.h"
#include "tcreport.h"
#include "tcsip/tcsipreg.h"
#include "tcsip/tcsipcall.h"
#include "tcsip/txsip_private.h"

#include "store/history.h"
#include "store/contacts.h"

#include <msgpack.h>

void report_call_change(struct tcsipcall* call, void *arg) {
    int cstate, reason;
    char *ckey = tcsipcall_ckey(call);

    msgpack_packer *pk = arg;

    tcsipcall_dirs(call, NULL, &cstate, &reason, NULL);

    if((cstate & CSTATE_ALIVE) == 0) {
        tcsipcall_remove(call); // wrong place for this

        msgpack_pack_array(pk, 3);
        push_cstr("sip.call.del");
        push_cstr_len(ckey);
        msgpack_pack_int(pk, reason);

	return;
    }

    if(cstate & CSTATE_EST) {
        msgpack_pack_array(pk, 2);
        push_cstr("sip.call.est");
        push_cstr_len(ckey);
 
	return;
    }

}

void report_call(struct tcsipcall* call, void *arg) {
    int cdir, cstate, ts;
    struct sip_addr *remote;
    char *ckey = tcsipcall_ckey(call);

    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, 7);
    push_cstr("sip.call.add");

    push_cstr_len(ckey);

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

void report_lp(int err, struct pl*token, void*arg)
{
    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, err ? 3 : 2);
    push_cstr("api.login_phone");
    msgpack_pack_int(pk, err);
    if(err==1)
        push_pl((*token));
}


static inline void do_write_history_el(msgpack_packer *pk, struct hist_el *hel)
{
    msgpack_pack_array(pk, 5);
    msgpack_pack_int(pk, hel->event);
    msgpack_pack_int(pk, hel->time);
    push_cstr_len(hel->key);
    push_cstr_len(hel->login);

    if(hel->name) {
        push_cstr_len(hel->name);
    } else {
        push_cstr_len(hel->login);
    }
}

bool write_history_el(struct le *le, void *arg)
{
    msgpack_packer *pk = arg;
    struct hist_el *hel = le->data;

    do_write_history_el(pk, hel);

    return false;
}


void report_hist(int err, char *idx, struct list*hlist, void*arg)
{
    int cnt;
    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, err ? 2 : 4);
    push_cstr("hist.res");
    msgpack_pack_int(pk, err);
    if(err) return;

    push_cstr_len(idx);
    cnt = list_count(hlist);
    msgpack_pack_array(pk, cnt);

    list_apply(hlist, true, write_history_el, arg);
}

void do_write_ctel(msgpack_packer *pk, struct contact_el *ctel)
{
    msgpack_pack_array(pk, 3);
    push_cstr_len(ctel->login);
    push_cstr_len(ctel->name);
    push_cstr_len(ctel->phone);

}

bool write_contact_el(struct le *le, void *arg)
{
    msgpack_packer *pk = arg;
    struct contact_el *ctel = le->data;

    do_write_ctel(pk, ctel);
    
    return false;
}

void report_ctlist(int err, struct list*ctlist, void*arg)
{
    int cnt;
    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, err ? 2 : 3);
    push_cstr("contacts.res");
    msgpack_pack_int(pk, err);
    if(err) return;

    cnt = list_count(ctlist);
    msgpack_pack_array(pk, cnt);

    list_apply(ctlist, true, write_contact_el, arg);

}

void report_histel(int err, int op, struct hist_el* hel, void*arg)
{
    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, err ? 2 : 4);
    push_cstr("hist.add");
    msgpack_pack_int(pk, err);
    if(err) return;

    msgpack_pack_int(pk, op);
    do_write_history_el(pk, hel);
}
