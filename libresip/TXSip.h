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
enum reg_state;

struct reapp;

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

@protocol Wrapper;
@class ReWrap;
@class TXSipReg;
@class TXUplinks;
@class MailBox;

@interface TXSip : NSObject {
    // sip core and sip services
    uac_t *uac;
    struct reapp *app;

    // move invite listener to separate class
    TXAccount *account;

    TXSipReg *sreg;
    TXUplinks *uplinks;
    id auth;
    TXSipUser *user;

    NSMutableArray * calls;
    NSMutableArray * chats;

    ReWrap* wrapper;
    id<SipDelegate> delegate;
    MailBox *mbox;
}
- (TXSip*) initWithAccount: (id) account;

- (void) startCall: (NSString*)dest;
- (void) startCallUser: (TXSipUser*)dest;
- (void) callIncoming: (id)in_call;
- (void) startChat: (NSString*)dest;

- (uac_t*) getUa;
- (oneway void) setRegObserver: (id)obs;
- (oneway void) apns_token:(NSData*)token;
- (oneway void) setOnline: (reg_state)state;
- (oneway void) close;
- (void)reportReg:(enum reg_state)state;

@property id auth;
@property id uplinks;
@property (readonly) TXSipUser* user;
@property id<Wrapper> wrapper;
@property id delegate;
@property MailBox* mbox;

@end

TXSip* sip_init();

#define _byte(_x) ([_x cStringUsingEncoding:NSASCIIStringEncoding])

char* byte(NSString * input);

#define _str(__x, __len) ([[NSString alloc] initWithBytes:__x length:__len encoding:NSASCIIStringEncoding])
#define _pstr(__pl) (_str(__pl.p, __pl.l))

extern NSDateFormatter *http_df;

