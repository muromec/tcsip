#include "sound_utils.h"
#include <AudioToolbox/AudioServices.h>


int default_device(char in) {
#if IPHONE
    return 0;
#else
	OSStatus status;
	AudioDeviceID deviceID;
	AudioObjectPropertyAddress prop;
	UInt32 sz = sizeof(deviceID);
	
	prop.mSelector = in ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice;
	prop.mScope = kAudioObjectPropertyElementMaster;
	prop.mElement = kAudioObjectPropertyElementMaster;

	status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
		       &prop, 0, NULL, &sz, &deviceID);

	ERR("cant get default device err %d\n");

	return (int)deviceID;

#endif
err:
	return -1;
};

int get_current(AudioUnit unit) {
	OSStatus status;
	UInt32 current;
	UInt32 sz = sizeof(UInt32);

	status = AudioUnitGetProperty(unit,
		kAudioOutputUnitProperty_CurrentDevice,
		kAudioUnitScope_Global,
		0,
		&current,
		&sz);

	ERR("cant get current device err: %d\n");

	return current;

err:
	return -1;
}

float get_srate(AudioUnit unit, int id) {
	Float64	    in_srate = kAudioStreamAnyRate;
	UInt32	    sz = sizeof(in_srate);
	OSStatus status;

	status = AudioUnitGetProperty(unit,
			kAudioUnitProperty_SampleRate,
			kAudioUnitScope_Input,
			1,
			&in_srate,
			&sz);

	ERR("cant get current sample rate err: %d\n");
	return in_srate;
err:
	return 0;

}

int set_current(AudioUnit unit, int id) {

	return AudioUnitSetProperty(unit,
		kAudioOutputUnitProperty_CurrentDevice,
		kAudioUnitScope_Global,
		0,
		&id,
		sizeof(id));
}

int set_enable_io(AudioUnit unit, int bus, int state) {
	OSStatus status;

	status = AudioUnitSetProperty(unit,
		kAudioOutputUnitProperty_EnableIO,
		bus ? kAudioUnitScope_Input :  kAudioUnitScope_Output,
		bus,
		&state,
		sizeof(UInt32));
	ERR("cant enable/disable io, err %d\n");

	return 0;
err:
	return -1;
}

int set_format(AudioUnit unit, int bus, int samplerate) {
	AudioStreamBasicDescription streamDesc;

	/* mono audio PCM 1600 */
	streamDesc.mFormatID         = kAudioFormatLinearPCM;
	streamDesc.mFormatFlags      = (
			kLinearPCMFormatFlagIsSignedInteger |
			kLinearPCMFormatFlagIsPacked);
	
	streamDesc.mSampleRate       = samplerate;
	streamDesc.mBitsPerChannel   = 16;
	streamDesc.mChannelsPerFrame = 1;
	streamDesc.mBytesPerFrame    = 2;
	streamDesc.mFramesPerPacket  = 1;
	streamDesc.mBytesPerPacket   = 2;

	// Note: use scope output for input format
	//       and scope input for output format
	return AudioUnitSetProperty(unit,
		kAudioUnitProperty_StreamFormat,
		bus ? kAudioUnitScope_Output :  kAudioUnitScope_Input,
		bus,
		&streamDesc,         
		sizeof(streamDesc));

}

int set_cb(AudioUnit unit, int bus, void *cb, void *user) {
	AURenderCallbackStruct Callback;
	Callback.inputProc = cb;
	Callback.inputProcRefCon = user;

	return AudioUnitSetProperty(unit,
		bus ? kAudioOutputUnitProperty_SetInputCallback : kAudioUnitProperty_SetRenderCallback,
		bus ? kAudioUnitScope_Input :  kAudioUnitScope_Output,
		bus,
		&Callback,
		sizeof(Callback));
}
