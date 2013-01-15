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
#include "rtp_io.h"

// ring buffer limit
#define O_LIM (320*12)

@interface TXCallMedia : NSObject {
    struct pjmedia_snd_stream *media;
    struct rtp_sock *rtp;
    struct tmr rtp_tmr;

    void *send_io_ctx;
    rtp_recv_arg recv_io_arg;

    srtp_t srtp_in;
    srtp_t srtp_out;
    unsigned char srtp_in_key[64];
    unsigned char srtp_out_key[64];

    struct sdp_session *sdp;
    struct sdp_media *sdp_media;
    struct sdp_media *sdp_media_s;

    struct sa *laddr;
    struct sa *dst;
    int pt;
    int fmt;
}

- (id) initWithLaddr: (struct sa*)pLaddr;
- (int) offer: (struct mbuf*)offer ret:(struct mbuf **)ret;
- (int) offer: (struct mbuf **)ret;

- (int) answer: (struct mbuf*)offer;
- (bool) start;
- (void) stop;

@end
