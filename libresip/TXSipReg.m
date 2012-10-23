//
//  TXSipReg.m
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXSip.h"
#import "TXSipReg.h"

#import "txsip_private.h"

static const char *registrar = "sip:crap.muromec.org.ua";

/* called when register responses are received */
static void register_handler(int err, const struct sip_msg *msg, void *arg)
{
        id ctx = (__bridge id)arg;

        if(err == 0 && msg)
            [ctx response: msg->scode  phrase:msg->reason.p];
        else
            [ctx response: 900 phrase: "Internal error"];
}


@implementation TXSipReg

- (void) setup
{
    rstate = REG_NONE;
}

- (void) send
{
    int err;
    const char *uri = _byte(dest.addr);
    const char *name = _byte(dest.name);

    err = sipreg_register(&reg, uac->sip, registrar, uri, uri, 60, name,
                          NULL, 0, 1, auth_handler, ctx, false,
                          register_handler, ctx, NULL, NULL);

    NSLog(@"send register %d", err);
    rstate |= REG_START;
}

- (void) response: (int) status phrase:(const char*)phrase
{
    NSLog(@"reg response %d, %@", status, cb);

    switch(status) {
    case 200:
	if((rstate & REG_AUTH)==0)
	    [cb response: @"ok"];

	rstate |= REG_AUTH;
	rstate |= REG_ONLINE;
	break;
    case 401:
	if(cb && (rstate & REG_AUTH)==0) {
	    mem_deref(reg);
	    [cb response: nil];
	}
	// fallthrough!
    default:
	rstate &= rstate^REG_ONLINE;
	[cb response: @"lost"];
    }
}

- (void) dealloc
{
   NSLog(@"dealloc sreg");
   mem_deref(reg);
}

@end
