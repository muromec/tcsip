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


@protocol BoxDelegate
- (void)obCmd:(struct msgpack_object)ob;
@end

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
- (void) cmd: (char*)data len:(int)len;
- (struct msgpack_packer*) packer;
- (void)kick;

@property int readFd;
@property id delegate;

@end
