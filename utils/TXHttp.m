//
//  TXHttp.m
//  libresip
//
//  Created by Ilya Petrov on 3/22/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import "TXSip.h"
#import "TXHttp.h"
#import "JSONKit.h"
#include "http.h"

#define URL(__x) ((NSString *)CFBridgingRelease(CFURLCreateStringByAddingPercentEscapes(NULL,\
   (__bridge CFStringRef)__x,\
    NULL,\
    CFSTR("!*'();:@&=+$,/?%#[]"),\
    kCFStringEncodingUTF8)))

static void http_ret(struct request *req, int code, void *arg) {

    TXHttp *r = (__bridge_transfer TXHttp*)arg;
    struct pl *data = http_data(req);
    NSData *nd = [NSData dataWithBytes:data->p length:data->l];
    [r code: code data:nd];
}

static void http_done(struct request *req, int code, void *arg) {
    int ok = -EINVAL;
    TXHttp *r = (__bridge TXHttp*)arg;

    if(code==401) {
	ok = [r sendAuth];

	if(!ok)
	    http_ret(req, code, arg); 
	return;
    }
    http_ret(req, code, arg);
}

static void http_err(int err, void *arg) {
    TXHttp *r = (__bridge_transfer TXHttp*)arg;
    [r fail];
}

@implementation TXHttp
- (void)rload: (NSString*)path cb:(id)pCb
{
    NSString *url = [NSString stringWithFormat:@"https://www.texr.net/api/%@",path];
    http_init(app, &request, (char*)_byte(url));
    http_cb(request, (__bridge_retained void*)self, http_done, http_err);
    cb = pCb;
    request = mem_ref(request);
}

- (void)post:(NSString*)key val:(NSString*)val
{
    http_post(request,
                  (char*)_byte(URL(key)),
                  (char*)_byte(URL(val)));
}

- (void)post:(NSString*)val
{
    http_post(request, NULL, (char*)_byte(val));
}
- (void)ifs:(NSDate*)date
{
    NSString *header = [http_df stringFromDate:date];
    http_header(request, "If-Modified-Since", (char*)_byte(header));
}

- (void)start
{
    //http_send(request);
}

- (void)code:(int) code data:(NSData*)data
{
    request = NULL;
    // Use responseData
    if(code!=200) {
        [self fail];
        return;
    }

    JSONDecoder* decoder = [JSONDecoder decoder];
    NSDictionary *ret = [decoder objectWithData: data];
    NSLog(@"ret: %@", ret);

    //[cb response: ret];
}
- (void)fail
{
   //[cb response: nil];
}

- (void) setAuth:(NSString*)pU password:(NSString*)pW
{
    username = pU;
    password = pW;
}

- (BOOL) sendAuth
{
    char *_user = byte(username);
    char *_passw = byte(password);
    struct request *new_req;
    int err = http_auth(request, &new_req, _user, _passw);
    if(!err) {
        request = mem_deref(request);
	request = mem_ref(new_req);
	return YES;
    }
    return NO;
}

- (void)dealloc
{
    mem_deref(request);
}


@end
