//
//  TXAccount.m
//  Texr
//
//  Created by Ilya Petrov on 10/1/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXAccount.h"
#import "TXRestApi.h"
#import "TXKey.h"

#import <Security/Security.h>

static NSString *kUserCertPassword = @"nop";

@implementation TXAccount
@synthesize user;
@synthesize name;

- (id) initWithUser: (NSString*) pUser
{
    self = [super init];
    user = pUser;
    return self;
}

- (NSString*) cert
{
    return [TXAccount userCert: user];
}
+ (bool) available: (NSString*)user
{
    if(!user) return NO;
    NSFileManager *fm = [[NSFileManager alloc] init];
    bool ok = [fm isReadableFileAtPath:[self
                              userCert: user]];
    NSLog(@"check if user %@ available: %d", user, ok);
    return ok;
}

+ (NSString*) userCert: (NSString*)user ext:(NSString*)ext
{
    return [[NSString alloc]
            initWithFormat:@"%@/Library/Texr/%@.%@",
            NSHomeDirectory(), user, ext];
}

+ (NSString*) userCert: (NSString*)user
{
    return [self userCert:user ext:@"cert"];
}

+ (NSString*) certDir
{
    return [[NSString alloc]
            initWithFormat:@"%@/Library/Texr",
            NSHomeDirectory()];;

}

+ (NSString*) deflt
{
    NSArray *args = [[NSProcessInfo processInfo] arguments];
    NSString *first;
    if([args count] == 2) {
        first = [args objectAtIndex:1];
        if([first characterAtIndex:0] != '-')
            return first;
    }

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    return [defaults stringForKey:kSipUser];
}

+ (bool) canLogin {
    return [self available: [self deflt]];
}

- (void) saveUser {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:user forKey:kSipUser];
    [defaults synchronize];
}

- (void) saveCert:(NSString*)pem_cert
{
    NSData *priv = [self findKey: NO];
    NSString *pem = [NSString stringWithFormat:
        @"%@\n%s", pem_cert, priv.bytes];

    NSFileManager *fm = [[NSFileManager alloc] init];
    [fm
        createDirectoryAtPath: [TXAccount certDir]
        withIntermediateDirectories: YES
        attributes: nil
        error: nil
    ];
    [fm
        createFileAtPath: [TXAccount userCert: user]
        contents: [pem dataUsingEncoding:NSASCIIStringEncoding]
        attributes: nil
    ];

}

- (void)keygen
{
    key = [[TXKey alloc] init];
}

- (NSData*) findKey:(bool)pub
{
    if(key) {
        return pub ? key.pub_key : key.priv_key;
    }
    if(!pub)
        return nil;

    NSString *path = [TXAccount userCert:user ext:@"pub"];
    return [[NSData alloc] initWithContentsOfFile:path];
}

- (void) auth:(NSString*)pUser password:(NSString*)pPassw cb:(Callback*)CB
{
    NSLog(@"try auth");
    user = pUser;

    NSData *pubkey = [self findKey:YES];
    if(!pubkey) {
        [self keygen];
        pubkey = [self findKey:YES];
    }

    auth_cb = CB;
    id api = [[TXRestApi alloc] init];
    [api rload: @"cert"
               cb: CB(self, certLoaded:)];
    [api setAuth:pUser password:pPassw];
    [api post:@"pub_key" val:[NSString stringWithFormat:
	    @"%s", pubkey.bytes]];
    [api start];
}

- (void) certLoaded:(NSDictionary*)payload
{
    if(!payload) {
        [auth_cb response: nil];
        auth_cb = nil;
        return;
    }
    NSString *pem_cert = [payload objectForKey:@"pem"];
    [self saveUser];
    [self saveCert: pem_cert];

    [auth_cb response: @"ok"];

}

- (NSString*) uuid
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString* ret;
    ret = [defaults stringForKey:kSipUUID];
    if(ret)
        return ret;

    CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
    ret = (__bridge_transfer NSString *)CFUUIDCreateString(kCFAllocatorDefault, uuid);
    CFRelease(uuid);

    [defaults setObject:ret forKey:kSipUUID];
    [defaults synchronize];

    return ret;
}

@end
