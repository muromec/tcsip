//
//  TXUplinks.h
//  libresip
//
//  Created by Ilya Petrov on 3/9/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>

@class Callback;

@interface TXUplinks : NSObject {
    NSMutableArray *data;
    Callback *cb;
}

@property id cb;

@end
