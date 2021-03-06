//
//  txsip_private.h
//  libresip
//
//  Created by Ilya Petrov on 10/23/12.
//  Copyright (c) 2012 enodev. All rights reserved.
//

#include <re.h>

#ifndef libresip_txsip_private_h
#define libresip_txsip_private_h

struct uac {
    struct sip *sip;
    struct sa laddr;
    struct sipsess_sock *sock;
    struct dnsc *dnsc;
    struct sa nsv[20];
    uint32_t nsc;
    struct tls *tls;
    struct pl instance_id;
    struct mbuf apns;
};

struct uplink {
    struct le le;
    struct pl uri;
    bool ok;
};


#endif
