//
//  TXKey.h
//  libresip
//
//  Created by Ilya Petrov on 12/28/12.
//  Copyright (c) 2012 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface TXKey : NSObject {
    NSData *pub_key;
    NSData *priv_key;
}

@property (readonly) NSData* pub_key;
@property (readonly) NSData* priv_key;

@end
