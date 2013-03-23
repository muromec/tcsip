//
//  TXUplinks.h
//  libresip
//
//  Created by Ilya Petrov on 3/9/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol UpDelegate
- (void) uplinkUpd: (NSString*)uri state:(NSString*)state;
- (void) uplinkAdd: (NSString*)uri state:(NSString*)state;
- (void) uplinkRm: (NSString*)uri ;
@end

@interface TXUplinks : NSObject {
    NSMutableArray *data;
    id<UpDelegate> delegate;
}

@property id delegate;

@end
