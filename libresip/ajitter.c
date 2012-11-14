#include "ajitter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//#define DBG(x) (printf x);
#define DBG(x) {}

#define min(a,b) ({ \
    typeof(a) _a_temp_; \
    typeof(b) _b_temp_; \
    _a_temp_ = (a); \
    _b_temp_ = (b); \
    _a_temp_ = _a_temp_ < _b_temp_ ? _a_temp_ : _b_temp_; \
    })


ajitter * ajitter_init(int chunk_size)
{

	ajitter *aj = malloc(sizeof(ajitter));
	if(!aj)
		return 0;

	aj->buffer = malloc((chunk_size + sizeof(ajitter_packet)) * AJD);
	if(!aj->buffer)
		goto err;

	aj->out_buffer = malloc(1024); // XXX! pass arg
	if(!aj->out_buffer)
		goto err2;

	aj->used = 0;
	aj->last = 0;
	aj->csize = chunk_size;
	aj->out_have = 0;
	memset(&aj->time[0], 0, sizeof(int) * AJD);

	ajitter_packet *pkt;
	int idx;
	char *off = aj->buffer;
	off += sizeof(ajitter_packet) * AJD;
	pkt = (ajitter_packet*)aj->buffer;
	for(idx=0; idx<AJD; idx++) {
		pkt->idx = idx;
		pkt->left = 0;
		pkt->off = 0;
		pkt->data = off + (chunk_size*idx);
		pkt ++;
	}

	return aj;
err2:
	free(aj->buffer);
err:
	free(aj);
	return 0;
}

ajitter_packet * ajitter_put_ptr(ajitter *aj) {
	int idx, found = 0;
	ajitter_packet *ret;
	for(idx=0; idx<AJD; idx++) {
		if((aj->used & (1<<idx))==0) {
			found = 1;
			break;
		}
	}

	if(!found) {
		idx = 0;
		aj->used = 1;
		aj->last = 0;
	}

	ret = (ajitter_packet*)(aj->buffer + (idx * sizeof(ajitter_packet)));

	DBG(("jitter put idx %d\n", idx));

	return ret;
}

void ajitter_put_done(ajitter *aj, int idx, double time) {
	aj->time[idx] = time;
	aj->last = time;
	aj->used |= (1<<idx);
	DBG(("jitter put done %d %f\n", idx, time));

}

void ajitter_get_done(ajitter *aj, int idx) {
	aj->used &= aj->used ^ (1<<idx);
	DBG(("jitter gett done %d\n", idx));
}

ajitter_packet *ajitter_get_ptr(ajitter *aj) {

	int idx, min_idx = -1;
	double min_time = aj->last;
	ajitter_packet *ret;

	if(!aj->used)
		return 0;

	for(idx=0; idx<AJD; idx++) {
		if((aj->used & (1<<idx))==0) {
			continue;
		}
		if(aj->time[idx] < min_time) {
			min_time = aj->time[idx];
			min_idx = idx;
		}
	}
	if(min_idx < 0)
		return 0;

	ret = (ajitter_packet*)(aj->buffer + (min_idx * sizeof(ajitter_packet)));
	DBG(("jitter get idx %d %x\n", min_idx, ret->left));

	return ret;
}

char *ajitter_get_chunk(ajitter *aj, int size)
{
	int need, get_now;
	ajitter_packet *ajp;

	ajp = ajitter_get_ptr(aj);
	if(!ajp)
		return 0;

	need = size - aj->out_have;
	get_now = min(need, ajp->left);
	memcpy(aj->out_buffer + aj->out_have, ajp->data + ajp->off, get_now);
	ajp->left -= get_now;
	ajp->off += get_now;
	if(ajp->left < 1)
		ajitter_get_done(aj, ajp->idx);

	aj->out_have += get_now;
	DBG(("now have %d this time got %d from idx %d\n",
	    aj->out_have, get_now, ajp->idx));
	if(aj->out_have < size)
		return 0;

	aj->out_have = 0;
	return aj->out_buffer;
}

void ajitter_destroy(ajitter *aj)
{
	free(aj->out_buffer);
	free(aj->buffer);
	free(aj);
}
