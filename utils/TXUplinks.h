//
//  TXUplinks.h
//  libresip
//
//  Created by Ilya Petrov on 3/9/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>

struct list;
struct uplink;
struct hash;
struct pl;

void uplink_upd(struct list*, void*);

@protocol UpDelegate
- (void) uplinkUpd: (struct pl*)uri state:(int)state;
- (void) uplinkAdd: (struct pl*)uri state:(int)state;
- (void) uplinkRm: (struct pl*)uri ;
@end

@interface TXUplinks : NSObject {
    NSMutableArray *data;
    id<UpDelegate> delegate;
    NSMutableArray *uris;

}

@property id delegate;

- (void)report:(struct list*)upl;
- (void)one_add:(struct uplink*)up;
- (void)one_rm:(struct uplink*)up;

@end
