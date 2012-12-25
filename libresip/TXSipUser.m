//
//  TXSipUser.m
//  Texr
//
//  Created by Ilya Petrov on 9/25/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXSipUser.h"
#include <re.h>

#define _str(__x, __len) ([[NSString alloc] initWithBytes:__x length:__len encoding:NSASCIIStringEncoding])
#define _pstr(__pl) (_str(__pl.p, __pl.l))

@implementation TXSipUser
@synthesize user;
@synthesize name;
@synthesize addr;

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

    user = [data objectForKey:@"login"];
    name = [data objectForKey:@"name"];

    return self;
}


- (NSString*) netaddr
{
    NSRange range = [user rangeOfString: @"@"];
    if(range.location!=NSNotFound)
	return user;

    return [
        NSString
        stringWithFormat:@"sip:%@@crap.muromec.org.ua",
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
