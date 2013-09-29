#include <re.h>

#include <stdlib.h>
#include "http.h"

static void tchttp_destruct(void *arg) {
  struct tchttp *http = arg;

  mem_deref(http->tls);
  mem_deref(http->dnsc);

}

struct tchttp * tchttp_alloc(char *cert) {
    int err;
    struct tchttp *http;
    int nsv;
    http = mem_zalloc(sizeof(struct tchttp), tchttp_destruct);
    if(!http)
        return NULL;

    char *home, *ca_cert;
    home = getenv("HOME");

    re_sdprintf(&ca_cert, "%s/STARTSSL.cert", home);

    http->nsc = ARRAY_SIZE(http->nsv);

    err = tls_alloc(&http->tls, TLS_METHOD_SSLV23, cert, NULL);
    tls_add_ca(http->tls, ca_cert);

    mem_deref(ca_cert);

    err = dns_srv_get(NULL, 0, http->nsv, &http->nsc);
    if(err) {
        http->nsc = 0;
    }
    err = dnsc_alloc(&http->dnsc, NULL, http->nsv, http->nsc);

    if(http->dnsc && !http->nsc) {
        sa_set_str(http->nsv, "8.8.8.8", 53);
        http->nsc = 1;
    }
    dnsc_srv_set(http->dnsc, http->nsv, http->nsc);

    return http;
}
