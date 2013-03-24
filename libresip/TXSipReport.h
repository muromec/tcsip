//
//  TXSipReport.h
//  libresip
//
//  Created by Ilya Petrov on 3/23/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>

@class MailBox;
@class TXSipCall;

@interface TXSipReport : NSObject {
    MailBox *box;
}

- (id) initWithBox:(MailBox*)pBox;

@property MailBox *box;

@end

enum reg_state;
struct tcsipcall;
void report_reg(enum reg_state state, void*arg);
void report_call_change(struct tcsipcall* call, void *arg);
void report_call(struct tcsipcall* call, void *arg);

