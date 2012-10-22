//
//  TXRTP.m
//  Texr
//
//  Created by Ilya Petrov on 8/27/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXRTP.h"
#import "TXSip.h" // XXX: move byte macros to utils.h
#import "TXCallMedia.h"

#if 0
#define DRTP(act, _m) {\
NSLog(@"%s seq:%d ssrc:%x ts:%d m:%d pt:%d v:%d p:%d x:%d cc:%d", act, \
     ntohs(_m.header.seq), ntohl(_m.header.ssrc), ntohs(_m.header.ts), \
     _m.header.m, _m.header.pt, _m.header.version, \
     _m.header.p, _m.header.x, _m.header.cc); }
#else
#define DRTP(act, _m)
#endif

void rtp_cb(
                     CFSocketRef s,
                     CFSocketCallBackType callbackType,
                     CFDataRef address,
                     const void *data,
                     void *info
                     ) {

    TXRTP *rtp = (__bridge TXRTP *)(info);
    switch(callbackType) {
    case kCFSocketDataCallBack:
        [rtp incoming:data];
	[rtp check_address_switch:address];
	break;
    case kCFSocketWriteCallBack:
        [rtp canSend];
	break;
    default:
        NSLog(@"event %ld", callbackType);
    }
}

@implementation TXRTP
- (id) initWithCaller: (id) pCaller {

    self = [super init];
    if(! self) {
        return self;
    }

    CFSocketContext ctx = {0, (__bridge void *)(self), 0, 0, 0};

    ref = CFSocketCreate(kCFAllocatorDefault,
	PF_INET, SOCK_DGRAM, 0,
        (
	 kCFSocketAcceptCallBack |
         kCFSocketReadCallBack |
         kCFSocketDataCallBack |
	 kCFSocketWriteCallBack |
         kCFSocketConnectCallBack
        ),
        rtp_cb, &ctx
    );
    
    CFOptionFlags sockopt = CFSocketGetSocketFlags(ref);
    sockopt |= kCFSocketAutomaticallyReenableWriteCallBack;
    CFSocketSetSocketFlags(ref, sockopt);

    self->caller = pCaller;

    seq = rand() & 0x7FFF;
    ts = 0;

    return self;
}

- (void) runLoop {
    CFRunLoopSourceRef loop_recv = CFSocketCreateRunLoopSource(NULL, ref , 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), loop_recv, kCFRunLoopCommonModes);

    NSLog(@"rtp loop started");
    CFRelease(loop_recv);
}

- (void) stopSocket
{
    caller = nil;
    CFSocketInvalidate(ref);
    CFRelease(ref);
}

- (void) listen: (NSString*)pAddr port:(int)pPort
{

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(_byte(pAddr));
    addr.sin_port = htons(pPort);

    NSData *address = [ NSData dataWithBytes: &addr length: sizeof(addr) ];
    CFSocketSetAddress(ref, (__bridge CFDataRef)(address));

}

- (void) connect: (NSString*)pAddr port:(int)pPort {

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(_byte(pAddr));
    addr.sin_port = htons(pPort);

    remote = [ NSData dataWithBytes: &addr length: sizeof(addr) ];

    sm.header.ssrc = rand(); // wtf
    sm.header.m = 0;
    sm.header.pt = 0;
    sm.header.version = 2;
    sm.header.p = 0;
    sm.header.x = 0;
    sm.header.cc = 0;
}

- (void) sendPt: (int)pt
{
    sm.header.pt = pt;
}

- (void) incoming:(CFDataRef)data
{
    if(!caller)
        return;

    rtp_msg_t message;

    CFIndex len = CFDataGetLength(data);
    CFDataGetBytes(data, ((CFRange){0, len}), (void*)&message);
    // XXX: chech data length;
    // XXX: check format, version, source and magic;

    len -= sizeof(message.header);
    [caller rtpData: message.body len:(int)len ts:ntohs(message.header.ts)];

    ts = ntohs(message.header.ts);
    DRTP("got", message);
}

- (void) canSend
{
    int len = 0;

    sm.header.seq = htons(seq);
    sm.header.ts = htons(ts);

    len = [caller rtpInput: &sm.body[0]];
    if (!len)
        return;
    
    seq ++;

    len += sizeof(sm.header);
    DRTP("send", sm);

    CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8*)&sm, len, kCFAllocatorNull);

    CFSocketError ret = CFSocketSendData(ref, (__bridge CFDataRef)remote, data, 5);
    CFRelease(data);
    
    if(ret == noErr)
        return;

    NSLog(@"rtp send error %ld", ret);
}

/* if rtp data packed arrived from
 * address different from * configured,
 * then reconfigure */
- (void) check_address_switch:(CFDataRef)address
{
    struct sockaddr_in addr;
    struct sockaddr_in *conf;

    CFDataGetBytes(address, ((CFRange){0, sizeof(addr)}), (void*)&addr);
    conf = (struct sockaddr_in *)CFDataGetBytePtr((__bridge CFDataRef)(remote));

    if(addr.sin_addr.s_addr == conf->sin_addr.s_addr)
	return;

    if(addr.sin_port != conf->sin_port)
        return;

    conf->sin_addr.s_addr = addr.sin_addr.s_addr;

    NSLog(@"rtp address switched");
}

@end
