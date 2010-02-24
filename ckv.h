
#ifndef CKV_H
#define CKV_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* open ckv-specific lua libraries */
int open_luabaselite(lua_State *L);

/* audio */
typedef void (*AudioCallback)(double *outputBuffer, double *inputBuffer,
                              unsigned int nFrames,
                              double streamTime,
                              void *userData);
int start_audio(AudioCallback callback, int sample_rate, void *data);
void stop_audio(void);

/* midi */
typedef struct {
	float vel;
	int note;
} MidiMsg;

int start_midi();
int get_midi_message(MidiMsg *message);

#endif
