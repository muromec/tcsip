//
//  TXSipIpc.m
//  libresip
//
//  Created by Ilya Petrov on 3/23/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import "TXSipIpc.h"
#include "tcsip.h"
#import "MailBox.h"
#include <msgpack.h>
#include "strmacro.h"
#include "re.h"
#include "tcreport.h"

@implementation TXSipIpc
@synthesize harg;
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

- (void)call:(NSString*)user name:(NSString*)name
{
    struct msgpack_packer *pk = box.packer;
    msgpack_pack_array(pk, 3);
    push_cstr("sip.call.place");
    push_str(user);
    push_str(name);
}

- (void) apns:(NSData*)token
{
    struct msgpack_packer *pk = box.packer;
    msgpack_pack_array(pk, 2);
    push_cstr("sip.apns");
    msgpack_pack_raw(pk, [token length]);
    msgpack_pack_raw_body(pk, [token bytes], [token length]);
}

- (void) uuid:(NSString*)uuid
{
    struct msgpack_packer *pk = box.packer;
    msgpack_pack_array(pk, 2);
    push_cstr("sip.uuid");
    push_str(uuid);
}

- (void)me:(NSString*)login
{
    struct msgpack_packer *pk = box.packer;
    msgpack_pack_array(pk, 2);
    push_cstr("sip.me");
    push_str(login);
}

- (void)obCmd:(msgpack_object)ob
{
    tcsip_ob_cmd(harg, ob);
}

@end
