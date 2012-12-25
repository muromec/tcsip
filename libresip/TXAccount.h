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
#define kSipUUID @"sipuuid"

@interface TXAccount : NSObject {
    NSString* user;
    NSString* name;
    Callback *auth_cb;
}
- (id) initWithUser: (NSString*) pUser;
- (NSString*) cert;

+ (bool) canLogin;
+ (NSString*) deflt;
- (void) auth:(NSString*)pUser password:(NSString*)pPassw cb:(Callback*)CB;

@property (readonly) NSString* user;
@property (readonly) NSString* uuid;
@property NSString* name;

@end
