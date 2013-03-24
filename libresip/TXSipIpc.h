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

@protocol Sip
@property (readonly) TXSipReg* sreg;

- (void)doCallControl:(NSString*)ckey op:(int)op;
- (oneway void) setOnline: (int)state;
- (oneway void) startCallUser: (struct sip_addr*)udest;
- (void)doApns: (const char*)data length:(size_t)length;

@end

@interface TXSipIpc : NSObject {
    MailBox *box;
    id<Sip> delegate;
}

- (id) initWithBox:(MailBox*)pBox;

@property id delegate;

- (void)online:(int)state;
- (void)control:(NSString*)ckey op:(int)op;
- (void)call:(NSString*)user name:(NSString*)name;
- (void)apns:(NSData*)token;

@end
