//
//  TXSipReg.m
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXSip.h"
#import "TXSipReg.h"

#include "txsip_private.h"

static const char *registrar = "sip:crap.muromec.org.ua";

void impossible_cb (
   CFSocketRef s,
   CFSocketCallBackType callbackType,
   CFDataRef address,
   const void *data,
   void *info
) {
    NSLog(@"impossible!\n");
}

/* called when register responses are received */
static void register_handler(int err, const struct sip_msg *msg, void *arg)
{
        id ctx = (__bridge id)arg;

        if(err == 0 && msg)
            [ctx response: msg->scode  phrase:msg->reason.p];
        else
            [ctx response: 900 phrase: "Internal error"];

	[ctx voipDest: sip_msg_tcpconn(msg)];
}


@implementation TXSipReg
- (void) setup
{
    rstate = REG_NONE;
}

- (void)resend
{
    mem_deref(reg);
    [self send];
}

- (void) send
{
    int err;
    const char *uri = _byte(dest.addr);
    const char *name = _byte(dest.name);

    NSString* np = [NSString
        stringWithFormat:@"+sip.instance=\"<urn:uuid:%@>\"",
        instance_id
    ];
    const char *params = _byte(np);
    const char *fmt = NULL;
    NSString* nfmt;
    if(apns_token) {
	nfmt = [NSString stringWithFormat:@"Push-Token: %@\r\n", apns_token];
	fmt = _byte(nfmt);
	NSLog(@"token fmt: %s", fmt);
    }

    err = sipreg_register(&reg, uac->sip, registrar, uri, uri, 60, name,
                          NULL, 0, 1, auth_handler, ctx, false,
                          register_handler, ctx, params, fmt);

    NSLog(@"send register %d %@", err, cb);
    if(err) {
        [cb response: @"off"];
    } else {
        rstate |= REG_START;
    }
    NSLog(@"sreg apns token %@", apns_token);
}

- (void) response: (int) status phrase:(const char*)phrase
{
    NSLog(@"reg response %d, %@", status, cb);

    switch(status) {
    case 200:
	[cb response: @"ok"];

	rstate |= REG_AUTH;
	rstate |= REG_ONLINE;
	break;
    case 401:
	if(cb && (rstate & REG_AUTH)==0) {
            NSLog(@"auth fail. how this can be?");
	}
	// fallthrough!
    default:
	rstate &= rstate^REG_ONLINE;
	[cb response: @"lost"];
    }
}

- (void) dealloc
{
   mem_deref(reg);
}

- (void) setInstanceId: (NSString*) pUUID
{
    instance_id = pUUID;
}

- (void)setApns_token:(NSData*)token
{
    char enc_token[50];
    int elen = sizeof(enc_token);
    memset(enc_token, 0, elen);

    base64_encode([token bytes], [token length], enc_token, &elen);

    apns_token = [NSString stringWithFormat:@"%s", enc_token];

    NSLog(@"set token: %@ token %s", token, enc_token);
    [self resend];
}

- (void) voipDest:(struct tcp_conn *)conn
{
    if(conn == upstream)
       return;

    int fd = tcp_conn_fd(conn);
    re_printf("fd %d\n", fd);
    if(upstream_ref)
        CFRelease(upstream_ref);

    CFReadStreamRef read_stream;
    CFStreamCreatePairWithSocket(NULL, fd, &read_stream, NULL);
    CFReadStreamSetProperty(read_stream, kCFStreamNetworkServiceType, kCFStreamNetworkServiceTypeVoIP);
    CFReadStreamOpen(read_stream);

    NSLog(@"read stream %@", read_stream);

    upstream = conn;
    upstream_ref = read_stream;
}

@end
