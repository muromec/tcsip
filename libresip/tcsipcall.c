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
    struct pl ckey;
    int cdir;
    int cstate;
    int reason;
    time_t ts;
};

void tcsipcall_activate(struct tcsipcall*call);
void tcsipcall_hangup(struct tcsipcall*call);

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


int tcsipcall_alloc(struct tcsipcall**rp, struct uac *uac)
{
    int err;
    struct tcsipcall *call;
    
    call = mem_zalloc(sizeof(struct tcsipcall), call_destruct);
    if(!call) {
	err = -ENOMEM;
	goto fail;
    }
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
    struct pl tmp;
    struct timeval now;

    pl_set_str(&tmp, "123");
    pl_dup(&call->ckey, &tmp);

    call->cstate = CSTATE_STOP;
    call->cdir = CALL_OUT;
    gettimeofday(&now, NULL);
    call->ts = now.tv_sec;
}

void tcsipcall_handler(struct tcsipcall*call, tcsipcall_h ch, void*arg)
{
}

void tcsipcall_activate(struct tcsipcall*call) {
    if(TEST(cstate, CSTATE_FLOW|CSTATE_MEDIA)) {
	D(@"rtp and media already started, wtf?");
	return;
    }

    /* XXX: move out */
    DROP(cstate, CSTATE_RING);
    cstate |= CSTATE_EST;

    int ret;
    ret = [media start];
    if(ret) {
        cstate |= CSTATE_ERR;
        D(@"media failed to start");
        goto out;
    }
    // XXX: assert rtp not null
    cstate |= CSTATE_FLOW;
    cstate |= CSTATE_MEDIA;

out:
    [self upd];

}

void tcsipcall_hangup(struct tcsipcall*call) {
    D(@"handgup");
    if(sess)
        sess = mem_deref(sess);

    if(date_start)
        date_end = [NSDate date];

    if(!end_reason && (cstate & CSTATE_EST)==CSTATE_EST)
        end_reason = CEND_OK;

    if(!end_reason && (cstate & CSTATE_EST)==0)
        end_reason = CEND_HANG;

    DROP(cstate, CSTATE_ALIVE);
    DROP(cstate, CSTATE_EST);
    /*
     * Call terminated
     * Remote party dropped something heavy
     * on red button
     * */

    [media stop];
    media = nil;

    /*
     * TODO: free all frames
     * TODO: move frames to call context
     * */
    [self upd];
    cb = nil;

}
void tcsipcall_accept(struct tcsipcall*call)
{

    int err;
    char *my_user;
    pl_strdup(&my_user, &local->uri.user);

    err = sipsess_accept(&sess, uac->sock, msg, 180, "Ringing",
                         my_user, "application/sdp", NULL,
                         NULL, ctx, false,
                         offer_handler, answer_handler,
                         establish_handler, NULL, NULL,
                         close_handler, ctx, NULL);
    if(err)
        cstate |= CSTATE_ERR;

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
	if(! TEST(cstate, CSTATE_IN_RING))
	    return;
	// 200
        err = [media offer: msg->mb ret:&mb];
	if(err) {
	    DROP(cstate, CSTATE_RING);
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
        sa_set_port((struct sa*)&msg->dst, sa_port(&uac->laddr));

        sipsess_answer(sess, 200, "OK", mb, NULL);

        DROP(cstate, CSTATE_RING);
	cstate |= CSTATE_EST;

	break;

    case CALL_REJECT:
    case CALL_BYE:
	if( TEST(cstate, CSTATE_IN_RING) ) {
            sipsess_reject(sess, 486, "Reject", NULL);
	    DROP(cstate, CSTATE_IN_RING);
            end_reason = CEND_HANG;
	}

	if( TEST(cstate, CSTATE_EST) ) {
	    // bye sent automatically in deref
	    DROP(cstate, CSTATE_EST);
            end_reason = CEND_OK;
	}

	if( TEST(cstate, CSTATE_OUT_RING ) ) {
	    // cancel
	    DROP(cstate, CSTATE_EST);
            end_reason = CEND_CANCEL;
	}

        [self hangup];
        break;

    default:
	NSLog(@"broked call control a:%d cs:%d", action, cstate);
   }

}

void tcsipcall_send(struct tcsipcall*call)
{
    struct mbuf *mb;
    int err;

    char *to_uri, *to_user;
    char *from_uri, *from_name;
    char *fmt = NULL;

    pl_strdup(&to_uri, &call->remote->auri);
    pl_strdup(&to_user, &call->remote->uri.user);

    pl_strdup(&from_uri, &call->local->auri);
    if(call->local->dname.l)
        pl_strdup(&from_name, &call->local->dname);
    else
	pl_strdup(&from_name, &call->local->uri.user);

    mb = mbuf_alloc(100);
    /*
    [media offer: &mb];

    NSString* nfmt = [NSString stringWithFormat:
            @"Date: %@\r\n",
	    [http_df stringFromDate:date_create]
    ];
    */

    err = sipsess_connect(&call->sess, call->uac->sock, to_uri, from_name,
                          from_uri, to_user,
                          NULL, 0, "application/sdp", mb,
                          NULL, call, false,
                          offer_handler, answer_handler,
                          progress_handler, establish_handler,
                          NULL, NULL, close_handler, call, fmt);
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

void tcsipcall_waitice(struct tcsipcall*call)
{
     tcsipcall_send(call);
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

