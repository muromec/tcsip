//
//  TXBaseOp.h
//  Texr
//
//  Created by Ilya Petrov on 9/21/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "TXSip.h"
#import "TXSipUser.h"
#import "Callback.h"

typedef int (auth_h_t)(char**, char**, const char*, void*arg);

int auth_handler(char **user, char **pass, const char *realm, void *arg);

@interface TXBaseOp : NSObject {
    TXSip* app;
    uac_t* uac;
    bool authSent;
    TXSipUser *dest;
    Callback *cb;

    void *ctx;
    void *auth_ctx;
}
@property bool authSent;
@property TXSipUser* dest;
@property Callback* cb;

- (id) initWithApp: (TXSip*) pApp;
- (void) createHandle;
- (void) setup;

@end
