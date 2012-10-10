//
//  TXSipAuth.h
//  Texr
//
//  Created by Ilya Petrov on 9/19/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "TXSip.h"
#import "TXSipUser.h"

@interface TXSipAuth : NSObject {
    id app;

    const char* password;
    const char* address;
    const char* user;
}
- (id) initWithApp: (id) pApp;

- (void) setCreds:(NSString*)pUser password:(NSString*)pPassword;
- (void) sendAuth;

@end
