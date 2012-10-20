//
//  MProxy.m
//  Texr
//
//  Created by Ilya Petrov on 10/7/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "MProxy.h"

@implementation MProxy
@synthesize mbox;
+ (id) withTarget: (id) target
{
    return [[self alloc] initWithTarget: target];
}

- (id) initWithTarget: (id) pTarget
{
    target = pTarget;
    mbox = [[MailBox alloc] init];
    return self;
}

+ (id) withTargetBox: (id) pTarget box:(id)pBox
{
    return [[self alloc] initWithTargetBox: pTarget box:pBox];
}

- (id) initWithTargetBox: (id) pTarget box:(id)pBox
{
    target = pTarget;
    mbox = pBox;
    return self;
}

- (NSMethodSignature *) methodSignatureForSelector: (SEL) aSelector {
    return [target methodSignatureForSelector: aSelector];
}

// abstract NSProxy method
- (void) forwardInvocation: (NSInvocation *) anInvocation {
    id sign = [anInvocation methodSignature];

    [anInvocation setTarget: target];
    if([sign isOneway]) {
        [anInvocation retainArguments];
        [mbox qput: anInvocation];
    } else {
	[anInvocation invoke];
    }
}
@end
