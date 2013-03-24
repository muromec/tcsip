//
//  TXUplinks.m
//  libresip
//
//  Created by Ilya Petrov on 3/9/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import "TXUplinks.h"
#import "Callback.h"
#import "re.h"
#include "txsip_private.h"
#include "strmacro.h"

void uplink_upd(struct list* upl, void* arg)
{
    TXUplinks *ctx = (__bridge TXUplinks*)arg;
    [ctx report: upl];
}

bool apply_add(struct le *le, void *arg)
{
     TXUplinks *ctx = (__bridge TXUplinks*)arg;
     [ctx one_add:le->data];

     return false;
}


@implementation TXUplinks
@synthesize delegate;
- (void)report:(struct list*)upl
{
    data = [self upd: upl];
    uris = nil;
}

- (id)upd:(struct list*)upl
{
    uris = [[NSMutableArray alloc] init];
    list_apply(upl, true, apply_add, (__bridge void*)self);
    for(id uri in data) {
        if(![uris containsObject:uri])
	      [delegate uplinkRm:uri];
    }

    return uris;
}

- (void)one_add:(struct uplink*)up
{
    NSString *uri;
    uri = _str(up->uri.p, up->uri.l);

    [uris addObject: uri];

    if([data containsObject:uri]) {
        [delegate uplinkUpd: uri state:up->ok];
    } else {
        [delegate uplinkAdd: uri state:up->ok];
    }

}

@end
