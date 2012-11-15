/**
 * Created by Robbie Hanson of Voalte, Inc.
 * 
 * Project page:
 * http://code.google.com/p/pjsip-iphone-audio-driver
 * 
 * Mailing list:
 * http://groups.google.com/group/pjsip-iphone-audio-driver
 * 
 * Open sourced under a BSD style license:
 * 
 * Copyright (c) 2010, Voalte, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Neither the name of Voalte nor the names of its contributors may be used to endorse
 *   or promote products derived from this software without specific prior written permission.
 * 
 * This software is provided by the copyright holders and contributors "as is" and any express
 * or implied warranties, including, but not limited to, the implied warranties of merchantability
 * and fitness for a particular purpose are disclaimed. In no event shall the copyright holder or
 * contributors be liable for any direct, indirect, incidental, special, exemplary, or
 * consequential damages (including, but not limited to, procurement of substitute goods or
 * services; loss of use, data, or profits; or business interruption) however caused and on any
 * theory of liability, whether in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if advised of the
 * possibility of such damage.
**/

#import <CoreAudio/CoreAudioTypes.h>
#include <AudioToolbox/AudioServices.h>

#include "sound.h"
#include "sound_utils.h"

#define THIS_FILE "iphonesound.c"

#define MANAGE_AUDIO_SESSION  IPHONE

#define REC_CH 94

#define PJ_LOG(x, y) ({})

static AudioComponent component = NULL;

static Boolean poppingSoundWorkaround;

// Static pointer to the stream that gets created when the sound driver is open.
// The sole purpose of this pointer is to get access to the stream from within the audio session interruption callback.
static struct pjmedia_snd_stream *snd_strm_instance = NULL;

#if MANAGE_AUDIO_SESSION
  static bool audio_session_initialized = 0;
#endif

/**
 * Conditionally initializes the audio session.
 * Use this method for proper audio session management.
**/
static void initializeAudioSession()
{
#if MANAGE_AUDIO_SESSION
	
	if(!audio_session_initialized)
	{
		PJ_LOG(5, (THIS_FILE, "AudioSessionInitialize"));
		
		AudioSessionInitialize(NULL, kCFRunLoopDefaultMode,
		    (void(*)(void*,UInt32))session_int_cb, NULL);
		
		audio_session_initialized = 1;
	}

#endif
}

/**
 * Conditionally activates the audio session.
 * Use this method for proper audio session management.
**/
static void startAudioSession(media_dir_t dir)
{
#if IPHONE
	UInt32 sessionCategory;
	
	if(dir == DIR_CAP) {
		sessionCategory = kAudioSessionCategory_RecordAudio;
	}
	else if(dir == DIR_PLAY) {
		sessionCategory = kAudioSessionCategory_MediaPlayback;
	}
	else {
		sessionCategory = kAudioSessionCategory_PlayAndRecord;
	}
	
#if MANAGE_AUDIO_SESSION
	
	AudioSessionSetProperty(kAudioSessionProperty_AudioCategory,
	                        sizeof(sessionCategory), &sessionCategory);
	
	AudioSessionSetActive(true);
	
#endif
#endif
}

/**
 * Conditionally deactivates the audio session when it is no longer needed.
 * Use this method for proper audio session management.
**/
static void stopAudioSession()
{
#if MANAGE_AUDIO_SESSION
#if IPHONE
	AudioSessionSetActive(false);
#endif	
#endif
}


/**
 * Invoked when our audio session is interrupted, or uninterrupted.
**/
void session_int_cb(void *userData, uint32_t interruptionState)
{
#if MANAGE_AUDIO_SESSION

        printf("session interrupted\n");
	if(interruptionState == kAudioSessionBeginInterruption)
	{
		PJ_LOG(3, (THIS_FILE, "interruptionListenerCallback: kAudioSessionBeginInterruption"));
		
		if(snd_strm_instance && snd_strm_instance->isActive)
		{
			// Audio session has already been stopped at this point
			
			// Stop the audio unit
			AudioOutputUnitStop(snd_strm_instance->out_unit);
		}
	}
	else if(interruptionState == kAudioSessionEndInterruption)
	{
		PJ_LOG(3, (THIS_FILE, "interruptionListenerCallback: kAudioSessionEndInterruption"));
		
		if(snd_strm_instance && snd_strm_instance->isActive)
		{
			// Activate the audio session
			startAudioSession(snd_strm_instance->dir);
			
			// Start the audio unit
			AudioOutputUnitStart(snd_strm_instance->out_unit);
		}
	}
#endif
}

int fetch_ring(struct pjmedia_snd_stream *snd_strm, char *buffer, UInt32 want) {


    int have = snd_strm->render_ring_size - snd_strm->render_ring_off;

    if(want > have)
        want = have;

    memcpy(buffer, snd_strm->render_ring + snd_strm->render_ring_off, want);

    snd_strm->render_ring_off += want;
    if(snd_strm->render_ring_off >= snd_strm->render_ring_size)
        snd_strm->render_ring_off = 0;

    return want;
}

/**
 * Voice Unit Callback.
 * Called when the voice unit output needs us to input the data that it should play through the speakers.
 * 
 * Parameters:
 * inRefCon
 *    Custom data that you provided when registering your callback with the audio unit.
 * ioActionFlags
 *    Flags used to describe more about the context of this call.
 * inTimeStamp
 *    The timestamp associated with this call of audio unit render.
 * inBusNumber
 *    The bus number associated with this call of audio unit render.
 * inNumberFrames
 *    The number of sample frames that will be represented in the audio data in the provided ioData parameter.
 * ioData
 *    The AudioBufferList that will be used to contain the provided audio data.
 **/
static OSStatus MyOutputBusRenderCallack(void                       *inRefCon,
                                         AudioUnitRenderActionFlags *ioActionFlags,
                                         const AudioTimeStamp       *inTimeStamp,
                                         UInt32                      inBusNumber,
                                         UInt32                      inNumberFrames,
                                         AudioBufferList            *ioData)
{
	struct pjmedia_snd_stream *snd_strm = (struct pjmedia_snd_stream *)inRefCon;
	if(!snd_strm->render_ring) {
		printf("dead render\n");
		return -1;
	}
	
	char *audioBuffer = (char *)(ioData->mBuffers[0].mData);

	UInt32 audioBufferSize = ioData->mBuffers[0].mDataByteSize;
	UInt32 chunkSize = 0;
	UInt32 audioBufferOffset = 0;

#define fetch() ({\
        chunkSize = fetch_ring(snd_strm, audioBuffer + audioBufferOffset,\
		audioBufferSize\
	);\
	audioBufferOffset += chunkSize;\
	audioBufferSize -= chunkSize;\
})

	fetch();

	if(audioBufferOffset < audioBufferSize) {
		fetch();
	}

	return noErr;
}


/**
 * Voice Unit Callback.
 * 
 * Called when the voice unit input has recorded data that we can fetch from it.
 * 
 * Parameters:
 * inRefCon
 *    Custom data that you provided when registering your callback with the audio unit.
 * ioActionFlags
 *    Flags used to describe more about the context of this call.
 * inTimeStamp
 *    The timestamp associated with this call of audio unit render.
 * inBusNumber
 *    The bus number associated with this call of audio unit render.
 * inNumberFrames
 *    The number of sample frames that will be represented in the audio data in the provided ioData parameter.
 * ioData
 *    This is NULL - use AudioUnitRender to fetch the audio data.
**/
static OSStatus MyInputBusInputCallback(void                       *inRefCon,
                                        AudioUnitRenderActionFlags *ioActionFlags,
                                        const AudioTimeStamp       *inTimeStamp,
                                        UInt32                      inBusNumber,
                                        UInt32                      inNumberFrames,
                                        AudioBufferList            *ioData)
{
	
	// The AudioUnit callbacks operate on a different real-time thread
	struct pjmedia_snd_stream *snd_strm = (struct pjmedia_snd_stream *)inRefCon;
	
	AudioBufferList *abl = snd_strm->inputBufferList;

	ajitter_packet *ajp = ajitter_put_ptr(snd_strm->record_jitter);
	abl->mNumberBuffers = 1;
	abl->mBuffers[0].mNumberChannels = 1;
	abl->mBuffers[0].mData = ajp->data;
	abl->mBuffers[0].mDataByteSize = inNumberFrames * 2;// XXX shame on me

	OSStatus status = AudioUnitRender(snd_strm->in_unit,
	                                  ioActionFlags,
	                                  inTimeStamp,
	                                  inBusNumber,
	                                  inNumberFrames,
	                                  abl);

	ERR("cant render input: %d");

	ajp->left = inNumberFrames * 2;
	ajp->off = 0;

	ajitter_put_done(snd_strm->record_jitter, ajp->idx, inTimeStamp->mSampleTime);

	return noErr;
err:
	return -1;
}

static OSStatus MyInputBusInputCallbackRS(void                       *inRefCon,
                                        AudioUnitRenderActionFlags *ioActionFlags,
                                        const AudioTimeStamp       *inTimeStamp,
                                        UInt32                      inBusNumber,
                                        UInt32                      inNumberFrames,
                                        AudioBufferList            *ioData)
{

	// The AudioUnit callbacks operate on a different real-time thread
	struct pjmedia_snd_stream *snd_strm = (struct pjmedia_snd_stream *)inRefCon;

	AudioBufferList *abl = snd_strm->inputBufferList;

	abl->mNumberBuffers = 1;
	abl->mBuffers[0].mNumberChannels = 1;
	abl->mBuffers[0].mData = NULL;
	abl->mBuffers[0].mDataByteSize = inNumberFrames * 2;// XXX shame on me
	
	OSStatus status = AudioUnitRender(snd_strm->in_unit,
	                                  ioActionFlags,
	                                  inTimeStamp,
	                                  inBusNumber,
	                                  inNumberFrames,
	                                  abl);
	
	ERR("cant render input: %d");

	UInt32 in_size, out_size;
	in_size = inNumberFrames;
	out_size = REC_CH;

	ajitter_packet *ajp = ajitter_put_ptr(snd_strm->record_jitter);
	status = speex_resampler_process_int(snd_strm->resampler, 0,
		abl->mBuffers[0].mData, &in_size,
		ajp->data, &out_size);
		
	ajp->left = out_size * 2;
	ajp->off = 0;

	ajitter_put_done(snd_strm->record_jitter, ajp->idx, inTimeStamp->mSampleTime);

	return noErr;
err:
	return -1;
}

// Order of calls from PJSIP:
// 
// SIP application is launched
// - pjmedia_snd_init
// 
// VoIP call started
// - pjmedia_snd_open
// - pjmedia_snd_stream_start
// - pjmedia_snd_stream_get_info
// - pjmedia_snd_get_dev_info
// 
// VoIP call stopped
// - ???
// 
// SIP application terminated
// - pjmedia_snd_stream_stop
// - pjmedia_snd_stream_close
// - pjmedia_snd_deinit

/**
 * Init the sound library.
 * 
 * This method is called when the SIP app starts.
 * We can use this method to setup and configure anything we might later need.
 * 
 * A call to this method does not mean the app needs to use our driver yet.
 * So it's just like any other init method in that respect.
 * 
 * Parameters:
 * factory - A memory pool factory.
**/
int media_snd_init()
{
	PJ_LOG(5, (THIS_FILE, "pjmedia_snd_init"));
	
	// Initialize audio session for iPhone
	initializeAudioSession();
	
	// To setup and use an audio unit, the following steps are performed in order:
	// 
	// - Ask the system for a reference to the audio unit
	// - Instantiate the audio unit
	// - Configure the audio unit instance
	// - Initialize the instance and start using it
	
	// Since this is an init method, we're not actually going to instantiate the audio unit,
	// but we can go ahead and get a reference to the voice unit.
	// In order to do this, we have to ask the system for a reference to it.
	// We do this by searching, based on a query of sorts, described by a AudioComponentDescription struct.
	
	AudioComponentDescription desc;
	
	// struct AudioComponentDescription {
	//   OSType componentType;
	//   OSType componentSubType;
	//   OSType componentManufacturer;
	//   UInt32 componentFlags;
	//   UInt32 componentFlagsMask;
	// };
	
	desc.componentType = kAudioUnitType_Output;
#if IPHONE
	desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
    desc.componentSubType = kAudioUnitSubType_HALOutput;
#endif
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	
	// AudioComponent
	// AudioComponentFindNext(AudioComponent inComponent, const AudioComponentDescription *inDesc)
	// 
	// This function is used to find an audio component that is the closest match to the provide values.
	// 
	// inComponent
	//   If NULL, then the search starts from the beginning until an audio component is found that matches
	//   the description provided by inDesc.
	//   If not-NULL, then the search starts (continues) from the previously found audio component specified
	//   by inComponent, and will return the nextfound audio component.
	// inDesc
	//   The type, subtype and manufacturer fields are used to specify the audio component to search for.
	//   A value of 0 (zero) for any of these fiels is a wildcard, so the first match found is returned.
	// 
	// Returns: An audio component that matches the search parameters, or NULL if none found.
	
	component = AudioComponentFindNext(NULL, &desc);
	
	if(component == NULL)
	{
		printf("Unable to find voice unit audio component!");
		return -1;
	}
	
	return 0;
}

/**
 * Deinitialize sound library.
**/
int media_snd_deinit(void)
{
	PJ_LOG(5, (THIS_FILE, "pjmedia_snd_deinit"));
	
	// Remove reference to the memory pool factory.
	// We check this variable in other parts of the code to see if we've been initialized.
	//snd_pool_factory = NULL;
	
	// Remove references to other variables we setup in the init method.
	component = NULL;
	
	return 0;
}

/**
 * This method is called by PJSIP to get the number of devices detected by our driver.
**/
int pjmedia_snd_get_dev_count()
{
	PJ_LOG(5, (THIS_FILE, "pjmedia_snd_get_dev_count"));
	
	// Only one device
	return 1;
}


/**
 * Create a unidirectional audio stream for capturing audio samples from the sound device.
 * This is a half-duplex method, as opposed to the full-duplex pjmedia_snd_open().
 * 
 * Parameters:
 * index
 *    Device index, or -1 to let the library choose the first available device.
 * clock_rate
 *    Sound device's clock rate to set.
 * samples_per_frame
 *    Number of samples per frame.
 * bits_per_sample
 *    Set the number of bits per sample.
 *    The normal value for this parameter is 16 bits per sample.
 * rec_cb
 *    Callback to handle captured audio samples.
 *    User data to be associated with the stream.
 * p_snd_strm
 *    Pointer to receive the stream instance.
 * 
 * Returns:
 * PJ_SUCCESS on success.
**/
int media_snd_open_rec(unsigned clock_rate,
                  struct pjmedia_snd_stream **p_snd_strm)
{
	PJ_LOG(5, (THIS_FILE, "pjmedia_snd_open_rec"));
	
	return media_snd_open(DIR_CAP, clock_rate, 0, p_snd_strm);
}

/**
 * Create a unidirectional audio stream for playing audio samples to the sound device.
 * This is a half-duplex method, as opposed to the full-duplex pjmedia_snd_open().
 * 
 * Parameters:
 * index
 *    Device index, or -1 to let the library choose the first available device.
 * clock_rate
 *    Sound device's clock rate to set.
 * samples_per_frame
 *    Number of samples per frame.
 * bits_per_sample
 *    Set the number of bits per sample.
 *    The normal value for this parameter is 16 bits per sample.
 * play_cb 
 *    Callback to be called when the sound player needs more audio samples to play.
 * user_data
 *    User data to be associated with the stream.
 * p_snd_strm
 *    Pointer to receive the stream instance.
 *
 * Returns:
 * PJ_SUCCESS on success.
**/
int media_snd_open_player(unsigned clock_rate,
				int render_ring_size,
                     struct pjmedia_snd_stream **p_snd_strm)
{
	PJ_LOG(5, (THIS_FILE, "pjmedia_snd_open_player"));
	
	return media_snd_open(DIR_PLAY,
	                        clock_rate,
				render_ring_size,
	                        p_snd_strm);
}

/**
 * Create sound stream for both capturing audio and audio playback, from the same device.
 * This is the recommended way to create simultaneous recorder and player streams (instead of
 * creating separate capture and playback streams), because it works on backends that
 * do not allow a device to be opened more than once.
 * 
 * This method is invoked when a call is started.
 * At this point we should setup our AudioQueue.
 * There's no need to start the audio session or audio queue yet though.
 * We can wait until pjmedia_snd_stream_start for that stuff.
 * 
 * Parameters:
 * rec_id
 *    Device index for recorder/capture stream, or -1 to use the first capable device.
 * play_id
 *    Device index for playback stream, or -1 to use the first capable device.
 * clock_rate
 *    Sound device's clock rate to set.
 * p_snd_strm
 *    Pointer to receive the stream instance.
 * 
 * Returns:
 * PJ_SUCCESS on success.
**/
int media_snd_open(media_dir_t dir,
    unsigned clock_rate,
    int render_ring_size,
    struct pjmedia_snd_stream **p_snd_strm)
{
	OSStatus status;
	
	// Sound stream structure that we'll allocate and populate.
	// When we're done, we'll make the p_snd_strm paramter point to it.
	struct pjmedia_snd_stream *snd_strm;
	
	// Allocate snd_stream structure to hold all of our "instance" variables
	snd_strm = malloc(sizeof(struct pjmedia_snd_stream));
	
	// Store our passed instance variables
	
	// Remember: The snd_strm variable is a pointer to a pjmedia_snd_stream struct.
	// The pjmedia_snd_stream struct is defined at the top of this file.
	
	snd_strm->clock_rate        = clock_rate;
	snd_strm->isActive          = false;
	snd_strm->resampler	    = NULL;

	snd_strm->render_ring = malloc(render_ring_size);
	snd_strm->render_ring_size = render_ring_size;
	snd_strm->render_ring_off = 0;
	if(!snd_strm->render_ring)
		goto err_out_ring2;

	snd_strm->record_jitter = ajitter_init(REC_CH*2);
	if(!snd_strm->record_jitter)
		goto err_out_ring;

	memset(snd_strm->render_ring, 0x60, render_ring_size/2);
	memset(snd_strm->render_ring + render_ring_size/2, 0x80, render_ring_size/2);

	// Allocate our inputBufferList.
	// This gets used in MyInputBusInputCallback() when calling AudioUnitRender to get microphone data.
	snd_strm->inputBufferList = malloc(sizeof(AudioBufferList));

	if(!snd_strm->inputBufferList)
		goto err_out_inbuf;
	

	int theDefaultOutputDeviceID, theDefaultInputDeviceID;
	theDefaultOutputDeviceID = default_device(0);
	theDefaultInputDeviceID = default_device(1);

	if(dir == DIR_BI && theDefaultOutputDeviceID != theDefaultInputDeviceID) {
		dir = dir | DIR_SEP;
	}

	snd_strm->dir = dir;

	// Instantiate the audio component
	
	if(dir == DIR_BI) {
		status = AudioComponentInstanceNew(component, &snd_strm->in_unit);
		snd_strm->out_unit = snd_strm->in_unit;

	} else {
		IF_CAP(dir) {
			status = AudioComponentInstanceNew(component, &snd_strm->in_unit);

		}
		IF_PLAY(dir) {
			status = AudioComponentInstanceNew(component, &snd_strm->out_unit);
		}
	}
	printf("instanced components dir %d  in: 0x%lx (%d), out: 0x%lx (%d)\n",
			dir,
			(long)snd_strm->in_unit, theDefaultInputDeviceID,
			(long)snd_strm->out_unit, theDefaultOutputDeviceID);
	
	// Enable input and output on the voice unit
	
	// Remember - there are two buses, input and output.
	// Output is bus #0, Input is bus #1.
	// Think: 'Output' starts with a 0, 'Input' starts with a 1.
	
	AudioUnitElement inputBus = 1;
	AudioUnitElement outputBus = 0;

	IF_CAP(dir) {
		status = enable_io(snd_strm->in_unit, inputBus);
		ERR("failed to enable input unit: %d\n");
		MSG("input enabled");
		IF_SEP(dir) {
			status = disable_io(snd_strm->in_unit, outputBus);
			ERR("failed to disable output bus: %d");
			MSG("output disabled");
		}

	}
	
	IF_PLAY(dir) {
		enable_io(snd_strm->out_unit, outputBus);
		ERR("Failed to enable voice unit output: %d");
	}

	if(dir == DIR_BI) {
		status = set_current(snd_strm->out_unit, theDefaultOutputDeviceID);
	} else {
		IF_CAP(dir) {
			status = set_current(snd_strm->in_unit, theDefaultInputDeviceID);
			ERR("failed to select input: %d");
			MSG("input selected");
		}
		IF_PLAY(dir) {
			status = set_current(snd_strm->out_unit, theDefaultOutputDeviceID);
			ERR("failed to select output: %d");
			MSG("output selected");
		}
	}

	if(snd_strm->dir & DIR_CAP)
	{
#if IPHONE
		status = set_format(snd_strm->in_unit, inputBus, 8000);
		snd_strm->resampler = = NULL;
#else
		status = set_format(snd_strm->in_unit, inputBus, 44100);
		ERR("cant set input format. err %d");
		MSG("input format ok");

		snd_strm->resampler = speex_resampler_init(1, 44100, clock_rate, 4, &status);
		printf("created resampler: %d %lx\n", status, (long)snd_strm->resampler );
		if(!snd_strm->resampler) {
			goto err_out_resampler;
		}
#endif
	}
	
	
	if(snd_strm->dir & DIR_PLAY)
	{
		status = set_format(snd_strm->out_unit, outputBus, clock_rate);
		ERR("cant set out format. err %d");
		MSG("out format ok");
	}
	
	startAudioSession(snd_strm->dir);
	
	if(dir == DIR_BI) {
		status = AudioUnitInitialize(snd_strm->out_unit);
		ERR("failed to init voice out unit: %d")
	} else {

		IF_PLAY(dir) {
			status = AudioUnitInitialize(snd_strm->out_unit);
			ERR("failed to init voice out unit: %d")
		}

		IF_CAP(dir) {
			status = AudioUnitInitialize(snd_strm->in_unit);
			ERR("failed to init voice in unit: %d")
		}
	}

	IF_PLAY(dir) {
		status = set_cb(snd_strm->out_unit, outputBus, MyOutputBusRenderCallack, snd_strm);
		ERR("Failed to set outputBus render callback: %d");
		MSG("render cb ok");
	}

	IF_CAP(dir) {
#if IPHONE
		status = set_cb(snd_strm->in_unit, inputBus, MyInputBusInputCallback, snd_strm);
#else
		status = set_cb(snd_strm->in_unit, inputBus, MyInputBusInputCallbackRS, snd_strm);
#endif
		ERR("Failed to set input callback: %d");
		MSG("input cb ok");
	}

	// return pointer
	*p_snd_strm = snd_strm;
	
	// global var
	snd_strm_instance = snd_strm;
	
	return 0;

err:
err_out:
	if(snd_strm->resampler) speex_resampler_destroy(snd_strm->resampler);
err_out_resampler:
	free(snd_strm->inputBufferList);
err_out_inbuf:
	ajitter_destroy(snd_strm->record_jitter);
err_out_ring:
	free(snd_strm->render_ring);
err_out_ring2:
	free(snd_strm);
	return -1;
}

/**
 * Starts the stream.
 * 
 * This method is called prior to starting a call.
 * It is only called for the first call?
 * Subsequent phone calls do not invoke this method?
**/
int media_snd_stream_start(struct pjmedia_snd_stream *snd_strm)
{
	PJ_LOG(5, (THIS_FILE, "pjmedia_snd_stream_start"));
	
	// Make note of the stream starting
	snd_strm->isActive = true;
	
	// Activate the audio session
	startAudioSession(snd_strm->dir);
	
	// Start the audio unit
	poppingSoundWorkaround = true;
	if(snd_strm->out_unit) {
		AudioOutputUnitStart(snd_strm->out_unit);
	}

	if(snd_strm->in_unit) {
		AudioOutputUnitStart(snd_strm->in_unit);
	}
	
	return 0;
}

/**
 * Stops the stream.
 * 
 * This method is only called when the PJSIP application is shutting down.
 * It is not called when a phone call is ended?
**/
int media_snd_stream_stop(struct pjmedia_snd_stream *snd_strm)
{
	// Stop the audio unit
	if(snd_strm->out_unit) {
		AudioOutputUnitStop(snd_strm->out_unit);
	}

	if(snd_strm->in_unit) {
		AudioOutputUnitStop(snd_strm->in_unit);
	}

	// Deactivate the audio session
	stopAudioSession();
	
	// Make a note of the stream stopping
	snd_strm->isActive = false;
	
	return 0;
}

/**
 * Destroy the stream.
**/
int media_snd_stream_close(struct pjmedia_snd_stream *snd_strm)
{
	if(snd_strm->out_unit)
	{
		if(snd_strm->out_unit == snd_strm->in_unit)
			snd_strm->in_unit = NULL;

		AudioUnitUninitialize(snd_strm->out_unit);
		AudioComponentInstanceDispose(snd_strm->out_unit);
		snd_strm->out_unit = NULL;
	}
	
	if(snd_strm->in_unit)
	{
		AudioUnitUninitialize(snd_strm->in_unit);
		AudioComponentInstanceDispose(snd_strm->in_unit);
		snd_strm->in_unit = NULL;
	}

	// Clear our static reference to the stream instance (used in the audio session interruption callback)
	snd_strm_instance = NULL;

	if(snd_strm->render_ring) {
		free(snd_strm->render_ring);
		snd_strm->render_ring = NULL;
	}

	if(snd_strm->record_jitter) {
		ajitter_destroy(snd_strm->record_jitter);
		snd_strm->record_jitter = NULL;
	}

	if(snd_strm->inputBufferList) {
		free(snd_strm->inputBufferList);
		snd_strm->inputBufferList = NULL;
	}

	if(snd_strm->resampler) {
		speex_resampler_destroy(snd_strm->resampler);
		snd_strm->resampler = NULL;
	}

	free(snd_strm);
	
	return 0;
}
