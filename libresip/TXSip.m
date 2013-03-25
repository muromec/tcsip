//
//  TXSip.m
//  Texr
//
//  Created by Ilya Petrov on 8/25/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//


#import "TXSip.h"
#import "TXSipReport.h"
#import "TXSipIpc.h"
#import "TXRestApi.h"
#import "TXUplinks.h"
#import "ReWrap.h"
#import "MailBox.h"

#include <re.h>
#include <msgpack.h>
#include "strmacro.h"

#define DEBUG_MODULE "txsip"
#define DEBUG_LEVEL 5

#include <re_dbg.h>

#include "txsip_private.h"
#include "re_wrap_priv.h"
#include "sound.h"
#include <srtp.h>

#include "tcsipuser.h"
#include "tcsipreg.h"
#include "tcsipcall.h"

#define _byte(_x) ([_x cStringUsingEncoding:NSASCIIStringEncoding])


#if TARGET_OS_IPHONE
#define USER_AGENT "TexR/iOS libre"
#else
#define USER_AGENT "TexR/OSX libre"
#endif

int ui_idiom;
const NSDateFormatter *http_df = NULL;

static void setup_df() {
    http_df = [[NSDateFormatter alloc] init];
    http_df.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"GMT"];
    http_df.dateFormat = @"EEE',' dd MMM yyyy HH':'mm':'ss 'GMT'";
    http_df.locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
}

char* byte(NSString * input){
      int len = (int)[input lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

      char *toReturn = (char *)malloc((sizeof(char) * len) +1);

      strcpy(toReturn,[input cStringUsingEncoding:NSUTF8StringEncoding]);

      return toReturn;
}

/* called upon incoming calls */
static void connect_handler(const struct sip_msg *msg, void *arg)
{
    D(@"incoming connect");
    TXSip* sip = (__bridge id)arg;
    [sip callIncoming: msg];

}

/* called when all sip transactions are completed */
static void exit_handler(void *arg)
{
    D(@"exit handler");
    /* stop libre main loop */
    re_cancel();
}

bool find_ckey(struct le *le, void *arg)
{
    struct tcsipcall *call = le->data;
    struct pl *ckey = arg, *thiskey;

    thiskey = tcsipcall_ckey(call);

    return !pl_cmp(ckey, thiskey);
}

@implementation TXSip
@synthesize uplinks;
- (id) init
{
    self = [super init];
    if (!self || self < 0) {
        return self;
    }

    report = [[TXSipReport alloc] init];

    [self create_ua];

    calls_c = mem_zalloc(sizeof(struct list), NULL);
    list_init(calls_c);
    
    uplinks = [[TXUplinks alloc] init];
    uplinks.delegate = report;

    return self;
}

- (void) create_ua {

    int err; /* errno return values */
    err = media_snd_init();
    err = srtp_init();

    if(!http_df) setup_df();

    uac = malloc(sizeof(uac_t));
    memset(uac, 0, sizeof(uac_t));

    void *_app = [ReWrap app];
    app = mem_alloc(sizeof(struct reapp), NULL);
    memcpy(app, _app, sizeof(struct reapp));
    app->tls = NULL;

    /* create SIP stack instance */
    err = sip_alloc(&uac->sip, app->dnsc, 32, 32, 32,
                USER_AGENT, exit_handler, NULL);

    uac->dnsc = app->dnsc;

}

- (void)listen_laddr
{
    int err;

    if(!app->tls) return;

    /* fetch local IP address */
    sa_init(&uac->laddr, AF_UNSPEC);
    err = net_default_source_addr_get(AF_INET, &uac->laddr);
    if(err) {
	D(@"no local addr found");
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
    err |= sip_transp_add(uac->sip, SIP_TRANSP_TLS, &uac->laddr, app->tls);
	
    /* create SIP session socket */
    err = sipsess_listen(&uac->sock, uac->sip, 32, connect_handler, (__bridge void*)self);
    DEBUG_INFO("got laddr %J, listen %d\n", &uac->laddr, err);
}

- (void) close
{

    D(@"close sip instance");
    
    sipsess_close_all(uac->sock);
    sip_transp_flush(uac->sip);
    sip_close(uac->sip, 1);
    mem_deref(uac->sip);
    mem_deref(uac->sock);
    mem_deref(app->tls);
    mem_deref(app);
    mem_deref(user_c);
    mem_deref(sreg_c);
    list_flush(calls_c);
    mem_deref(calls_c);

    free(uac);

    tmr_debug();
    mem_debug();

    media_snd_deinit();
}

- (oneway void) setOnline: (int)state
{
    int err;
    if(state == REG_OFF) {
        D(@"go offline");
        sip_transp_flush(uac->sip);
        if(uac->sock)
            uac->sock = mem_deref(uac->sock);
        sa_init(&uac->laddr, AF_UNSPEC);
        sa_init(app->nsv, AF_UNSPEC);
        app->nsc = 1;
    } else {
        D(@"go online %p", uac->sock);
        if(!uac->sock)
            [self listen_laddr];

        if(!app->nsc || !sa_isset(app->nsv, SA_ADDR)) {
            err = dns_srv_get(NULL, 0, app->nsv, &app->nsc);
            if(err) {
                sa_set_str(app->nsv, "8.8.8.8", 53);
                app->nsc = 1;
            }
            dnsc_srv_set(app->dnsc, app->nsv, app->nsc);
        }
    }
    if(!sreg_c && user_c && state != REG_OFF) {
        err = tcsipreg_alloc(&sreg_c, uac);
        if(err) return;
        tcop_users(sreg_c, user_c, user_c);

        if(uac->instance_id.l)
            tcsreg_set_instance_pl(sreg_c, &uac->instance_id);

        tcsreg_uhandler(sreg_c, uplink_upd, (__bridge void*)uplinks);

        if(report.box)
            tcsreg_handler(sreg_c, report_reg, report.box.packer);
    }

    if(sreg_c)
        tcsreg_state(sreg_c, state);
}

- (void)doApns: (const char*)data length:(size_t)length
{
    tcsreg_token(sreg_c, (const uint8_t*)data, length);
}

- (void)doUUID: (struct pl*)uuid
{
    if(sreg_c)
        tcsreg_set_instance_pl(sreg_c, uuid);

    if(uac->instance_id.l) mem_deref((void*)uac->instance_id.p);
    pl_dup(&uac->instance_id, uuid);
}

- (void)setLocal:(struct pl*)login name:(struct pl*)name
{
    int err;
    struct pl tmpl;
    pl_dup(&tmpl, login);
    if(user_c) user_c = mem_deref(user_c);

    err = sippuser_by_name(&user_c, tmpl.p);
    if(name) pl_dup(&user_c->dname, name);

    mem_deref((void*)tmpl.p);

    [self recreateTls];
}

- (void)recreateTls
{
    int err;
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
        re_sdprintf(&capath, "%s/Contents/Resources/CA.cert", bundle);
    }

    if(home) {
        re_sdprintf(&certpath, "%s/Library/Texr/%r.cert",
                home, &user_c->uri.user);

    } else {
        re_sdprintf(&certpath, "./%r.cert",
                &user_c->uri.user);
    }

    if(app->tls) app->tls = mem_deref(app->tls);

    err = tls_alloc(&app->tls, TLS_METHOD_SSLV23, certpath, NULL);
    if(capath)
        tls_add_ca(app->tls, capath);
}

- (void)doCallControl:(struct pl*)ckey op:(int)op {
    struct le *found;
    struct tcsipcall *call;
    found = list_apply(calls_c, true, find_ckey, ckey);
    if(!found) return;

    call = found->data;

    tcsipcall_control(call, op);

}

- (oneway void) startCallUser: (struct sip_addr*)udest
{
    if(! sa_isset(&uac->laddr, SA_ADDR)) {
	D(@"no laddr, cant call");
        return;
    }

    int err;
    struct tcsipcall *call;
    err = tcsipcall_alloc(&call, uac);
    tcop_users((void*)call, user_c, udest);
    tcsipcall_out(call);
    tcsipcall_handler(call, report_call_change, report.box.packer);
    tcsipcall_waitice(call);

    tcsipcall_append(call, calls_c);
    report_call(call, report.box.packer);
}


- (void) callIncoming: (const struct sip_msg *)msg
{
    int err;
    struct tcsipcall *call;
    err = tcsipcall_alloc(&call, uac);

    if(err) {
        mem_deref(call);
        return;
    }
    tcop_users((void*)call, user_c, NULL);
    err = tcsipcall_incomfing(call, msg);
    tcsipcall_handler(call, report_call_change, report.box.packer);
    tcsipcall_append(call, calls_c);
    tcsipcall_accept(call);
    report_call(call, report.box.packer);
}

- (void) callChange: (TXSipCall*) call {

}

- (oneway void) startChat: (NSString*)dest {

}

- (uac_t*) getUa
{
    return uac;
}

- (void)setMbox:(MailBox*)pBox {
    mbox = pBox;
    report.box = pBox;

    if(sreg_c)
        tcsreg_handler(sreg_c, report_reg, pBox.packer);
}

- (TXSipIpc*) ipc:(MailBox*)pBox {
    TXSipIpc *ipc = [[TXSipIpc alloc] initWithBox:pBox];
    ipc.delegate = self;
    return ipc;
}

- (MailBox*)mbox {
    return mbox;
}

@end
