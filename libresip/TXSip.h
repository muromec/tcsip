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

    MailBox *mbox;
    MailBox *own_box;
}
- (TXSip*) initWithAccount: (id) account;

// for callback
- (void) callIncoming: (id)in_call;

- (uac_t*) getUa;
- (oneway void) close;
- (void)reportReg:(enum reg_state)state;

// public api
- (void)online:(reg_state)state;
- (void)control:(NSString*)ckey op:(int)op;
- (void)call:(TXSipUser*)user;
- (void)apns:(NSData*)token;

@property id auth;
@property id uplinks;
@property (readonly) TXSipUser* user;
@property MailBox* mbox;
@property MailBox* own_box;

@end

TXSip* sip_init();

#define _byte(_x) ([_x cStringUsingEncoding:NSASCIIStringEncoding])

char* byte(NSString * input);

#define _str(__x, __len) ([[NSString alloc] initWithBytes:__x length:__len encoding:NSASCIIStringEncoding])
#define _pstr(__pl) (_str(__pl.p, __pl.l))

extern NSDateFormatter *http_df;

