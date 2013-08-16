#include <errno.h>

#include "re.h"
#include "tcsip/tcsip.h"
#include "tcsip/tcsipuser.h"
#include "tcsip/tcsipreg.h"
#include "tcsip/tcsipcall.h"
#include "x509/x509util.h"
#include "store/history.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define USER_AGENT "Linux re"

struct cli_app {
    struct tcsip *sip;
    void *current_call;
    struct sip_handlers *handlers;
    struct sip_addr *local;
};

void *glob_app;
static int exit_after = 0, auto_accept = 0;
void call(struct cli_app *app, char *name);

static void eat() {
    printf("\x1b[1K\n\x1b[1A");
}

static void prompt(struct cli_app *app) {
    eat();
    re_printf("texr %r> ", &app->local->uri.user);
    fflush(stdout);
}

static bool history_handler(struct le *le, void *arg)
{
    struct hist_el *hel = le->data;

    re_printf("hist %d event %d %r <%r>\n",
            hel->time,
            hel->event, &hel->name, &hel->login);

    return false;
}

static void cmd_handler(int flags, void *arg)
{
    struct cli_app *app = arg;
    int len;
    char buf[100];
    memset(buf, 0, 100);
    len = read(0, buf, 100);
    if(buf[len-1] != '\n') return;

    if(len > 5 && !strncmp(buf, "call ", 5) ) {
	buf[len-1] = '\0';
	call(app, buf+5);
	goto exit;
    }
    if(len > 4 && !strncmp(buf, "hang", 4)) {
        if(app->current_call)
            tcsipcall_control(app->current_call, CALL_BYE);
        app->current_call = NULL;
	goto exit;
    }
    if(len == 2 && !strncmp(buf, "A\n", 2)) {
	if(app->current_call)
            tcsipcall_control(app->current_call, CALL_ACCEPT);
	goto exit;
    }
    if(len > 4 && !strncmp(buf, "hist", 4)) {
        re_printf("fetch idx\n");
        char *idx;
        struct list *hlist = NULL;
        tcsip_hist_fetch(app->sip, &idx, &hlist);
        if(hlist) {
            list_apply(hlist, true, history_handler, NULL);
            list_flush(hlist);
            mem_deref(hlist);
        }
        if(idx)
          mem_deref(idx);

        re_printf("got idx %r\n", &idx);
    }
exit:
    prompt(app);
}

static void exit_handler(void *arg)
{
    re_printf("stop sip\n");
}

static void signal_handler(int sig)
{
    struct cli_app *app = glob_app;
    if(app->current_call)
        tcsipcall_control(app->current_call, CALL_BYE);
    app->current_call = NULL;
    re_cancel();
}

void call_change(struct tcsipcall* call, void *arg)
{
    int cstate;
    struct cli_app *app = arg;

    tcsipcall_dirs(call, NULL, &cstate, NULL, NULL);

    if((cstate & CSTATE_ALIVE) == 0) {
	eat();
        re_printf("call ended\n");
        tcsipcall_remove(call); // wrong place for this
	app->current_call = NULL;
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
    case REG_NONE:
        re_printf("offline\n");
        break;
    default:
        re_printf("reg change %d\n", state);
    }
    prompt(arg);
}

void call(struct cli_app *app, char *name)
{

    struct tcsipcall *call;
    struct sip_addr *dest;
    int err;

    err = sippuser_by_name(&dest, name);
    if(err) {
        printf("failed to create user\n");
        return;
    }

    tcsip_start_call(app->sip, dest);
    app->current_call = call;

    re_printf("dialing %r\n", &dest->auri);
}

void got_call(struct tcsipcall* call, void *arg) {
    int cdir;
    struct sip_addr *remote;
    struct cli_app *app = arg;

    tcsipcall_dirs(call, &cdir, NULL, NULL, NULL);

    app->current_call = call;

    tcop_lr((void*)call, NULL, &remote);
    if(cdir==CALL_IN) {
        eat();
        re_printf("incoming call %r\n", &remote->auri);
        prompt(arg);
    }
}

void got_cert(int err, struct pl*name, void*arg) {
    if(err) {
      re_cancel();
      return;
    }
    struct cli_app *app = arg;
    struct sip_addr *local = tcsip_user(app->sip);
    if(local)
        app->local = mem_ref(local);

    re_printf("Hello, %r <%r> !\n", name, &local->auri);
}

static struct sip_handlers cli_handlers = {
    .call_h = got_call,
    .call_ch = call_change,
    .cert_h = got_cert,
    .reg_h = reg_change,
};

int main(int argc, char *argv[]) {
    libre_init();

    int err;
    struct tcsip *sip;
    struct cli_app *app;

    app = mem_zalloc(sizeof(struct cli_app), NULL);
    if(!app) {
        err = -ENOMEM;
        goto fail;
    }
    glob_app = app;// signal handlers suck

    app->handlers = mem_zalloc(sizeof(*app->handlers), NULL);
    memcpy(app->handlers, &cli_handlers, sizeof(*app->handlers));
    app->handlers->arg = app;

    tcsip_alloc(&app->sip, 0, app->handlers);
    if(!app->sip) {
        printf("failed to create driver instance\n");
        return 1;
    }

    sip = app->sip;

    if(argc<2) {
        printf("provide login name\n");
        return 1;
    }
    struct pl instance_id;
    pl_set_str(&instance_id, "cc823c2297c211e28cd960c547067464");
    tcsip_uuid(sip, &instance_id);

    struct pl name;
    pl_set_str(&name, argv[1]);

    err = tcsip_local(sip, &name);
    if(err) {
        re_printf("local login failed  %d\n", err);
        return err;
    }

    tcsip_set_online(sip, 1);

    if(argc>2) {
        exit_after = 1;
        call(app, argv[2]);
    }

    fd_listen(0, FD_READ, cmd_handler, app);

    re_main(signal_handler);

    fd_close(0);

    mem_deref(sip);

fail:
    libre_close();
    return 0;
}
