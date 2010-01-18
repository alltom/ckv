
#include "ckvm.h"
#include "pq.h"
#include <stdlib.h>
#include <time.h>

#define GLOBAL_NAMESPACE "global"
#define THREADS_TABLE "threads"
#define ERROR_MESSAGE_BUFFER_SIZE (1024)

typedef struct CKVM_Thread {
	lua_State *L;
	double now;
	CKVM vm;
} Thread;

typedef struct CKVM {
	int running;
	PQ queue;
	int num_sleeping_threads;
	lua_State *L;
	Thread main_thread;
	
	ErrorCallback err_callback;
} VM;

typedef struct Event {
	PQ waiting;
} Event;

static Thread *new_thread(VM *vm, lua_State *parentL);
static void create_standalone_thread_env(Thread *thread);
static void free_thread(Thread *thread);
static int open_ckv(lua_State *L);

static
void
error(VM *vm, const char *fmt, ...)
{
	va_list argp;
	char err_buffer[ERROR_MESSAGE_BUFFER_SIZE];
	
	va_start(argp, fmt);
	vsnprintf(err_buffer, sizeof(err_buffer), fmt, argp);
	va_end(argp);
	
	vm->err_callback(vm, err_buffer);
}

static
void
terror(VM *vm, lua_State *L, const char *fmt, ...)
{
	va_list argp;
	char err_buffer[ERROR_MESSAGE_BUFFER_SIZE];
	
	luaL_where(L, 0);
	
	va_start(argp, fmt);
	vsnprintf(err_buffer, sizeof(err_buffer), fmt, argp);
	error(vm, "%s%s", lua_tostring(L, -1), err_buffer);
	va_end(argp);
	
	lua_pop(L, 2);
}

CKVM
ckvm_create(ErrorCallback err_callback)
{
	VM *vm = malloc(sizeof(VM));
	if(vm == NULL) {
		return NULL;
	}
	
	vm->queue = new_queue(1);
	if(!vm->queue) {
		free(vm);
		return NULL;
	}
	
	vm->L = luaL_newstate();
	if(vm->L == NULL) {
		free(vm->queue);
		free(vm);
		return NULL;
	}
	
	vm->err_callback = err_callback;
	
	vm->main_thread.vm = vm;
	vm->main_thread.L = vm->L;
	vm->main_thread.now = 0;
	
	/* put main thread in thread registry */
	lua_pushlightuserdata(vm->L, vm->L);
	lua_pushlightuserdata(vm->L, &vm->main_thread);
	lua_settable(vm->L, LUA_REGISTRYINDEX);
	
	/* create table to use as global namespace */
	lua_newtable(vm->L);
	lua_setfield(vm->L, LUA_REGISTRYINDEX, GLOBAL_NAMESPACE);
	
	/* create a table for storing lua thread references */
	lua_pushstring(vm->L, THREADS_TABLE);
	lua_newtable(vm->L);
	lua_rawset(vm->L, LUA_REGISTRYINDEX);
	
	lua_pushcfunction(vm->L, open_ckv);
	lua_call(vm->L, 0, 0);
	
	vm->num_sleeping_threads = 0;
	
	vm->running = 1;
	
	return vm;
}

lua_State *
ckvm_global_state(CKVM vm)
{
	return vm->L;
}

CKVM_Thread
ckvm_add_thread(CKVM vm, const char *filename)
{
	Thread *thread;
	
	if(!vm->running)
		return NULL;
	
	thread = new_thread(vm, vm->L);
	
	if(thread == NULL) {
		error(vm, "could not allocate memory for thread %s", filename);
		return NULL;
	}
	
	create_standalone_thread_env(thread);
	
	switch(luaL_loadfile(thread->L, filename)) {
	case LUA_ERRSYNTAX:
		error(vm, "%s", lua_tostring(thread->L, -1));
		break;
	case LUA_ERRMEM:
		error(vm, "%s: memory allocation error while loading", filename);
		break;
	case LUA_ERRFILE:
		error(vm, "%s: cannot open file\n", filename);
		break;
	default:
		if(!queue_insert(vm->queue, vm->main_thread.now, thread))
			fprintf(stderr, "[ckv] %s: could not add to thread queue\n", filename);
	}
	
	return thread;
}

void
ckvm_remove_thrad(CKVM_Thread thread)
{
	VM *vm = thread->vm;
	
	if(!vm->running)
		return;
	
	remove_queue_items(vm->queue, thread);
	
	lua_pushlightuserdata(vm->L, thread->L);
	lua_pushnil(vm->L);
	lua_settable(vm->L, LUA_REGISTRYINDEX); /* registry[state] = nil */
	
	lua_getfield(vm->L, LUA_REGISTRYINDEX, THREADS_TABLE);
	lua_pushlightuserdata(vm->L, thread->L);
	lua_pushnil(vm->L);
	lua_settable(vm->L, -3); /* registry.threads[state] = nil */
	lua_pop(vm->L, 1); /* pop registry.threads */
}

double
ckvm_now(CKVM vm)
{
	return vm->main_thread.now;
}

void
ckvm_run_one(CKVM vm)
{
	Thread *thread;
	
	if(!vm->running)
		return;
	
	thread = remove_queue_min(vm->queue);
	
	vm->main_thread.now = thread->now;
	
	switch(lua_resume(thread->L, lua_gettop(thread->L) - 1)) {
	case 0:
		ckvm_remove_thrad(thread);
		break;
	case LUA_YIELD: {
		if(lua_type(thread->L, 1) == LUA_TNIL) {
			terror(vm, thread->L, "attempted to yield nil");
		} else if(lua_type(thread->L, 1) == LUA_TTABLE) {
			/* sleep on an event */
			Event *ev;
			
			lua_pushstring(thread->L, "obj");
			lua_rawget(thread->L, 1);
			ev = lua_touserdata(thread->L, -1);
			lua_pop(thread->L, 1);
			
			if(ev == NULL) {
				terror(vm, thread->L, "attempted to yield something not an event or duration");
				break;
			}
			
			queue_insert(ev->waiting, thread->now, thread);
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
		terror(vm, thread->L, "runtime error: %s", lua_tostring(thread->L, -1));
		ckvm_remove_thrad(thread);
		break;
	case LUA_ERRMEM:
		terror(vm, thread->L, "memory allocation error");
		ckvm_remove_thrad(thread);
		break;
	}
}

void
ckvm_run_until(CKVM vm, double new_now)
{
	while(vm->running && !queue_empty(vm->queue) && ((Thread *)queue_min(vm->queue))->now < new_now)
		ckvm_run_one(vm);
	
	vm->main_thread.now = new_now;
}

void
ckvm_run(CKVM vm)
{
	while(vm->running && !queue_empty(vm->queue))
		ckvm_run_one(vm);
}

int
ckvm_running(CKVM vm)
{
	return vm->running;
}

void
ckvm_destroy(CKVM vm)
{
	lua_close(vm->L);
	free(vm->queue);
	free(vm);
}


/*
private methods
*/


static
Thread *
new_thread(VM *vm, lua_State *parentL)
{
	lua_State *L; /* new thread state */
	Thread *thread;
	
	thread = malloc(sizeof(Thread));
	if(!thread)
		return NULL;
	
	/* create the thread and save a reference in THREADS_TABLE */
	/* threads are garbage collected, so reference is saved until unregister_thread */
	lua_getfield(parentL, LUA_REGISTRYINDEX, THREADS_TABLE);
	L = lua_newthread(parentL); /* pushes thread reference */
	lua_pushlightuserdata(parentL, L);
	lua_pushvalue(parentL, -2); /* push thread ref again so it's on top */
	lua_rawset(parentL, -4); /* registry.threads[thread] = lua thread ref */
	lua_pop(parentL, 2); /* pop original lua thread ref and registry.threads */
	
	lua_pushlightuserdata(L, L);
	lua_pushlightuserdata(L, thread);
	lua_settable(L, LUA_REGISTRYINDEX); /* registry[state] = thread */
	
	thread->L = L;
	thread->now = vm->main_thread.now;
	thread->vm = vm;
	
	return thread;
}

static
void
create_standalone_thread_env(Thread *thread)
{	
	/* get "global", give thread a blank env, then copy "global" to it */
	lua_pushthread(thread->L);
	lua_getfield(thread->L, LUA_REGISTRYINDEX, GLOBAL_NAMESPACE);
	lua_newtable(thread->L);
	lua_setfenv(thread->L, -3); /* makes the newtable the env for the thread we pushed above */
	lua_setglobal(thread->L, GLOBAL_NAMESPACE); /* set "global" in new namespace */
	
	lua_pop(thread->L, 1); /* pop lua thread ref; stack empty */
	
	/* copy everything in prototype global env to this env */
	lua_pushnil(thread->vm->L);  /* first key */
	while(lua_next(thread->vm->L, LUA_GLOBALSINDEX) != 0) {
		/* copy to new env */
		lua_pushvalue(thread->vm->L, -2); /* push key */
		lua_pushvalue(thread->vm->L, -2); /* push value */
		lua_xmove(thread->vm->L, thread->L, 2);
		lua_rawset(thread->L, LUA_GLOBALSINDEX);
		
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(thread->vm->L, 1);
	}
}

static
void
free_thread(Thread *thread)
{
	free(thread);
}

CKVM_Thread
ckvm_get_thread(lua_State *L)
{
	Thread *thread;
	
	lua_pushlightuserdata(L, L);
	lua_gettable(L, LUA_REGISTRYINDEX);
	thread = lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	return thread;
}

/*
Lua methods
*/

static
int
ckv_now(lua_State *L)
{
	lua_pushnumber(L, ckvm_get_thread(L)->now);
	return 1;
}

static
int
ckv_exit(lua_State *L)
{
	VM *vm = ckvm_get_thread(L)->vm;
	
	vm->running = 0;
	
	/* yield(0) to pause this thread and exit run_one */
	lua_pushnumber(L, 0);
	return lua_yield(L, 1);
}

static
int
ckv_fork(lua_State *L)
{
	Thread *parent = ckvm_get_thread(L);
	Thread *thread;
	
	thread = new_thread(parent->vm, parent->L);
	if(!thread) {
		terror(parent->vm, parent->L, "could not allocate child thread");
		return 0;
	}
	
	lua_xmove(parent->L, thread->L, lua_gettop(parent->L)); /* move function and args over */
	
	if(!queue_insert(parent->vm->queue, thread->now, thread)) {
		terror(parent->vm, parent->L, "could not enqueue child thread");
		free_thread(thread);
	}
	
	return 0;
}

static
int
ckv_fork_eval(lua_State *L)
{
	Thread *parent = ckvm_get_thread(L);
	const char *code = luaL_checkstring(parent->L, -1);
	Thread *thread;
	
	thread = new_thread(parent->vm, parent->L);
	if(!thread) {
		terror(parent->vm, parent->L, "could not allocate child thread");
		return 0;
	}
	
	switch(luaL_loadstring(thread->L, code)) {
	case LUA_ERRSYNTAX:
		terror(parent->vm, parent->L, "could not create thread: %s", lua_tostring(thread->L, -1));
		free(thread);
		break;
	case LUA_ERRMEM:
		terror(parent->vm, parent->L, "could not create thread: memory allocation error");
		free(thread);
		break;
	default:
		if(!queue_insert(parent->vm->queue, thread->now, thread))
			terror(parent->vm, parent->L, "could not enqueue child thread");
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
	double now;
	Event *ev;
	
	luaL_checktype(L, 1, LUA_TTABLE); /* event */
	now = ckvm_get_thread(L)->now;
	
	lua_getfield(L, 1, "obj");
	ev = lua_touserdata(L, -1);
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
	Event *ev;
	
	luaL_checktype(L, 1, LUA_TTABLE); /* Event */
	
	ev = malloc(sizeof(Event));
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
	lua_register(L, "exit", ckv_exit);
	lua_register(L, "fork", ckv_fork);
	lua_register(L, "fork_eval", ckv_fork_eval);
	lua_register(L, "yield", ckv_yield);
	lua_register(L, "sleep", ckv_yield);
	
	/* Event */
	lua_createtable(L, 0 /* array items */, 1 /* non-array items */);
	lua_pushcfunction(L, ckv_event_new); lua_setfield(L, -2, "new");
	lua_setglobal(L, "Event"); /* pops */
	
	(void) luaL_dostring(L,
	"forever = Event:new()"
	);
	
	return 1;
}

void
pushstdglobal(lua_State *L, const char *name)
{
	Thread *thread = ckvm_get_thread(L);
	lua_getglobal(thread->vm->L, name);
	lua_xmove(thread->vm->L, L, 1);
}
