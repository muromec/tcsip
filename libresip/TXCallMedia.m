//
//  TXCallMedia.m
//  Texr
//
//  Created by Ilya Petrov on 9/28/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXCallMedia.h"
#import "TXSipCall.h"
#include "ajitter.h"
#include "txsip_private.h"

@implementation TXCallMedia
- (id) initWithUAC: (struct uac*)pUac dir:(call_dir_t)pDir
{

    self = [super init];
    if(!self) return self;

    uac = pUac;
    laddr = &uac->laddr;

    [self setup: pDir];

    return self;
}

- (void) setup:(call_dir_t)pDir { }

- (void)gencname { }

- (void)set_ssrc { }

- (void) gather: (uint16_t)scode err:(int)err reason:(const char*)reason { }
- (void) conn_check: (bool)update err:(int)err { }

- (void)update_media { }

- (void)stun:(const struct sa*)srv { }

- (void)setIceCb:(Callback*)pCB
{
    iceCB = pCB;
}

- (void) setupSRTP { }

- (void) stop { }

- (void)dealloc { }
- (void) open { }
- (bool) start {return YES;};
- (void)change_dst { }
- (int) offer: (struct mbuf*)offer ret:(struct mbuf **)ret { return -1;}
- (int) offer: (struct mbuf **)ret { return -1; }
- (int) answer: (struct mbuf*)offer { return -1; }
- (void) sdpFormats { }
- (void) set_format:(char*)fmt_name { }

@end
