//
//  TXRestApi.m
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXRestApi.h"
#import "JSONKit.h"
#import "Callback.h"
#import "ASIHTTPRequest.h"

@implementation TXRestApi
+ (void)r: (NSString*)path cb:(id)cb
{
    TXRestApi *api = [[TXRestApi alloc] init];
    [api rload: path cb: cb];
}

- (void)rload: (NSString*)path cb:(id)pCb {
    NSURL *myURL = [NSURL URLWithString:[NSString stringWithFormat:@"https://texr.enodev.org/api/%@",path]];
NSLog(@"api %@ url %@", path, myURL);
    
    ASIHTTPRequest *request = [ASIHTTPRequest requestWithURL:myURL];

    self->cb = pCb;

    [self importCA: @"/Users/muromec/Library/Texr/neko.cert"];

    SecCertificateRef cert = [self findCert];
    SecIdentityRef ident;

    int ok = SecIdentityCreateWithCertificate(nil, cert, &ident);
    if(ok != 0) {
        NSLog(@"ident not found %d", ok);
	goto ident_err;
    }


[request setValidatesSecureCertificate:NO];
    [request setClientCertificateIdentity:ident];

    [request setDelegate:self];
    [request startAsynchronous];

out:
    CFRelease(ident);
ident_err:
    CFRelease(cert);
}


- (void)requestFinished:(ASIHTTPRequest *)request
{
    // Use responseData

    JSONDecoder* decoder = [JSONDecoder decoder];
    id ret = [decoder objectWithData: [request responseData]];
    //NSString *status = [ret objectForKey:@"status"];
    NSArray *payload = [ret objectForKey:@"data"];

NSLog(@"https ok payload");
    [cb response: payload];
}
- (void)requestFailed:(ASIHTTPRequest *)request
{
   NSError *error = [request error];
   NSLog(@"req failed %@", error);
}

- (void) setAuth:(NSString*)pU password:(NSString*)pW
{
    username = pU;
    password = pW;
}

- (void) importCA: (NSString*)thePath;
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
    const char *certLabelString = "sip:neko@texr.enodev.org";
    CFStringRef certLabel = CFStringCreateWithCString(
                                NULL, certLabelString,
                                kCFStringEncodingUTF8);

    const void *keys[] =   { kSecClass, kSecAttrLabel, kSecReturnRef };
    const void *values[] = { kSecClassCertificate, certLabel, kCFBooleanTrue };
    CFDictionaryRef dict = CFDictionaryCreate(NULL, keys,
                                               values, 3,
                                             NULL, NULL);
    status = SecItemCopyMatching(dict, &certificateRef);
    CFRelease(dict);
    CFRelease(certLabel);

    if (status != errSecSuccess)
	return nil;

    return certificateRef;
}

@end
