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
	(void)arg;

	re_printf("session progress: %u %r\n", msg->scode, &msg->reason);
}


/* called when the session is established */
static void establish_handler(const struct sip_msg *msg, void *arg)
{
	(void)msg;
	(void)arg;

        re_printf("session established: %u %r\n", msg->scode, &msg->reason);

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
@synthesize media;
- (id) initIncoming: (const struct sip_msg *)pMsg app:(id)pApp;
{
    self = [super initWithApp:pApp];

    // XXX: fill remote part

    cstate = CSTATE_IN_RING;
    msg = mem_ref((void*)pMsg);
    [self parseFrom];

    return self;
}

- (void) parseFrom
{
    dest = [TXSipUser withAddr: (struct sip_taddr*)&msg->from];
}

- (void) setup
{
    media = [[TXCallMedia alloc] initWithLaddr: &uac->laddr];

    cstate = CSTATE_STOP;
}

- (void) send
{
    struct mbuf *mb;
    int err;

    const char *to_uri = _byte(dest.addr);
    const char *to_name = _byte(dest.name);
    const char *from_uri = _byte(app.user.addr);
    const char *from_name = _byte(app.user.name);
    
    [media offer: &mb];

    err = sipsess_connect(&sess, uac->sock, to_uri, from_name,
                          from_uri, to_name,
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
    DROP(cstate, CSTATE_ALIVE);

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

    [cb response: self];
}

- (oneway void) control:(int)action;
{

    int err;
    struct mbuf *mb;
    const char *my_name = _byte(app.user.name);

    /*
     * XXX: drop bytes when confirmation received
     * */
    switch(action) {
    case CALL_ACCEPT:
	if(! TEST(cstate, CSTATE_IN_RING))
	    return;
	// 200
        err = [media offer: msg->mb ret:&mb];

	err = sipsess_accept(&sess, uac->sock, msg, 200, "OK",
			     my_name, "application/sdp", mb,
			     auth_handler, NULL, false,
			     offer_handler, answer_handler,
			     establish_handler, NULL, NULL,
			     close_handler, NULL, NULL);

        DROP(cstate, CSTATE_RING);
	cstate |= CSTATE_EST;

	break;

    case CALL_REJECT:
    case CALL_BYE:
	if( TEST(cstate, CSTATE_IN_RING) ) {
	    (void)sip_treply(NULL, uac->sip, msg, 486, "Busy Here");
	    DROP(cstate, CSTATE_IN_RING);
	}

	if( TEST(cstate, CSTATE_EST) ) {
	    // bye sent automatically in deref
	    mem_deref(sess);
	    DROP(cstate, CSTATE_EST);
	}

	if( TEST(cstate, CSTATE_OUT_RING ) ) {
	    // cancel
	    mem_deref(sess);
	    DROP(cstate, CSTATE_EST);
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

    bool ok;
    ok = [media start];
    if(!ok) {
        cstate |= CSTATE_ERR;
        NSLog(@"media failed to start");
        return;
    }
    // XXX: assert rtp not null
    cstate |= CSTATE_FLOW;
    cstate |= CSTATE_MEDIA;
}

- (NSInteger) cid
{
   return (NSInteger)(self);
}

@end
