//
//  TXUplinks.m
//  libresip
//
//  Created by Ilya Petrov on 3/9/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import "TXUplinks.h"
#import "Callback.h"

@implementation TXUplinks
@synthesize delegate;
- (void)report:(NSArray*)report
{
    data = [self upd: report];
}

- (id)upd:(id)report
{
    NSMutableArray *uris = [[NSMutableArray alloc] init];
    NSString *uri, *state;
    for(NSArray *up in report) {
        uri = [up objectAtIndex: 0];
        state = [up objectAtIndex: 1];

        [uris addObject: uri];

        if([data containsObject:uri]) {
	    [delegate uplinkUpd: uri state: state];
            continue;
        }
	[delegate uplinkAdd: uri state:state];
    }
    for(id uri in data) {
        if(![uris containsObject:uri])
	      [delegate uplinkRm:uri];
    }

    return uris;
}
@end
