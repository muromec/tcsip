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
}

- (void)rload: (NSString*)path cb:(id)pCb;
- (void)start;
- (void)post:(NSString*)key val:(NSString*)val;
- (void)post:(NSString*)val;
- (void)code:(int) code data:(NSData*)data;
- (void)ifs:(NSDate*)date;

- (void)fail;

+ (void)r: (NSString*)path cb:(id)cb;
+ (void)r: (NSString*)path cb:(id)cb user:(NSString*)u password:(NSString*)p;
+ (void) cert:(NSString*)cert;

+ (void)wrapper: (id)wrapper;
+ (void)retbox: (MailBox*)box;
+ (id)api;

- (void) setAuth:(NSString*)pU password:(NSString*)pW;
- (void) getAuth:(char**)_user password:(char**)_passw;

@end
