//
//  ReWrap.m
//  libresip
//
//  Created by Ilya Petrov on 1/11/13.
//  Copyright (c) 2013 enodev. All rights reserved.
//

#import "ReWrap.h"
#import "MailBox.h"
#include <re.h>
#include "re_wrap_priv.h"
#include "strmacro.h"

static struct reapp* root_app;

static void pq_cb(int flags, void *arg)
{
    if(!(flags & FD_READ))
        return;

    MailBox *mbox  = (__bridge MailBox *)arg;
    [mbox kick];
}

@implementation ReWrap
@synthesize mbox;
- (id)init
{
    self = [super init];
    if(self) {
	mbox = [[MailBox alloc] init];
	[self setup];
    }

    return self;
}

- (void)setup
{
    int err;
    app = malloc(sizeof(struct reapp));
    app->nsc = ARRAY_SIZE(app->nsv);

    NSBundle *b = [NSBundle mainBundle];
    NSString *ca_cert = [b pathForResource:@"STARTSSL" ofType: @"cert"];
    if(!ca_cert)
        ca_cert = @"cert/STARTSSL.cert";

    err = libre_init(); /// XXX: do this conditionally!!!
    err = tls_alloc(&app->tls, TLS_METHOD_SSLV23, NULL, NULL);
    tls_add_ca(app->tls, _byte(ca_cert));

    err = dns_srv_get(NULL, 0, app->nsv, &app->nsc);

    err = dnsc_alloc(&app->dnsc, NULL, app->nsv, app->nsc);

    root_app = app;

}

- (void) worker
{

    D(@"start re worker");
    fd_listen(mbox.readFd, FD_READ, pq_cb, (__bridge void*)mbox);

    re_main(NULL);

    app->tls = mem_deref(app->tls);
    app->dnsc = mem_deref(app->dnsc);
    libre_close();

    tmr_debug();
    mem_debug();

    NSLog(@"re loop end");
}

- (oneway void) stop
{
    D(@"re stop");
    re_cancel();
}

- (void*)app
{
    return app;
}

+ (void*)app
{   
    return root_app;
}

- (void)dealloc
{
    free(app);
    root_app = NULL;
}

@end
