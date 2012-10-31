//
//  TXSipCall.h
//  Texr
//
//  Created by Ilya Petrov on 8/27/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "TXBaseOp.h"

typedef struct {
    int magic;
    int usable;
    int off;
    char *data;
    void *next;
    int seq;
} frame_t;

typedef enum {
    CSTATE_STOP=0,
    CSTATE_IN_RING=1,
    CSTATE_OUT_RING=(1<<1),
    CSTATE_RING=(CSTATE_IN_RING|CSTATE_OUT_RING),
    CSTATE_FLOW=(1<<2),
    CSTATE_MEDIA=(1<<3),
    CSTATE_EST=(1<<4),
    CSTATE_ERR=(1<<5),
    CSTATE_ALIVE=(CSTATE_EST|CSTATE_RING),
} call_state_t;

typedef enum {
    CEND_OK=1,
    CEND_CANCEL,
    CEND_HANG,
} call_end_t;

typedef enum {
    CALL_ACCEPT,
    CALL_REJECT,
    CALL_BYE,
}
call_action_t;

typedef enum {
    CALL_IN,
    CALL_OUT,
} call_dir_t;

// bitmath helpers
#define DROP(x, bit) x&=x^bit
#define TEST(x, bit) ((x&(bit))==(bit))

@class TXCallMedia;

@protocol Call
- (oneway void) control:(call_action_t)action;

@property (readonly)NSInteger cid;
@property (readonly)TXSipUser* dest;
@property (readonly)call_end_t end_reason;
@property (readonly)call_dir_t cdir;
@end

@interface TXSipCall : TXBaseOp<Call> {
    TXCallMedia *media;
    call_state_t cstate;
    call_end_t end_reason;
    call_dir_t cdir;
    const struct sip_msg *msg;

    // XXX: move out speex-specific code
    	
    struct sdp_session *sdp;    
    struct sipsess *sess;
}

- (id) initIncoming: (const struct sip_msg *)pMsg app:(id)pApp;

- (void) send;
- (void) hangup;
- (void) control:(int)action;

- (void) callActivate;
- (NSInteger) cid;

@property (readonly) call_state_t cstate;
@property (readonly) id media;

@end
