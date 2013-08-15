#include "re.h"
#include "tcsipreg.h"
#include "tcsipuser.h"
#include "txsip_private.h"
#include <string.h>

#if __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#if TARGET_OS_IPHONE
#include "CFNetwork/CFSocketStream.h"
#endif
#endif

static const char *registrar = "sip:sip.texr.net";

struct tcsipreg {
    struct uac *uac;
    struct sipreg *reg;
    struct sip_addr *remote;
    struct sip_addr *local;
    reg_state_t rstate;
    struct pl instance_id;
    struct pl apns_token;
    int reg_time;
    struct tmr reg_tmr;
    tcsipreg_h* handler;
    void *handler_arg;
    uplink_h* uhandler;
    void *uhandler_arg;
#if TARGET_OS_IPHONE
    struct tcp_conn *upstream;
    CFReadStreamRef upstream_ref;
#endif
};

void tcsreg_send(struct tcsipreg *reg);
void tcsreg_resend(struct tcsipreg *reg);
void tcsreg_response(struct tcsipreg *reg, int status);
void tcsreg_contacts(struct tcsipreg *reg, const struct sip_msg*msg);
void tcsreg_voip(struct tcsipreg *reg, struct tcp_conn *conn);

#define report(__x) (reg->handler(__x, reg->handler_arg))
#define ureport(__x) (reg->uhandler(__x, reg->uhandler_arg))

static void dummy_handler(enum reg_state state, void*arg){
}
static void dummy_uhandler(struct list *upl, void*arg){
}


static void register_handler(int err, const struct sip_msg *msg, void *arg)
{
    struct tcsipreg *ctx = (struct tcsipreg*)arg;

    if(err == 0 && msg)
        tcsreg_response(ctx, msg->scode);
    else
        tcsreg_response(ctx, 900);

    tcsreg_contacts(ctx, msg);
    tcsreg_voip(ctx, sip_msg_tcpconn(msg));
}

void tcsreg_response(struct tcsipreg *reg, int status)
{
    switch(status) {
    case 200:
	report(REG_ONLINE);

        reg->rstate |= REG_AUTH;
        reg->rstate |= REG_ONLINE;
        break;
    case 401:
        if((reg->rstate & REG_AUTH)==0) {
        }
        // fallthrough!
    default:
        reg->rstate &= reg->rstate^REG_ONLINE;
	report(REG_NONE);
    }
}

void reg_destruct(void *arg) {
    struct tcsipreg *reg = arg;

    if(reg->local) reg->local = mem_deref(reg->local);
    if(reg->remote) reg->remote = mem_deref(reg->remote);
    reg->uac = mem_deref(reg->uac);

    if(reg->instance_id.p) mem_deref((void*)reg->instance_id.p);
    if(reg->apns_token.p) mem_deref((void*)reg->apns_token.p);

    if(reg->reg) reg->reg = mem_deref(reg->reg);

    tmr_cancel(&reg->reg_tmr);
}

int tcsipreg_alloc(struct tcsipreg**rp, struct uac *uac) {
    int err;
    struct tcsipreg *ret;

    ret = mem_zalloc(sizeof(struct tcsipreg), reg_destruct);
    if(!ret) {
        err = -ENOMEM;
        goto fail;
    }
    ret->rstate = REG_NONE;
    ret->reg_time = 60;
    ret->uac = mem_ref(uac);
    ret->handler = dummy_handler;
    ret->uhandler = dummy_uhandler;

    *rp = ret;
    return 0;
fail:
    return err;
}

void tcsreg_state(struct tcsipreg *reg, enum reg_cmd state) {
    int reg_time;

    switch(state) {
    case REG_OFF:
        reg->reg = mem_deref(reg->reg);
        return;
    case REG_FG:
        reg_time = 60;
        break;
    case REG_BG:
        reg_time = 610;
        break;
    }
 
    reg->reg_time = reg_time;
    if(reg->reg)
	tcsreg_resend(reg);
    else
	tcsreg_send(reg);
}

void tcsreg_send(struct tcsipreg *reg) {
    int err;
    char *uri, *user;
    pl_strdup(&uri, &reg->remote->auri);
    pl_strdup(&user, &reg->remote->uri.user);

    char *params=NULL, *fmt=NULL;
    if(reg->instance_id.l)
        re_sdprintf(&params, "+sip.instance=\"<urn:uuid:%r>\"",
                &reg->instance_id);

    if(reg->apns_token.l) {
        re_sdprintf(&fmt, "Push-Token: %r\r\n", &reg->apns_token);
    }

    err = sipreg_register(&reg->reg, reg->uac->sip, registrar, uri, uri, reg->reg_time, user,
                          NULL, 0, 1, NULL, reg, false,
                          register_handler, reg, params, fmt);

    if(err) {
        report(REG_NONE);
    } else {
        reg->rstate |= REG_START;
        report(REG_TRY);
    }

    if(fmt) mem_deref(fmt);
    mem_deref(params);

    mem_deref(uri);
    mem_deref(user);

}

void tcsreg_resend(struct tcsipreg *reg) {
    int err;
    reg->reg_time++;
    err = sipreg_expires(reg->reg, reg->reg_time);
    if(err) {
        reg->reg = mem_deref(reg->reg);
        report(REG_NONE);
    }
    // delegate TRY
}


void tcop_users(struct tcsipreg *op, struct sip_addr *lo, struct sip_addr *re)
{
    if(lo) {
        if(op->local) mem_deref(op->local);
        op->local = mem_ref(lo);
    }
    if(re) {
        if(op->remote) mem_deref(op->remote);
        op->remote = mem_ref(re);
    }
}

void tcop_lr(struct tcsipreg *op, struct sip_addr **lo, struct sip_addr **re)
{
    if(lo) {
	*lo = op->local;
    }
    if(re) {
	*re = op->remote;
    }
}


void tcsreg_handler(struct tcsipreg *reg, tcsipreg_h rh, void *arg)
{
    if(!reg)
        return;

    reg->handler = rh;
    reg->handler_arg = arg; // XXX: mem_ref maby ?
}

void tcsreg_uhandler(struct tcsipreg *reg, uplink_h uh, void*arg)
{
    if(!reg)
        return;

    reg->uhandler = uh;
    reg->uhandler_arg = arg; // XXX: mem_ref maby?
}

void tcsreg_set_instance(struct tcsipreg *reg, const char* instance_id)
{
    struct pl tmp;
    pl_set_str(&tmp, instance_id);
    pl_dup(&reg->instance_id, &tmp);
}

void tcsreg_set_instance_pl(struct tcsipreg *reg, struct pl* instance_id)
{
    pl_dup(&reg->instance_id, instance_id);
}

void tcsreg_token(struct tcsipreg *reg, struct mbuf *tdata)
{
    char enc_token[50];
    char *fmt=NULL;
    struct pl tmp;
    tmp.p = enc_token;
    tmp.l = sizeof(enc_token);
    memset(enc_token, 0, tmp.l);

    base64_encode(mbuf_buf(tdata), mbuf_get_left(tdata), (char*)tmp.p, &tmp.l);

    if(reg->apns_token.p) mem_deref((void*)reg->apns_token.p);
    pl_dup(&reg->apns_token, &tmp);

    if(reg->apns_token.l) {
        re_sdprintf(&fmt, "Push-Token: %r\r\n", &reg->apns_token);
        sipreg_headers(reg->reg, fmt);
        mem_deref(fmt);
    }
}

void tcsreg_voip(struct tcsipreg *reg, struct tcp_conn *conn)
{
#if TARGET_OS_IPHONE
    if(conn == reg->upstream)
       return;

    int fd = tcp_conn_fd(conn);
    if(reg->upstream_ref) {
        CFRelease(reg->upstream_ref);
        reg->upstream_ref = NULL;
    }

    if(fd==-1) {
        return;
    }

    CFReadStreamRef read_stream;
    CFStreamCreatePairWithSocket(NULL, fd, &read_stream, NULL);
    CFReadStreamSetProperty(read_stream, kCFStreamNetworkServiceType, kCFStreamNetworkServiceTypeVoIP);
    CFReadStreamOpen(read_stream);

    reg->upstream = conn;
    reg->upstream_ref = read_stream;
#endif
}

void uplink_de(void *arg)
{
    struct uplink *up = arg;
    mem_deref((void*)up->uri.p);
}

static bool find_uplink(const struct sip_hdr *hdr, const struct sip_msg *msg, void *arg)
{
    struct sip_addr addr;
    struct pl state;
    struct list *upl = arg;
    state.l = 0;
    int err;

    err = sip_addr_decode(&addr, &hdr->val);
    if(err) {
        return false;
    }
    err = sip_param_exists(&addr.params, "uplink", &state);
    if(err == ENOENT) {
        return false;
    }
    if(err) {
        return false;
    }

    state.l = 0;
    sip_param_decode(&addr.params, "uplink", &state);

    bool ok;
    if(state.l) {
        ok = 0;
    } else {
        ok = 1;
    }

    struct uplink *up = mem_zalloc(sizeof(struct uplink), uplink_de);
    pl_dup(&up->uri, &addr.auri);
    up->ok = ok;

    list_append(upl, &up->le, up);

    return false;
}


void tcsreg_contacts(struct tcsipreg *reg, const struct sip_msg*msg)
{
    struct list upl;
    list_init(&upl);

    sip_msg_hdr_apply(msg, true, SIP_HDR_CONTACT, find_uplink, &upl);
    ureport(&upl);

    list_flush(&upl);
}

