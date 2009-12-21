extern "C" {
#include "ckv.h"
}

#include <RtAudio.h>
#include <iostream>

static RtAudio dac;
static AudioCallback callback;

static
int
render(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
       double streamTime, RtAudioStreamStatus status, void *userData)
{
	if(status)
		std::cerr << "Stream underflow detected!" << std::endl;
	
	callback((double *)outputBuffer, (double *)inputBuffer, nBufferFrames, streamTime, userData);

	return 0;
}

extern "C" {

/* returns 0 on failure */
int
start_audio(AudioCallback _callback, void *data)
{
	if(dac.getDeviceCount() < 1) {
		std::cout << "No audio devices found!\n";
		return 0;
	}
	
	RtAudio::StreamParameters parameters;
	parameters.deviceId = dac.getDefaultOutputDevice();
	parameters.nChannels = 1;
	parameters.firstChannel = 0;
	unsigned int sampleRate = 44100;
	unsigned int bufferFrames = 256;
	
	callback = _callback;
	
	try {
		dac.openStream(&parameters, NULL, RTAUDIO_FLOAT64, sampleRate, &bufferFrames, &render, data);
		dac.startStream();
	} catch(RtError& e) {
		e.printMessage();
		return 0;
	}
	
	return 1;
}

void
stop_audio(void)
{
	try {
		dac.stopStream();
	} catch(RtError& e) {
		e.printMessage();
	}
	
	if(dac.isStreamOpen())
		dac.closeStream();
}

} // extern "C"
