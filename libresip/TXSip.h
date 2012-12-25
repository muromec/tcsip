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

struct uac;
typedef struct uac uac_t;
struct uac_serv;
typedef struct uac_serv uac_serv_t;

typedef enum {
    REG_OFF,
    REG_BG,
    REG_FG
} reg_state;

@protocol SipDelegate <NSObject>

- (void) addCall: (id)call;
- (void) dropCall: (id)call;
- (void) estabCall: (id)call;

@end

@class TXSipReg;

@interface TXSip : NSObject {
    // sip core and sip services
    uac_t *uac;
    uac_serv_t *uac_serv;

    // move invite listener to separate class
    TXAccount *account;

    TXSipReg *sreg;
    id auth;
    TXSipUser *user;

    NSMutableArray * calls;
    NSMutableArray * chats;

    MProxy *proxy;
    id<SipDelegate> delegate;
}
- (TXSip*) initWithAccount: (id) account;
- (void) register;

- (void) startCall: (NSString*)dest;
- (void) startCallUser: (TXSipUser*)dest;
- (void) callIncoming: (id)in_call;
- (void) startChat: (NSString*)dest;

- (uac_t*) getUa;
- (oneway void) worker;
- (oneway void) stop;
- (oneway void) setRegObserver: (id)obs;
- (oneway void) apns_token:(NSData*)token;
- (oneway void) setOnline: (reg_state)state;

@property id auth;
@property (readonly) TXSipUser* user;
@property (readonly) id proxy;
@property id delegate;

@end

TXSip* sip_init();

#define _byte(_x) ([_x cStringUsingEncoding:NSASCIIStringEncoding])

char* byte(NSString * input);

