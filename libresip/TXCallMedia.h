//
//  TXCallMedia.h
//  Texr
//
//  Created by Ilya Petrov on 9/28/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>

#include "re.h"

#import <srtp.h>
#include <speex/speex.h>

#include "sound.h"

// ring buffer limit
#define O_LIM (320*20)

typedef struct {
    struct tmr tmr;
    void *arg;
} timer_cb_t;

@interface TXCallMedia : NSObject {
    struct pjmedia_snd_stream *media;
    struct rtp_sock *rtp;
    timer_cb_t rtp_timer;

    void *enc_state;
    void *dec_state;
    int frame_size;
    SpeexBits enc_bits;
    SpeexBits dec_bits;

    srtp_t srtp_in;
    srtp_t srtp_out;
    char srtp_in_key[64];
    char srtp_out_key[64];

    int read_off;
    int write_off;

    struct sdp_session *sdp;
    struct sdp_media *sdp_media;

    struct sa *laddr;
    struct sa *dst;
    int pt;
    int ts;
}

- (id) initWithLaddr: (struct sa*)pLaddr;
- (int) offer: (struct mbuf*)offer ret:(struct mbuf **)ret;
- (int) offer: (struct mbuf **)ret;

- (int) answer: (struct mbuf*)offer;
- (bool) start;
- (void) stop;

- (void) rtpData: (struct mbuf*)mb header:(const struct rtp_header *)hdr;
- (int) rtpInput: (char *)data;

@end
