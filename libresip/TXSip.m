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
#import "ReWrap.h"

#include <re.h>
#include "txsip_private.h"
#include "re_wrap_priv.h"
#include "sound.h"
#include <srtp.h>

#define _byte(_x) ([_x cStringUsingEncoding:NSASCIIStringEncoding])
#define delegate( ) (self->delegate)
#define here(x) ([wrapper wrap:x])

#if TARGET_OS_IPHONE
#define USER_AGENT "TexR/iOS libre"
#else
#define USER_AGENT "TexR/OSX libre"
#endif

char* byte(NSString * input){
      int len = (int)[input lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

      char *toReturn = (char *)malloc((sizeof(char) * len) +1);

      strcpy(toReturn,[input cStringUsingEncoding:NSUTF8StringEncoding]);

      return toReturn;
}

void pq_cb(int flags, void *arg)
{
    if(!(flags & FD_READ))
        return;

    MailBox *mbox  = (__bridge MailBox *)arg;
    id inv = [mbox qpop];
    [inv invoke];
}

/* called upon incoming calls */
static void connect_handler(const struct sip_msg *msg, void *arg)
{
    NSLog(@"incoming connect");
    TXSip* sip = (__bridge id)arg;

    id in_call = [[TXSipCall alloc] initIncoming:msg app:sip];
    [sip callIncoming: in_call];
}

/* called when all sip transactions are completed */
static void exit_handler(void *arg)
{
    NSLog(@"exit handler");
    /* stop libre main loop */
    re_cancel();
}


@implementation TXSip
@synthesize auth;
@synthesize user;
@synthesize wrapper;
@synthesize delegate;

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

    return self;
}

- (void) create_ua {

    int err; /* errno return values */
    err = media_snd_init();
    err = srtp_init();

    uac = malloc(sizeof(uac_t));

    /* fetch local IP address */
    err = net_default_source_addr_get(AF_INET, &uac->laddr);

    app = [ReWrap app];

    /* create SIP stack instance */
    err = sip_alloc(&uac->sip, app->dnsc, 32, 32, 32,
                USER_AGENT, exit_handler, NULL);

    NSBundle *b = [NSBundle mainBundle];
    NSString *ca_cert = [b pathForResource:@"CA" ofType: @"cert"];
    NSString *cert = [account cert];
    err = tls_alloc(&app->tls, TLS_METHOD_SSLV23, cert ? _byte(cert) : NULL, NULL);
    tls_add_ca(app->tls, _byte(ca_cert));
	
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
}

- (void)https_ua
{
    [TXRestApi https: (struct httpc*)app];
}

- (void) close
{

    NSLog(@"close");
    sreg = nil;
    
    sipsess_close_all(uac->sock);
    sip_transp_flush(uac->sip);
    sip_close(uac->sip, 1);
    mem_deref(uac->sip);
    mem_deref(uac->sock);

    free(uac);

    tmr_debug();
    mem_debug();

    media_snd_deinit();
}

- (oneway void) apns_token:(NSData*)token
{
    sreg.apns_token = token;
}

- (oneway void) register
{
    NSLog(@"register ...");
}

- (oneway void) setOnline: (reg_state)state
{
    [sreg setState: state];
}

- (oneway void) startCall: (NSString*)dest {

    [self startCallUser: [TXSipUser withName: dest]];
}

- (oneway void) startCallUser: (TXSipUser*)udest
{
    id out_call = [[TXSipCall alloc] initWithApp:self];
    [out_call setDest: udest];
    [out_call setCb: CB(self, callChange:)];
    [out_call send];
    [calls addObject: out_call];
    [delegate() addCall: here(out_call)];
}

- (void) callIncoming: (id)in_call {
    [in_call setCb: CB(self, callChange:)];
    [calls addObject: in_call];
    [delegate() addCall: here(in_call)];
}

- (void) callChange: (TXSipCall*) call {
    NSLog(@"call changed %@, state %d", call, call.cstate);
    if((call.cstate & CSTATE_ALIVE) == 0) {
        [calls removeObject: call];
	[delegate() dropCall: call];
	return;
    }

    if(call.cstate & CSTATE_EST) {
	[delegate() estabCall: call];
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

- (oneway void) setRegObserver: (id)obs {
    sreg.obs = obs;
}

@end
