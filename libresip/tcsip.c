#include <re.h>
#include <sys/time.h>
#include <msgpack.h>
#include "strmacro.h"

#define DEBUG_MODULE "txsip"
#define DEBUG_LEVEL 5

#include <re_dbg.h>

#include "txsip_private.h"
#include "sound.h"
#include <srtp.h>

#include "tcreport.h"
#include "tcsipuser.h"
#include "tcsipreg.h"
#include "tcsipcall.h"
#include "tcuplinks.h"
#include "tcsip.h"
#include "x509util.h"

#if TARGET_OS_IPHONE
#define USER_AGENT "TexR/iOS libre"
#else
#define USER_AGENT "TexR/OSX libre"
#endif

int ui_idiom;

struct tcsip {

    struct uac *uac;

    struct tcsipreg *sreg_c;
    struct tcuplinks *uplinks_c;
    struct sip_addr *user_c;

    struct list *calls_c;

    int rmode;
    struct sip_handlers *rarg;
    void *xdnsc; // XXX we have two dns clients
};

static struct sip_handlers msgpack_handlers = {
    .reg_h = report_reg,
    .call_ch = report_call_change,
    .call_h = report_call,
    .up_h = report_up,
    .cert_h = report_cert,
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
    struct pl *ckey = arg, *thiskey;

    thiskey = tcsipcall_ckey(call);

    return !pl_cmp(ckey, thiskey);
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
    mem_deref(sip->user_c);
    mem_deref(sip->sreg_c);
    list_flush(sip->calls_c);
    mem_deref(sip->calls_c);
    mem_deref(sip->uplinks_c);

    mem_deref(uac);
    if(sip->rarg)
        mem_deref(sip->rarg);

    tmr_debug();
    mem_debug();

#if __APPLE__
    apple_sound_deinit();
#endif
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
#if __APPLE__
    err = apple_sound_init();
#endif
    err = srtp_init();

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

        tcsreg_uhandler(sip->sreg_c, uplink_upd, sip->uplinks_c);

        if(sip->rarg && sip->rarg->reg_h)
            tcsreg_handler(sip->sreg_c, sip->rarg->reg_h, sip->rarg->arg);
    }

    if(sip->sreg_c)
        tcsreg_state(sip->sreg_c, state);
}

void tcsip_apns(struct tcsip *sip, const char*data, size_t length)
{
    tcsreg_token(sip->sreg_c, (const uint8_t*)data, length);
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
    char *certpath, *capath=NULL;
    char *home = getenv("HOME"), *bundle = NULL;
    char path[2048];

#if __APPLE__
    CFBundleRef mbdl = CFBundleGetMainBundle();
    if(!mbdl) goto afail;

    CFURLRef url = CFBundleCopyBundleURL(mbdl);
    if (!CFURLGetFileSystemRepresentation(url, TRUE, (UInt8 *)path, sizeof(path)))
    {
        goto afail1;
    }
    bundle = path;
afail1:
    CFRelease(url);
afail:
#endif

    if(bundle) {
#if TARGET_OS_IPHONE
        re_sdprintf(&capath, "%s/CA.cert", bundle);
#else
        re_sdprintf(&capath, "%s/Contents/Resources/CA.cert", bundle);
#endif
    }

    if(home) {
        re_sdprintf(&certpath, "%s/Library/Texr/%r.cert",
                home, login);

    } else {
        re_sdprintf(&certpath, "./%r.cert", login);
    }

    char *cname;
    struct pl cname_p;
    int after, before;
    struct timeval now;

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

    return 0;
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
        
    tcsipcall_handler(call, sip->rarg->call_ch, sip->rarg->arg);
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
        
    tcsipcall_handler(call, sip->rarg->call_ch, sip->rarg->arg);
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

