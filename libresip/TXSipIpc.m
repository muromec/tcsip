//
//  TXSipIpc.m
//  libresip
//
//  Created by Ilya Petrov on 3/23/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import "TXSipIpc.h"
#import "TXSipUser.h"
#import "TXSipReg.h"
#import "MailBox.h"
#include <msgpack.h>
#include "strmacro.h"

@implementation TXSipIpc
@synthesize delegate;
- (id) initWithBox:(MailBox*)pBox {
    self = [super init];
    if(!self) return self;

    box = pBox;

    return self;
}

- (void)online:(int)state
{
    struct msgpack_packer *pk = box.packer;
    msgpack_pack_array(pk, 2);
    push_cstr("sip.online");
    msgpack_pack_int(pk, state);
}

- (void)control:(NSString*)ckey op:(int)op
{
    struct msgpack_packer *pk = box.packer;
    msgpack_pack_array(pk, 3);
    push_cstr("sip.call.control");
    push_str(ckey);
    msgpack_pack_int(pk, op);
}

- (void)call:(TXSipUser*)dest
{
    struct msgpack_packer *pk = box.packer;
    msgpack_pack_array(pk, 3);
    push_cstr("sip.call.place");
    push_str(dest.user);
    push_str(dest.name);
}

- (void) apns:(NSData*)token
{
    struct msgpack_packer *pk = box.packer;
    msgpack_pack_array(pk, 2);
    push_cstr("sip.apns");
    msgpack_pack_raw(pk, [token length]);
    msgpack_pack_raw_body(pk, [token bytes], [token length]);
}

- (void)obCmd:(msgpack_object)ob
{
    msgpack_object *arg;
    msgpack_object_raw cmd;
    arg = ob.via.array.ptr;

    if(arg->type != MSGPACK_OBJECT_RAW)
	    return;

    cmd = arg->via.raw;

    if(!strncmp(cmd.ptr, "sip.online", cmd.size)) {
        arg++;
        [delegate setOnline: (reg_state)arg->via.i64];
    }

    if(!strncmp(cmd.ptr, "sip.call.place", cmd.size)) {
        arg++;
        NSString *login = _str(arg->via.raw.ptr, arg->via.raw.size);
        arg++;
        TXSipUser* dest = [TXSipUser withName:login];
        dest.name = _str(arg->via.raw.ptr, arg->via.raw.size);
        [delegate startCallUser:dest];
    }
    if(!strncmp(cmd.ptr, "sip.call.control", cmd.size)) {
        arg++;
        NSString *ckey = _str(arg->via.raw.ptr, arg->via.raw.size);
        arg++;
	int op = (int)arg->via.i64;
	[delegate doCallControl:ckey op:op];
    }

    if(!strncmp(cmd.ptr, "sip.apns", cmd.size)) {
        arg++;
	delegate.sreg.apns_token = [NSData dataWithBytes:
		arg->via.raw.ptr
		length:	arg->via.raw.size];
    }
}



@end
