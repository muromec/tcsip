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

static NSString *kUserCertPassword = @"Gahghad6tah4Oophahg2tohSAeCithe1Shae8ahkeik9eoYo";

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

- (void) saveCert:(NSString*)pem_pub
{
    NSData *priv = [self findKey: NO];
    NSString *pem = [NSString stringWithFormat:
        @"%@\n%s", pem_pub, priv.bytes];

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
               cb: CB(self, certLoaded:)
	    ident: nil
	     post: YES];
    [api setAuth:pUser password:pPassw];
    [api post:@"pub_key" val:[NSString stringWithFormat:
	    @"%s", pubkey.bytes]];
    [api start];
}

- (void) certLoaded:(NSDictionary*)payload
{
    NSLog(@"cert loaded: %d", payload);
    if(!payload) {
        [auth_cb response: nil];
        auth_cb = nil;
        return;
    }
    NSString *pem_pub = [payload objectForKey:@"pem"];
    [self saveUser];
    [self saveCert: pem_pub];

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

    ident = [self importIdent: [TXAccount userCert: user ext: @"p12"]];
    if(!ident) {
        NSLog(@"cant load p12 ident for %@", user);
    }

    _ssl_ident = ident;
    return ident;
}

- (SecIdentityRef) importIdent: (NSString*)thePath;
{

    NSData *PKCS12Data = [[NSData alloc] initWithContentsOfFile:thePath];
    if(!PKCS12Data) {
        NSLog(@"p12 file not found %@", thePath);
        return nil;
    }
    CFDataRef inPKCS12Data = (__bridge CFDataRef)PKCS12Data;
    NSMutableDictionary * opts = [NSDictionary
        dictionaryWithObjectsAndKeys:
        kUserCertPassword,
        kSecImportExportPassphrase,
        nil
    ];

    CFArrayRef items = NULL;
    NSLog(@"import %@ %@ len %d", opts, thePath, CFDataGetLength(inPKCS12Data));

    int ok = SecPKCS12Import(inPKCS12Data, (__bridge CFDictionaryRef)opts, &items);
    switch(ok){
    case 0:
        NSLog(@"items %@", items);
        break;
    case errSecDuplicateItem:
	NSLog(@"already impoerted");
        return nil;
    default:
        NSLog(@"import fail %d", ok);
        return nil;
    }

    CFDictionaryRef identityDict = CFArrayGetValueAtIndex(items, 0);
    SecIdentityRef identityApp = (SecIdentityRef)CFDictionaryGetValue(identityDict, kSecImportItemIdentity);
    NSLog(@"identity %@", identityApp);
    return identityApp;

out:
    CFRelease(items);
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
