//
//  TXSipUser.m
//  Texr
//
//  Created by Ilya Petrov on 9/25/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXSipUser.h"
#include <re.h>
#include "strmacro.h"


@implementation TXSipUser
@synthesize user;
@synthesize name;
@synthesize addr;
@synthesize host;

+ (id) withName: (NSString*)pUser
{
    return [[TXSipUser alloc] initWithName:pUser];
}

- (id) initWithName: (NSString*)pUser
{
    self = [super self];
    if (!self) return self;

    user = [[NSString alloc] initWithFormat: @"%@", pUser];
    name = user;
    addr = [self netaddr];

    return self;
}

+ (id) withAddr: (struct sip_taddr*)addr
{
    return [[TXSipUser alloc] initWithAddr:addr];
}

- (id) initWithAddr: (struct sip_taddr*)pAddr
{
    self = [super self];

    name = _pstr(pAddr->dname);
    user = _pstr(pAddr->uri.user);
    addr = [self netaddr]; // XXX wrong, use full addr

    return self;
}

+ (id) withData: (NSDictionary*) data
{
    return [[TXSipUser alloc] initWithData: data];
}

- (id) initWithData: (NSDictionary*) data
{
    self = [super self];

    // XXX: wtf? unify format!
    user = [data objectForKey:@"login"];
    if(!user)
	user = [data objectForKey:@"user"];

    name = [data objectForKey:@"name"];
    addr = [self netaddr]; // XXX wrong, use full addr

    return self;
}


- (NSString*) netaddr
{
    NSRange range = [user rangeOfString: @"@"];
    if(range.location!=NSNotFound)
	return user;

    return [
        NSString
        stringWithFormat:@"sip:%@@texr.net",
	user
    ];
}

- (NSDictionary*) asDict
{

    return [NSDictionary 
        dictionaryWithObjectsAndKeys:
        name, @"name",
	user, @"login", 
	addr, @"addr",
       nil];
}

@end
