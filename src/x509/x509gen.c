#include <openssl/pem.h>
#include <openssl/err.h>
#include "re.h"

static int _ssl_init = 0;

static void xload() {
    if(!_ssl_init) {
        SSLeay_add_all_algorithms();
        ERR_load_crypto_strings();
        _ssl_init = 1;
    }
}

int x509_pub_priv(struct mbuf **rpriv, struct mbuf **rpub) {
    RSA *rsa;
    EVP_PKEY *pk;
    BIO *mem;

    long len;
    char *pp;
    struct mbuf *priv, *pub;

    mem = BIO_new(BIO_s_mem());

    pk = EVP_PKEY_new();
    rsa = RSA_generate_key(2048, RSA_F4,NULL,NULL);
    EVP_PKEY_assign_RSA(pk,rsa);

    PEM_write_bio_PUBKEY(mem, pk);
    len = BIO_get_mem_data(mem, &pp);

    pub = mbuf_alloc(len+2);
    mbuf_write_mem(pub, (const uint8_t *)pp, len);
    mbuf_write_mem(pub, (const uint8_t *)"\0\0", 2);
    pub->pos = 0;

    (void)BIO_reset(mem);

    PEM_write_bio_PrivateKey(mem, pk, NULL, NULL, 0, NULL, NULL);
    len = BIO_get_mem_data(mem, &pp);
    priv = mbuf_alloc(len+2);
    mbuf_write_mem(priv, (const uint8_t *)pp, len);
    mbuf_write_mem(priv, (const uint8_t *)"\0\0", 2);
    priv->pos = 0;

    (void)BIO_reset(mem);

    *rpriv = priv;
    *rpub = pub;

    BIO_free(mem);
    EVP_PKEY_free(pk);

    return 0;
}
