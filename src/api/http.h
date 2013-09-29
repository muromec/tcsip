#ifndef TCSIP_API_HTTP_H
#define TCSIP_API_HTTP_H

#include <re.h>

struct tchttp {
    struct dnsc *dnsc;
    struct tls *tls;
    struct sa nsv[16];
    uint32_t nsc;
    char* login;
    char* password;
};

#endif
