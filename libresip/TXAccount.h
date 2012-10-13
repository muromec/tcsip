//
//  TXAccount.h
//  Texr
//
//  Created by Ilya Petrov on 10/1/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <libresip/Callback.h>

#define kSipUser @"sipu"

@interface TXAccount : NSObject {
    NSString* user;
    Callback *auth_cb;
}
- (id) initWithUser: (NSString*) pUser;
- (NSString*) cert;

+ (bool) canLogin;
+ (NSString*) deflt;
- (void) auth:(NSString*)pUser password:(NSString*)pPassw cb:(Callback*)CB;

@property (readonly) NSString* user;

@end
