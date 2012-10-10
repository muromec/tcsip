//
//  TXSipAuth.m
//  Texr
//
//  Created by Ilya Petrov on 9/19/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXSipAuth.h"

/* called when challenged for credentials */
int auth_handler(char **user, char **pass, const char *realm, void *arg)
{
	int err = 0;
	(void)realm;
	(void)arg;

	err |= str_dup(user, "demo");
	err |= str_dup(pass, "secret");

	return err;
}

@implementation TXSipAuth
- (id) initWithApp: (id) pApp
{
    self = [super init];
    if(!self) {
	return self;
    }
    app = pApp;
    return self;
}

- (void) setCreds:(TXSipUser*)pUser password:(NSString*)pPassword
{
    self->address = byte(pUser.addr);
    self->password = byte(pPassword);
    self->user = byte(pUser.user);
}

- (void) sendAuth
{

}

@end
