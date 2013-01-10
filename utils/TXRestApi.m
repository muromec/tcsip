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
#import "TXSip.h"

#include "http.h"

static struct httpc* app;

static void http_done(struct request *req, int code, void *arg) {
    TXRestApi *r = (__bridge_transfer TXRestApi*)arg;
    struct pl *data = http_data(req);
    NSData *nd = [NSData dataWithBytes:data->p length:data->l];
    [r data:nd];
}

static void http_err(int err, void *arg) {
    TXRestApi *r = (__bridge_transfer TXRestApi*)arg;
    [r fail];
}


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
    NSString *url = [NSString stringWithFormat:@"https://texr.enodev.org/api/%@",path];
    http_init(app, &request, _byte(url));
    http_cb(request, (__bridge_retained void*)self, http_done, http_err);
    http_send(request);
}

- (void)post:(NSString*)key val:(NSString*)val
{
}

- (void)start
{
}

- (void)data:(NSData*)data
{
    // Use responseData

    JSONDecoder* decoder = [JSONDecoder decoder];
    NSDictionary *ret = [decoder objectWithData: data];
    NSArray *payload = [ret objectForKey:@"data"];
    NSLog(@"payload %@", payload);

    [cb response: payload];
}
- (void)fail
{
   [cb response: nil];
}

- (void) setAuth:(NSString*)pU password:(NSString*)pW
{
}

+ (void) https:(struct httpc*)_app
{
    app = _app;
}

- (void) dealloc
{
    mem_deref(request);
}


@end
