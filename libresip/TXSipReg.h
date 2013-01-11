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

typedef enum {
    REG_NONE,
    REG_START=1,
    REG_AUTH=2,
    REG_ONLINE=4
} reg_state_t;

@protocol RegObserver
- (void) onlineState: (NSString*)state;
- (void) uplink: (NSString*)uri alive:(bool)state;
@end

@interface TXSipReg : TXBaseOp {
    struct sipreg *reg;
    struct tcp_conn *upstream;
    CFReadStreamRef upstream_ref;
    reg_state_t rstate;
    NSString* instance_id;
    NSString* apns_token;
    id<RegObserver> obs;
    int reg_time;
    struct tmr reg_tmr;
}

- (void) setup;

- (void) send;

- (void) response: (int) status phrase:(const char*)phrase;
- (void) setInstanceId: (NSString*) pUUID;
- (void) voipDest:(struct tcp_conn *)conn;
- (void) contacts: (const struct sip_msg*)msg;
- (void) uplink: (NSString*) up;
- (void) setState: (reg_state) state;

@property NSData* apns_token;
@property id<RegObserver> obs;

@end
