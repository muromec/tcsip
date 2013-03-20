//
//  MailBox.m
//  ActorKit
//
//  Created by Ilya Petrov on 10/7/12.
//
//

#import "MailBox.h"
#include <msgpack.h>

typedef struct {
    int magic;
    void *arg;
} box_msg;

static void __wtf() {
}

int write_fd(void* data, const char* buf, unsigned int len)
{
    int fd = *(int*)data;
    return (int)write(fd, buf, len);
}
@implementation MailBox
@synthesize readFd;
@synthesize delegate;
- (id) init
{
    self = [super init];
    if(!self) return self;

    int pipe_fd[2];
    pipe(pipe_fd);
    kickFd = pipe_fd[1];
    readFd = pipe_fd[0];
    call = YES;

    mempool = msgpack_zone_new(2048);
    packer = msgpack_packer_new(&kickFd, write_fd);
    up = msgpack_unpacker_new(128);
    return self;
}

- (id) inv_pop
{
    id ret;
    box_msg msg;

    read(readFd, &msg, sizeof(msg));
    ret = (__bridge_transfer id)msg.arg;
    return ret;
}

- (struct msgpack_object) qpop:(int)rd
{
    size_t rsize;
    msgpack_object ob;


    if(msgpack_unpacker_execute(up))
        goto one;

    if(!rd)
        goto none;

    msgpack_unpacker_reserve_buffer(up, 256);
    rsize = read(readFd, msgpack_unpacker_buffer(up), msgpack_unpacker_buffer_capacity(up));
    if(rsize)
        msgpack_unpacker_buffer_consumed(up, rsize);

    if(!msgpack_unpacker_execute(up))
        goto none;

one:
    ob = msgpack_unpacker_data(up);
    msgpack_unpacker_reset(up);
    
    return ob;

none:
    ob.type = 0x22;
    return ob;
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

- (void) cmd: (char*)data len:(int)len
{
    if(!data || !len || !kickFd)
        return;

    box_msg msg;
    msg.arg = len;
    msg.magic = 0xDAFACCE1;

    write(kickFd, &msg, sizeof(msg));
    write(kickFd, data, len);
}

- (void)linker_sucks
{
    msgpack_object ob;
    msgpack_object_print(stdout, ob);
}


- (struct msgpack_packer*) packer
{
    call = NO;
    return packer;
}

- (void) dealloc
{
    close(readFd);
    msgpack_zone_destroy(mempool);
    msgpack_zone_free(mempool);
}

@end
