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
    struct sip_addr *local;
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

void call_change(struct tcsipcall* call, void *arg)
{
    int cstate;

    tcsipcall_dirs(call, NULL, &cstate, NULL, NULL);

    if((cstate & CSTATE_ALIVE) == 0) {
        re_printf("call ended\n");
        tcsipcall_remove(call); // wrong place for this
	current_call = NULL;
	re_cancel();
    }
}

void reg_change(enum reg_state state, void*arg) {
    re_printf("reg change %d\n", state);
}

static void connect_handler(const struct sip_msg *msg, void *arg)
{
    int err;
    struct uac *uac = arg;
    struct tcsipcall *call;

    err = tcsipcall_alloc(&call, uac);
    tcop_users((void*)call, uac->local, NULL);
    tcsipcall_incomfing(call, msg);
    tcsipcall_accept(call);
    tcsipcall_handler(call, call_change, NULL);
    tcsipcall_control(call, CALL_ACCEPT);

    re_printf("incoming call %r\n", &msg->from.auri);
}

void call(struct uac *uac, char *name)
{
    struct tcsipcall *call;
    struct sip_addr *dest;
    int err;

    err = sippuser_by_name(&dest, name);
    err = tcsipcall_alloc(&call, uac); 
    tcop_users((void*)call, uac->local, dest);
    tcsipcall_out(call);
    tcsipcall_handler(call, call_change, NULL);

    current_call = call;

    re_printf("call %r\n", &dest->auri);
}

int main(int argc, char *argv[]) {
    libre_init();
    srtp_init();
#if __APPLE__
    apple_sound_init();
#endif

    int err;
    struct sip_addr *user_c;
    struct uac uac;

    net_default_source_addr_get(AF_INET, &uac.laddr);
    sa_set_port(&uac.laddr, 0);

    if(argc<2) {
        printf("provide certificate path\n");
        return 1;
    }

    if(argc<3) {
        printf("also provide usernames\n");
        return 1;
    }

    err = tls_alloc(&uac.tls, TLS_METHOD_SSLV23, argv[1], NULL);
    tls_add_ca(uac.tls, "CA.cert");

    err = dns_srv_get(NULL, 0, uac.nsv, &uac.nsc);

    if(uac.nsc==0) {
        uac.nsc = 1;
        sa_set_str(uac.nsv, "8.8.8.8", 53);
    }

    err = dnsc_alloc(&uac.dnsc, NULL, uac.nsv, uac.nsc);


    err = sip_alloc(&uac.sip, uac.dnsc, 32, 32, 32, 
          USER_AGENT, exit_handler, NULL);

    err = sip_transp_add(uac.sip, SIP_TRANSP_TLS, &uac.laddr,
          uac.tls);

    err = sipsess_listen(&uac.sock, uac.sip, 32, connect_handler, &uac);

    err = sippuser_by_name(&user_c, argv[2]);

    uac.local = user_c;

    struct tcsipreg *sreg;
    struct pl instance_id;
    pl_set_str(&instance_id, "cc823c2297c211e28cd960c547067464");
    tcsipreg_alloc(&sreg, &uac);
    tcop_users(sreg, user_c, user_c);
    tcsreg_set_instance_pl(sreg, &instance_id);
    tcsreg_handler(sreg, reg_change, NULL);
    tcsreg_state(sreg, 1);

    if(argc>3) {
        call(&uac, argv[3]);
    }

    re_main(signal_handler);

    mem_deref(uac.sip);
    mem_deref(uac.dnsc);

    libre_close();
    return 0;
}
