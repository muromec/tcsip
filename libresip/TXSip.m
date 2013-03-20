//
//  TXSip.m
//  Texr
//
//  Created by Ilya Petrov on 8/25/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//


#import "TXSip.h"
#import "TXSipReg.h"
#import "TXSipCall.h"
#import "TXSipMessage.h"
#import "TXSipAuth.h"
#import "TXRestApi.h"
#import "TXUplinks.h"
#import "ReWrap.h"
#import "MailBox.h"

#include <re.h>
#include <msgpack.h>

#define DEBUG_MODULE "txsip"
#define DEBUG_LEVEL 5

#include <re_dbg.h>

#include "txsip_private.h"
#include "re_wrap_priv.h"
#include "sound.h"
#include <srtp.h>

#define _byte(_x) ([_x cStringUsingEncoding:NSASCIIStringEncoding])


#if TARGET_OS_IPHONE
#define USER_AGENT "TexR/iOS libre"
#else
#define USER_AGENT "TexR/OSX libre"
#endif

int ui_idiom;
NSDateFormatter *http_df = NULL;

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

    TXSipCall* in_call = [[TXSipCall alloc] initWithApp:sip];
    [in_call incoming: msg];
    if(in_call.cstate & CSTATE_ERR)
        return;

    [sip callIncoming: in_call];
}

/* called when all sip transactions are completed */
static void exit_handler(void *arg)
{
    D(@"exit handler");
    /* stop libre main loop */
    re_cancel();
}


@implementation TXSip
@synthesize auth;
@synthesize uplinks;
@synthesize user;
@synthesize mbox;
@synthesize own_box;
- (id) initWithAccount: (id) pAccount
{
    self = [super init];
    if (!self || self < 0) {
        return self;
    }

    account = pAccount;
    user = [TXSipUser withName: account.user];
    user.name = account.name;

    [self create_ua];
    [self https_ua];
    calls = [[NSMutableArray alloc] init];
    chats = [[NSMutableArray alloc] init];
    
    auth = [[TXSipAuth alloc] initWithApp: self];
    sreg = [[TXSipReg alloc] initWithApp:self];
    [sreg setInstanceId: account.uuid];
    [sreg setDest: user];
    uplinks = [[TXUplinks alloc] init];

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

    /* create SIP stack instance */
    err = sip_alloc(&uac->sip, app->dnsc, 32, 32, 32,
                USER_AGENT, exit_handler, NULL);

    uac->dnsc = app->dnsc;
    NSBundle *b = [NSBundle mainBundle];
    NSString *ca_cert = [b pathForResource:@"CA" ofType: @"cert"];
    if(!ca_cert)
        ca_cert = @"cert/CA.cert";

    NSString *cert = [account cert];
    err = tls_alloc(&app->tls, TLS_METHOD_SSLV23, cert ? _byte(cert) : NULL, NULL);
    tls_add_ca(app->tls, _byte(ca_cert));
	
    [self listen_laddr];
}

- (void)listen_laddr
{
    int err;

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

- (void)https_ua
{
    [TXRestApi cert: [account cert]];
}

- (void) close
{

    D(@"close sip instance");
    sreg = nil;
    
    sipsess_close_all(uac->sock);
    sip_transp_flush(uac->sip);
    sip_close(uac->sip, 1);
    mem_deref(uac->sip);
    mem_deref(uac->sock);
    mem_deref(app->tls);
    mem_deref(app);

    free(uac);

    tmr_debug();
    mem_debug();

    media_snd_deinit();
}

- (oneway void) setOnline: (reg_state)state
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
    [sreg setState: state];
}

- (void)obCmd:(msgpack_object)ob
{
    msgpack_object *arg;
    msgpack_object_raw cmd;
    arg = ob.via.array.ptr;

    if(arg->type != MSGPACK_OBJECT_RAW)
	    return;

    cmd = arg->via.raw;

    if(!strncmp(cmd.ptr, "sip.online", cmd.size)) {
        arg++;
        [self setOnline: (reg_state)arg->via.i64];
    }

    if(!strncmp(cmd.ptr, "sip.call.place", cmd.size)) {
        arg++;
        NSString *login = _str(arg->via.raw.ptr, arg->via.raw.size);
        arg++;
        TXSipUser* dest = [TXSipUser withName:login];
        dest.name = _str(arg->via.raw.ptr, arg->via.raw.size);
        [self startCallUser:dest];
    }
    if(!strncmp(cmd.ptr, "sip.call.control", cmd.size)) {
        arg++;
        NSString *ckey = _str(arg->via.raw.ptr, arg->via.raw.size);
        arg++;
	int op = (int)arg->via.i64;
	for(TXSipCall* call in calls) {
            if(![call.ckey isEqualToString:ckey]) continue;

	    [call control: op];
	    break;
	}
    }

    if(!strncmp(cmd.ptr, "sip.apns", cmd.size)) {
        arg++;
	sreg.apns_token = [NSData dataWithBytes:
		arg->via.raw.ptr
		length:	arg->via.raw.size];
    }
}

- (void)online:(reg_state)state
{
    struct msgpack_packer *pk = own_box.packer;
    msgpack_pack_array(pk, 2);
    push_cstr("sip.online");
    msgpack_pack_int(pk, state);
}

- (void)control:(NSString*)ckey op:(int)op
{
    struct msgpack_packer *pk = own_box.packer;
    msgpack_pack_array(pk, 3);
    push_cstr("sip.call.control");
    push_str(ckey);
    msgpack_pack_int(pk, op);
}

- (void)call:(TXSipUser*)dest
{
    struct msgpack_packer *pk = own_box.packer;
    msgpack_pack_array(pk, 3);
    push_cstr("sip.call.place");
    push_str(dest.user);
    push_str(dest.name);
}

- (void) apns:(NSData*)token
{
    struct msgpack_packer *pk = own_box.packer;
    msgpack_pack_array(pk, 2);
    push_cstr("sip.apns");
    msgpack_pack_raw(pk, [token length]);
    msgpack_pack_raw_body(pk, [token bytes], [token length]);
}


- (oneway void) startCall: (NSString*)dest {

    [self startCallUser: [TXSipUser withName: dest]];
}

- (oneway void) startCallUser: (TXSipUser*)udest
{
    if(! sa_isset(&uac->laddr, SA_ADDR)) {
	D(@"no laddr, cant call");
        return;
    }
    TXSipCall *out_call = [[TXSipCall alloc] initWithApp:self];
    [out_call outgoing: udest];
    [out_call setDest: udest];
    [out_call setCb: CB(self, callChange:)];
    [out_call waitIce];
    [calls addObject: out_call];

    [self reportCall:out_call];
}

- (void) reportCall:(TXSipCall*)call {

    NSString *ckey = [call ckey];

    msgpack_packer *pk = [mbox packer];
    msgpack_pack_array(pk, 7);
    push_cstr("sip.call.add");
    push_str(ckey);
    msgpack_pack_int(pk, call.cdir);
    msgpack_pack_int(pk, call.cstate);
    msgpack_pack_int(pk, (int)[call.date_create timeIntervalSince1970]);
    push_str(call.dest.name);
    push_str(call.dest.user);
}

- (void) callIncoming: (id)in_call {
    [in_call setCb: CB(self, callChange:)];
    [calls addObject: in_call];
    [self reportCall:in_call];

}

- (void) callChange: (TXSipCall*) call {
    NSString *ckey = [call ckey];
    D(@"call changed %@, state %d", ckey, call.cstate);

    if((call.cstate & CSTATE_ALIVE) == 0) {
        [calls removeObject: call];

	msgpack_packer *pk = [mbox packer]; 
	msgpack_pack_array(pk, 3);
	push_cstr("sip.call.del");
        push_str(ckey);
        msgpack_pack_int(pk, call.end_reason);
	return;
    }

    if(call.cstate & CSTATE_EST) {
	msgpack_packer *pk = [mbox packer]; 
	msgpack_pack_array(pk, 2);
	push_cstr("sip.call.est");
        push_str(ckey);

	return;
    }
}

- (oneway void) startChat: (NSString*)dest {
    TXSipMessage *out_chat = [[TXSipMessage alloc] initWithApp:self];
    id udest  = [TXSipUser withName: dest];
    [out_chat setDest: udest];

    [out_chat send];

    [chats addObject: out_chat];
}

- (uac_t*) getUa
{
    return uac;
}

- (void)reportReg:(reg_state_t)state
{
    msgpack_packer *pk = [mbox packer];
    msgpack_pack_array(pk, 2);
    msgpack_pack_raw(pk, 7);
    msgpack_pack_raw_body(pk, "sip.reg", 7);
    msgpack_pack_int(pk, state);

}

- (oneway void) setRegObserver: (id)obs {
    //sreg.obs = obs;
    //uplinks.cb = CB(obs, uplinks:);
}

@end
