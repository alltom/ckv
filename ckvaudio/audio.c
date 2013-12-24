
#include "audio.h"
#include "ugen/ugen.h"

#include <stdlib.h>

extern int open_ckvugen(lua_State *L);
static void open_audio_libs(CKVAudio audio, CKVM vm);
static int ckv_audio_ffwd(lua_State *L);

struct _CKVAudio {
	CKVM vm;
	unsigned int now;
	float silent_until;
	int sample_rate;
	int channels;
	double hard_clip;
	int print_time;
};

CKVAudio
ckva_open(CKVM vm, int sample_rate, int channels, double hard_clip, int print_time)
{
	lua_State *L;
	CKVAudio audio;
	
	audio = (CKVAudio)malloc(sizeof(struct _CKVAudio));
	audio->vm = vm;
	audio->now = (int) ckvm_now(vm);
	audio->silent_until = 0;
	audio->sample_rate = sample_rate;
	audio->channels = channels;
	audio->hard_clip = hard_clip;
	audio->print_time = print_time;
	
	open_audio_libs(audio, vm);
	
	L = ckvm_global_state(vm);
	lua_pushlightuserdata(L, audio);
	lua_setfield(L, LUA_REGISTRYINDEX, "audio");
	
	return audio;
}

void
ckva_destroy(CKVAudio audio)
{
	free(audio);
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
	int i, c;
	int oldtop, adc, dac, sinks, ugen_graph, tick_all;
	double sample;

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

	for(i = 0; i < frames; ) {
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

		/* get sample */
		lua_getfield(L, dac, "last");
		sample = lua_tonumber(L, -1);
		lua_pop(L, 1);
		
		/* clip if requested */
		if(audio->hard_clip > 0) {
			if(sample > audio->hard_clip)
				sample = audio->hard_clip;
			if(sample < -audio->hard_clip)
				sample = -audio->hard_clip;
		}
		
		/* audio => speaker */
		outputBuffer[i * 2] = sample;
		outputBuffer[i * 2 + 1] = sample;

		audio->now++;
		if(audio->print_time && audio->now % audio->sample_rate == 0) {
			int second = audio->now / audio->sample_rate;
			fprintf(stderr, "[ckv] %02d:%02d:%02d\n", (int) (second / 60.0 / 60.0), (int) (second / 60.0) % 60, second % 60);
		}
		
		if(audio->now >= audio->silent_until)
			i++;
	}

	lua_settop(L, oldtop);
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
	
	/* function for fast-forwarding in audio stream */
	lua_pushcfunction(L, ckv_audio_ffwd); lua_setglobal(L, "audio_ffwd");
}

static
int
ckv_audio_ffwd(lua_State *L)
{
	CKVAudio audio;
	lua_Number amt;
	
	amt = luaL_checknumber(L, 1);
	if(amt < 0) {
		fprintf(stderr, "[ckv] attempt to audio_ffwd with negative value\n");
		return 0;
	}
	
	lua_getfield(L, LUA_REGISTRYINDEX, "audio");
	audio = lua_touserdata(L, -1);
	if(audio->silent_until < audio->now + amt)
		audio->silent_until = audio->now + amt;
	
	lua_pushnumber(L, amt);
	return 1;
}
