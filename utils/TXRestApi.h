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

    NSURLCredential *credential;

}

- (void)rload: (NSString*)path cb:(id)cb;
+ (void)r: (NSString*)path cb:(id)cb;

- (void) setAuth:(NSString*)pU password:(NSString*)pW;

@end
