//
//  TXRestApi.m
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXRestApi.h"
#import "TXHttp.h"
#import "Callback.h"
#import "ReWrap.h"

@implementation TXRestApi
- (id) request:(NSString*) url cb:(Callback*)cb {
    id req = [[TXHttp alloc] init];
    [req rload: url cb:cb];

    return req;
}

- (id) load:(NSString*) url cb:(Callback*)cb {
    id req = [self request:url cb:cb];
    [req start];
    return req;
}
@end
