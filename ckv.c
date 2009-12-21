
#include "ckv.h"

#include <stdio.h>
#include <stdlib.h>

#define THREADS_TABLE "threads"
#define GLOBAL_NAMESPACE "global"

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
} VM;

/* returns 0 on failure */
static
int
init_vm(VM *vm)
{
	vm->now = 0;
	
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
	
	/*
	L is never executed.
	It's just used for holding references to the other threads
	  to keep them from being garbage collected.
	Its global environment is used for shred-accessible cross-shredd storage.
	*/
	lua_newtable(vm->L); /* push an empty table */
	lua_setglobal(vm->L, THREADS_TABLE); /* call the empty table THREADS_TABLE (pops table) */
	
	return 1;
}

static
void
close_vm(VM *vm)
{
	free(vm->queue.threads);
	lua_close(vm->L);
	/* free threads? */
}

/*
creates a new thread and adds references to THREADS_TABLE in gL
*/
static
Thread *
new_thread(lua_State *L, const char *filename, double now, VM *vm)
{
	Thread *thread = malloc(sizeof(Thread));
	if(!thread)
		return NULL;
	
	/* append reference to this thread to THREADS_TABLE */
	lua_getglobal(L, THREADS_TABLE); /* push threads table */
	
	lua_pushlightuserdata(L, thread); /* push pointer to Thread */
	lua_State *s = lua_newthread(L); /* push thread reference to L's stack */
	lua_settable(L, -3); /* threads[thread] = s (pops s and thread) */
	
	lua_pushlightuserdata(L, s); /* push pointer to s */
	lua_pushlightuserdata(L, thread); /* push pointer to Thread */
	lua_settable(L, -3); /* threads[s] = thread (pops s and thread) */
	
	lua_pop(L, 1); /* pop THREADS_TABLE */
	
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
	lua_getglobal(vm->L, THREADS_TABLE); /* push threads table */
	
	lua_pushlightuserdata(vm->L, thread); /* push pointer to Thread */
	lua_pushnil(vm->L); /* pushes nil */
	lua_settable(vm->L, -3); /* threads[s] = nil (pops s and thread) */
	
	lua_pushlightuserdata(vm->L, thread->L); /* push pointer to Thread */
	lua_pushnil(vm->L); /* pushes nil */
	lua_settable(vm->L, -3); /* threads[s] = nil (pops s and thread) */
	
	lua_pop(vm->L, 1); /* pop THREADS_TABLE */
}

/*
Create a private global namespace for this thread,
leaving a reference ("global") to the global global namespace.
This is only done for files. Their children inherit the parent's namepsace.
*/
static
void
prepenv(lua_State *L)
{
	lua_pushvalue(L, LUA_GLOBALSINDEX); /* push globals table */
	lua_newtable(L); /* push an empty table */
	lua_setfenv(L, 0); /* set env to the empty table */
	lua_setglobal(L, GLOBAL_NAMESPACE); /* import old global env as "global" */
	
	/* load all the libraries shreds could need */
	lua_gc(L, LUA_GCSTOP, 0);  /* stop collector during initialization */
	lua_pushcfunction(L, luaopen_string); lua_call(L, 0, 0);
	lua_pushcfunction(L, luaopen_table); lua_call(L, 0, 0);
	lua_pushcfunction(L, luaopen_math); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ckv); lua_call(L, 0, 0);
	lua_gc(L, LUA_GCRESTART, 0);
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

int
main(int argc, const char *argv[])
{
	VM vm;
	int num_scripts = argc - 1;
	int i;
	
	if(!init_vm(&vm)) {
		fprintf(stderr, "could not initialize VM\n");
		return EXIT_FAILURE;
	}
	
	for(i = 0; i < num_scripts; i++) {
		Thread *thread = new_thread(vm.L, argv[i + 1], 0, &vm);
		prepenv(thread->L);
		
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
	
	while(vm.queue.count > 0) {
		Thread *thread = next_thread(&vm.queue);
		
		switch(lua_resume(thread->L, lua_gettop(thread->L) - 1)) {
		case 0:
			unregister_thread(&vm, thread);
			unschedule_thread(&vm, thread);
			break;
		case LUA_YIELD: {
			lua_Number amount = luaL_checknumber(thread->L, -1);
			if(amount > 0)
				thread->now += amount;
			break;
		}
		case LUA_ERRRUN:
			fprintf(stderr, "runtime error in '%s': %s\n", thread->filename, lua_tostring(thread->L, -1));
			unschedule_thread(&vm, thread);
			break;
		case LUA_ERRMEM:
			fprintf(stderr, "memory allocation error while running '%s'\n", thread->filename);
			unschedule_thread(&vm, thread);
			break;
		}
	}
	
	close_vm(&vm);
	
	return EXIT_SUCCESS;
}

/* non-static functions mostly for calling from ckvlib */

Thread *
get_thread(lua_State *L)
{
	lua_getglobal(L, GLOBAL_NAMESPACE); /* push globals namespace */
	lua_pushstring(L, THREADS_TABLE); /* push "threads" */
	lua_gettable(L, -2); /* get globals["threads"] (pops "threads" and pushes threads table) */
	lua_pushlightuserdata(L, L); /* push pointer to thread state */
	lua_gettable(L, -2); /* gets threads[thread state] (pops thread state ptr and pushes ThreadPtr) */
	Thread *thread = lua_touserdata(L, -1);
	lua_pop(L, 3); /* pops ThreadPtr, threads table, global namespace */
	return thread;
}

double
now(Thread *thread)
{
	return thread->now;
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
