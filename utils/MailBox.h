//
//  MailBox.h
//  ActorKit
//
//  Created by Ilya Petrov on 10/7/12.
//
//

#import <Foundation/Foundation.h>
struct msgpack_zone;
struct msgpack_packer;
struct msgpack_unpacker;
struct msgpack_object;

@interface MailBox : NSObject {
    int kickFd;
    int readFd;
    id delegate;
    struct msgpack_zone *mempool;
    struct msgpack_packer *packer;
    struct msgpack_unpacker *up;
    BOOL call;
}
- (struct msgpack_object) qpop:(int)read;
- (id) inv_pop;

- (void) qput: (id) data;
- (void) cmd: (char*)data len:(int)len;
- (struct msgpack_packer*) packer;

@property int readFd;
@property id delegate;

@end
