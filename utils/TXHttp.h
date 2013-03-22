//
//  TXHttp.h
//  libresip
//
//  Created by Ilya Petrov on 3/22/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>
struct httpc;
@class Callback;

@interface TXHttp : NSObject {
    struct httpc* app;

    NSMutableData *responseData;
    int status_code;
    Callback* cb;
    id url_con;

    NSString *username;
    NSString *password;

    struct request *request;
}
- (void)rload: (NSString*)path cb:(id)pCb;

- (void)setAuth:(NSString*)pU password:(NSString*)pW;
- (BOOL)sendAuth;

- (void)post:(NSString*)key val:(NSString*)val;
- (void)post:(NSString*)val;
- (void)code:(int) code data:(NSData*)data;
- (void)ifs:(NSDate*)date;

- (void)start;
- (void)fail;

@end
