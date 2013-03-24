//
//  TXSip.h
//  Texr
//
//  Created by Ilya Petrov on 8/25/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Callback.h"
#import "TXAccount.h" // XXX: depends out!

#include <string.h>

struct uac;
typedef struct uac uac_t;
struct sip_msg;
enum reg_state;

struct reapp;
struct sip_addr;
struct tcsipreg;
struct list;

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

    struct tcsipreg *sreg_c;
    TXUplinks *uplinks;
    struct sip_addr *user_c;

    struct list *calls_c;

    MailBox *mbox;
}
- (TXSip*) initWithAccount: (id) account;

// for callback
- (void) callIncoming: (const struct sip_msg *)msg;

- (uac_t*) getUa;
- (oneway void) close;

- (TXSipIpc*) ipc:(MailBox*)pBox;

@property id uplinks;
@property MailBox* mbox;

@end

TXSip* sip_init();


char* byte(NSString * input);


extern NSDateFormatter *http_df;

