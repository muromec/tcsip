//
//  TXKey.m
//  libresip
//
//  Created by Ilya Petrov on 12/28/12.
//  Copyright (c) 2012 enodev. All rights reserved.
//

#import "TXKey.h"

#include <openssl/pem.h>

@implementation TXKey
@synthesize pub_key;
@synthesize priv_key;
- (id) init
{
    self = [super init];
    if(self)
        [self gen];
    return self;
}

- (void) gen
{
    EVP_PKEY *pk = NULL;
    RSA *rsa;
    BIO *mem;

    mem = BIO_new(BIO_s_mem());

    pk = EVP_PKEY_new();
    rsa = RSA_generate_key(2048, RSA_F4,NULL,NULL);
    EVP_PKEY_assign_RSA(pk,rsa);


    int len;
    char *pp;

    PEM_write_bio_PUBKEY(mem, pk);
    len = BIO_get_mem_data(mem, &pp);

    pub_key = [NSData dataWithBytes:pp length:len];
    BIO_reset(mem);

    PEM_write_bio_PrivateKey(mem, pk, NULL, NULL, 0, NULL, NULL);
    len = BIO_get_mem_data(mem, &pp);
    priv_key = [NSData dataWithBytes:pp length:len];

    BIO_free(mem);
    EVP_PKEY_free(pk);
}

@end
