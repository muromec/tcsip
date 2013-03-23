//
//  TXBaseOp.m
//  Texr
//
//  Created by Ilya Petrov on 9/21/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXSip.h"
#import "TXBaseOp.h"
#include "re.h"

@implementation TXBaseOp
@synthesize authSent;
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

- (void)setRemote:(struct sip_addr*)val {
    remote = mem_ref(val);
}

- (struct sip_addr*)remote {
    return remote;
}

- (void)setLocal:(struct sip_addr*)val {
    local = mem_ref(val);
}

- (struct sip_addr*)local {
    return local;
}

- (void)dealloc {
    if(remote) mem_deref(remote);
    if(local) mem_deref(local);
}

@end
