//
//  TXSip.h
//  Texr
//
//  Created by Ilya Petrov on 8/25/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Callback.h"
#import "TXSipUser.h"
#import "TXAccount.h" // XXX: depends out!

#import "MProxy.h"

#include <string.h>
#include "re.h"

typedef struct {
    struct sip *sip;
    struct sa laddr;
    struct sipsess_sock *sock;
}
uac_t;

typedef struct {
    struct sa nsv[16];
    uint32_t nsc;
    struct dnsc *dns;
    struct tls *tls;
} uac_serv_t;

@protocol SipDelegate <NSObject>

- (void) addCall: (id)call;
- (void) dropCall: (id)call;

@end

@interface TXSip : NSObject {
    // sip core and sip services
    uac_t uac;
    uac_serv_t uac_serv;

    // move invite listener to separate class
    TXAccount *account;

    id sreg;
    id auth;
    id user;

    NSMutableArray * calls;
    NSMutableArray * chats;

    MProxy *proxy;
    id delegate;
}
- (TXSip*) initWithAccount: (id) account;
- (void) register:(NSString*)user;

- (void) startCall: (NSString*)dest;
- (void) callIncoming: (id)in_call;
- (void) startChat: (NSString*)dest;

- (void) callControl:(int)action;

- (uac_t*) getUa;
- (oneway void) worker;

@property id auth;
@property (readonly) TXSipUser* user;
@property (readonly) id proxy;

@end

TXSip* sip_init();

#define _byte(_x) ([_x cStringUsingEncoding:NSASCIIStringEncoding])

char* byte(NSString * input);

