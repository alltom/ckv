
/* TODO:
 * - keep one thread from crashing the VM
 * - random functions (rand, maybe, etc)
 */

#include "ckv.h"
#include "pq.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define GLOBAL_NAMESPACE "global"
#define THREADS_TABLE "threads"

typedef struct Event {
	PQ waiting;
	unsigned long int next_pri;
} Event;

typedef struct Thread {
	const char *filename;
	lua_State *L;
	double now;
	VMPtr vm;
} Thread;

typedef struct VM {
	PQ queue;
	int num_sleeping_threads;
	lua_State *L; /* global global state */
	double now;
	int stopped;
	
	unsigned int audio_now; /* used in audio callback; unused in silent mode */
	int sample_rate;
	int channels;
} VM;

VM *g_vm; /* TODO: remove this HACK once again! */

static int ckv_event_new(lua_State *L);
static int open_ckv(lua_State *L);

static
void
print_warning(lua_State *L, const char *fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	luaL_where(L, 1);
	lua_pushvfstring(L, fmt, argp);
	va_end(argp);
	lua_concat(L, 2);
	fprintf(stderr, "[ckv] %s\n", lua_tostring(L, -1));
	lua_pop(L, 1);
}

/* returns 0 on failure */
static
int
init_vm(VM *vm, int all_libs)
{
	vm->now = 0;
	vm->stopped = 0;
	vm->num_sleeping_threads = 0;
	
	vm->audio_now = 0;
	vm->sample_rate = 44100;
	vm->channels = 1;
	
	vm->L = luaL_newstate();
	if(vm->L == NULL) {
		fprintf(stderr, "[ckv] cannot init lua\n");
		return 0;
	}
	
	vm->queue = new_queue(1);
	if(!vm->queue) {
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
	/* TODO: free events */
	/* TODO: free threads */
	free(vm->queue);
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

static
void
run_one(VM *vm)
{
	Thread *thread = remove_queue_min(vm->queue);
	
	if(thread == NULL)
		return;
	
	vm->now = thread->now;
	
	switch(lua_resume(thread->L, lua_gettop(thread->L) - 1)) {
	case 0:
		unregister_thread(vm, thread);
		break;
	case LUA_YIELD: {
		if(lua_type(thread->L, 1) == LUA_TNIL) {
			print_warning(thread->L, "attempted to yield nil");
		} else if(lua_type(thread->L, 1) == LUA_TTABLE) {
			/* sleep on an event */
			
			lua_pushstring(thread->L, "obj");
			lua_rawget(thread->L, 1);
			Event *ev = lua_touserdata(thread->L, -1);
			lua_pop(thread->L, 1);
			
			if(ev == NULL) {
				print_warning(thread->L, "attempted to yield something not an event or duration");
				break;
			}
			
			queue_insert(ev->waiting, ev->next_pri++, thread);
			vm->num_sleeping_threads++;
			
			/* for when they resume */
			lua_pushvalue(thread->L, 1);
		} else {
			/* yield some samples */
			
			lua_Number amount = luaL_checknumber(thread->L, -1);
			if(amount > 0)
				thread->now += amount;
			
			queue_insert(vm->queue, thread->now, thread);
			
			/* for when they resume */
			lua_pushvalue(thread->L, 1);
		}
		
		break;
	}
	case LUA_ERRRUN:
		print_warning(thread->L, "runtime error: %s", lua_tostring(thread->L, -1));
		unregister_thread(vm, thread);
		break;
	case LUA_ERRMEM:
		print_warning(thread->L, "memory allocation error");
		unregister_thread(vm, thread);
		break;
	}
}

static
void
run(VM *vm)
{
	/* note: will exit when all threads have died OR are asleep */
	while(!queue_empty(vm->queue))
		run_one(vm);
}

static
void
render_audio(double *outputBuffer, double *inputBuffer, unsigned int nFrames,
             double streamTime, void *userData)
{
	VM *vm = (VM *)userData;
	unsigned int i;
	
	for(i = 0; i < nFrames; i++) {
		if(vm->now < vm->audio_now) {
			while(!queue_empty(vm->queue) && ((Thread *)queue_min(vm->queue))->now < vm->audio_now)
				run_one(vm);
			
			if(queue_empty(vm->queue) && vm->num_sleeping_threads == 0) {
				vm->stopped = 1;
				/* TODO: if we finish this buffer, UGens will be
				         asked for samples past the point where all
				         threads have died. This might not be okay. */
			}
			
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
	int i, c;
	int num_scripts;
	
	int silent_mode = 0; /* whether to execute without using the sound card */
	int all_libs = 0; /* whether to load even lua libraries that could screw things up */
	
	while((c = getopt(argc, (char ** const) argv, "sa")) != -1)
		switch(c) {
		case 's':
			silent_mode = 1;
			break;
		case 'a':
			all_libs = 1;
			break;
		}
	
	num_scripts = argc - optind;
	
	if(!init_vm(&vm, all_libs)) {
		fprintf(stderr, "[ckv] could not initialize VM\n");
		return EXIT_FAILURE;
	}
	
	g_vm = &vm;
	
	for(i = optind; i < argc; i++) {
		Thread *thread = new_thread(vm.L, argv[i], 0, &vm);
		prepenv(&vm, thread->L);
		
		switch(luaL_loadfile(thread->L, thread->filename)) {
		case LUA_ERRSYNTAX:
			fprintf(stderr, "[ckv] %s\n", lua_tostring(thread->L, -1));
			break;
		case LUA_ERRMEM:
			fprintf(stderr, "[ckv] %s: memory allocation error while loading script\n", thread->filename);
			break;
		case LUA_ERRFILE:
			fprintf(stderr, "[ckv] %s: cannot open file\n", thread->filename);
			break;
		default:
			if(!queue_insert(vm.queue, 0, thread))
				fprintf(stderr, "[ckv] %s: could not add to thread queue\n", thread->filename);
		}
	}
	
	if(queue_empty(vm.queue))
		return num_scripts == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	
	if(silent_mode) {
		
		run(&vm);
		
	} else {
		
		if(!start_audio(render_audio, vm.sample_rate, &vm)) {
			fprintf(stderr, "[ckv] could not start audio\n");
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

static
Thread *
get_thread(lua_State *L)
{
	lua_pushlightuserdata(L, L);
	lua_gettable(L, LUA_REGISTRYINDEX);
	Thread *thread = lua_touserdata(L, -1);
	lua_pop(L, 1);
	return thread;
}

/* TODO: make this a Lua variable instead of a function */
static
int
ckv_now(lua_State *L)
{
	lua_pushnumber(L, g_vm->now);
	return 1;
}

static
int
ckv_fork(lua_State *L)
{
	Thread *parent = get_thread(L); /* TODO: what if a UGen forks? */
	
	Thread *thread = new_thread(parent->L, "", parent->now, parent->vm);
	if(!thread) {
		print_warning(parent->L, "could not allocate child thread");
		return 0;
	}
	
	lua_xmove(parent->L, thread->L, lua_gettop(parent->L)); /* move function and args over */
	
	if(!queue_insert(parent->vm->queue, thread->now, thread))
		print_warning(parent->L, "could not enqueue child thread");
	
	return 0;
}

static
int
ckv_fork_eval(lua_State *L)
{
	Thread *parent = get_thread(L); /* TODO: what if a UGen forks? */
	const char *code = luaL_checkstring(parent->L, -1);
	
	Thread *thread = new_thread(parent->L, "", parent->now, parent->vm);
	if(!thread) {
		print_warning(parent->L, "could not allocate child thread");
		return 0;
	}
	
	switch(luaL_loadstring(thread->L, code)) {
	case LUA_ERRSYNTAX:
		print_warning(parent->L, "could not create thread: %s", lua_tostring(thread->L, -1));
		free(thread);
		break;
	case LUA_ERRMEM:
		print_warning(parent->L, "could not create thread: memory allocation error");
		free(thread);
		break;
	default:
		if(!queue_insert(parent->vm->queue, thread->now, thread))
			print_warning(parent->L, "could not enqueue child thread");
	}
	
	return 0;
}

static
int
ckv_yield(lua_State *L)
{
	return lua_yield(L, lua_gettop(L));
}

static
int
ckv_event_broadcast(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE); /* event */
	double now = get_thread(L)->vm->now; /* TODO: what if a UGen broadcasts? */
	
	lua_getfield(L, 1, "obj");
	Event *ev = lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	while(!queue_empty(ev->waiting)) {
		Thread *thread = remove_queue_min(ev->waiting);
		thread->now = now;
		queue_insert(thread->vm->queue, thread->now, thread);
		thread->vm->num_sleeping_threads--;
	}
	
	return 0;
}

static
int
ckv_event_new(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE); /* Event */
	
	Event *ev = malloc(sizeof(Event));
	if(ev == NULL) {
		lua_pushnil(L);
		return 1;
	}
	
	ev->waiting = new_queue(1);
	if(ev->waiting == NULL) {
		free(ev);
		lua_pushnil(L);
		return 1;
	}
	
	ev->next_pri = 0;
	
	/* our new_event object */
	lua_createtable(L, 0 /* array items */, 1 /* non-array items */);
	
	lua_pushstring(L, "obj");
	lua_pushlightuserdata(L, ev);
	lua_rawset(L, -3);
	
	lua_pushcfunction(L, ckv_event_broadcast);
	lua_setfield(L, -2, "broadcast");
	
	return 1;
}

/* opens ckv functions */
static
int
open_ckv(lua_State *L) {
	lua_register(L, "now", ckv_now);
	lua_register(L, "fork", ckv_fork);
	lua_register(L, "fork_eval", ckv_fork_eval);
	lua_register(L, "yield", ckv_yield);
	
	/* Event */
	lua_createtable(L, 0 /* array items */, 1 /* non-array items */);
	lua_pushcfunction(L, ckv_event_new); lua_setfield(L, -2, "new");
	lua_setglobal(L, "Event"); /* pops */
	
	return 1;
}
