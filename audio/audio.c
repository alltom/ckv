
#include "audio.h"
#include "ugen/ugen.h"

#include <stdlib.h>

extern int open_ckvugen(lua_State *L);
static void open_audio_libs(CKVAudio audio, CKVM vm);

struct CKVAudio {
	CKVM vm;
	unsigned int now;
	int sample_rate;
	int channels;
};

CKVAudio
ckva_open(CKVM vm, int sample_rate, int channels)
{
	CKVAudio audio = malloc(sizeof(struct CKVAudio));
	if(audio == NULL)
		return NULL;

	audio->vm = vm;
	audio->now = (int) ckvm_now(vm);
	audio->sample_rate = sample_rate;
	audio->channels = channels;
	
	open_audio_libs(audio, vm);
	
	return audio;
}

int
ckva_sample_rate(CKVAudio audio)
{
	return audio->sample_rate;
}

int
ckva_channels(CKVAudio audio)
{
	return audio->channels;
}

void
ckva_fill_buffer(CKVAudio audio, double *outputBuffer, double *inputBuffer, int frames)
{
	lua_State *L;
	unsigned int i, c;
	int oldtop, adc, dac, sinks, ugen_graph, tick_all;

	if(!ckvm_running(audio->vm)) {
		for(i = 0; i < frames; i++)
			for(c = 0; c < audio->channels; c++)
				outputBuffer[i * audio->channels + c] = 0;
		return;
	}
	
	L = ckvm_global_state(audio->vm);
	
	oldtop = lua_gettop(L);

	lua_getglobal(L, "adc");
	adc = lua_gettop(L);

	lua_getglobal(L, "dac");
	dac = lua_gettop(L);

	/* sinks */
	lua_createtable(L, 2 /* array */, 0 /* non-array */);
	lua_getglobal(L, "dac");
	lua_rawseti(L, -2, 1);
	lua_getglobal(L, "blackhole");
	lua_rawseti(L, -2, 2);
	sinks = lua_gettop(L);

	lua_getfield(L, LUA_REGISTRYINDEX, "ugen_graph");
	ugen_graph = lua_gettop(L);

	lua_getfield(L, ugen_graph, "tick_all");
	tick_all = lua_gettop(L);

	for(i = 0; i < frames; i++) {
		ckvm_run_until(audio->vm, audio->now);

		if(!ckvm_running(audio->vm)) {
			for(; i < frames; i++)
				for(c = 0; c < audio->channels; c++)
					outputBuffer[i * audio->channels + c] = 0;
			break;
		}

		/* set mic sample */
		lua_pushnumber(L, inputBuffer[i]);
		lua_setfield(L, adc, "next");

		/* tick all ugens */
		lua_pushvalue(L, tick_all);
		lua_pushvalue(L, ugen_graph);
		lua_pushvalue(L, sinks);
		lua_call(L, 2, 0);

		/* audio => speaker */
		lua_getfield(L, dac, "last");
		outputBuffer[i * 2] = lua_tonumber(L, -1);
		outputBuffer[i * 2 + 1] = outputBuffer[i * 2];
		lua_pop(L, 1);

		audio->now++;
	}

	lua_settop(L, oldtop);
}

void
ckva_destroy(CKVAudio audio)
{
	free(audio);
}

static
void
open_audio_libs(CKVAudio audio, CKVM vm)
{
	lua_State *L = ckvm_global_state(vm);
	
	lua_gc(L, LUA_GCSTOP, 0); /* stop collector during initialization */
	lua_pushcfunction(L, open_ckvugen); lua_call(L, 0, 0);
	lua_gc(L, LUA_GCRESTART, 0);
	
	/* set the sample rate */
	lua_pushnumber(L, audio->sample_rate);
	lua_setglobal(L, "sample_rate");
	
	/* set duration helper variables */
	lua_pushnumber(L, 1);
	lua_pushnumber(L, 1);
	lua_setglobal(L, "sample");
	lua_setglobal(L, "samples");
	lua_pushnumber(L, audio->sample_rate / 1000.0);
	lua_setglobal(L, "ms");
	lua_pushnumber(L, audio->sample_rate);
	lua_pushnumber(L, audio->sample_rate);
	lua_setglobal(L, "second");
	lua_setglobal(L, "seconds");
	lua_pushnumber(L, audio->sample_rate * 60);
	lua_pushnumber(L, audio->sample_rate * 60);
	lua_setglobal(L, "minute");
	lua_setglobal(L, "minutes");
	lua_pushnumber(L, audio->sample_rate * 60 * 60);
	lua_pushnumber(L, audio->sample_rate * 60 * 60);
	lua_setglobal(L, "hour");
	lua_setglobal(L, "hours");
	lua_pushnumber(L, audio->sample_rate * 60 * 60 * 24);
	lua_pushnumber(L, audio->sample_rate * 60 * 60 * 24);
	lua_setglobal(L, "day");
	lua_setglobal(L, "days");
	lua_pushnumber(L, audio->sample_rate * 60 * 60 * 24 * 7);
	lua_pushnumber(L, audio->sample_rate * 60 * 60 * 24 * 7);
	lua_setglobal(L, "week");
	lua_setglobal(L, "weeks");
	lua_pushnumber(L, audio->sample_rate * 60 * 60 * 24 * 7 * 2);
	lua_pushnumber(L, audio->sample_rate * 60 * 60 * 24 * 7 * 2);
	lua_setglobal(L, "fortnight");
	lua_setglobal(L, "fortnights");
}
