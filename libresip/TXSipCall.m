//
//  TXSipCall.m
//  Texr
//
//  Created by Ilya Petrov on 8/27/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//
#import "TXSip.h"
#import "TXSipCall.h"
#import "TXCallMedia.h"

#import "txsip_private.h"

static int offer_handler(struct mbuf **mbp, const struct sip_msg *msg,
			 void *arg) {
    NSLog(@"sdp offer");
    TXSipCall *call = (__bridge TXSipCall*)arg;
    return [call.media offer: msg->mb ret:mbp];
}

/* called when an SDP answer is received */
static int answer_handler(const struct sip_msg *msg, void *arg)
{
    NSLog(@"sdp answer");
    TXSipCall *call = (__bridge TXSipCall*)arg;

    [call.media answer: msg->mb];

    return 0;
}

/* called when SIP progress (like 180 Ringing) responses are received */
static void progress_handler(const struct sip_msg *msg, void *arg)
{
        TXSipCall *call = (__bridge TXSipCall*)arg;
	re_printf("session progress: %u %r\n", msg->scode, &msg->reason);
        if((msg->scode == 183) && mbuf_get_left(msg->mb)) {
            re_printf("early media");
            [call.media answer: msg->mb];
            [call callActivate];
        }
}


/* called when the session is established */
static void establish_handler(const struct sip_msg *msg, void *arg)
{
	(void)msg;
	(void)arg;

        re_printf("session established: %u %r\n", msg->scode, &msg->reason);

        id ctx = (__bridge id)arg;
        [ctx callActivate];
}

/* called when the session fails to connect or is terminated from peer */
static void close_handler(int err, const struct sip_msg *msg, void *arg)
{
        NSLog(@"session close handler");
        id ctx = (__bridge id)arg;
        [ctx hangup];
}

@implementation TXSipCall
@synthesize cstate;
@synthesize end_reason;
@synthesize cdir;
@synthesize media;
@synthesize date_start;
@synthesize date_end;
@synthesize date_create;

- (void) incoming:(const struct sip_msg *)pMsg
{

    [self setup: CALL_IN];
    cstate = CSTATE_IN_RING;
    msg = mem_ref((void*)pMsg);
    [self parseFrom];
    [self acceptSession];
}
 
- (void) parseFrom
{
    dest = [TXSipUser withAddr: (struct sip_taddr*)&msg->from];
}

- (void) acceptSession
{

    int err;
    const char *my_user = _byte(app.user.user);

    err = sipsess_accept(&sess, uac->sock, msg, 180, "Ringing",
                         my_user, "application/sdp", NULL,
                         auth_handler, ctx, false,
                         offer_handler, answer_handler,
                         establish_handler, NULL, NULL,
                         close_handler, ctx, NULL);
    NSLog(@"accept %d", err);
    if(err)
        cstate |= CSTATE_ERR;


}

- (void) outgoing:(TXSipUser*)pDest
{
    [self setDest: pDest];
    [self setup: CALL_OUT];
    cstate = CSTATE_STOP;
}

- (void) setup:(call_dir_t)pDir;
{
    media = [[TXCallMedia alloc] initWithUAC:uac dir:pDir];

    cdir = pDir;
    date_create = [NSDate date];
}

- (void) send
{
    struct mbuf *mb;
    int err;

    const char *to_uri = _byte(dest.addr);
    const char *to_user = _byte(dest.user);
    const char *from_uri = _byte(app.user.addr);
    const char *from_name = _byte(app.user.name);
    
    [media offer: &mb];

    err = sipsess_connect(&sess, uac->sock, to_uri, from_name,
                          from_uri, to_user,
                          NULL, 0, "application/sdp", mb,
                          auth_handler, ctx, false,
                          offer_handler, answer_handler,
                          progress_handler, establish_handler,
                          NULL, NULL, close_handler, ctx, NULL);
    mem_deref(mb); /* free SDP buffer */

    NSLog(@"invite sent %d", err);

    cstate |= CSTATE_OUT_RING;
    return;
}

- (void) hangup
{
    NSLog(@"handgup");
    mem_deref(sess);

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

}

- (void) upd {
    [cb response: self];
}

- (oneway void) control:(int)action;
{

    int err;
    struct mbuf *mb;

    /*
     * XXX: drop bytes when confirmation received
     * */
    switch(action) {
    case CALL_ACCEPT:
	if(! TEST(cstate, CSTATE_IN_RING))
	    return;
	// 200
        err = [media offer: msg->mb ret:&mb];

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

- (void) callActivate
{
    NSLog(@"activate call");
    date_start = [NSDate date];
    /*
     * Everything ok, now activate media subsystem
     * */

    if(TEST(cstate, CSTATE_FLOW|CSTATE_MEDIA)) {
	NSLog(@"rtp and media already started, wtf?");
	return;
    }

    /* XXX: move out */
    DROP(cstate, CSTATE_RING);
    cstate |= CSTATE_EST;

    int ret;
    ret = [media start];
    if(ret) {
        cstate |= CSTATE_ERR;
        NSLog(@"media failed to start");
        goto out;
    }
    // XXX: assert rtp not null
    cstate |= CSTATE_FLOW;
    cstate |= CSTATE_MEDIA;

out:
    [self upd];
}

- (NSInteger) cid
{
   return (NSInteger)(self);
}

- (NSString*) ckey
{
    NSString *ret = [NSString stringWithFormat:
	@"%f@%@->%@",
       [date_create timeIntervalSince1970],
       cdir ? app.user.addr : dest.addr,
       cdir ? dest.addr : app.user.addr
    ];
    NSLog(@"key: %@", ret);

    return ret;
}

@end
