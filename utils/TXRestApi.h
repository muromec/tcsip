//
//  TXRestApi.h
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface TXRestApi : NSObject {
    struct httpc* app;
}

- (id)request: (NSString*)path cb:(id)pCb;
- (id)load: (NSString*)path cb:(id)pCb;

@end
