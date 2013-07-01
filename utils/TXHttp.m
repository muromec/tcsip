//
//  TXHttp.m
//  libresip
//
//  Created by Ilya Petrov on 3/22/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import "TXHttp.h"
#import "TXRestApi.h"
#import "Callback.h"
#import "JSONKit.h"
#include "http.h"
#include "strmacro.h"

#define URL(__x) ((NSString *)CFBridgingRelease(CFURLCreateStringByAddingPercentEscapes(NULL,\
   (__bridge CFStringRef)__x,\
    NULL,\
    CFSTR("!*'();:@&=+$,/?%#[]"),\
    kCFStringEncodingUTF8)))

char* byte(NSString * input){
      int len = (int)[input lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

      char *toReturn = (char *)malloc((sizeof(char) * len) +1);

      strcpy(toReturn,[input cStringUsingEncoding:NSUTF8StringEncoding]);

      return toReturn;
}

static void http_ret(struct request *req, int code, void *arg) {

    TXHttp *r = (__bridge_transfer TXHttp*)arg;
    struct mbuf *data = http_data(req);
    NSData *nd = [NSData dataWithBytes:mbuf_buf(data)
                                length:mbuf_get_left(data)];
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
- (id) initWithApp:(struct httpc*)app api:(id)pApi {
    self = [super init];
    if(!self) return self;

    httpc = mem_ref(app);
    api = pApi;

    return self;
}


- (void)rload: (NSString*)path cb:(id)pCb
{
    NSString *url = [NSString stringWithFormat:@"https://www.texr.net/api/%@",path];
    http_init(httpc, &request, (char*)_byte(url));
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

- (void)ct:(NSString*)content
{
    http_header(request, "Accept", (char*)_byte(content));
}

- (void)start
{
    http_send(request);
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
    ret = [decoder objectWithData: data];

    ready = YES;
    [api ready: self];
}
- (void)fail
{
    ready = YES;
    [api ready: self];
}

- (BOOL)kick
{
    if(!ready) return NO;

    [cb response: ret];
    cb = nil;
    return YES;
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
        http_send(new_req);
	return YES;
    }
    return NO;
}

- (void)dealloc
{
    mem_deref(request);
}
@end
