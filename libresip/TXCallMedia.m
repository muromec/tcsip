//
//  TXCallMedia.m
//  Texr
//
//  Created by Ilya Petrov on 9/28/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXCallMedia.h"
#import "TXRTP.h"

@implementation TXCallMedia
- (id) initWithLaddr: (struct sa*)pLaddr {

    self = [super init];
    if(!self) return self;

    laddr = pLaddr;
    [self setup];

    return self;
}

- (void) setup {

    int err, port;
    media = NULL;

    enc_state = speex_encoder_init(&speex_nb_mode);
    dec_state = speex_decoder_init(&speex_nb_mode);
    speex_bits_init(&enc_bits);
    speex_bits_init(&dec_bits);

    read_off = 0;
    write_off = 0;

    speex_decoder_ctl(dec_state, SPEEX_GET_FRAME_SIZE, &frame_size); 
    //speex_jitter_init(&jitter, dec_state, 800);
    frame_size *= 2;

    /* create SDP session */
    err = sdp_session_alloc(&sdp, laddr);

    port = rand_u16();
    port |= 0x400;

    err = sdp_media_add(&sdp_media, sdp, "audio", port, "RTP/AVP");

    err = sdp_format_add(NULL, sdp_media, true, 
	"97", "speex", 8000, 1,
	 NULL, NULL, NULL, false, NULL);

    // dump local
    const struct sdp_format *fmt;

    fmt = sdp_media_lformat(sdp_media, 97);
    if (!fmt) {
        re_printf("no local media format found\n");
        return;
    }

    re_printf("local format: %s/%u/%u (payload type: %u)\n",
		  fmt->name, fmt->srate, fmt->ch, fmt->pt);


    rtp = [[TXRTP alloc] initWithCaller:self];
    [rtp listen: @"0.0.0.0" port:port];
}

- (void) stop {
    if(media) {
        media_snd_stream_stop(media);
	media_snd_stream_close(media);
	media = NULL;
    }

    if(rtp) {
        [rtp stopSocket];
	rtp = NULL;
    }

    if(dec_state) {
	speex_decoder_destroy(dec_state);
	dec_state = NULL;
    }

    if(enc_state) {
	speex_encoder_destroy(enc_state);
	enc_state = NULL;
    }

    speex_bits_reset(&enc_bits);
    speex_bits_reset(&dec_bits);

    speex_bits_destroy(&enc_bits);
    speex_bits_destroy(&dec_bits);
}

- (void) open {

    // XXX: use audio, video, chat flags
    int ok = media_snd_open(DIR_BI, 8000, O_LIM, &media);
    if(ok!=0) {
        NSLog(@"media faled to open %d", ok);
        return;
    }

}

- (bool) start {
    int ok;
    [rtp runLoop];

    if(!media)
        [self open];

    if(!media)
        return -1;

    ok = media_snd_stream_start(media);

    return (bool)ok;
}

- (void) rtpData: (char *)data len:(int)len ts:(unsigned int) ts
{

    speex_bits_read_from(&dec_bits, data, len);
    speex_decode_int(dec_state, &dec_bits, (spx_int16_t*)(media->render_ring + write_off));
    write_off += frame_size;

    if(write_off >= O_LIM)
	    write_off = 0;


}

- (int) rtpInput: (char *)data
{

    if(media->record_ring_fill < frame_size)
        return 0;

    int len;
    speex_encode_int(enc_state, (spx_int16_t*)(media->record_ring + read_off), &enc_bits);
    read_off += frame_size;

    if(read_off >= O_LIM)
        read_off = 0;

    len = speex_bits_write(&enc_bits, data, 200); // XXX: constant
    media->record_ring_fill -= frame_size; // err threadsafe damn!

    return len;
}

- (int) offer: (struct mbuf*)offer ret:(struct mbuf **)ret {

    int err;

    /*
     * Workarround here
     *
     * for some reason libre sets mb->end to the
     * offset, pointing to payload start.
     *
     * Settind mb->end to mb->size breaks connection buffer,
     * so we just copy payload to separate mb
     *
     * */
    struct mbuf *mb = mbuf_alloc(0);
    mbuf_write_mem(mb, mbuf_buf(offer), mbuf_get_space(offer));
    mb->pos = 0;

    NSLog(@"processing offer in call media");
    err = sdp_decode(sdp, mb, true);
    if(err) {
        NSLog(@"cant decode offer %d\n", err);
        goto out;
    }

    [self sdpFormats];

    err = sdp_encode(ret, sdp, false);
out:
    mem_deref(mb);
    return err;
}
- (int) offer: (struct mbuf **)ret {
    return sdp_encode(ret, sdp, true); 
}

- (int) answer: (struct mbuf*)offer {
    int err;
    NSLog(@"processing answer");

    struct mbuf *mb = mbuf_alloc(0);
    mbuf_write_mem(mb, mbuf_buf(offer), mbuf_get_space(offer));
    mb->pos = 0;

    err = sdp_decode(sdp, mb, true);
    if(err) {
        NSLog(@"cant decode answer");
        goto out;
    }

    [self sdpFormats];

out:
    mem_deref(mb);
    return err;
}

- (void) sdpFormats {

    const struct sdp_format *fmt;

    fmt = sdp_media_rformat(sdp_media, NULL);
    if (!fmt) {
        re_printf("no common media format found\n");
        return;
    }

    re_printf("SDP peer address: %J\n", sdp_media_raddr(sdp_media));

    re_printf("SDP media format: %s/%u/%u (payload type: %u)\n",
		  fmt->name, fmt->srate, fmt->ch, fmt->pt);
}

@end
