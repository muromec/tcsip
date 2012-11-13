#include "ajitter.h"
#include <stdlib.h>
#include <string.h>

//#define DBG(x) (printf x);
#define DBG(x) {}


ajitter * ajitter_init(int chunk_size)
{

	ajitter *aj = malloc(sizeof(ajitter));
	if(!aj)
		return 0;

	chunk_size += sizeof(int) * 3;
	aj->buffer = malloc(chunk_size * AJD);
	if(!aj->buffer)
		goto err;

	memset(aj->buffer, 0xA3, chunk_size * AJD);
	aj->out_buffer = malloc(chunk_size * 4); // XXX! pass arg
	if(!aj->out_buffer)
		goto err2;

	aj->used = 0;
	aj->last = 0;
	aj->csize = chunk_size;
	aj->out_have = 0;
	memset(&aj->time[0], 0, sizeof(int) * AJD);

	ajitter_packet *pkt;
	int idx;
	for(idx=0; idx<AJD; idx++) {
		pkt = (ajitter_packet*)(aj->buffer + (idx * aj->csize));
		pkt->idx = idx;
		pkt->left = 0;
		pkt->off = 0;
		pkt->data = (char*)pkt;
		pkt->data += sizeof(int) * 3;
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

	ret = (ajitter_packet*)(aj->buffer + (idx * aj->csize));

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

	int idx, min_idx;
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

	ret = (ajitter_packet*)(aj->buffer + (min_idx * aj->csize));
	DBG(("jitter get idx %d %x\n", min_idx, ret->left));

	return ret;
}

void ajitter_destroy(ajitter *aj)
{
	free(aj->out_buffer);
	free(aj->buffer);
	free(aj);
}
