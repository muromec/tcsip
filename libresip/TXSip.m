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


#define _byte(_x) ([_x cStringUsingEncoding:NSASCIIStringEncoding])
#define delegate( ) (self->delegate)
#define here(obj) ([MProxy withTargetBox: obj box:proxy.mbox])


char* byte(NSString * input){
      int len = (int)[input lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

      char *toReturn = (char *)malloc((sizeof(char) * len) +1);

      strcpy(toReturn,[input cStringUsingEncoding:NSUTF8StringEncoding]);

      return toReturn;
}

typedef struct {
    struct tmr tmr;
    void *arg;
} timer_cb_t;

void timer_cb(void *arg)
{
    timer_cb_t *cb = arg;

    MProxy *proxy = (__bridge MProxy *)(cb->arg);
    id inv;
    while((inv = [proxy.mbox qpop])) {
	[inv invoke];
    }

next:
    tmr_start(&cb->tmr, 100, timer_cb, arg);
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
@synthesize proxy;

- (id) initWithAccount: (id) pAccount
{
    self = [super init];
    if (!self || self < 0) {
        return self;
    }

    account = pAccount;

    [self create_ua];
    calls = [[NSMutableArray alloc] init];
    chats = [[NSMutableArray alloc] init];
    
    auth = [[TXSipAuth alloc] initWithApp: self];
    sreg = [[TXSipReg alloc] initWithApp:self];

    proxy = [MProxy withTarget: self];

    return self;
}

- (void) create_ua {
	
	int err; /* errno return values */

	/* fetch local IP address */
	err = net_default_source_addr_get(AF_INET, &uac.laddr);

	uac_serv.nsc = ARRAY_SIZE(uac_serv.nsv);
	/* fetch list of DNS server IP addresses */
	err = dns_srv_get(NULL, 0, uac_serv.nsv, &uac_serv.nsc);
	
	/* create DNS client */
	err = dnsc_alloc(&uac_serv.dns, NULL, uac_serv.nsv, uac_serv.nsc);

	/* create SIP stack instance */
	err = sip_alloc(&uac.sip, uac_serv.dns, 32, 32, 32,
			"TexR/OSX libre",
			exit_handler, NULL);


        NSBundle *b = [NSBundle mainBundle];
        NSString *ca_cert = [b pathForResource:@"CA" ofType: @"cert"];
	NSString *cert = [account cert];
        err = tls_alloc(&uac_serv.tls, TLS_METHOD_SSLV23, cert ? _byte(cert) : NULL, NULL);
        tls_add_ca(uac_serv.tls, _byte(ca_cert));
	/* listen on random port */
	sa_set_port(&uac.laddr, 0);

	/* add supported SIP transports */
	err |= sip_transp_add(uac.sip, SIP_TRANSP_TLS, &uac.laddr, uac_serv.tls);

	/* create SIP session socket */
	err = sipsess_listen(&uac.sock, uac.sip, 32, connect_handler, (__bridge void*)self);

}

- (oneway void) register:(NSString*)pUser
{
    NSLog(@"register ...");

    user = [TXSipUser withName: pUser];
    //[auth setCreds: user password:pPassword];

    [sreg setDest: user];
    [sreg send];

}
- (oneway void) worker
{
    NSLog(@"start worker");
    timer_cb_t *cb = malloc(sizeof(timer_cb_t));
    tmr_init(&cb->tmr);
    cb->arg = (__bridge void*)proxy;
    tmr_start(&cb->tmr, 100, timer_cb, cb);
    re_main(NULL);
}

- (oneway void) startCall: (NSString*)dest {
    id out_call = [[TXSipCall alloc] initWithApp:self];

    id udest  = [TXSipUser withName: dest];
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
    }
}

- (oneway void) startChat: (NSString*)dest {
    TXSipMessage *out_chat = [[TXSipMessage alloc] initWithApp:self];
    id udest  = [TXSipUser withName: dest];
    [out_chat setDest: udest];

    [out_chat send];

    [chats addObject: out_chat];
}

- (oneway void) callControl:(int)action
{
    if(![calls count])
        return;

    [[calls lastObject] control:action];
}

- (uac_t*) getUa
{
    return &uac;
}

@end
