extern "C" {
#include "ckv.h"
}

#include "rtaudio/RtAudio.h"

static RtAudio audio;
static AudioCallback callback;

static
int
render(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
       double streamTime, RtAudioStreamStatus status, void *userData)
{
	if(status)
		std::cerr << "[ckv] Stream underflow detected!" << std::endl;
	
	callback((double *)outputBuffer, (double *)inputBuffer, nBufferFrames, streamTime, userData);

	return 0;
}

extern "C" {

/* returns 0 on failure */
int
start_audio(AudioCallback _callback, int sample_rate, void *data)
{
	if(audio.getDeviceCount() < 1) {
		std::cout << "No audio devices found!\n";
		return 0;
	}
	
	RtAudio::StreamParameters iparams, oparams;
	iparams.deviceId = audio.getDefaultInputDevice();
	iparams.nChannels = 1;
	iparams.firstChannel = 0;
	oparams.deviceId = audio.getDefaultOutputDevice();
	oparams.nChannels = 2;
	oparams.firstChannel = 0;
	unsigned int bufferFrames = 256;
	
	callback = _callback;
	
	try {
		audio.openStream(&oparams, &iparams, RTAUDIO_FLOAT64, sample_rate, &bufferFrames, &render, data);
		audio.startStream();
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
		audio.stopStream();
	} catch(RtError& e) {
		e.printMessage();
	}
	
	if(audio.isStreamOpen())
		audio.closeStream();
}

} // extern "C"
