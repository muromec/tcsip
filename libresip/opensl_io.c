/*
opensl_io.c:
Android OpenSL input/output module
Copyright (c) 2012, Victor Lazzarini
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "re.h"
#include "ajitter.h"

#include "opensl_io.h"
#define CONV16BIT 32768
#define CONVMYFLT (1./32768.)

#include <android/log.h>

#define LOG_TAG "audio"
#define LOGI(x...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,x)

static void* createThreadLock(void);
static int waitThreadLock(void *lock);
static void notifyThreadLock(void *lock);
static void destroyThreadLock(void *lock);
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
static void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

void sles_write(void *arg);

#define BUFFERFRAMES 512
#define REC_CH BUFFERFRAMES
#define VECSAMPS_MONO 64
#define VECSAMPS_STEREO 128
#define SR 8000

// creates the OpenSL ES audio engine
static SLresult openSLCreateEngine(OPENSL_STREAM *p)
{
  SLresult result;
  // create engine
  result = slCreateEngine(&(p->engineObject), 0, NULL, 0, NULL, NULL);
  if(result != SL_RESULT_SUCCESS) goto  engine_end;

  // realize the engine 
  result = (*p->engineObject)->Realize(p->engineObject, SL_BOOLEAN_FALSE);
  if(result != SL_RESULT_SUCCESS) goto engine_end;

  // get the engine interface, which is needed in order to create other objects
  result = (*p->engineObject)->GetInterface(p->engineObject, SL_IID_ENGINE, &(p->engineEngine));
  if(result != SL_RESULT_SUCCESS) goto  engine_end;

 engine_end:
  return result;
}

// opens the OpenSL ES device for output
static SLresult openSLPlayOpen(OPENSL_STREAM *p)
{
  SLresult result;
  SLuint32 sr = p->sr;
  SLuint32  channels = p->outchannels;

  if(channels){
    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

    switch(sr){

    case 8000:
      sr = SL_SAMPLINGRATE_8;
      break;
    case 11025:
      sr = SL_SAMPLINGRATE_11_025;
      break;
    case 16000:
      sr = SL_SAMPLINGRATE_16;
      break;
    case 22050:
      sr = SL_SAMPLINGRATE_22_05;
      break;
    case 24000:
      sr = SL_SAMPLINGRATE_24;
      break;
    case 32000:
      sr = SL_SAMPLINGRATE_32;
      break;
    case 44100:
      sr = SL_SAMPLINGRATE_44_1;
      break;
    case 48000:
      sr = SL_SAMPLINGRATE_48;
      break;
    case 64000:
      sr = SL_SAMPLINGRATE_64;
      break;
    case 88200:
      sr = SL_SAMPLINGRATE_88_2;
      break;
    case 96000:
      sr = SL_SAMPLINGRATE_96;
      break;
    case 192000:
      sr = SL_SAMPLINGRATE_192;
      break;
    default:
      return -1;
    }
   
    const SLInterfaceID ids[] = {SL_IID_VOLUME};
    const SLboolean req[] = {SL_BOOLEAN_FALSE};
    result = (*p->engineEngine)->CreateOutputMix(p->engineEngine, &(p->outputMixObject), 1, ids, req);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // realize the output mix
    result = (*p->outputMixObject)->Realize(p->outputMixObject, SL_BOOLEAN_FALSE);
   
    int speakers;
    if(channels > 1) 
      speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    else speakers = SL_SPEAKER_FRONT_CENTER;
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,channels, sr,
				   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
				   speakers, SL_BYTEORDER_LITTLEENDIAN};

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, p->outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    // create audio player
    const SLInterfaceID ids1[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req1[] = {SL_BOOLEAN_TRUE};
    result = (*p->engineEngine)->CreateAudioPlayer(p->engineEngine, &(p->bqPlayerObject), &audioSrc, &audioSnk,
						   1, ids1, req1);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // realize the player
    result = (*p->bqPlayerObject)->Realize(p->bqPlayerObject, SL_BOOLEAN_FALSE);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // get the play interface
    result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_PLAY, &(p->bqPlayerPlay));
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // get the buffer queue interface
    result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
						&(p->bqPlayerBufferQueue));
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // register callback on the buffer queue
    result = (*p->bqPlayerBufferQueue)->RegisterCallback(p->bqPlayerBufferQueue, bqPlayerCallback, p);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // set the player's state to playing
    result = (*p->bqPlayerPlay)->SetPlayState(p->bqPlayerPlay, SL_PLAYSTATE_PLAYING);
 
  end_openaudio:
    return result;
  }
  return SL_RESULT_SUCCESS;
}

// Open the OpenSL ES device for input
static SLresult openSLRecOpen(OPENSL_STREAM *p){

  SLresult result;
  SLuint32 sr = p->sr;
  SLuint32 channels = p->inchannels;

  if(channels){

    switch(sr){

    case 8000:
      sr = SL_SAMPLINGRATE_8;
      break;
    case 11025:
      sr = SL_SAMPLINGRATE_11_025;
      break;
    case 16000:
      sr = SL_SAMPLINGRATE_16;
      break;
    case 22050:
      sr = SL_SAMPLINGRATE_22_05;
      break;
    case 24000:
      sr = SL_SAMPLINGRATE_24;
      break;
    case 32000:
      sr = SL_SAMPLINGRATE_32;
      break;
    case 44100:
      sr = SL_SAMPLINGRATE_44_1;
      break;
    case 48000:
      sr = SL_SAMPLINGRATE_48;
      break;
    case 64000:
      sr = SL_SAMPLINGRATE_64;
      break;
    case 88200:
      sr = SL_SAMPLINGRATE_88_2;
      break;
    case 96000:
      sr = SL_SAMPLINGRATE_96;
      break;
    case 192000:
      sr = SL_SAMPLINGRATE_192;
      break;
    default:
      return -1;
    }
    
    // configure audio source
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
				      SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
    SLDataSource audioSrc = {&loc_dev, NULL};

    // configure audio sink
    int speakers;
    if(channels > 1) 
      speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    else speakers = SL_SPEAKER_FRONT_CENTER;
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, channels, sr,
				   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
				   speakers, SL_BYTEORDER_LITTLEENDIAN};
    SLDataSink audioSnk = {&loc_bq, &format_pcm};

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    result = (*p->engineEngine)->CreateAudioRecorder(p->engineEngine, &(p->recorderObject), &audioSrc,
						     &audioSnk, 1, id, req);
    if (SL_RESULT_SUCCESS != result) goto end_recopen;

    // realize the audio recorder
    result = (*p->recorderObject)->Realize(p->recorderObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) goto end_recopen;

    // get the record interface
    result = (*p->recorderObject)->GetInterface(p->recorderObject, SL_IID_RECORD, &(p->recorderRecord));
    if (SL_RESULT_SUCCESS != result) goto end_recopen;
 
    // get the buffer queue interface
    result = (*p->recorderObject)->GetInterface(p->recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
						&(p->recorderBufferQueue));
    if (SL_RESULT_SUCCESS != result) goto end_recopen;

    // register callback on the buffer queue
    result = (*p->recorderBufferQueue)->RegisterCallback(p->recorderBufferQueue, bqRecorderCallback,
							 p);
    if (SL_RESULT_SUCCESS != result) goto end_recopen;
    result = (*p->recorderRecord)->SetRecordState(p->recorderRecord, SL_RECORDSTATE_RECORDING);

  end_recopen: 
    return result;
  }
  else return SL_RESULT_SUCCESS;


}

// close the OpenSL IO and destroy the audio engine
static void openSLDestroyEngine(OPENSL_STREAM *p){

  // destroy buffer queue audio player object, and invalidate all associated interfaces
  if (p->bqPlayerObject != NULL) {
    (*p->bqPlayerObject)->Destroy(p->bqPlayerObject);
    p->bqPlayerObject = NULL;
    p->bqPlayerPlay = NULL;
    p->bqPlayerBufferQueue = NULL;
    p->bqPlayerEffectSend = NULL;
  }

  // destroy audio recorder object, and invalidate all associated interfaces
  if (p->recorderObject != NULL) {
    (*p->recorderObject)->Destroy(p->recorderObject);
    p->recorderObject = NULL;
    p->recorderRecord = NULL;
    p->recorderBufferQueue = NULL;
  }

  // destroy output mix object, and invalidate all associated interfaces
  if (p->outputMixObject != NULL) {
    (*p->outputMixObject)->Destroy(p->outputMixObject);
    p->outputMixObject = NULL;
  }

  // destroy engine object, and invalidate all associated interfaces
  if (p->engineObject != NULL) {
    (*p->engineObject)->Destroy(p->engineObject);
    p->engineObject = NULL;
    p->engineEngine = NULL;
  }

}


// open the android audio device for input and/or output
OPENSL_STREAM *android_OpenAudioDevice(int sr, int inchannels, int outchannels, int bufferframes){
  
  OPENSL_STREAM *p;
  p = (OPENSL_STREAM *) calloc(sizeof(OPENSL_STREAM),1);

  p->inchannels = inchannels;
  p->outchannels = outchannels;
  p->sr = sr;

  if(openSLCreateEngine(p) != SL_RESULT_SUCCESS) {
    android_CloseAudioDevice(p);
    return NULL;
  }

  if(openSLRecOpen(p) != SL_RESULT_SUCCESS) {
    android_CloseAudioDevice(p);
    return NULL;
  } 

  if(openSLPlayOpen(p) != SL_RESULT_SUCCESS) {
    android_CloseAudioDevice(p);
    return NULL;
  }  

  return p;
}

// close the android audio device
void android_CloseAudioDevice(OPENSL_STREAM *p){

  if (p == NULL)
    return;

  openSLDestroyEngine(p);

  free(p);
}

// this callback handler is called every time a buffer finishes recording
void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
  struct timeval now;
  OPENSL_STREAM *p = (OPENSL_STREAM *) context;
  struct opensl_sound *snd = p->ctx;
  ajitter_packet *ajp = p->half_read;

  if(!snd)
    return;

  ajp->left = REC_CH;
  ajp->off = 0;

  gettimeofday(&now, NULL);

  ajitter_put_done(snd->record_jitter, ajp->idx, now.tv_sec);

  android_AudioIn(p);

}
 
// gets a buffer of size samples from the device
int android_AudioIn(OPENSL_STREAM *p) {

    struct opensl_sound *snd = p->ctx;

    ajitter_packet *ajp = ajitter_put_ptr(snd->record_jitter);
    p->half_read = ajp;

    (*p->recorderBufferQueue)->Enqueue(p->recorderBufferQueue,
            ajp->data, REC_CH);
  
    return 0;
}

// this callback handler is called every time a buffer finishes playing
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
  OPENSL_STREAM *p = (OPENSL_STREAM *) context;
  if(!p->ctx) {
      goto stop;
  }

  sles_write(p->ctx);
  return;

stop:
  android_CloseAudioDevice(p);
}

// puts a buffer of size samples to the device
int android_AudioOut(OPENSL_STREAM *p, char *buffer,int size){

  (*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue, buffer, size);

  return size;
}

//----------------------------------------------------------------------
// thread Locks
// to ensure synchronisation between callbacks and processing code
void* createThreadLock(void)
{
  threadLock  *p;
  p = (threadLock*) malloc(sizeof(threadLock));
  if (p == NULL)
    return NULL;
  memset(p, 0, sizeof(threadLock));
  if (pthread_mutex_init(&(p->m), (pthread_mutexattr_t*) NULL) != 0) {
    free((void*) p);
    return NULL;
  }
  if (pthread_cond_init(&(p->c), (pthread_condattr_t*) NULL) != 0) {
    pthread_mutex_destroy(&(p->m));
    free((void*) p);
    return NULL;
  }
  p->s = (unsigned char) 1;

  return p;
}

int waitThreadLock(void *lock)
{
  threadLock  *p;
  int   retval = 0;
  p = (threadLock*) lock;
  pthread_mutex_lock(&(p->m));
  while (!p->s) {
    pthread_cond_wait(&(p->c), &(p->m));
  }
  p->s = (unsigned char) 0;
  pthread_mutex_unlock(&(p->m));
}

void notifyThreadLock(void *lock)
{
  threadLock *p;
  p = (threadLock*) lock;
  pthread_mutex_lock(&(p->m));
  p->s = (unsigned char) 1;
  pthread_cond_signal(&(p->c));
  pthread_mutex_unlock(&(p->m));
}

void destroyThreadLock(void *lock)
{
  threadLock  *p;
  p = (threadLock*) lock;
  if (p == NULL)
    return;
  notifyThreadLock(p);
  pthread_cond_destroy(&(p->c));
  pthread_mutex_destroy(&(p->m));
  free(p);
}

static char silence[320];

void sles_write(void *arg)
{
    struct opensl_sound *snd = arg;
    OPENSL_STREAM *stream = snd->stream;
    struct ajitter *aj = snd->play_jitter;

    int avail, ret, ts, write;
    char *obuf;

    avail = 160;

    obuf = ajitter_get_chunk(aj, avail, &ts);

    if(obuf) {
        android_AudioOut(stream,obuf, avail);
    } else {
        android_AudioOut(stream, silence, sizeof(silence));
    }
}

int opensl_sound_open(struct opensl_sound**rp)
{
    struct opensl_sound *snd;
    OPENSL_STREAM *stream;
   
    snd = mem_zalloc(sizeof(struct opensl_sound), NULL);
    stream = android_OpenAudioDevice(SR,1,1,BUFFERFRAMES);
    LOGI("stream open %p snd %p", stream, snd);
    if(!stream) {
        fprintf(stderr, "opensl failed to open\n");
        goto err;
    }

    snd->play_jitter = ajitter_init(320); // speex frame
    snd->record_jitter = ajitter_init(BUFFERFRAMES*2);

    memset(silence, 0, sizeof(silence));


    stream->ctx = snd;
    snd->stream = stream;


    *rp = snd;
    return 0;

err:
    mem_deref(snd);
    return -1;
}

int opensl_sound_start(struct opensl_sound*snd){

    LOGI("opensl timer start %p", snd);

    sles_write(snd);
    android_AudioIn(snd->stream);

    return 0;
}

void opensl_sound_close(struct opensl_sound*snd){

    OPENSL_STREAM *stream = snd->stream;
    stream->ctx = NULL;

    mem_deref(snd->play_jitter);
    mem_deref(snd->record_jitter);
    mem_deref(snd);
}
