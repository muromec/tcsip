#include "re.h"
#include "tcsipcall.h"
#include "tcsipuser.h"
#include "txsip_private.h"
#include "tcmedia.h"
#include <sys/time.h>

struct tcsipcall {
    struct uac *uac;
    struct sipsess *sess;
    struct sip_addr *remote;
    struct sip_addr *local;
    struct le le;
    struct tcmedia *media;
    struct sip_msg *msg;
    tcsipcall_h *handler;
    void *handler_arg;
    struct pl ckey;
    int cdir;
    int cstate;
    int reason;
    time_t ts;
};

void tcsipcall_activate(struct tcsipcall*call);
void tcsipcall_hangup(struct tcsipcall*call);

static void dummy_handler(struct tcsipcall*call, void*arg){
}

void call_destruct(void *arg)
{
    struct tcsipcall *call = arg;

    if(call->local) call->local = mem_deref(call->local);
    if(call->remote) call->remote = mem_deref(call->remote);
    call->uac = mem_deref(call->uac);

}

static int offer_handler(struct mbuf **mbp, const struct sip_msg *msg,
			 void *arg) {
    re_printf("sdp offer\n");

    struct tcsipcall *call = arg;
    return tcmedia_offer(call->media, msg->mb, mbp);
}

/* called when an SDP answer is received */
static int answer_handler(const struct sip_msg *msg, void *arg)
{
    re_printf("sdp answer\n");
    struct tcsipcall *call = arg;
    tcmedia_answer(call->media, msg->mb);

    return 0;
}

/* called when SIP progress (like 180 Ringing) responses are received */
static void progress_handler(const struct sip_msg *msg, void *arg)
{
    struct tcsipcall *call = arg;
    re_printf("session progress: %u %r\n", msg->scode, &msg->reason);
    if((msg->scode == 183) && mbuf_get_left(msg->mb)) {
	re_printf("early media");
	tcmedia_answer(call->media, msg->mb);
	tcsipcall_activate(call);
    }
}


/* called when the session is established */
static void establish_handler(const struct sip_msg *msg, void *arg)
{
    (void)msg;

    struct tcsipcall *call = arg;
    re_printf("session established: %u %r\n", msg->scode, &msg->reason);
    tcsipcall_activate(call);
}

/* called when the session fails to connect or is terminated from peer */
static void close_handler(int err, const struct sip_msg *msg, void *arg)
{
    struct tcsipcall *call = arg;
    tcsipcall_hangup(call);
}

static bool find_date(const struct sip_hdr *hdr, const struct sip_msg *msg,
			  void *arg)
{
	int *stamp = arg;
	char *ret;
	struct tm tv;
	struct pl tmp;
	pl_dup(&tmp, &hdr->val);
	ret = strptime(tmp.p, "%a, %d %b %Y %H:%M:%S GMT", &tv);

	mem_deref((void*)tmp.p);

	if(ret) {
	    *stamp = (int)timegm(&tv);
	    return false;
	}

	return true;
}


int tcsipcall_alloc(struct tcsipcall**rp, struct uac *uac)
{
    int err;
    struct tcsipcall *call;
    
    call = mem_zalloc(sizeof(struct tcsipcall), call_destruct);
    if(!call) {
	err = -ENOMEM;
	goto fail;
    }
    call->handler = dummy_handler;
    call->uac = mem_ref(uac);

    *rp = call;
    return 0;
fail:
    return err;
}

void tcsipcall_append(struct tcsipcall*call, struct list* cl)
{
    list_append(cl, &call->le, call);
}

void tcsipcall_remove(struct tcsipcall*call)
{
    list_unlink(&call->le);
}

void tcsipcall_out(struct tcsipcall*call)
{
    int err;
    struct pl tmp;
    struct timeval now;

    pl_set_str(&tmp, "123");
    pl_dup(&call->ckey, &tmp);

    call->cstate = CSTATE_STOP;
    call->cdir = CALL_OUT;
    gettimeofday(&now, NULL);
    call->ts = now.tv_sec;

    err = tcmedia_alloc(&call->media, call->uac, CALL_OUT);
    if(err)
        call->cstate |= CSTATE_ERR;
}

void tcsipcall_parse_from(struct tcsipcall*call)
{
    call->remote = mem_ref((void*)&call->msg->from);
}

void tcsipcall_parse_date(struct tcsipcall*call)
{
    int timestamp = 0;
    struct timeval now;

    sip_msg_hdr_apply(call->msg, true, SIP_HDR_DATE, find_date, &timestamp);
    if(timestamp)
	call->ts = timestamp;
    else {
        gettimeofday(&now, NULL);
        call->ts = now.tv_sec;
    }
}


int tcsipcall_incomfing(struct tcsipcall*call, const struct sip_msg* msg)
{
    int err = 0;
    struct pl tmp;

    pl_set_str(&tmp, "123");
    pl_dup(&call->ckey, &tmp);

    call->cstate = CSTATE_IN_RING;
    call->cdir = CALL_IN;

    call->msg = mem_ref((void*)msg);

    err = tcmedia_alloc(&call->media, call->uac, CALL_IN);
    if(err)
        call->cstate |= CSTATE_ERR;

    tcsipcall_parse_from(call);
    tcsipcall_parse_date(call);

    return err;
}

void tcsipcall_handler(struct tcsipcall*call, tcsipcall_h ch, void*arg)
{
    if(!call)
	return;

    call->handler = ch;
    call->handler_arg = arg;
}

void tcsipcall_activate(struct tcsipcall*call) {

    if(TEST(call->cstate, CSTATE_FLOW|CSTATE_MEDIA)) {
	return;
    }

    /* XXX: move out */
    DROP(call->cstate, CSTATE_RING);
    call->cstate |= CSTATE_EST;

    int err;
    err = tcmedia_start(call->media);
    if(err) {
        call->cstate |= CSTATE_ERR;
        goto out;
    }
    // XXX: assert rtp not null
    call->cstate |= (CSTATE_FLOW | CSTATE_MEDIA);

out:
    call->handler(call, call->handler_arg);
}

void tcsipcall_hangup(struct tcsipcall*call) {
    if(call->sess) call->sess = mem_deref(call->sess);

    if(!call->reason && (call->cstate & CSTATE_EST)==CSTATE_EST)
        call->reason = CEND_OK;

    if(!call->reason && (call->cstate & CSTATE_EST)==0)
        call->reason = CEND_HANG;

    DROP(call->cstate, CSTATE_ALIVE);
    DROP(call->cstate, CSTATE_EST);
    /*
     * Call terminated
     * Remote party dropped something heavy
     * on red button
     * */

    if(call->media) {
        tcmedia_stop(call->media);
        call->media = mem_deref(call->media);
    }

    call->handler(call, call->handler_arg);
}
void tcsipcall_accept(struct tcsipcall*call)
{

    int err;
    char *my_user;
    pl_strdup(&my_user, &call->local->uri.user);

    err = sipsess_accept(&call->sess, call->uac->sock, call->msg, 180, "Ringing",
                         my_user, "application/sdp", NULL,
                         NULL, call, false,
                         offer_handler, answer_handler,
                         establish_handler, NULL, NULL,
                         close_handler, call, NULL);
    if(err)
        call->cstate |= CSTATE_ERR;

    mem_deref(my_user);
}

void tcsipcall_control(struct tcsipcall*call, int action)
{

    int err;
    struct mbuf *mb = NULL;

    /*
     * XXX: drop bytes when confirmation received
     * */
    switch(action) {
    case CALL_ACCEPT:
	if(! TEST(call->cstate, CSTATE_IN_RING))
	    return;
	// 200
        err = tcmedia_offer(call->media, call->msg->mb, &mb);
	if(err) {
	    DROP(call->cstate, CSTATE_RING);
            err |= CSTATE_ERR;
	    break;
	}

        /*
         * Workarround
         *
         * libre uses msg->dst to fill in Contact header
         * value.
         * This works well for UDP where one socket used
         * for both send and recieve, but no for TCP
         * TCP transport uses one socket for listen
         * and other to connect to registar.
         * Address msg->dst is the recieving part
         * of upstream socket UAC->REGISTAR
         * and cannot be used to connect from outside
         *
         * */
        sa_set_port((struct sa*)&call->msg->dst, sa_port(&call->uac->laddr));

        sipsess_answer(call->sess, 200, "OK", mb, NULL);

        DROP(call->cstate, CSTATE_RING);
	call->cstate |= CSTATE_EST;

	break;

    case CALL_REJECT:
    case CALL_BYE:
	if( TEST(call->cstate, CSTATE_IN_RING) ) {
            sipsess_reject(call->sess, 486, "Reject", NULL);
	    DROP(call->cstate, CSTATE_IN_RING);
            call->reason = CEND_HANG;
	}

	if( TEST(call->cstate, CSTATE_EST) ) {
	    // bye sent automatically in deref
	    DROP(call->cstate, CSTATE_EST);
            call->reason = CEND_OK;
	}

	if( TEST(call->cstate, CSTATE_OUT_RING ) ) {
	    // cancel
	    DROP(call->cstate, CSTATE_EST);
            call->reason = CEND_CANCEL;
	}

	tcsipcall_hangup(call);
        break;

   }

}

void tcsipcall_send(struct tcsipcall*call)
{
    struct mbuf *mb;
    int err;

    char *to_uri, *to_user;
    char *from_uri, *from_name;

    pl_strdup(&to_uri, &call->remote->auri);
    pl_strdup(&to_user, &call->remote->uri.user);

    pl_strdup(&from_uri, &call->local->auri);
    if(call->local->dname.l)
        pl_strdup(&from_name, &call->local->dname);
    else
	pl_strdup(&from_name, &call->local->uri.user);

    char date[100];
    struct tm *tv;
    tv = gmtime(&call->ts);

    strftime(date, sizeof(date), "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", tv);
    tcmedia_get_offer(call->media, &mb);

    err = sipsess_connect(&call->sess, call->uac->sock, to_uri, from_name,
                          from_uri, to_user,
                          NULL, 0, "application/sdp", mb,
                          NULL, call, false,
                          offer_handler, answer_handler,
                          progress_handler, establish_handler,
                          NULL, NULL, close_handler, call, date);
    mem_deref(mb); /* free SDP buffer */

    if(err)
	call->cstate |= CSTATE_ERR;
    else
        call->cstate |= CSTATE_OUT_RING;

    mem_deref(to_uri);
    mem_deref(to_user);
    mem_deref(from_uri);
    mem_deref(from_name);

}

static void iceok(struct tcmedia* media, void*arg)
{
    struct tcsipcall*call = arg;
    tcsipcall_send(call);
}

void tcsipcall_waitice(struct tcsipcall*call)
{
     tcmedia_iceok(call->media, iceok, call);
}

void tcsipcall_dirs(struct tcsipcall*call, int *dir, int *state, int *reason, int *ts)
{
    if(dir) *dir = call->cdir;
    if(state) *state = call->cstate;
    if(reason) *reason = call->reason;
    if(ts) *ts = (int)call->ts;
}

void tcsipcall_ckey(struct tcsipcall*call, char **ckey)
{
    *ckey = (void*)call->ckey.p;
}

