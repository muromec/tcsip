//
//  TXSipReport.m
//  libresip
//
//  Created by Ilya Petrov on 3/23/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import "TXSipReport.h"
#import "MailBox.h"
#import "TXSipCall.h"
#include <msgpack.h>
#include "strmacro.h"
#include "re.h"
#include "tcsipreg.h"
#include "tcsipcall.h"

void report_call_change(struct tcsipcall* call, void *arg) {
    int cstate, reason;
    char *ckey;
    msgpack_packer *pk = arg;

    tcsipcall_dirs(call, NULL, &cstate, &reason, NULL);
    tcsipcall_ckey(call, &ckey);

    if((cstate & CSTATE_ALIVE) == 0) {
        tcsipcall_remove(call); // wrong place for this

        msgpack_pack_array(pk, 3);
        push_cstr("sip.call.del");
        push_cstr(ckey);
        msgpack_pack_int(pk, reason);

	return;
    }

    if(cstate & CSTATE_EST) {
        msgpack_pack_array(pk, 2);
        push_cstr("sip.call.est");
        push_cstr(ckey);

	return;
    }

}

void report_call(struct tcsipcall* call, void *arg) {
    int cdir, cstate, ts;
    struct sip_addr *remote;
    char *ckey;

    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, 7);
    push_cstr("sip.call.add");

    tcsipcall_ckey(call, &ckey);
    push_cstr(ckey);

    tcsipcall_dirs(call, &cdir, &cstate, NULL, &ts);

    msgpack_pack_int(pk, cdir);
    msgpack_pack_int(pk, cstate);
    msgpack_pack_int(pk, ts);

    tcop_lr((void*)call, NULL, &remote);
    push_pl(remote->dname);
    push_pl(remote->uri.user);

}

void report_reg(enum reg_state state, void*arg) {
    msgpack_packer *pk = arg;
    msgpack_pack_array(pk, 2);
    push_cstr("sip.reg");
    msgpack_pack_int(pk, state);
}

@implementation TXSipReport 
@synthesize box;
- (id) initWithBox:(MailBox*)pBox {
    self = [super init];
    if(!self) return self;

    box = pBox;

    return self;
}

- (void) uplinkUpd: (NSString*)uri state:(int)state
{
    msgpack_packer *pk = [box packer];
    msgpack_pack_array(pk, 4);
    push_cstr("sip.up");
    msgpack_pack_int(pk, 2);
    push_str(uri);
    msgpack_pack_int(pk, state);
}
- (void) uplinkAdd: (NSString*)uri state:(int)state
{
    msgpack_packer *pk = [box packer];
    msgpack_pack_array(pk, 4);
    push_cstr("sip.up");
    msgpack_pack_int(pk, 1);
    push_str(uri);
    msgpack_pack_int(pk, state);
}
- (void) uplinkRm: (NSString*)uri
{
    msgpack_packer *pk = [box packer];
    msgpack_pack_array(pk, 3);
    push_cstr("sip.up");
    msgpack_pack_int(pk, 0);
    push_str(uri);
}

- (void)reportReg:(int)state
{
    msgpack_packer *pk = [box packer];
    report_reg(state, pk);
}

- (void) reportCall:(TXSipCall*)call {
}

- (void) reportCallEst:(TXSipCall*)call {
    NSString *ckey = [call ckey];

    msgpack_packer *pk = [box packer]; 
    msgpack_pack_array(pk, 2);
    push_cstr("sip.call.est");
    push_str(ckey);
}

- (void) reportCallDrop:(TXSipCall*)call {
    NSString *ckey = [call ckey];

    msgpack_packer *pk = [box packer]; 
    msgpack_pack_array(pk, 3);
    push_cstr("sip.call.del");
    push_str(ckey);
    msgpack_pack_int(pk, call.end_reason);
}
@end
