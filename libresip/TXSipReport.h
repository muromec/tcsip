//
//  TXSipReport.h
//  libresip
//
//  Created by Ilya Petrov on 3/23/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>

@class MailBox;
@class TXSipCall;

@interface TXSipReport : NSObject {
    MailBox *box;
}

- (id) initWithBox:(MailBox*)pBox;
- (void) reportCall:(TXSipCall*)call;
- (void) reportCallDrop:(TXSipCall*)call;
- (void) reportCallEst:(TXSipCall*)call;

@property MailBox *box;

@end
