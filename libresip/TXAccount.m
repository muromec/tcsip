//
//  TXAccount.m
//  Texr
//
//  Created by Ilya Petrov on 10/1/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXAccount.h"
#import "TXRestApi.h"

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

+ (NSString*) userCert: (NSString*)user
{
    return [[NSString alloc]
            initWithFormat:@"%@/Library/Texr/%@.cert",
            NSHomeDirectory(), user];
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

- (void) auth:(NSString*)pUser password:(NSString*)pPassw cb:(Callback*)CB
{
    NSLog(@"try auth");

    auth_cb = CB;
    user = pUser;
    TXRestApi *api = [[TXRestApi alloc] init];
    [api setAuth: pUser password:pPassw];
    [api rload: @"cert" cb: CB(self, certLoaded:)];
}

- (void) certLoaded:(NSString*)payload
{
    NSLog(@"cert loaded: %d", payload != nil);
    if(!payload) {
        [auth_cb response: nil];
        auth_cb = nil;
        return;
    }

    [self saveUser];
    NSFileManager *fm = [[NSFileManager alloc] init];
    [fm
        createDirectoryAtPath: [TXAccount certDir]
        withIntermediateDirectories: YES
        attributes: nil
        error: nil
    ];
    [fm
        createFileAtPath: [TXAccount userCert: user]
        contents: [payload dataUsingEncoding:NSASCIIStringEncoding]
        attributes: nil
    ];

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
