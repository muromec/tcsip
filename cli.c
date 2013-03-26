#include "re.h"
#include "tcsipuser.h"
#include "tcsipreg.h"
#include "tcsipcall.h"
#include <stdio.h>

#define USER_AGENT "Linux re"

struct uac {
    struct sip *sip;
    struct sa laddr;
    struct sipsess_sock *sock;
    struct dnsc *dnsc;
    struct tls *tls;
    struct sa nsv[16];
    uint32_t nsc;
};

static void exit_handler(void *arg)
{
    re_printf("stop sip\n");
}

static void *current_call = NULL;

static void signal_handler(int sig)
{
    if(current_call) tcsipcall_control(current_call, CALL_BYE);
    current_call = NULL;
    re_cancel();
}

static void connect_handler(const struct sip_msg *msg, void *arg)
{
}

int main(int argc, char *argv[]) {
    libre_init();
    srtp_init();
#if __APPLE__
    media_snd_init();
#endif

    int err;
    struct sip_addr *user_c, *dest;
    struct uac uac;

    net_default_source_addr_get(AF_INET, &uac.laddr);
    sa_set_port(&uac.laddr, 0);
    re_printf("addr %J\n", &uac.laddr);

    if(argc<2) {
        printf("provide certificate path\n");
        return 1;
    }

    if(argc<4) {
        printf("also provide usernames\n");
        return 1;
    }

    err = tls_alloc(&uac.tls, TLS_METHOD_SSLV23, argv[1], NULL);
    tls_add_ca(uac.tls, "CA.cert");

    err = dns_srv_get(NULL, 0, uac.nsv, &uac.nsc);

    re_printf("nsv %J %d %d\n", uac.nsv, uac.nsc, err);
    if(uac.nsc==0) {
        uac.nsc = 1;
        sa_set_str(uac.nsv, "8.8.8.8", 53);
    }

    err = dnsc_alloc(&uac.dnsc, NULL, uac.nsv, uac.nsc);


    err = sip_alloc(&uac.sip, uac.dnsc, 32, 32, 32, 
          USER_AGENT, exit_handler, NULL);

    err = sip_transp_add(uac.sip, SIP_TRANSP_TLS, &uac.laddr,
          uac.tls);

    re_printf("transport add %J\n", &uac.laddr);

    err = sipsess_listen(&uac.sock, uac.sip, 32, connect_handler, NULL);

    re_printf("sock %r\n", uac.sock);

    err = sippuser_by_name(&user_c, argv[2]);
    err = sippuser_by_name(&dest, argv[3]);

    struct tcsipcall *call;
    err = tcsipcall_alloc(&call, &uac); 
    tcop_users((void*)call, user_c, dest);
    tcsipcall_out(call);
    tcsipcall_waitice(call);

    current_call = call;

    re_main(signal_handler);

    re_printf("user %r\n", &user_c->auri);

    mem_deref(uac.sip);
    mem_deref(uac.dnsc);

    libre_close();
    return 0;
}
