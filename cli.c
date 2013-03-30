#include "re.h"
#include "tcsipuser.h"
#include "tcsipreg.h"
#include "tcsipcall.h"
#include "x509util.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

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

static void *current_call = NULL;
static int exit_after = 0, auto_accept = 0;
void call(struct uac *uac, char *name);

static void eat() {
    printf("\x1b[1K\n\x1b[1A");
}

static void prompt(struct uac *uac) {
    eat();
    re_printf("texr %r> ", &uac->local->uri.user);
    fflush(stdout);
}

static void cmd_handler(int flags, void *arg)
{
    struct uac *uac = arg;
    int len;
    char buf[100];
    memset(buf, 0, 100);
    len = read(0, buf, 100);
    if(buf[len-1] != '\n') return;

    if(len > 5 && !strncmp(buf, "call ", 5) ) {
	buf[len-1] = '\0';
	call(uac, buf+5);
	goto exit;
    }
    if(len > 4 && !strncmp(buf, "hang", 4)) {
        if(current_call)
            tcsipcall_control(current_call, CALL_BYE);
        current_call = NULL;
	goto exit;
    }
    if(len == 2 && !strncmp(buf, "A\n", 2)) {
	if(current_call)
            tcsipcall_control(current_call, CALL_ACCEPT);
	goto exit;
    }
exit:
    prompt(uac);
}

static void exit_handler(void *arg)
{
    re_printf("stop sip\n");
}

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
	eat();
        re_printf("call ended\n");
        tcsipcall_remove(call); // wrong place for this
	current_call = NULL;
	if(exit_after)
	    re_cancel();
    }

    prompt(arg);
}

void reg_change(enum reg_state state, void*arg) {
    eat();
    switch(state) {
    case REG_TRY:
        re_printf("trying to register...\n");
        break;
    case REG_ONLINE:
	re_printf("online\n");
	break;
    default:
        re_printf("reg change %d\n", state);
    }
    prompt(arg);
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
    tcsipcall_handler(call, call_change, uac);
    if(auto_accept)
        tcsipcall_control(call, CALL_ACCEPT);
    current_call = call;

    eat();
    re_printf("incoming call %r\n", &msg->from.auri);
    prompt(uac);
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
    tcsipcall_handler(call, call_change, uac);

    current_call = call;

    re_printf("dialing %r\n", &dest->auri);
}

int main(int argc, char *argv[]) {
    libre_init();
    srtp_init();
#if __APPLE__
    apple_sound_init();
#endif

    int err;
    struct sip_addr *user_c;
    struct uac *uac;
    uac = mem_alloc(sizeof(struct uac), NULL);

    net_default_source_addr_get(AF_INET, &uac->laddr);
    sa_set_port(&uac->laddr, 0);

    if(argc<2) {
        printf("provide certificate path\n");
        return 1;
    }

    char *cname;
    struct pl cname_p;
    int after, before;
    err = x509_info(argv[1], &after, &before, &cname);
    if(err) {
	fprintf(stderr, "can't load cert at %s\n", argv[1]);
	return 1;
    }

    pl_set_str(&cname_p, cname);
    user_c = mem_zalloc(sizeof(*user_c), NULL);

    err = sip_addr_decode(user_c, &cname_p);
    if(err) return 1;

    err = tls_alloc(&uac->tls, TLS_METHOD_SSLV23, argv[1], NULL);
    tls_add_ca(uac->tls, "CA.cert");

    err = dns_srv_get(NULL, 0, uac->nsv, &uac->nsc);

    if(uac->nsc==0) {
        uac->nsc = 1;
        sa_set_str(uac->nsv, "8.8.8.8", 53);
    }

    err = dnsc_alloc(&uac->dnsc, NULL, uac->nsv, uac->nsc);


    err = sip_alloc(&uac->sip, uac->dnsc, 32, 32, 32, 
          USER_AGENT, exit_handler, NULL);

    err = sip_transp_add(uac->sip, SIP_TRANSP_TLS, &uac->laddr,
          uac->tls);

    err = sipsess_listen(&uac->sock, uac->sip, 32, connect_handler, uac);


    uac->local = user_c;

    struct tcsipreg *sreg;
    struct pl instance_id;
    pl_set_str(&instance_id, "cc823c2297c211e28cd960c547067464");
    tcsipreg_alloc(&sreg, uac);
    tcop_users(sreg, user_c, user_c);
    tcsreg_set_instance_pl(sreg, &instance_id);
    tcsreg_handler(sreg, reg_change, uac);
    tcsreg_state(sreg, 1);

    if(argc>2) {
        exit_after = 1;
        call(uac, argv[2]);
    }

    fd_listen(0, FD_READ, cmd_handler, uac);

    re_main(signal_handler);

    mem_deref(uac->sip);
    mem_deref(uac->dnsc);

    libre_close();
    return 0;
}
