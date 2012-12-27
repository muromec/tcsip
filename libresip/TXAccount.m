//
//  TXAccount.m
//  Texr
//
//  Created by Ilya Petrov on 10/1/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXAccount.h"
#import "TXRestApi.h"

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

- (void) certLoaded:(NSDictionary*)payload
{
    NSLog(@"cert loaded: %d", payload);
    if(!payload) {
        [auth_cb response: nil];
        auth_cb = nil;
        return;
    }
    NSString *pem = [payload objectForKey:@"pem"];
    NSString *p12 = [payload objectForKey:@"p12"];
    NSData *b64 = [p12 dataUsingEncoding:NSASCIIStringEncoding];

    int p12_len = 4096, ok;
    char raw_p12[4096];

    ok = base64_decode([b64 bytes], (int)[b64 length], raw_p12, &p12_len);
    if(ok!=0) {
        NSLog(@"cant b63 decode p12");
        return;
    }
    NSLog(@"decoded %d bytes of p12-base64", p12_len);
    NSData *raw = [NSData dataWithBytes:raw_p12 length:p12_len];
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
        contents: [pem dataUsingEncoding:NSASCIIStringEncoding]
        attributes: nil
    ];

    [fm
        createFileAtPath: [TXAccount userCert: user ext:@"p12"]
        contents: raw
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
