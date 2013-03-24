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
#include "strmacro.h"

#import "txsip_private.h"

static int offer_handler(struct mbuf **mbp, const struct sip_msg *msg,
			 void *arg) {
    D(@"sdp offer");
    TXSipCall *call = (__bridge TXSipCall*)arg;
    return [call.media offer: msg->mb ret:mbp];
}

/* called when an SDP answer is received */
static int answer_handler(const struct sip_msg *msg, void *arg)
{
    D(@"sdp answer");
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
        D(@"session close handler");
        id ctx = (__bridge id)arg;
        [ctx hangup];
}

static bool find_date(const struct sip_hdr *hdr, const struct sip_msg *msg,
			  void *arg)
{
	int *stamp = arg;
	NSString *hdr_str = _pstr(hdr->val);
	NSDate *cdate = [http_df dateFromString:hdr_str];
	*stamp = (int)[cdate timeIntervalSince1970];
	return true;
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
    [self parseDate];
}
 
- (void) parseFrom
{
    remote = mem_ref((void*)&msg->from);
}

- (void) parseDate
{
    int timestamp = 0;
    sip_msg_hdr_apply(msg, true, SIP_HDR_DATE, find_date, &timestamp);
    if(timestamp)
        date_create = [NSDate dateWithTimeIntervalSince1970: timestamp];
}

- (void) acceptSession
{
}

- (void) outgoing
{
    [self setup: CALL_OUT];
    cstate = CSTATE_STOP;
}

- (void) setup:(call_dir_t)pDir;
{
    media = [[TXCallMedia alloc] initWithUAC:uac dir:pDir];

    cdir = pDir;
    date_create = [NSDate date];
}

- (void)waitIce
{
    [media setIceCb: CB(self, iceReady)];
}

- (void)iceReady
{
    [self send];
    [self upd];
}

- (void) send
{
}

- (void) hangup
{
}

- (void) upd {
    [cb response: self];
}

- (oneway void) control:(int)action;
{
}

- (void) callActivate
{
}

- (NSInteger) cid
{
   return (NSInteger)(self);
}

- (NSString*) ckey
{

    char *tmp;
    int tstamp = (int)[date_create timeIntervalSince1970];

    re_sdprintf(&tmp, "%d@%r->%r", tstamp,
       cdir ? &local->auri: &remote->auri,
       cdir ? &remote->auri : &local->auri
    );

    NSString *ret = [NSString stringWithFormat:
	@"%s", tmp];
    D(@"key: %@", ret);

    mem_deref(tmp);

    return ret;
}

- (void)dealloc
{
    if(msg) mem_deref((void*)msg);
}

@end
