//
//  TXSipIpc.h
//  libresip
//
//  Created by Ilya Petrov on 3/23/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>
@class MailBox;
@class TXSipUser;
@class TXSipReg;

@protocol Sip
@property (readonly) TXSipReg* sreg;

- (void)doCallControl:(NSString*)ckey op:(int)op;
- (oneway void) setOnline: (int)state;
- (oneway void) startCallUser: (TXSipUser*)udest;

@end

@interface TXSipIpc : NSObject {
    MailBox *box;
    id<Sip> delegate;
}

- (id) initWithBox:(MailBox*)pBox;

@property id delegate;

- (void)online:(int)state;
- (void)control:(NSString*)ckey op:(int)op;
- (void)call:(TXSipUser*)user;
- (void)apns:(NSData*)token;

@end
