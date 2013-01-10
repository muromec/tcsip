//
//  TXRestApi.h
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface TXRestApi : NSObject <NSURLConnectionDelegate> {
    NSMutableData *responseData;
    int status_code;
    id cb;
    id url_con;

    NSString *username;
    NSString *password;

    id request;
}

- (void)rload: (NSString*)path cb:(id)pCb ident:(SecIdentityRef)ident post:(bool)post;
- (void)start;
- (void)post:(NSString*)key val:(NSString*)val;

+ (void)r: (NSString*)path cb:(id)cb;
+ (void)r: (NSString*)path cb:(id)cb ident:(SecIdentityRef)ident;
+ (void)r: (NSString*)path cb:(id)cb user:(NSString*)u password:(NSString*)p;


- (void) setAuth:(NSString*)pU password:(NSString*)pW;

@end
