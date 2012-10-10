//
//  TXRTP.h
//  Texr
//
//  Created by Ilya Petrov on 8/27/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <rtp.h>

@interface TXRTP : NSObject{
    CFSocketRef ref;
    id caller;
    int seq;
    int ts;

    NSData *remote;
    rtp_msg_t sm;
}
- (void) incoming:(CFDataRef)data;
- (void) check_address_switch:(CFDataRef)address;
- (void) canSend;
- (id) initWithCaller: (id) caller;

- (void) listen: (NSString*)pAddr port:(int)pPort;
- (void) connect: (NSString*)pAddr port:(int)pPort;
- (void) sendPt: (int)pt;

- (void) runLoop;
- (void) stopSocket;

@end
