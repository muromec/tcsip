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

static const char *registrar = "sip:sip.texr.net";

void impossible_cb (
   CFSocketRef s,
   CFSocketCallBackType callbackType,
   CFDataRef address,
   const void *data,
   void *info
) {
    NSLog(@"impossible!\n");
}

static bool find_uplink(const struct sip_hdr *hdr, const struct sip_msg *msg,
			  void *arg)
{
    id ctx = (__bridge id)arg;

    struct sip_addr addr;
    struct pl state;
    state.l = 0;
    int err;

    err = sip_addr_decode(&addr, &hdr->val);
    if(err) {
        return false;
    }
    err = sip_param_exists(&addr.params, "uplink", &state);
    if(err == ENOENT) {
        return false;
    }
    if(err) {
        return false;
    }

    state.l = 0;
    sip_param_decode(&addr.params, "uplink", &state);

    NSString *nstate;
    NSString *str = [[NSString alloc] 
        initWithBytes: addr.auri.p
               length: addr.auri.l
             encoding: NSASCIIStringEncoding
    ];
    if(state.l) {
        nstate = [[NSString alloc]
            initWithBytes: state.p
               length: state.l
             encoding: NSASCIIStringEncoding
        ];
    } else {
        nstate = @"ok";
    }
    [ctx addObject: [NSArray arrayWithObjects: str, nstate, nil]];

    return false;
}


/* called when register responses are received */
static void register_handler(int err, const struct sip_msg *msg, void *arg)
{
        id ctx = (__bridge id)arg;

        if(err == 0 && msg)
            [ctx response: msg->scode  phrase:msg->reason.p];
        else
            [ctx response: 900 phrase: "Internal error"];

        [ctx contacts: msg];
	[ctx voipDest: sip_msg_tcpconn(msg)];
}

@implementation TXSipReg
@synthesize obs;
- (void) setup
{
    rstate = REG_NONE;
    reg_time = 60;
}

- (void) setState: (reg_state) state
{
    switch(state) {
    case REG_OFF:
        return;
    case REG_FG:
        reg_time = 60;
        break;
    case REG_BG:
        reg_time = 610;
        break;
    }
 
    NSLog(@"set state %d time %d", state, reg_time);
    if(reg)
        [self resend];
    else
	[self send];
}

- (void)resend
{
    reg_time++;
    sipreg_expires(reg, reg_time);
    [obs onlineState: @"try"];
}

- (void) send
{
    int err;
    const char *uri = _byte(dest.addr);
    const char *user = _byte(dest.user);

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
    }

    err = sipreg_register(&reg, uac->sip, registrar, uri, uri, reg_time, user,
                          NULL, 0, 1, auth_handler, ctx, false,
                          register_handler, ctx, params, fmt);

    NSLog(@"send register %d %d %s", err, reg_time, user);
    if(err) {
        [obs onlineState: @"off"];
    } else {
        [obs onlineState: @"try"];
        rstate |= REG_START;
    }
    NSLog(@"sreg apns token %@", apns_token);
}

- (void) response: (int) status phrase:(const char*)phrase
{
    NSLog(@"reg response %d, %@", status, cb);

    switch(status) {
    case 200:
        [obs onlineState: @"ok"];

	rstate |= REG_AUTH;
	rstate |= REG_ONLINE;
	break;
    case 401:
	if((rstate & REG_AUTH)==0) {
            NSLog(@"auth fail. how this can be?");
	}
	// fallthrough!
    default:
	rstate &= rstate^REG_ONLINE;
        [obs onlineState: @"lost"];
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
    const char *fmt = NULL;
    NSString* nfmt;
    char enc_token[50];
    size_t elen = sizeof(enc_token);
    memset(enc_token, 0, elen);

    base64_encode([token bytes], [token length], enc_token, &elen);
    apns_token = [NSString stringWithFormat:@"%s", enc_token];

    if(apns_token) {
	nfmt = [NSString stringWithFormat:@"Push-Token: %@\r\n", apns_token];
	fmt = _byte(nfmt);
    }
    sipreg_headers(reg, fmt);

    NSLog(@"set token: %@ token %s", token, enc_token);
}

- (NSData*)apns_token
{
    return nil;
}

- (void) voipDest:(struct tcp_conn *)conn
{
    if(conn == upstream)
       return;

    int fd = tcp_conn_fd(conn);
    re_printf("fd %d\n", fd);
    if(upstream_ref) {
        CFRelease(upstream_ref);
        upstream_ref = NULL;
    }

    if(fd==-1) {
        return;
    }

    CFReadStreamRef read_stream;
    CFStreamCreatePairWithSocket(NULL, fd, &read_stream, NULL);
    CFReadStreamSetProperty(read_stream, kCFStreamNetworkServiceType, kCFStreamNetworkServiceTypeVoIP);
    CFReadStreamOpen(read_stream);

    NSLog(@"read stream %@", read_stream);

    upstream = conn;
    upstream_ref = read_stream;
}

- (void) contacts: (const struct sip_msg*)msg
{
    NSMutableArray *ups = [[NSMutableArray alloc] init];

    sip_msg_hdr_apply(msg, true, SIP_HDR_CONTACT, find_uplink, (__bridge void*)ups);

    [app.uplinks report: ups];
}

@end
