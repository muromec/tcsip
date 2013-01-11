//
//  TXRestApi.h
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>

struct httpc;
@class MProxy;
@class MailBox;

@interface TXRestApi : NSObject <NSURLConnectionDelegate> {
    NSMutableData *responseData;
    int status_code;
    id cb;
    id url_con;

    NSString *username;
    NSString *password;

    struct request *request;
    MProxy *proxy;
    bool running;
}

- (void)rload: (NSString*)path cb:(id)pCb;
- (void)start;
- (void)stop;
- (void)post:(NSString*)key val:(NSString*)val;
- (void)code:(int) code data:(NSData*)data;

- (void)fail;

+ (void)r: (NSString*)path cb:(id)cb;
+ (void)r: (NSString*)path cb:(id)cb user:(NSString*)u password:(NSString*)p;
+ (void)https: (struct httpc*)_app;
+ (void)retbox: (MailBox*)box;

- (void) setAuth:(NSString*)pU password:(NSString*)pW;

@property (readonly) id proxy;
@property (readonly) bool running;

@end
