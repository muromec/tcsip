//
//  TXSipIpc.h
//  libresip
//
//  Created by Ilya Petrov on 3/23/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>
@class MailBox;
@class TXSipReg;

struct sip_addr;
struct pl;

@interface TXSipIpc : NSObject {
    MailBox *box;
    void *harg;
}

- (id) initWithBox:(MailBox*)pBox;

@property void* harg;

- (void)online:(int)state;
- (void)control:(NSString*)ckey op:(int)op;
- (void)call:(NSString*)user name:(NSString*)name;
- (void)apns:(NSData*)token;
- (void)uuid:(NSString*)uuid;
- (void)me:(NSString*)login name:(NSString*)name;

@end
