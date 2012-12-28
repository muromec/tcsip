//
//  TXKey.m
//  libresip
//
//  Created by Ilya Petrov on 12/28/12.
//  Copyright (c) 2012 enodev. All rights reserved.
//

#import "TXKey.h"

static char P12PASSW[] = "nop";

static bool _ssl_init = NO;

@implementation TXKey
@synthesize pub_key;
@synthesize priv_key;
- (id) init
{
    self = [super init];
    if(!_ssl_init) {
        SSLeay_add_all_algorithms();
        ERR_load_crypto_strings();
        _ssl_init = YES;
    }
    if(self) {
        [self gen];
    }
    return self;
}

- (void) gen
{
    RSA *rsa;
    long len;
    char *pp;

    mem = BIO_new(BIO_s_mem());

    pk = EVP_PKEY_new();
    rsa = RSA_generate_key(2048, RSA_F4,NULL,NULL);
    EVP_PKEY_assign_RSA(pk,rsa);

    PEM_write_bio_PUBKEY(mem, pk);
    len = BIO_get_mem_data(mem, &pp);

    pub_key = [NSData dataWithBytes:pp length:len];
    BIO_reset(mem);

    PEM_write_bio_PrivateKey(mem, pk, NULL, NULL, 0, NULL, NULL);
    len = BIO_get_mem_data(mem, &pp);
    priv_key = [NSData dataWithBytes:pp length:len];
    BIO_reset(mem);

}
- (NSData*) pkcs12:(NSData*)data_cert
{
    PKCS12 *p12;
    char *pp;
    long len;
    X509 *cert;

    BIO_puts(mem, data_cert.bytes);
    cert = PEM_read_bio_X509(mem, NULL, NULL, NULL);
    NSLog(@"cert loaded %p", cert);
    BIO_reset(mem);

    p12 = PKCS12_create(&P12PASSW[0], "sip cert", pk, cert, NULL, 0,0,0,0,0);
    NSLog(@"p12 created %p", p12);
    i2d_PKCS12_bio(mem, p12);
    len = BIO_get_mem_data(mem, &pp);

    NSData *p12_data = [NSData dataWithBytes:pp length:len];
    BIO_reset(mem);

    PKCS12_free(p12);
    X509_free(cert);

    return p12_data;
}

- (void) dealloc
{
    BIO_free(mem);
    EVP_PKEY_free(pk);
}

@end
