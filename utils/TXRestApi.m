//
//  TXRestApi.m
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXRestApi.h"
#import "TXHttp.h"
#import "Callback.h"
#import "MailBox.h"
#import "ReWrap.h"
#import "re.h"
#import "http.h"
#import "TXSip.h"
#include <msgpack.h>

@implementation TXRestApi
@synthesize ret_box;
- (id) initWithApp:(struct httpc*)app {
    self = [super init];
    if(!self) return self;

    httpc = mem_ref(app);
    rq = [[NSMutableArray alloc] init];

    return self;
}
- (id) request:(NSString*) url cb:(Callback*)cb {
    id req = [[TXHttp alloc] initWithApp:httpc api:self];
    [req rload: url cb:cb];
    [rq addObject: req];

    return req;
}

- (id) load:(NSString*) url cb:(Callback*)cb {
    id req = [self request:url cb:cb];
    [req start];
    return req;
}

- (void)ready: (id)req {
    struct msgpack_packer *pk = ret_box.packer;
    msgpack_pack_array(pk, 2);
    push_cstr("https.ready");
    push_cstr("-");
}

- (void)kick {

    BOOL ret;

    NSUInteger idx;
restart:
    idx = 0;
    for(TXHttp* http in rq) {
        ret = [http kick];
        if(ret) {
            [rq removeObjectAtIndex:idx];
            goto restart;
        }
        idx ++;
    }
}

- (void) cert:(NSString*)cert
{
    int err;
    if(httpc->tls) httpc->tls = mem_deref(httpc->tls);

    err = tls_alloc(&httpc->tls, TLS_METHOD_SSLV23, cert ? _byte(cert) : NULL, NULL);

    NSBundle *b = [NSBundle mainBundle];
    NSString *ca_cert;
    ca_cert = [b pathForResource:@"STARTSSL" ofType: @"cert"];
    err = tls_add_ca(httpc->tls, _byte(ca_cert));

}

- (void)dealloc {
    mem_deref(httpc);
}

@end
