//
//  ReWrap.h
//  libresip
//
//  Created by Ilya Petrov on 1/11/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>

struct reapp;
@class MailBox;

@interface ReWrap : NSObject {
    struct reapp *app;
    MailBox *mbox;
}

- (oneway void)stop;
+ (void*)app;

@property (readonly) MailBox* mbox;
@property (readonly) void* app;

@end
