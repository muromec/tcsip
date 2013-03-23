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
@class TXSipReport;
@class TXSipIpc;

@interface TXSip : NSObject {
    // sip core and sip services
    TXSipReport* report;
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
}
- (TXSip*) initWithAccount: (id) account;

// for callback
- (void) callIncoming: (id)in_call;

- (uac_t*) getUa;
- (oneway void) close;

- (TXSipIpc*) ipc:(MailBox*)pBox;

@property id auth;
@property id uplinks;
@property (readonly) TXSipUser* user;
@property MailBox* mbox;
@property (readonly) TXSipReg* sreg;

@end

TXSip* sip_init();


char* byte(NSString * input);


extern NSDateFormatter *http_df;

