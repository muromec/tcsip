//
//  TXBaseOp.m
//  Texr
//
//  Created by Ilya Petrov on 9/21/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXSip.h"
#import "TXBaseOp.h"

@implementation TXBaseOp
@synthesize authSent;
@synthesize dest;
@synthesize cb;

- (id) initWithApp: (TXSip*) pApp
{
    self = [super init];
    if(!self) {
	return self;
    }
    app = pApp;
    uac = [app getUa];

    ctx = (__bridge void*)self;
    auth_ctx = (__bridge void*)app.auth;

    [self setup];
    authSent = NO;

    return self;
}

- (void) setup
{
    // do nothing
}

- (void) createHandle
{

}

@end
