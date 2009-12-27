
#include "ckv.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define GLOBAL_NAMESPACE "global"
#define THREADS_TABLE "threads"

typedef struct Thread {
	const char *filename;
	lua_State *L;
	double now;
	VMPtr vm;
} Thread;

typedef struct {
	int count, capacity;
	Thread **threads;
} ThreadQueue;

typedef struct VM {
	ThreadQueue queue;
	Thread *current_thread;
	lua_State *L; /* global global state */
	double now;
	int stopped;
	
	unsigned int audio_now; /* used in audio callback; unused in silent mode */
	int sample_rate;
	int channels;
} VM;

VM *g_vm; /* TODO: remove this HACK once again! */

/* returns 0 on failure */
static
int
init_vm(VM *vm, int all_libs)
{
	vm->now = 0;
	vm->stopped = 0;
	
	vm->audio_now = 0;
	vm->sample_rate = 44100;
	vm->channels = 1;
	
	vm->L = luaL_newstate();
	if(vm->L == NULL) {
		fprintf(stderr, "cannot init lua\n");
		return 0;
	}
	
	vm->queue.count = 0;
	vm->queue.capacity = 10;
	vm->queue.threads = malloc(sizeof(Thread) * vm->queue.capacity);
	if(!vm->queue.threads) {
		lua_close(vm->L);
		return 0;
	}
	
	/* create table to use as global namespace */
	lua_newtable(vm->L);
	lua_setfield(vm->L, LUA_REGISTRYINDEX, GLOBAL_NAMESPACE);
	
	/* create a table for storing lua thread references */
	lua_pushstring(vm->L, THREADS_TABLE);
	lua_newtable(vm->L);
	lua_rawset(vm->L, LUA_REGISTRYINDEX);
	
	/* set the sample rate */
	lua_pushnumber(vm->L, vm->sample_rate);
	lua_setglobal(vm->L, "sample_rate");
	
	/* load all the libraries a thread could need */
	lua_gc(vm->L, LUA_GCSTOP, 0); /* stop collector during initialization */
	if(all_libs) {
		lua_pushcfunction(vm->L, luaopen_base); lua_call(vm->L, 0, 0);
		lua_pushcfunction(vm->L, luaopen_package); lua_call(vm->L, 0, 0);
		lua_pushcfunction(vm->L, luaopen_io); lua_call(vm->L, 0, 0);
		lua_pushcfunction(vm->L, luaopen_os); lua_call(vm->L, 0, 0);
	} else {
		lua_pushcfunction(vm->L, open_ckvbaselite); lua_call(vm->L, 0, 0);
	}
	lua_pushcfunction(vm->L, luaopen_string); lua_call(vm->L, 0, 0);
	lua_pushcfunction(vm->L, luaopen_table); lua_call(vm->L, 0, 0);
	lua_pushcfunction(vm->L, luaopen_math); lua_call(vm->L, 0, 0);
	lua_pushcfunction(vm->L, open_ckv); lua_call(vm->L, 0, 0);
	lua_pushcfunction(vm->L, open_ckvugen); lua_call(vm->L, 0, 0);
	lua_gc(vm->L, LUA_GCRESTART, 0);
	
	return 1;
}

static
void
close_vm(VM *vm)
{
	/* TODO: free threads */
	free(vm->queue.threads);
	lua_close(vm->L);
}

/*
creates a new thread and adds reference to THREADS_TABLE
*/
static
Thread *
new_thread(lua_State *L, const char *filename, double now, VM *vm)
{
	lua_State *s; /* new thread state */
	Thread *thread;
	
	thread = malloc(sizeof(Thread));
	if(!thread)
		return NULL;
	
	/* create the thread and save a reference in THREADS_TABLE */
	/* threads are garbage collected, so reference is saved until unregister_thread */
	lua_getfield(L, LUA_REGISTRYINDEX, THREADS_TABLE);
	s = lua_newthread(L); /* pushes thread reference */
	lua_pushlightuserdata(L, s);
	lua_pushvalue(L, -2); /* push thread ref again so it's on top */
	lua_rawset(L, -4); /* registry.threads[thread] = lua thread ref */
	lua_pop(L, 2); /* pop original lua thread ref and registry.threads */
	
	lua_pushlightuserdata(s, s);
	lua_pushlightuserdata(s, thread);
	lua_settable(s, LUA_REGISTRYINDEX); /* registry[state] = thread */
	
	thread->filename = filename;
	thread->L = s;
	thread->now = now;
	thread->vm = vm;
	
	return thread;
}

static
void
unregister_thread(VM *vm, Thread *thread)
{
	lua_pushlightuserdata(vm->L, thread->L);
	lua_pushnil(vm->L);
	lua_settable(vm->L, LUA_REGISTRYINDEX); /* registry[state] = nil */
	
	lua_getfield(vm->L, LUA_REGISTRYINDEX, THREADS_TABLE);
	lua_pushlightuserdata(vm->L, thread->L);
	lua_pushnil(vm->L);
	lua_settable(vm->L, -3); /* registry.threads[state] = nil */
	lua_pop(vm->L, 1); /* pop registry.threads */
}

/*
Create a private global namespace for this thread,
leaving a reference ("global") to the global global namespace.
This is only done for files. Their children inherit the parent's namepsace.
*/
static
void
prepenv(VM *vm, lua_State *L)
{
	/* get lua thread ref */
	lua_pushthread(L);
	
	/* get "global", give thread a blank env, then copy "global" to it */
	lua_getfield(L, LUA_REGISTRYINDEX, GLOBAL_NAMESPACE);
	lua_newtable(L);
	lua_setfenv(L, -3); /* makes the newtable the env for the thread we pushed above */
	lua_setglobal(L, GLOBAL_NAMESPACE); /* set "global" in new namespace */
	
	lua_pop(L, 1); /* pop lua thread ref; stack empty */
	
	/* copy everything in prototype global env to this env */
	lua_pushnil(vm->L);  /* first key */
	while(lua_next(vm->L, LUA_GLOBALSINDEX) != 0) {
		/* copy to new env */
		lua_pushvalue(vm->L, -2); /* push key */
		lua_pushvalue(vm->L, -2); /* push value */
		lua_xmove(vm->L, L, 2);
		lua_rawset(L, LUA_GLOBALSINDEX);
		
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(vm->L, 1);
	}
}

/* returns 0 on failure */
static
int
schedule_thread(VM *vm, Thread *thread)
{
	if(vm->queue.count == vm->queue.capacity) {
		fprintf(stderr, "resizing thread queue\n");
		Thread **new_thread_array = realloc(vm->queue.threads, vm->queue.capacity * 2 * sizeof(Thread));
		if(new_thread_array == NULL) {
			fprintf(stderr, "could not resize thread queue\n");
			return 0;
		}
		
		vm->queue.threads = new_thread_array;
	}
	
	vm->queue.threads[vm->queue.count++] = thread;
	return 1;
}

static
Thread *
next_thread(ThreadQueue *queue)
{
	double min_now;
	int i, min_thread;
	
	if(queue->count == 0)
		return NULL;
	
	min_now = queue->threads[0]->now;
	min_thread = 0;
	for(i = 1; i < queue->count; i++) {
		if(queue->threads[i]->now < min_now) {
			min_now = queue->threads[i]->now;
			min_thread = i;
		}
	}
	
	return queue->threads[min_thread];
}

static
void
unschedule_thread(VM *vm, Thread *thread)
{
	int i;
	for(i = 0; i < vm->queue.count; i++) {
		if(vm->queue.threads[i] == thread) {
			for(; i < vm->queue.count - 1; i++)
				vm->queue.threads[i] = vm->queue.threads[i+1];
			vm->queue.count--;
			return;
		}
	}
}

static
void
run_one(Thread *thread)
{
	VM *vm = thread->vm;
	
	switch(lua_resume(thread->L, lua_gettop(thread->L) - 1)) {
	case 0:
		unschedule_thread(vm, thread);
		unregister_thread(vm, thread);
		break;
	case LUA_YIELD: {
		lua_Number amount = luaL_checknumber(thread->L, -1);
		if(amount > 0)
			thread->now += amount;
		break;
	}
	case LUA_ERRRUN:
		fprintf(stderr, "runtime error: %s\n", lua_tostring(thread->L, -1));
		unschedule_thread(vm, thread);
		unregister_thread(vm, thread);
		break;
	case LUA_ERRMEM:
		fprintf(stderr, "memory allocation error while running '%s'\n", thread->filename);
		unschedule_thread(vm, thread);
		unregister_thread(vm, thread);
		break;
	}
}

static
void
run(VM *vm)
{
	Thread *thread;
	while((thread = next_thread(&vm->queue)) != NULL) {
		vm->now = thread->now;
		run_one(thread);
	}
}

static
void
render_audio(double *outputBuffer, double *inputBuffer, unsigned int nFrames,
             double streamTime, void *userData)
{
	VM *vm = (VM *)userData;
	Thread *thread;
	unsigned int i;
	
	for(i = 0; i < nFrames; i++) {
		if(vm->now < vm->audio_now) {
			while((thread = next_thread(&vm->queue)) != NULL && thread->now < vm->audio_now) {
				vm->now = thread->now;
				run_one(thread);
			}
			
			if(vm->queue.count == 0)
				vm->stopped = 1;
			
			vm->now = vm->audio_now;
		}
		
		lua_getglobal(vm->L, "dac");
		lua_getfield(vm->L, -1, "tick");
		lua_pushvalue(vm->L, -2); /* push dac */
		lua_call(vm->L, 1, 1); /* dac.tick(dac) yields a sample */
		outputBuffer[i * 2] = lua_tonumber(vm->L, -1);
		outputBuffer[i * 2 + 1] = outputBuffer[i * 2];
		lua_pop(vm->L, 2); /* pop sample and dac */
		
		vm->audio_now++;
	}
}

int
main(int argc, const char *argv[])
{
	VM vm;
	int num_scripts = argc - 1;
	int i;
	
	int silent_mode = 0; /* whether to execute without using the sound card */
	int all_libs = 0; /* whether to load even lua libraries that could screw things up */
	
	if(!init_vm(&vm, all_libs)) {
		fprintf(stderr, "could not initialize VM\n");
		return EXIT_FAILURE;
	}
	
	g_vm = &vm;
	
	for(i = 0; i < num_scripts; i++) {
		Thread *thread = new_thread(vm.L, argv[i + 1], 0, &vm);
		prepenv(&vm, thread->L);
		
		switch(luaL_loadfile(thread->L, thread->filename)) {
		case LUA_ERRSYNTAX:
			fprintf(stderr, "syntax error: %s\n", lua_tostring(thread->L, -1));
			break;
		case LUA_ERRMEM:
			fprintf(stderr, "memory allocation error while loading '%s'\n", thread->filename);
			break;
		case LUA_ERRFILE:
			fprintf(stderr, "cannot open script '%s'\n", thread->filename);
			break;
		default:
			if(!schedule_thread(&vm, thread))
				fprintf(stderr, "could not add '%s' to thread queue\n", thread->filename);
		}
	}
	
	if(next_thread(&vm.queue) == NULL)
		return num_scripts == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	
	if(silent_mode) {
		
		run(&vm);
		
	} else {
		
		if(!start_audio(render_audio, vm.sample_rate, &vm)) {
			fprintf(stderr, "could not start audio\n");
			return EXIT_FAILURE;
		}
		
		while(!vm.stopped)
			sleep(1); /* TODO: please fix this HACK */
		
		stop_audio();
		
	}
	
	close_vm(&vm);
	
	return EXIT_SUCCESS;
}

/* non-static functions mostly for calling from ckvlib */

Thread *
get_thread(lua_State *L)
{
	lua_pushlightuserdata(L, L);
	lua_gettable(L, LUA_REGISTRYINDEX);
	Thread *thread = lua_touserdata(L, -1);
	lua_pop(L, 1);
	return thread;
}

double
now(Thread *thread)
{
	if(thread)
		return thread->now;
	else
		return g_vm->now;
}

void
fork_child(Thread *parent)
{
	Thread *thread = new_thread(parent->L, "", parent->now, parent->vm);
	if(!thread) {
		fprintf(stderr, "could not allocate thread for child thread of '%s'\n", parent->filename);
		return;
	}
	
	lua_xmove(parent->L, thread->L, lua_gettop(parent->L)); /* move function and args over */
	
	if(!schedule_thread(parent->vm, thread))
		fprintf(stderr, "could not add '%s' to thread queue\n", thread->filename);
}

void
fork_child_with_eval(Thread *parent)
{
	const char *code = luaL_checkstring(parent->L, -1);
	Thread *thread = new_thread(parent->L, "", parent->now, parent->vm);
	if(!thread) {
		fprintf(stderr, "could not allocate thread for child thread of '%s'\n", parent->filename);
		return;
	}
	
	switch(luaL_loadstring(thread->L, code)) {
	case LUA_ERRSYNTAX:
		fprintf(stderr, "could not create thread: syntax error: %s\n", lua_tostring(thread->L, -1));
		free(thread);
		break;
	case LUA_ERRMEM:
		fprintf(stderr, "could not create thread: memory allocation error\n");
		free(thread);
		break;
	default:
		if(!schedule_thread(parent->vm, thread))
			fprintf(stderr, "could not add to thread queue\n");
	}
}
