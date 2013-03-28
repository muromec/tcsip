//
//  TXHttp.h
//  libresip
//
//  Created by Ilya Petrov on 3/22/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import <Foundation/Foundation.h>
struct httpc;
@class TXRestApi;
@class Callback;

@interface TXHttp : NSObject {
    struct httpc* httpc;

    NSMutableData *responseData;
    int status_code;
    Callback* cb;
    TXRestApi* api;
    BOOL ready;
    id url_con;

    NSDictionary *ret;
    NSString *username;
    NSString *password;

    struct request *request;
}
- (id) initWithApp:(struct httpc*)app api:(id)pApi;

- (void)rload: (NSString*)path cb:(id)pCb;

- (void)setAuth:(NSString*)pU password:(NSString*)pW;
- (BOOL)sendAuth;

- (void)post:(NSString*)key val:(NSString*)val;
- (void)post:(NSString*)val;
- (void)code:(int) code data:(NSData*)data;
- (void)ifs:(NSDate*)date;
- (void)ct:(NSString*)content;

- (void)start;
- (void)fail;
- (BOOL)kick;

@end
