//
//  MailBox.m
//  ActorKit
//
//  Created by Ilya Petrov on 10/7/12.
//
//

#import "MailBox.h"

typedef struct {
    int magic;
    void *arg;
} box_msg;

@implementation MailBox
@synthesize readFd;
- (id) init
{
    self = [super init];
    if(!self) return self;

    int pipe_fd[2];
    pipe(pipe_fd);
    kickFd = pipe_fd[1];
    readFd = pipe_fd[0];

    return self;
}

- (id) qpop
{

    id ret;
    box_msg msg;

    read(readFd, &msg, sizeof(msg));

    ret = (__bridge_transfer id)msg.arg;
    return ret;
}

- (void) qput: (id) data;
{
    if(!data) return;
    box_msg msg;
    msg.arg = (__bridge_retained void*)data;
    msg.magic = 0xDEADF123;

    if(!kickFd) {
        NSLog(@"wtf??");
        return;
    }

    write(kickFd, &msg, sizeof(msg));
}

- (void) dealloc
{
    close(readFd);
}

@end
