#include <re.h>
#include <sys/time.h>
#include <msgpack.h>
#include "strmacro.h"

#define DEBUG_MODULE "txsip"
#define DEBUG_LEVEL 5

#include <re_dbg.h>

#include "txsip_private.h"

#include "ipc/tcreport.h"

#include "tcsipuser.h"
#include "tcsipreg.h"
#include "tcmessage.h"
#include "tcsipcall.h"
#include "tcuplinks.h"
#include "tcsip.h"
#include "x509/x509util.h"
#include "util/platpath.h"
#include "rehttp/http.h"

#include "store/store.h"
#include "store/history.h"
#include "store/contacts.h"

#include "api/login.h"
#include "api/login_phone.h"
#include "api/signup.h"
#include "api/api.h"

#if __APPLE__
#include "sound/apple/sound.h"
#if TARGET_OS_IPHONE
#define USER_AGENT "TexR/iOS libre"
#else
#define USER_AGENT "TexR/OSX libre"
#endif
#endif

#if __linux__
#if ANDROID
#define USER_AGENT "TexR/android libre"
#else
#define USER_AGENT "TexR/linux libre"
#endif
#endif

int ui_idiom;


struct tcsip {

    struct uac *uac;

    struct tcsipreg *sreg_c;
    struct tcuplinks *uplinks_c;
    struct tcmessages *msg;
    struct sip_addr *user_c;
    char *cname;

    struct list *calls_c;

    struct history *hist;
    struct contacts *contacts;

    int rmode;
    struct sip_handlers *rarg;
    void *xdnsc; // XXX we have two dns clients
    struct tchttp *http;
};

static struct sip_handlers msgpack_handlers = {
    .reg_h = report_reg,
    .call_ch = report_call_change,
    .call_h = report_call,
    .msg_h = report_msg,
    .up_h = report_up,
    .cert_h = report_cert,
    .lp_h = report_lp,
    .signup_h = report_signup,
    .hist_h = report_hist,
    .histel_h = report_histel,
    .ctlist_h = report_ctlist,
};

void tcsip_call_incoming(struct tcsip* sip, const struct sip_msg *msg);
static void sip_init(struct tcsip*sip);
static void create_ua(struct tcsip*sip);
void listen_laddr(struct tcsip*sip);

/* called upon incoming calls */
static void connect_handler(const struct sip_msg *msg, void *arg)
{
    tcsip_call_incoming(arg, msg);
}

/* called when all sip transactions are completed */
static void exit_handler(void *arg)
{
    /* stop libre main loop */
}

bool find_ckey(struct le *le, void *arg)
{
    struct tcsipcall *call = le->data;
    struct pl *ckey = arg;
    char *thiskey;

    thiskey = tcsipcall_ckey(call);

    return !pl_strcmp(ckey, thiskey);
}

void sipc_destruct(void *arg)
{
    struct tcsip *sip = arg;
    struct uac *uac = sip->uac;

    sipsess_close_all(uac->sock);
    sip_transp_flush(uac->sip);
    sip_close(uac->sip, 1);
    mem_deref(uac->sip);
    mem_deref(uac->sock);
    mem_deref(uac->tls);
    mem_deref(uac->dnsc);
    mbuf_reset(&uac->apns);
    mem_deref(sip->user_c);
    mem_deref(sip->sreg_c);
    mem_deref(sip->msg);
    list_flush(sip->calls_c);
    mem_deref(sip->calls_c);
    sip->hist = mem_deref(sip->hist);
    sip->contacts = mem_deref(sip->contacts);
    mem_deref(sip->uplinks_c);
    mem_deref(sip->cname);
    mem_deref(sip->http);

    mem_deref(uac);
    if(sip->rarg)
        mem_deref(sip->rarg);


}


int tcsip_alloc(struct tcsip**rp, int mode, void *rarg)
{
    int err = 0;

    struct tcsip *sip;
    sip = mem_zalloc(sizeof(struct tcsip), sipc_destruct);
    if(!sip) {
        err = -ENOMEM;
	goto fail;
    }

    sip->uac = mem_zalloc(sizeof(struct uac), NULL);
    if(mode) {
        sip->rarg = mem_alloc(sizeof(struct sip_handlers), NULL);
        memcpy(sip->rarg, &msgpack_handlers, sizeof(struct sip_handlers));
        sip->rarg->arg = rarg;
    } else {
        sip->rarg = mem_ref(rarg);
    }

    if(sip->rarg && !sip->rarg->arg)
        sip->rarg = mem_deref(sip->rarg);

    sip->rmode = mode;

    mbuf_init(&sip->uac->apns);

    sip_init(sip);

    *rp = sip;

fail:
    return err;
}

static void sip_init(struct tcsip*sip)
{

    create_ua(sip);

    sip->calls_c = mem_zalloc(sizeof(struct list), NULL);
    list_init(sip->calls_c);
    
    tcuplinks_alloc(&sip->uplinks_c);
    tcuplinks_handler(sip->uplinks_c, sip->rarg->up_h, sip->rarg->arg);

}

static void create_ua(struct tcsip*sip)
{

    int err; /* errno return values */
    struct uac *uac = sip->uac;

    uac->nsc = sizeof(uac->nsv) / sizeof(uac->nsv[0]);
    err = dns_srv_get(NULL, 0, uac->nsv, &uac->nsc);

    err = dnsc_alloc(&uac->dnsc, NULL, uac->nsv, uac->nsc);

    /* create SIP stack instance */
    err = sip_alloc(&sip->uac->sip, sip->uac->dnsc, 32, 32, 32,
                USER_AGENT, exit_handler, NULL);
}

void listen_laddr(struct tcsip*sip)
{
    int err;
    struct uac *uac = sip->uac;

    if(!sip || !uac->tls) return;

    /* fetch local IP address */
    sa_init(&uac->laddr, AF_UNSPEC);
    err = net_default_source_addr_get(AF_INET, &uac->laddr);
    if(err) {
	return;
    }

    /*
     * Workarround.
     *
     * When using sa_set_port(0) we cant get port
     * number we bind for listening.
     * This indead is faulty cz random port cant
     * bue guarranted to be free to bind
     * */

    int port = rand_u32() | 1024;
    sa_set_port(&uac->laddr, port);
	
    /* add supported SIP transports */
    err |= sip_transp_add(uac->sip, SIP_TRANSP_TLS, &uac->laddr, uac->tls);

    /* create SIP session socket */
    err = sipsess_listen(&uac->sock, uac->sip, 32, connect_handler, sip);
    DEBUG_INFO("got laddr %J, listen %d\n", &uac->laddr, err);
}

void tcsip_message(struct tcsip* sip, struct sip_addr* dest, struct pl* text)
{
    char *c_text;
    pl_strdup(&c_text, text);
    tcmessage(sip->msg, dest, c_text);
    mem_deref(c_text);
}

void tcsip_set_online(struct tcsip *sip, int state)
{
    struct uac *uac = sip->uac;

    int err;
    if(state == REG_OFF) {
        sip_transp_flush(uac->sip);
        if(uac->sock)
            uac->sock = mem_deref(uac->sock);
        sa_init(&uac->laddr, AF_UNSPEC);
        sa_init(uac->nsv, AF_UNSPEC);
	uac->nsc = 1;
    } else {
        if(!uac->sock)
	    listen_laddr(sip);

        if(!uac->nsc || !sa_isset(uac->nsv, SA_ADDR)) {
            err = dns_srv_get(NULL, 0, uac->nsv, &uac->nsc);
            if(err) {
                sa_set_str(uac->nsv, "8.8.8.8", 53);
                uac->nsc = 1;
            }
            dnsc_srv_set(uac->dnsc, uac->nsv, uac->nsc);
	    if(sip->xdnsc)
                dnsc_srv_set(sip->xdnsc, uac->nsv, uac->nsc);
        }
    }
    if(!sip->sreg_c && sip->user_c && state != REG_OFF) {
        err = tcsipreg_alloc(&sip->sreg_c, uac);
        if(err) return;
        tcop_users(sip->sreg_c, sip->user_c, sip->user_c);

        if(uac->instance_id.l)
            tcsreg_set_instance_pl(sip->sreg_c, &uac->instance_id);
        if(mbuf_get_left(&uac->apns))
            tcsreg_token(sip->sreg_c, &uac->apns);

        tcsreg_uhandler(sip->sreg_c, uplink_upd, sip->uplinks_c);

        if(sip->rarg && sip->rarg->reg_h)
            tcsreg_handler(sip->sreg_c, sip->rarg->reg_h, sip->rarg->arg);
    }

    if(!sip->msg) {
        tcmessage_alloc(&sip->msg, uac, sip->user_c);
        tcmessage_handler(sip->msg, NULL, sip);
    }

    if(sip->sreg_c)
        tcsreg_state(sip->sreg_c, state);
}

void tcsip_apns(struct tcsip *sip, const char*data, size_t length)
{
    struct uac *uac = sip->uac;
    mbuf_reset(&uac->apns);
    mbuf_write_mem(&uac->apns, (const uint8_t*)data, length);
    mbuf_set_pos(&uac->apns, 0);

    if(sip->sreg_c)
        tcsreg_token(sip->sreg_c, &uac->apns);

}

void tcsip_uuid(struct tcsip *sip, struct pl *uuid)
{
    struct uac *uac = sip->uac;

    if(sip->sreg_c)
        tcsreg_set_instance_pl(sip->sreg_c, uuid);

    if(uac->instance_id.l) mem_deref((void*)uac->instance_id.p);
    pl_dup(&uac->instance_id, uuid);
}


int tcsip_local(struct tcsip* sip, struct pl* login)
{
    int err;
    struct uac *uac = sip->uac;
    char *certpath=NULL, *capath=NULL;
    char path[2048];
    char *cname;
    struct pl cname_p;
    int after, before;
    struct timeval now;

    platpath(login, &certpath, &capath);

#define cert_h(_code, _name) {\
    if(sip->rarg && sip->rarg->cert_h){\
        sip->rarg->cert_h(_code, _name, sip->rarg->arg);\
    }}

    err = x509_info(certpath, &after, &before, &cname);
    if(err) {
        re_printf("cert load failed %s\n", certpath);
        cert_h(1, NULL);
        return 1;
    }

    pl_set_str(&cname_p, cname);
    sip->user_c = mem_zalloc(sizeof(struct sip_addr), NULL);
    sip->cname = cname;
    err = sip_addr_decode(sip->user_c, &cname_p);
    if(err) {
        re_printf("CN parse failed\n");
        cert_h(2, NULL);
        return 2;
    }

    gettimeofday(&now, NULL);

    if(after < now.tv_sec) {
        re_printf("cert[%d] timed out[%d]. get new\n",
                after, (int)now.tv_sec);
        cert_h(3, NULL);
        return 3;
    }

    if(uac->tls) uac->tls = mem_deref(uac->tls);

    err = tls_alloc(&uac->tls, TLS_METHOD_SSLV23, certpath, NULL);
    if(err) {
        re_printf("tls failed\n");
        cert_h(4, NULL);
        return 4;
    }
    if(capath)
        tls_add_ca(uac->tls, capath);

    cert_h(0, &sip->user_c->dname);

        
    sip->http = mem_deref(sip->http);
    sip->hist = mem_deref(sip->hist);
    sip->contacts = mem_deref(sip->contacts);

    sip->http = tchttp_alloc(certpath);

    struct store *st;
    store_alloc(&st, login);

    err = history_alloc(&sip->hist, store_open(st, 'h'));

    contacts_alloc(&sip->contacts, store_open(st, 'c'), (struct httpc *) sip->http);

    if(sip->rarg) {
        contacts_handler(sip->contacts, sip->rarg->ctlist_h, sip->rarg->arg);
        history_report(sip->hist, sip->rarg->histel_h, sip->rarg->arg);
    }

    mem_deref(capath);
    mem_deref(certpath);
    mem_deref(st);

    return 0;
}

int tcsip_hist_fetch(struct tcsip* sip, char **pidx, struct list **hlist) {
    if(!sip || !sip->hist)
        return -EINVAL;

    return history_next(sip->hist, pidx, hlist);
}

void tcsip_hist_ipc(struct tcsip* sip, int flag)
{
    int err;
    char *idx = NULL;
    struct list *hlist;

    if(!sip || !sip->hist)
        return;

    if(flag)
        history_reset(sip->hist);

    err = history_next(sip->hist, &idx, &hlist); 
    if(err)
        goto out;

    if(sip->rarg && sip->rarg->hist_h){
        if(hlist)
            sip->rarg->hist_h(0, idx, hlist, sip->rarg->arg);
        else
            sip->rarg->hist_h(1, NULL, NULL, sip->rarg->arg);
    }

    mem_deref(hlist);
    mem_deref(idx);
out:
    return;
}

void tcsip_contacts_ipc(struct tcsip* sip)
{
    int err;
    if(!sip->contacts) {
        err = -EINVAL;
        goto fail;
    }

    contacts_fetch(sip->contacts);

fail:
    return;
}


int tcsip_get_cert(struct tcsip* sip, struct pl* login, struct pl*password) {
    tcapi_login(sip, login, password);
}

void tcsip_login_phone(struct tcsip* sip, struct pl *phone)
{
    tcapi_login_phone(sip, phone);
}

void tcsip_signup(struct tcsip* sip, struct pl *token, struct pl *otp, struct pl*login, struct pl* name)
{
    tcapi_signup(sip, token, otp, login, name);
}

int tcsip_report_cert(struct tcsip*sip, int code, struct pl *name)
{
#define cert_h(_code, _name) {\
    if(sip->rarg && sip->rarg->cert_h){\
        sip->rarg->cert_h(_code, _name, sip->rarg->arg);\
    }}

    cert_h(code, name);
}

int tcsip_report_login(struct tcsip*sip, int code, struct pl *token) {

#define lp_h(_code, _token) {\
    if(sip->rarg && sip->rarg->lp_h){\
        sip->rarg->lp_h(_code, _token, sip->rarg->arg);\
    }}

    lp_h(code, token);

}

int tcsip_report_signup(struct tcsip*sip, int code, struct list*elist) {
#define signup_h(_code, _list) {\
    if(sip->rarg && sip->rarg->signup_h){\
        sip->rarg->signup_h(_code, _list, sip->rarg->arg);\
    }}

    signup_h(code, elist);
}

int tcsip_report_message(struct tcsip*sip, const struct sip_taddr *from, struct mbuf* data)
{
    if(sip->rarg && sip->rarg->msg_h) {
        sip->rarg->msg_h(from, data, sip->rarg->arg);
    }
}


struct sip_addr *tcsip_user(struct tcsip*sip)
{
    return sip->user_c;
}

void tcsip_call_control(struct tcsip*sip, struct pl* ckey, int op)
{
    struct le *found;
    struct tcsipcall *call;
    found = list_apply(sip->calls_c, true, find_ckey, ckey);
    if(!found) return;

    call = found->data;

    tcsipcall_control(call, op);

}

void tcsip_call_history(struct tcsip* sip, struct tcsipcall *call)
{
    int dir, state, reason, ts, event;
    int err;
    char *tmp;
    struct pl ckey;
    struct sip_addr *remote;

    tcsipcall_dirs(call, &dir, &state, &reason, &ts);
    if(state & CSTATE_ALIVE)
        return;

    event = 0;
    if(reason == CEND_OK)
        event |= HIST_OK;
    else
        event |= HIST_HANG;

    if(dir==CALL_IN)
        event |= HIST_IN;
    else
        event |= HIST_OUT;

    tmp = tcsipcall_ckey(call);
    pl_set_str(&ckey, tmp);
    tcop_lr((void*)call, NULL, &remote);

    err = history_add(sip->hist, event, ts, &ckey, &remote->uri.user, &remote->dname);

}

void tcsip_call_changed(struct tcsipcall *call, void *arg)
{
    struct tcsip* sip = arg;

    sip->rarg->call_ch(call, sip->rarg->arg);
    tcsip_call_history(sip, call);
}

void tcsip_start_call(struct tcsip* sip, struct sip_addr*udest)
{
    struct uac *uac = sip->uac;
    if(! sa_isset(&uac->laddr, SA_ADDR)) {
        return;
    }

    int err;
    struct tcsipcall *call;
    err = tcsipcall_alloc(&call, uac);
    tcop_users((void*)call, sip->user_c, udest);
    tcsipcall_out(call);

    tcsipcall_append(call, sip->calls_c);
    if(!sip->rarg || !sip->rarg->call_ch)
	return;
        
    tcsipcall_handler(call, tcsip_call_changed, sip);
    sip->rarg->call_h(call, sip->rarg->arg);
}


void tcsip_call_incoming(struct tcsip* sip, const struct sip_msg *msg)
{
    int err;
    struct tcsipcall *call;
    err = tcsipcall_alloc(&call, sip->uac);

    if(err) {
        mem_deref(call);
        return;
    }
    tcop_users((void*)call, sip->user_c, NULL);
    err = tcsipcall_incomfing(call, msg);
    tcsipcall_append(call, sip->calls_c);
    tcsipcall_accept(call);

    if(!sip->rarg || !sip->rarg->call_ch)
	return;
        
    tcsipcall_handler(call, tcsip_call_changed, sip);
    sip->rarg->call_h(call, sip->rarg->arg);
}

void tcsip_xdns(struct tcsip* sip, void *arg)
{
    if(sip) sip->xdnsc = arg;
}

void tcsip_close(struct tcsip*sip)
{
    mem_deref(sip);
}

