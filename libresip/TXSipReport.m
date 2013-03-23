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

@implementation TXSipReport 
@synthesize box;
- (id) initWithBox:(MailBox*)pBox {
    self = [super init];
    if(!self) return self;

    box = pBox;

    return self;
}

- (void) uplinkUpd: (NSString*)uri state:(NSString*)state
{
    msgpack_packer *pk = [box packer];
    msgpack_pack_array(pk, 4);
    push_cstr("sip.up");
    msgpack_pack_int(pk, 2);
    push_str(uri);
    push_str(state);
}
- (void) uplinkAdd: (NSString*)uri state:(NSString*)state
{
    msgpack_packer *pk = [box packer];
    msgpack_pack_array(pk, 4);
    push_cstr("sip.up");
    msgpack_pack_int(pk, 1);
    push_str(uri);
    push_str(state);

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
    msgpack_pack_array(pk, 2);
    push_cstr("sip.reg");
    msgpack_pack_int(pk, state);
}

- (void) reportCall:(TXSipCall*)call {

    NSString *ckey = [call ckey];

    msgpack_packer *pk = [box packer];
    msgpack_pack_array(pk, 7);
    push_cstr("sip.call.add");
    push_str(ckey);
    msgpack_pack_int(pk, call.cdir);
    msgpack_pack_int(pk, call.cstate);
    msgpack_pack_int(pk, (int)[call.date_create timeIntervalSince1970]);
    push_str(call.dest.name);
    push_str(call.dest.user);
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
