#include "re.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <openssl/pem.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

int cert_time(ASN1_TIME* asntime)
{
	int time=0;
	struct tm tv;
	char *ptime;
	switch(asntime->type) {
	case V_ASN1_UTCTIME:
	    ptime = strptime((const char*)asntime->data, "%y%m%d%H%M%S", &tv);
	    break;
	case V_ASN1_GENERALIZEDTIME:
	    ptime = strptime((const char*)asntime->data, "%Y%m%d%H%M%S", &tv);
	    break;
	default:
	    return time;
	}
	if(!ptime) return time;

	return (int)timegm(&tv);
}

int x509_info(char *path, int *rafter, int *rbefore, char **cname)
{
	int err = 0;
	X509 *x509=NULL;
	X509_NAME *name=NULL;

	BIO *bp = NULL;

	bp = BIO_new_file(path, "r");
	if(!bp) {
		err = -1;
		goto fail;
	}
	x509 = PEM_read_bio_X509(bp, &x509, NULL, NULL);
	BIO_free(bp);
	if(!x509) {
		err = -2;
		goto fail;
	}

	name=X509_get_subject_name(x509);
	char buf[100];
	int idx=0;
	X509_NAME_oneline(name, buf, 100);

	int start = 0, end = 100;
	for(idx=0;idx<(100-4);idx++) {
	     if(strncmp(buf+idx, "/CN=", 4))
		continue;

	     start = idx+4;
	     break;
	}
	for(idx=start;idx<end;idx++) {
	    switch(buf[idx]){
	    case '/':
	    case '\0':
		end = idx;
	    break;
	    }
	}

	char *cnret;
	cnret = mem_zalloc(end-start+1, NULL);
	memcpy(cnret, buf+start, end-start);

	if(rafter)
	    *rafter = cert_time(X509_get_notAfter(x509));

	if(rbefore)
	    *rbefore = cert_time(X509_get_notBefore(x509));

	X509_free(x509);

	*cname = cnret;

#ifndef OPENSSL_NO_ENGINE
	ENGINE_cleanup();
#endif
	CRYPTO_cleanup_all_ex_data();

fail:
	return err;
}
