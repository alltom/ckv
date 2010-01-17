
#include "ckv.h"
#include "ckvm.h"
#include "pq.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

typedef struct VM {
	CKVM ckvm;
	int running;
	
	unsigned int audio_now; /* used in audio callback; unused in silent mode */
	pthread_mutex_t audio_done;
	int sample_rate;
	int channels;
} VM;

static void render_audio(double *outputBuffer, double *inputBuffer, unsigned int nFrames, double streamTime, void *userData);

static
void
usage(void)
{
	printf("usage: ckv [-has] [file ...]\n");
	printf("  -h print this usage information\n");
	printf("  -a load all Lua libraries (enough to shoot yourself in the foot), including file IO\n");
	printf("  -s silent mode (no audio processing, non-real-time)\n");
}

static
void
print_error(const char *message)
{
	fprintf(stderr, "[ckv] %s\n", message);
}

static
void
error_callback(CKVM vm, const char *message)
{
	print_error(message);
}

static
void
open_base_libs(VM *vm, int all_libs)
{
	lua_State *L = ckvm_global_state(vm->ckvm);
	
	lua_gc(L, LUA_GCSTOP, 0); /* stop collector during initialization */
	if(all_libs) {
		lua_pushcfunction(L, luaopen_base); lua_call(L, 0, 0);
		lua_pushcfunction(L, luaopen_package); lua_call(L, 0, 0);
		lua_pushcfunction(L, luaopen_debug); lua_call(L, 0, 0);
		lua_pushcfunction(L, luaopen_io); lua_call(L, 0, 0);
		lua_pushcfunction(L, luaopen_os); lua_call(L, 0, 0);
	} else {
		lua_pushcfunction(L, open_luabaselite); lua_call(L, 0, 0);
	}
	lua_pushcfunction(L, luaopen_string); lua_call(L, 0, 0);
	lua_pushcfunction(L, luaopen_table); lua_call(L, 0, 0);
	lua_pushcfunction(L, luaopen_math); lua_call(L, 0, 0);
	lua_gc(L, LUA_GCRESTART, 0);
	
	/* import math.random */
	lua_getglobal(L, "math");
	lua_getfield(L, -1, "random");
	lua_setglobal(L, "random"); /* alias as random */
	lua_getfield(L, -1, "random");
	lua_setglobal(L, "rand"); /* alias as rand */
	lua_pop(L, 1); /* pop math */
	
	/* handy random functions */
	(void) luaL_dostring(L,
	"function maybe() return random() < 0.5 end "
	"function probably() return random() < 0.7 end "
	"function usually() return random() < 0.9 end "
	);
	
	/* seed the random number generator */
	lua_getglobal(L, "math");
	lua_getfield(L, -1, "randomseed");
	lua_pushnumber(L, time(NULL));
	lua_call(L, 1, 0);
	lua_pop(L, 1); /* pop math */
}

static
void
open_audio_libs(VM *vm)
{
	lua_State *L = ckvm_global_state(vm->ckvm);
	
	lua_gc(L, LUA_GCSTOP, 0); /* stop collector during initialization */
	lua_pushcfunction(L, open_ckvugen); lua_call(L, 0, 0);
	lua_gc(L, LUA_GCRESTART, 0);
	
	/* set the sample rate */
	lua_pushnumber(L, vm->sample_rate);
	lua_setglobal(L, "sample_rate");
	
	/* set duration helper variables */
	lua_pushnumber(L, 1);
	lua_pushnumber(L, 1);
	lua_setglobal(L, "sample");
	lua_setglobal(L, "samples");
	lua_pushnumber(L, vm->sample_rate / 1000.0);
	lua_setglobal(L, "ms");
	lua_pushnumber(L, vm->sample_rate);
	lua_pushnumber(L, vm->sample_rate);
	lua_setglobal(L, "second");
	lua_setglobal(L, "seconds");
	lua_pushnumber(L, vm->sample_rate * 60);
	lua_pushnumber(L, vm->sample_rate * 60);
	lua_setglobal(L, "minute");
	lua_setglobal(L, "minutes");
	lua_pushnumber(L, vm->sample_rate * 60 * 60);
	lua_pushnumber(L, vm->sample_rate * 60 * 60);
	lua_setglobal(L, "hour");
	lua_setglobal(L, "hours");
	lua_pushnumber(L, vm->sample_rate * 60 * 60 * 24);
	lua_pushnumber(L, vm->sample_rate * 60 * 60 * 24);
	lua_setglobal(L, "day");
	lua_setglobal(L, "days");
	lua_pushnumber(L, vm->sample_rate * 60 * 60 * 24 * 7);
	lua_pushnumber(L, vm->sample_rate * 60 * 60 * 24 * 7);
	lua_setglobal(L, "week");
	lua_setglobal(L, "weeks");
	lua_pushnumber(L, vm->sample_rate * 60 * 60 * 24 * 7 * 2);
	lua_pushnumber(L, vm->sample_rate * 60 * 60 * 24 * 7 * 2);
	lua_setglobal(L, "fortnight");
	lua_setglobal(L, "fortnights");
}

int
main(int argc, char *argv[])
{
	VM vm;
	int i, c;
	int num_scripts, scripts_added;
	int silent_mode = 0; /* whether to execute without using the sound card */
	int all_libs = 0; /* whether to load even lua libraries that could screw things up */
	
	vm.running = 1;
	vm.audio_now = 0;
	vm.sample_rate = 44100;
	vm.channels = 2;
	
	vm.ckvm = ckvm_create(error_callback);
	if(vm.ckvm == NULL) {
		print_error("could not initialize VM");
		return EXIT_FAILURE;
	}
	
	while((c = getopt(argc, (char ** const) argv, "hsa")) != -1)
		switch(c) {
		case 'h':
			usage();
			return EXIT_SUCCESS;
		case 's':
			silent_mode = 1;
			break;
		case 'a':
			all_libs = 1;
			break;
		}
	
	open_base_libs(&vm, all_libs);
	open_audio_libs(&vm);
	
	num_scripts = argc - optind;
	
	scripts_added = 0;
	for(i = optind; i < argc; i++)
		if(ckvm_add_thread(vm.ckvm, argv[i]))
			scripts_added++;
	
	if(scripts_added == 0 && silent_mode)
		return num_scripts == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	
	if(silent_mode) {
		
		ckvm_run(vm.ckvm);
		
	} else {
		
		pthread_mutex_init(&vm.audio_done, NULL /* attr */);
		pthread_mutex_lock(&vm.audio_done);
		
		if(!start_audio(render_audio, vm.sample_rate, &vm)) {
			fprintf(stderr, "[ckv] could not start audio\n");
			return EXIT_FAILURE;
		}
		
		/* wait for audio to finish */
		/* ugh, but there's a race here */
		pthread_mutex_lock(&vm.audio_done);
		
		stop_audio();
		
	}
	
	ckvm_destroy(vm.ckvm);
	
	return EXIT_SUCCESS;
}

static
void
render_audio(double *outputBuffer, double *inputBuffer, unsigned int nFrames,
             double streamTime, void *userData)
{
	VM *vm = (VM *)userData;
	lua_State *L;
	unsigned int i;
	int adc, dac, sinks, ugen_graph, tick_all;
	
	L = ckvm_global_state(vm->ckvm);
	
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
	
	for(i = 0; i < nFrames; i++) {
		ckvm_run_until(vm->ckvm, vm->audio_now);
		
		if(!vm->running) {
			for(; i < nFrames; i++) {
				outputBuffer[i * 2] = 0;
				outputBuffer[i * 2 + 1] = 0;
			}
			
			goto bail;
		}
		
		/* set mic sample */
		lua_pushnumber(L, inputBuffer[i]);
		lua_setfield(L, adc, "next");
		
		/* tick all ugens */
		lua_pushvalue(L, tick_all);
		lua_pushvalue(L, ugen_graph);
		lua_pushvalue(L, sinks);
		lua_call(L, 2, 0);
		
		/* sweet, audio => speaker */
		lua_getfield(L, dac, "last");
		outputBuffer[i * 2] = lua_tonumber(L, -1);
		outputBuffer[i * 2 + 1] = outputBuffer[i * 2];
		lua_pop(L, 1);
		
		vm->audio_now++;
	}
	
bail:
	lua_settop(L, 0); /* reset stack to empty */
}
