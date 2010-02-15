
#include "ckvm.h"
#include "pq.h"
#include <stdlib.h>
#include <time.h>

#define GLOBAL_NAMESPACE "global"
#define THREADS_TABLE "threads"
#define ERROR_MESSAGE_BUFFER_SIZE (1024)

typedef struct CKVM_Thread {
	lua_State *L;
	CKVM vm;
	struct Scheduler *scheduler;
} Thread;

typedef struct Scheduler {
	PQ queue;
	double now; /* invariant: every scheduler's now always refers to the same time, with different units */
	            /* invariant: now is always updated before any thread is resumed */
	double rate;
	struct Scheduler *next; /* next in linked list of schedulers */
} Scheduler;

typedef struct CKVM {
	int running;
	double now;
	Scheduler *scheduler; /* invariant: first scheduler is always real time (audio sample units) */
	int num_sleeping_threads;
	lua_State *L;
	Thread main_thread;
	
	ErrorCallback err_callback;
} VM;

typedef struct Event {
	PQ waiting;
} Event;

static int ckv_now(lua_State *L);
static int ckv_exit(lua_State *L);
static int ckv_fork(lua_State *L);
static int ckv_fork_eval(lua_State *L);
static int ckv_yield(lua_State *L);
static int ckv_event_broadcast(lua_State *L);
static int ckv_event_new(lua_State *L);
static int open_ckv(lua_State *L);

static Scheduler *new_scheduler(double now, double rate);
static void free_scheduler(Scheduler *scheduler);
static int enqueue_thread(Scheduler *scheduler, double now, Thread *thread);
static Scheduler *scheduler_with_next_thread(VM *vm);

static Thread *new_thread(VM *vm, lua_State *parentL);
static void create_standalone_thread_env(Thread *thread);
static void free_thread(Thread *thread);
static void yield_time(CKVM vm, Thread *thread);
static void yield_on_event(CKVM vm, Thread *thread);
static void run_thread(VM *vm, Thread *thread);
static double real_time(VM *vm, Scheduler *scheduler, double t);
static void fast_forward(VM *vm, double new_now);

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
	
	vm->scheduler = new_scheduler(0 /* now */, 1 /* rate */);
	if(vm->scheduler == NULL) {
		free(vm);
		return NULL;
	}
	
	vm->L = luaL_newstate();
	if(vm->L == NULL) {
		free_scheduler(vm->scheduler);
		free(vm);
		return NULL;
	}
	
	vm->err_callback = err_callback;
	
	vm->main_thread.vm = vm;
	vm->main_thread.L = vm->L;
	vm->main_thread.scheduler = vm->scheduler;
	
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
		if(!enqueue_thread(vm->scheduler, vm->scheduler->now, thread))
			fprintf(stderr, "[ckv] %s: could not add to thread queue\n", filename);
	}
	
	return thread;
}

void
ckvm_remove_thread(CKVM_Thread thread)
{
	VM *vm = thread->vm;
	
	if(!vm->running)
		return;
	
	remove_queue_items(thread->scheduler->queue, thread);
	
	lua_pushlightuserdata(vm->L, thread->L);
	lua_pushnil(vm->L);
	lua_settable(vm->L, LUA_REGISTRYINDEX); /* registry[state] = nil */
	
	lua_getfield(vm->L, LUA_REGISTRYINDEX, THREADS_TABLE);
	lua_pushlightuserdata(vm->L, thread->L);
	lua_pushnil(vm->L);
	lua_settable(vm->L, -3); /* registry.threads[state] = nil */
	lua_pop(vm->L, 1); /* pop registry.threads */
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

double
ckvm_now(CKVM vm)
{
	return vm->scheduler->now;
}

void
ckvm_run_one(CKVM vm)
{
	double new_now;
	Thread *thread;
	Scheduler *scheduler;
	
	if(!vm->running)
		return;
	
	scheduler = scheduler_with_next_thread(vm);
	if(scheduler == NULL)
		return;
	
	new_now = real_time(vm, scheduler, queue_min_priority(scheduler->queue));
	thread = remove_queue_min(scheduler->queue);
	fast_forward(vm, new_now);
	run_thread(vm, thread);
}

void
ckvm_run_until(CKVM vm, double new_now)
{
	double now;
	Thread *thread;
	Scheduler *scheduler;
	
	while(vm->running) {
		scheduler = scheduler_with_next_thread(vm);
		if(scheduler == NULL)
			break;

		now = real_time(vm, scheduler, queue_min_priority(scheduler->queue));
		thread = queue_min(scheduler->queue);
		
		if(now > new_now)
			break;
		
		remove_queue_min(scheduler->queue);
		fast_forward(vm, now);
		run_thread(vm, thread);
	}
	
	fast_forward(vm, new_now);
}

void
ckvm_run(CKVM vm)
{
	double new_now;
	Thread *thread;
	Scheduler *scheduler;
	
	while(vm->running) {
		scheduler = scheduler_with_next_thread(vm);
		if(scheduler == NULL)
			return;

		new_now = real_time(vm, scheduler, queue_min_priority(scheduler->queue));
		thread = remove_queue_min(scheduler->queue);
		
		fast_forward(vm, new_now);
		run_thread(vm, thread);
	}
}

int
ckvm_running(CKVM vm)
{
	return vm->running;
}

void
ckvm_destroy(CKVM vm)
{
	Scheduler *scheduler;
	
	lua_close(vm->L);
	
	scheduler = vm->scheduler;
	while(scheduler != NULL) {
		Scheduler *next = scheduler->next;
		free_scheduler(scheduler);
		scheduler = next;
	}
	
	free(vm);
}

void
ckvm_pushstdglobal(lua_State *L, const char *name)
{
	Thread *thread = ckvm_get_thread(L);
	lua_getglobal(thread->vm->L, name);
	lua_xmove(thread->vm->L, L, 1);
}

void
ckvm_push_new_scheduler(lua_State *L, double rate)
{
	Thread *thread = ckvm_get_thread(L);
	Scheduler *main_scheduler = thread->vm->scheduler;
	Scheduler *scheduler = new_scheduler(0, rate);
	
	/* insert after main scheduler */
	scheduler->next = main_scheduler->next;
	main_scheduler->next = scheduler;
	
	lua_pushlightuserdata(L, scheduler);
}

int
ckvm_set_scheduler_rate(lua_State *L, int stack_index, double rate)
{
	Scheduler *scheduler = lua_touserdata(L, stack_index);
	
	if(rate <= 0)
		return -1;
	
	scheduler->rate = rate;
	return 1;
}

double
ckvm_get_scheduler_rate(lua_State *L, int stack_index)
{
	Scheduler *scheduler = lua_touserdata(L, stack_index);
	return scheduler->rate;
}


/*
Lua methods
*/

static
int
ckv_now(lua_State *L)
{
	Thread *thread = ckvm_get_thread(L);
	lua_pushnumber(L, thread->scheduler->now);
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
	
	if(!enqueue_thread(parent->vm->scheduler, parent->vm->scheduler->now, thread)) {
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
		if(!enqueue_thread(parent->vm->scheduler, parent->vm->scheduler->now, thread))
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
	now = ckvm_get_thread(L)->vm->scheduler->now;
	
	lua_getfield(L, 1, "obj");
	ev = lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	while(!queue_empty(ev->waiting)) {
		Thread *thread = remove_queue_min(ev->waiting);
		if(!enqueue_thread(thread->vm->scheduler, thread->vm->scheduler->now, thread))
			terror(thread->vm, thread->L, "could not insert woken thread into scheduler");
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
	lua_register(L, "y", ckv_yield);
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


/*
private methods
*/

static
Scheduler *
new_scheduler(double now, double rate)
{
	Scheduler *scheduler = malloc(sizeof(Scheduler));
	if(!scheduler)
		return NULL;
	
	scheduler->queue = new_queue(1);
	if(!scheduler->queue) {
		free(scheduler);
		return NULL;
	}
	
	scheduler->now = now;
	scheduler->rate = rate;
	scheduler->next = NULL;
	
	return scheduler;
}

static
void
free_scheduler(Scheduler *scheduler)
{
	free(scheduler->queue);
	free(scheduler);
}

static
int
enqueue_thread(Scheduler *scheduler, double now, Thread *thread)
{
	return queue_insert(scheduler->queue, now, thread);
}

static
Scheduler *
scheduler_with_next_thread(VM *vm)
{
	int found = 0;
	double earliest_now = 0;
	Scheduler *earliest_scheduler = NULL;
	Scheduler *scheduler = vm->scheduler;
	
	while(scheduler != NULL) {
		double now = queue_min_priority(scheduler->queue);
		double real_now = real_time(vm, scheduler, now);
		
		if(!queue_empty(scheduler->queue) && (!found || real_now < earliest_now)) {
			earliest_now = real_now;
			earliest_scheduler = scheduler;
			found = 1;
		}
		
		scheduler = scheduler->next;
	}
	
	return earliest_scheduler;
}

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
	thread->vm = vm;
	thread->scheduler = vm->scheduler;
	
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

static
void
yield_time(CKVM vm, Thread *thread)
{
	lua_Number amount;
	Scheduler *scheduler;
	
	/* stack has duration, and possibly a scheduler */
	
	amount = luaL_checknumber(thread->L, 1);
	
	if(lua_type(thread->L, 2) == LUA_TLIGHTUSERDATA)
		scheduler = lua_touserdata(thread->L, 2);
	else
		scheduler = thread->vm->scheduler;
	
	if(amount > 0) {
		enqueue_thread(scheduler, scheduler->now + amount, thread);
	} else {
		terror(thread->vm, thread->L, "attempted to yield negative time");
		enqueue_thread(scheduler, scheduler->now, thread);
	}
	
	/* for when they resume */
	lua_pushvalue(thread->L, 1);
}

static
void
yield_on_event(CKVM vm, Thread *thread)
{
	Event *ev;
	
	/* stack has event object */
	
	lua_pushstring(thread->L, "obj");
	lua_rawget(thread->L, 1);
	ev = lua_touserdata(thread->L, -1);
	lua_pop(thread->L, 1);
	
	if(ev == NULL) {
		terror(vm, thread->L, "attempted to yield something not an event or duration");
		return;
	}
	
	queue_insert(ev->waiting, thread->vm->scheduler->now, thread);
	vm->num_sleeping_threads++;
	
	/* for when they resume */
	lua_pushvalue(thread->L, 1);
}

static
void
run_thread(VM *vm, Thread *thread)
{
	switch(lua_resume(thread->L, lua_gettop(thread->L) - 1)) {
	case 0:
		ckvm_remove_thread(thread);
		break;
	case LUA_YIELD: {
		if(lua_type(thread->L, 1) == LUA_TNIL) {
			terror(vm, thread->L, "attempted to yield nil");
		} else if(lua_type(thread->L, 1) == LUA_TTABLE) {
			yield_on_event(vm, thread);
		} else {
			yield_time(vm, thread);
		}
		
		break;
	}
	case LUA_ERRRUN:
		terror(vm, thread->L, "runtime error: %s", lua_tostring(thread->L, -1));
		ckvm_remove_thread(thread);
		break;
	case LUA_ERRMEM:
		terror(vm, thread->L, "memory allocation error");
		ckvm_remove_thread(thread);
		break;
	}
}

static
double
real_time(VM *vm, Scheduler *scheduler, double t)
{
	if(scheduler == vm->scheduler)
		return t; /* avoid rounding errors by not using below expression below (identity) */
	
	return vm->scheduler->now + (t - scheduler->now) / scheduler->rate;
}

static
void
fast_forward(VM *vm, double new_now)
{
	double dt = new_now - vm->scheduler->now;
	Scheduler *scheduler = vm->scheduler->next;
	
	vm->scheduler->now = new_now;
	while(scheduler != NULL) {
		scheduler->now += dt * scheduler->rate;
		scheduler = scheduler->next;
	}
}
