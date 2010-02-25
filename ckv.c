
#include "ckv.h"
#include "ckvm.h"
#include "ckvaudio/audio.h"
#include "ckvmidi/midi.h"
#include "pq.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <string.h>

typedef struct VM {
	CKVM ckvm;
	CKVAudio audio;
	CKVMIDI midi;
	
	pthread_mutex_t audio_done_mutex;
	pthread_cond_t audio_done;
} VM;

static void render_audio(double *outputBuffer, double *inputBuffer, unsigned int nFrames, double streamTime, void *userData);

static
void
usage(void)
{
	printf("usage: ckv [-has] [file ...]\n");
	printf("  -h     print this usage information\n");
	printf("  -a     load all Lua libraries (enough to shoot yourself in the foot), including file IO\n");
	printf("  -s     silent mode (no audio processing, non-real-time)\n");
	printf("  -m N   listen on MIDI port N (without -m defaults to 0)\n");
	printf("  -m no  disable real-time MIDI input\n");
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
int
clock_set(lua_State *L)
{
	const char *attr = lua_tostring(L, 2);
	
	if(strcmp(attr, "bpm") == 0) {
		lua_Number sample_rate;
		double bpm = lua_tonumber(L, 3);
		
		ckvm_pushstdglobal(L, "sample_rate");
		sample_rate = lua_tonumber(L, -1);

		if(!ckvm_set_scheduler_rate(L, 1, bpm / (60.0 * sample_rate)))
			print_error("attempt to set invalid (negative or 0) bpm");
		
		return 0;
	}
	
	return 0;
}

static
int
clock_get(lua_State *L)
{
	const char *attr = lua_tostring(L, 2);
	
	if(strcmp(attr, "bpm") == 0) {
		lua_Number sample_rate;
		
		ckvm_pushstdglobal(L, "sample_rate");
		sample_rate = lua_tonumber(L, -1);
		
		lua_pushnumber(L, ckvm_get_scheduler_rate(L, 1) * 60.0 * sample_rate);
		return 1;
	}
	
	return 0;
}

static
int
clock_new(lua_State *L)
{
	lua_Number bpm, sample_rate, rate;
	
	if(lua_gettop(L) > 0)
		bpm = lua_tonumber(L, 1);
	else
		bpm = 120;
	
	ckvm_pushstdglobal(L, "sample_rate");
	sample_rate = lua_tonumber(L, -1);
	
	rate = 1.0 / (60.0 / bpm * sample_rate);
	
	/* the new Clock */
	ckvm_push_new_scheduler(L, rate);
	
	/* the metatable for the Clock */
	lua_createtable(L, 0 /* array */, 2 /* non-array */);
	lua_pushcfunction(L, clock_get); lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, clock_set); lua_setfield(L, -2, "__newindex");
	lua_setmetatable(L, -2);
	
	return 1;
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
	
	/* handy function for synching incoming shreds */
	(void) luaL_dostring(L,
	"function sync(period, clock) yield(period - (now(clock) % period), clock); return period end"
	);
	
	/* beat clocks */
	lua_pushcfunction(L, clock_new);
	lua_setglobal(L, "Clock");
	
	/* midi helpers */
	(void) luaL_dostring(L,
	"function mtof(m) return math.pow(2, (m-69)/12) * 440; end"
	);
	
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

int
main(int argc, char *argv[])
{
	VM vm;
	int i, c;
	int num_scripts, scripts_added;
	int silent_mode = 0; /* whether to execute without using the sound card */
	int all_libs = 0; /* whether to load even lua libraries that could screw things up */
	int midi_port = 0;
	
	vm.ckvm = ckvm_create(error_callback);
	if(vm.ckvm == NULL) {
		print_error("could not initialize VM");
		return EXIT_FAILURE;
	}
	
	vm.audio = NULL;
	vm.midi = NULL;
	
	while((c = getopt(argc, (char ** const) argv, "hsam:")) != -1)
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
		case 'm':
			if(strcmp(optarg, "no") == 0)
				midi_port = -1; /* disable MIDI input */
			else
				midi_port = atoi(optarg);
			break;
		}
	
	/* libraries must be loaded before any scripts which use them */
	
	open_base_libs(&vm, all_libs);
	
	if(!silent_mode) {
		vm.audio = ckva_open(vm.ckvm, 44100, 2);
		if(vm.audio == NULL) {
			print_error("could not initialize ckv audio");
			return EXIT_FAILURE;
		}
	}
	
	if(midi_port != -1) {
		vm.midi = ckvmidi_open(vm.ckvm);
		if(vm.midi == NULL) {
			print_error("could not initialize MIDI input");
			return EXIT_FAILURE;
		}
	}
	
	/* load the scripts specified on the command-line */
	
	num_scripts = argc - optind;
	
	scripts_added = 0;
	for(i = optind; i < argc; i++)
		if(ckvm_add_thread(vm.ckvm, argv[i]))
			scripts_added++;
	
	if(scripts_added == 0 && silent_mode)
		return num_scripts == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	
	/* begin execution */
	
	if(silent_mode) {
		
		ckvm_run(vm.ckvm);
		ckvm_destroy(vm.ckvm);
		
	} else {
		
		if(midi_port != -1 && !start_midi(midi_port))
			print_error("could not start MIDI"); /* not fatal */
		
		pthread_mutex_init(&vm.audio_done_mutex, NULL /* attr */);
		pthread_cond_init(&vm.audio_done, NULL /* attr */);
		
		pthread_mutex_lock(&vm.audio_done_mutex);
		
		if(!start_audio(render_audio, ckva_sample_rate(vm.audio), &vm)) {
			print_error("could not start audio");
			return EXIT_FAILURE;
		}
		
		/* wait for audio to finish */
		pthread_cond_wait(&vm.audio_done, &vm.audio_done_mutex);
		pthread_mutex_unlock(&vm.audio_done_mutex);
		
		stop_audio();
		ckvm_destroy(vm.ckvm);
		
	}
	
	return EXIT_SUCCESS;
}

static
void
render_audio(double *outputBuffer, double *inputBuffer, unsigned int nFrames,
             double streamTime, void *userData)
{
	VM *vm = (VM *)userData;
	MidiMsg midiMsg;
	
	while(get_midi_message(&midiMsg)) {
		if(!midiMsg.control && !midiMsg.pitch_bend) {
			if(midiMsg.velocity > 0)
				ckvmidi_dispatch_note_on(vm->midi, midiMsg.channel, midiMsg.note, midiMsg.velocity);
			else
				ckvmidi_dispatch_note_off(vm->midi, midiMsg.channel, midiMsg.note);
		}
	}
	
	ckva_fill_buffer(vm->audio, outputBuffer, inputBuffer, nFrames);
	
	if(!ckvm_running(vm->ckvm)) {
		pthread_mutex_lock(&vm->audio_done_mutex);
		pthread_cond_signal(&vm->audio_done);
		pthread_mutex_unlock(&vm->audio_done_mutex);
	}
}
