//
//  MailBox.m
//  ActorKit
//
//  Created by Ilya Petrov on 10/7/12.
//
//

#import "MailBox.h"
#import <CHCircularBufferQueue.h>

@implementation MailBox
- (id) init
{
    self = [super init];
    if(!self) return self;

    queue = [[CHCircularBufferQueue alloc] init];

    return self;
}

- (id) qpop
{

    id ret; 
    @synchronized(queue)
    {
	ret = [queue firstObject];
	[queue removeFirstObject];
    }

    return ret;
}

- (void) qput: (id) data;
{
    if(!data) return;

    @synchronized(queue)
    {
	[queue addObject: data];
    }
}

@end
