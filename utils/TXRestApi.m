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


static struct httpc app;
static MailBox* root_box;

static void pq_cb(int flags, void *arg)
{
    printf("pq cb\n");
    if(!(flags & FD_READ))
        return;

    MailBox *mbox  = (__bridge MailBox *)arg;
    id inv = [mbox qpop];
    [inv invoke];
}

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
@synthesize proxy;
@synthesize running;

+ (void)r: (NSString*)path cb:(id)cb
{
    TXRestApi *api = [[TXRestApi alloc] init];
    [api rload: path cb: cb];
    [api start];
}

+ (void)r: (NSString*)path cb:(id)cb user:(NSString*)u password:(NSString*)p
{
    TXRestApi *api = [[TXRestApi alloc] init];
    [api rload: path cb: cb];
    [api setAuth: u password:p];
    [api start];
}

- (void)rload: (NSString*)path cb:(id)pCb
{
    NSString *url = [NSString stringWithFormat:@"https://texr.enodev.org/api/%@",path];
    http_init(&app, &request, _byte(url));
    http_cb(request, (__bridge_retained void*)self, http_done, http_err);
    cb = [MProxy withTargetBox: pCb box:root_box];
}

- (void)post:(NSString*)key val:(NSString*)val
{
}

- (void)start
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
    NSLog(@"http dealloc %@", proxy);
}

- (void) worker
{
    int err;
    struct sa nsv[16];
    uint32_t nsc = ARRAY_SIZE(nsv);

    NSBundle *b = [NSBundle mainBundle];
    NSString *ca_cert = [b pathForResource:@"CA" ofType: @"cert"];

    err = libre_init(); /// XXX: do this conditionally!!!
    err = tls_alloc(&app.tls, TLS_METHOD_SSLV23, NULL, NULL);
    tls_add_ca(app.tls, _byte(ca_cert));

    err = dns_srv_get(NULL, 0, nsv, &nsc);

    err = dnsc_alloc(&app.dnsc, NULL, nsv, nsc);

    NSLog(@"start http worker");
    proxy = [MProxy withTarget: self];
    fd_listen(proxy.mbox.readFd, FD_READ, pq_cb, (__bridge void*)proxy.mbox);

    running = YES;
    NSLog(@"started %d", self.running);

    re_main(NULL);

    app.tls = mem_deref(app.tls);
    app.dnsc = mem_deref(app.dnsc);
    libre_close();

    tmr_debug();
    mem_debug();

    NSLog(@"http loop end");

    running = NO;
}

- (oneway void) stop
{
    NSLog(@"http stop");
    re_cancel();
}

@end
