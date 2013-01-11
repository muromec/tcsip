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
#import "ReWrap.h"

#include "http.h"

static struct httpc app;
static MailBox* root_box;
static ReWrap* wrapper;

static void http_done(struct request *req, int code, void *arg) {
    TXRestApi *r = (__bridge_transfer TXRestApi*)arg;
    struct pl *data = http_data(req);
    NSData *nd = [NSData dataWithBytes:data->p length:data->l];
    [r code: code data:nd];
}

static void http_err(int err, void *arg) {
    TXRestApi *r = (__bridge_transfer TXRestApi*)arg;
    [r fail];
}


@implementation TXRestApi
+ (void)r: (NSString*)path cb:(id)cb
{
    TXRestApi *api = [[TXRestApi alloc] init];
    api = [wrapper wrap:api];
    [api rload: path cb: cb];
    [api start];
}

+ (void)r: (NSString*)path cb:(id)cb user:(NSString*)u password:(NSString*)p
{
    TXRestApi *api = [[TXRestApi alloc] init];
    api = [wrapper wrap:api];
    [api rload: path cb: cb];
    [api setAuth: u password:p];
    [api start];
}

- (void)rload: (NSString*)path cb:(id)pCb
{
    NSString *url = [NSString stringWithFormat:@"https://texr.enodev.org/api/%@",path];
    http_init(&app, &request, (char*)_byte(url));
    http_cb(request, (__bridge_retained void*)self, http_done, http_err);
    cb = [MProxy withTargetBox: pCb box:root_box];
}

- (void)post:(NSString*)key val:(NSString*)val
{
}

- (oneway void)start
{
    http_send(request);
}

- (void)code:(int) code data:(NSData*)data
{
    // Use responseData
    if(code!=200) {
        [self fail];
        return;
    }

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
    if(app.tls)
        mem_deref(app.tls);
    if(app.dnsc)
        mem_deref(app.dnsc);

    memcpy(&app, _app, sizeof(struct httpc));
}

+ (void)wrapper: (id)_wrapper
{
    wrapper = _wrapper;
    void *_app = wrapper.app;
    memcpy(&app, _app, sizeof(struct httpc));
}

+ (void)retbox: (MailBox*)_box
{
    root_box = _box;
}

- (void) dealloc
{
    if(request)
        mem_deref(request);
}

@end
