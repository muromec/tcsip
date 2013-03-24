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

void uplink_upd(struct list*, void*);

@protocol UpDelegate
- (void) uplinkUpd: (NSString*)uri state:(int)state;
- (void) uplinkAdd: (NSString*)uri state:(int)state;
- (void) uplinkRm: (NSString*)uri ;
@end

@interface TXUplinks : NSObject {
    NSMutableArray *data;
    id<UpDelegate> delegate;
    NSMutableArray *uris;
}

@property id delegate;

- (void)report:(struct list*)upl;
- (void)one_add:(struct uplink*)up;

@end
