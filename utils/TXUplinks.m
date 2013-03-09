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
@synthesize cb;
- (void)report:(NSArray*)report
{
    data = [self upd: report];
}

- (id)upd:(id)report
{
    NSMutableArray *rep = [[NSMutableArray alloc] init];
    NSMutableArray *uris = [[NSMutableArray alloc] init];
    NSString *uri, *state;
    for(NSArray *up in report) {
        uri = [up objectAtIndex: 0];
        state = [up objectAtIndex: 1];

        [uris addObject: uri];

        if([data containsObject:uri]) {
            [rep addObject: [NSArray arrayWithObjects: uri, @"upd", state, nil]];
            continue;
        }
        [rep addObject: [NSArray arrayWithObjects: uri, @"add", state, nil]];
    }
    for(id uri in data) {
        if(![uris containsObject:uri])
            [rep addObject: [NSArray arrayWithObjects: uri, @"rm", @"nan", nil]];
    }

    [cb response: rep];
    return uris;
}
@end
