#ifndef TCSOUND_H
#define TCSOUND_H

struct tcsound;

typedef void(capture_h)(const uint16_t* buf, size_t sz, void* arg);

int tcsound_start(struct tcsound *snd);
int tcsound_alloc(struct tcsound **rp, capture_h cap_h, void *arg);

#endif
