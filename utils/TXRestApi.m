//
//  TXRestApi.m
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXRestApi.h"
#import "JSONKit.h"
#import "Callback.h"

@implementation TXRestApi
+ (void)r: (NSString*)path cb:(id)cb ident:(SecIdentityRef)ident
{
    TXRestApi *api = [[TXRestApi alloc] init];
    [api rload: path cb: cb ident:ident post:NO];
    [api start];
}

+ (void)r: (NSString*)path cb:(id)cb
{
    TXRestApi *api = [[TXRestApi alloc] init];
    [api rload: path cb: cb ident:nil post:NO];
    [api start];
}

+ (void)r: (NSString*)path cb:(id)cb user:(NSString*)u password:(NSString*)p
{
    TXRestApi *api = [[TXRestApi alloc] init];
    [api rload: path cb: cb ident:nil post:NO];
    [api setAuth: u password:p];
    [api start];
}

- (void)rload: (NSString*)path cb:(id)pCb ident:(SecIdentityRef)ident post:(bool)post
{
    NSURL *myURL = [NSURL URLWithString:[NSString stringWithFormat:@"https://texr.enodev.org/api/%@",path]];
    // XXX: use rehttp
}

- (void)post:(NSString*)key val:(NSString*)val
{
}

- (void)start
{
}

- (void)requestFinished:(id)request
{
    // Use responseData

    JSONDecoder* decoder = [JSONDecoder decoder];
    NSDictionary *ret = [decoder objectWithData: nil];
    NSArray *payload = [ret objectForKey:@"data"];

    [cb response: payload];
}
- (void)requestFailed:(id)rp
{
   NSError *error = [rp error];
   NSLog(@"req failed %@", error);
   [cb response: nil];
}

- (void) setAuth:(NSString*)pU password:(NSString*)pW
{
}


@end
