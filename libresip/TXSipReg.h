//
//  TXSipReg.h
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "TXBaseOp.h"
#include <re.h>

struct tcp_conn;

enum reg_state {
    REG_NONE,
    REG_START=1,
    REG_AUTH=2,
    REG_ONLINE=4,
    REG_TRY=5,
};

typedef enum reg_state reg_state_t;

@protocol RegObserver
- (void)reportReg:(reg_state_t)state;
@end

@protocol Obs
- (void) report:(id)data;
@end

@interface TXSipReg : TXBaseOp {
    struct sipreg *reg;
    struct tcp_conn *upstream;
    CFReadStreamRef upstream_ref;
    reg_state_t rstate;
    NSString* instance_id;
    NSString* apns_token;
    id<RegObserver> delegate;
    int reg_time;
    struct tmr reg_tmr;
}

- (void) setup;

- (void) send;

- (void) response: (int) status phrase:(const char*)phrase;
- (void) setInstanceId: (NSString*) pUUID;
- (void) voipDest:(struct tcp_conn *)conn;
- (void) contacts: (const struct sip_msg*)msg;
- (void) setState: (reg_state) state;

@property NSData* apns_token;
@property id delegate;

@end
