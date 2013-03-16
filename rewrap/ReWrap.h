//
//  ReWrap.h
//  libresip
//
//  Created by Ilya Petrov on 1/11/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>

@class MProxy;
struct reapp;

@protocol Wrapper <NSObject>
- (id) wrap: (id)ob;
@end

@interface ReWrap : NSObject <Wrapper>{
    struct reapp *app;
    MProxy *proxy;
}

- (id)wrap:(id)ob;
- (oneway void)stop;
+ (void*)app;

@property (readonly) id proxy;
@property (readonly) void* app;

@end
