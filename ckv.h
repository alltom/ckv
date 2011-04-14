
#ifndef CKV_H
#define CKV_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* luabaselite.c */
int open_luabaselite(lua_State *L); /* open ckv-specific lua libraries */

/* rtaudio_wrapper.cpp */
typedef void (*AudioCallback)(double *outputBuffer, double *inputBuffer,
                              unsigned int nFrames,
                              double streamTime,
                              void *userData);
int start_audio(AudioCallback callback, int sample_rate, void *data);
void stop_audio(void);

/* rtmidi_wrapper.cpp */
typedef struct {
	int control; /* control message? boolean */
	int pitch_bend; /* pitch bend? boolean */
	int channel;
	int note;
	float velocity;
} MidiMsg;

int start_midi(int port);
int get_midi_message(MidiMsg *message);

#endif
