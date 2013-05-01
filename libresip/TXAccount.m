//
//  TXAccount.m
//  Texr
//
//  Created by Ilya Petrov on 10/1/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXAccount.h"
#import "TXRestApi.h"
#import "TXHttp.h"
#import "TXKey.h"
#import "ReWrap.h"

#import <Security/Security.h>

@implementation TXAccount
@synthesize user;
@synthesize name;
@synthesize api;

- (id) init
{
    self = [super init];
    if(!self) return self;

    api = [[TXRestApi alloc] initWithApp:[ReWrap app]];

    return self;
}

- (id) initWithUser: (NSString*) pUser
{
    self = [self init];
    if(!self)
        return self;

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
    D(@"check if user %@ available: %d", user, ok);
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
    [self drop_changes];
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

- (void)drop
{
    NSFileManager *fm = [[NSFileManager alloc] init];
    [fm
        removeItemAtPath: [TXAccount userCert: user]
		   error: nil];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *nkey = [[NSString alloc]
            initWithFormat:@"changes/%@", user];
    [defaults removeObjectForKey:nkey];
}

- (NSDate*)changes_stamp
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *nkey = [[NSString alloc]
            initWithFormat:@"changes/%@", user];
    NSInteger chstamp = [defaults integerForKey:nkey];
    if(!chstamp) return nil;

    return [NSDate dateWithTimeIntervalSince1970:chstamp];
}

- (void)setChanges_stamp:(NSDate*)val
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *nkey = [[NSString alloc]
            initWithFormat:@"changes/%@", user];
    NSInteger ts = [val timeIntervalSince1970];

    [defaults setInteger:ts forKey:nkey];
    [defaults synchronize];
}

- (void)drop_changes
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *nkey = [[NSString alloc]
            initWithFormat:@"changes/%@", user];
    [defaults removeObjectForKey:nkey];
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
    D(@"try auth");
    user = pUser;

    NSData *pubkey = [self findKey:YES];
    if(!pubkey) {
        [self keygen];
        pubkey = [self findKey:YES];
    }

    auth_cb = CB;
    id req = [api request: @"cert" cb:CB(self, certLoaded:)];
    [req setAuth:pUser password:pPassw];
    [req post:[NSString stringWithFormat:
	    @"%s", pubkey.bytes]];
    [req start];
}

- (void) saveName:(NSString*)fullname user:(NSString*)pUser password:(NSString*)pPassw cb:(Callback*)cb
{
    id req = [api request: @"fullname" cb:cb];
    [req setAuth:pUser password:pPassw];
    [req post:@"name" val:fullname];
    [req start];
}

- (void) certLoaded:(NSDictionary*)ret
{
    NSDictionary *payload = [ret objectForKey:@"data"];
    if(!payload) {
        id _cb = auth_cb;
        auth_cb = nil;
        [_cb response:nil];
        return;
    }
    [self saveUser];

    NSString *pem_cert = [payload objectForKey:@"pem"];
    [self saveCert: pem_cert];
 
    [auth_cb response: @"ok"];
}

- (void)create:(NSString*)email phone:(NSString*)phone cb:(Callback*)cb
{
    auth_cb = cb;
    id req = [api request: @"signup" cb: CB(self, createRet:)];
    [req post:@"email" val:email];
    [req post:@"phone" val:phone];
    [req start];
}

- (void)createRet:(NSDictionary*)ret
{
    id _cb = auth_cb;
    auth_cb = nil;
    [_cb response:ret];
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
