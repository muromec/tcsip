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
#import "ASIHTTPRequest.h"
#import "ASIFormDataRequest.h"

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
    
    if(post)
        request = [ASIFormDataRequest requestWithURL:myURL];
    else
        request = [ASIHTTPRequest requestWithURL:myURL];

    self->cb = pCb;

    // XXX: check root CA after connect
    [request setValidatesSecureCertificate:NO];

    if(ident)
        [request setClientCertificateIdentity:ident];

    [request setDelegate:self];
}

- (void)post:(NSString*)key val:(NSString*)val
{
    [request addPostValue:val forKey:key];
}

- (void)start
{
    [request startAsynchronous];
}

- (void)requestFinished:(ASIHTTPRequest *)request
{
    // Use responseData

    JSONDecoder* decoder = [JSONDecoder decoder];
    NSDictionary *ret = [decoder objectWithData: [request responseData]];
    //NSString *status = [ret objectForKey:@"status"];
    NSArray *payload = [ret objectForKey:@"data"];

    [cb response: payload];
}
- (void)requestFailed:(ASIHTTPRequest *)request
{
   NSError *error = [request error];
   NSLog(@"req failed %@", error);
   [cb response: nil];
}

- (void) setAuth:(NSString*)pU password:(NSString*)pW
{
    [request setUsername:pU];
    [request setPassword:pW];
}


@end
