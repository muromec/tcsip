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
    [TXRestApi r: @"cert"
               cb: CB(self, certLoaded:)
             user: pUser
         password: pPassw];
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

- (SecIdentityRef) ssl
{
    if(_ssl_ident) return _ssl_ident;
    SecIdentityRef ident = nil;

    [self importCert: [self cert]];
    SecCertificateRef cert = [self findCert];

    int ok = SecIdentityCreateWithCertificate(nil, cert, &ident);
    if(ok != 0) {
        NSLog(@"ident not found. somebody tampered with keychain");
        return nil;
    }

    _ssl_ident = ident;
    return ident;
}

- (void) importCert: (NSString*)thePath;
{

    NSData *PKCS12Data = [[NSData alloc] initWithContentsOfFile:thePath];
    CFDataRef inPKCS12Data = (__bridge CFDataRef)PKCS12Data;
    CFDictionaryRef opts = CFDictionaryCreate(NULL, NULL, NULL, 0, NULL, NULL); 
    CFArrayRef items = NULL;

    int ok = SecPKCS12Import(inPKCS12Data, opts, &items);
    switch(ok){
    case 0:
        NSLog(@"items %@", items);
        CFRelease(items);
    case errSecDuplicateItem:
	NSLog(@"import ok!");
	break;
    default:
        NSLog(@"import fail %d", ok);
    }
}

- (SecCertificateRef) findCert
{
    OSStatus status = errSecSuccess;
    CFTypeRef   certificateRef     = NULL;                      // 1
    NSString *label = [NSString
        stringWithFormat: @"sip:%@@texr.enodev.org",
        user
    ];

    const void *keys[] =   { kSecClass, kSecAttrLabel, kSecReturnRef };
    const void *values[] = { kSecClassCertificate, (__bridge void*)label, kCFBooleanTrue };
    CFDictionaryRef dict = CFDictionaryCreate(NULL, keys,
                                               values, 3,
                                             NULL, NULL);
    status = SecItemCopyMatching(dict, &certificateRef);
    CFRelease(dict);

    if (status != errSecSuccess)
	return nil;

    return certificateRef;
}

@end
