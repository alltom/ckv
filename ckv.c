
#include "ckv.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
	const char *filename;
	lua_State *L;
	double now;
} Thread;

typedef struct {
	int count, capacity;
	Thread **threads;
} ThreadQueue;

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
	lua_setglobal(L, "global"); /* import old global env as "global" */
	
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
add_thread(ThreadQueue *queue, Thread *thread)
{
	if(queue->count == queue->capacity) {
		fprintf(stderr, "resizing thread queue\n");
		Thread **new_thread_array = realloc(queue->threads, queue->capacity * 2 * sizeof(Thread));
		if(new_thread_array == NULL) {
			fprintf(stderr, "could not resize thread queue\n");
			return 0;
		}
		
		queue->threads = new_thread_array;
	}
	
	queue->threads[queue->count++] = thread;
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
remove_thread(ThreadQueue *queue, Thread *thread)
{
	int i;
	for(i = 0; i < queue->count; i++) {
		if(queue->threads[i] == thread) {
			for(; i < queue->count - 1; i++)
				queue->threads[i] = queue->threads[i+1];
			queue->count--;
			return;
		}
	}
}

int
main(int argc, char *argv[])
{
	ThreadQueue queue;
	int num_scripts = argc - 1;
	int i;
	
	lua_State *L;
	L = luaL_newstate();
	if(L == NULL) {
		fprintf(stderr, "cannot init lua\n");
		return EXIT_FAILURE;
	}
	
	queue.count = 0;
	queue.capacity = 10;
	queue.threads = malloc(sizeof(Thread) * queue.capacity);
	if(!queue.threads) {
		fprintf(stderr, "could not allocate thread queue\n");
		return EXIT_FAILURE;
	}
	
	/*
	L is never executed.
	It's just used for holding references to the other threads
	  to keep them from being garbage collected.
	Its global environment is used for shred-accessible cross-shredd storage.
	*/
	lua_newtable(L); /* push an empty table */
	lua_setglobal(L, "threads"); /* call the empty table "threads" (pops table) */
	
	for(i = 0; i < num_scripts; i++) {
		Thread *thread = malloc(sizeof(Thread));
		if(!thread) {
			fprintf(stderr, "could not allocate thread for '%s'\n", argv[1 + i]);
			return EXIT_FAILURE;
		}
		
		lua_getglobal(L, "threads"); /* push threads table */
		lua_pushlightuserdata(L, thread); /* push pointer to Thread */
		lua_State *s = lua_newthread(L); /* push thread reference to L's stack */
		lua_settable(L, -3); /* threads[thread] = s (pops s and thread) */
		lua_pop(L, 1); /* pop "threads" */
		
		thread->filename = argv[1 + i];
		thread->L = s;
		thread->now = 0;
		
		prepenv(thread->L);
		
		switch(luaL_loadfile(thread->L, thread->filename)) {
		case LUA_ERRSYNTAX:
			fprintf(stderr, "syntax error in '%s'\n", thread->filename);
			break;
		case LUA_ERRMEM:
			fprintf(stderr, "memory allocation error while loading '%s'\n", thread->filename);
			break;
		case LUA_ERRFILE:
			fprintf(stderr, "cannot open script '%s'\n", thread->filename);
			break;
		default:
			if(!add_thread(&queue, thread))
				fprintf(stderr, "could not add '%s' to thread queue\n", thread->filename);
		}
	}
	
	while(queue.count > 0) {
		Thread *thread = next_thread(&queue);
		
		switch(lua_resume(thread->L, 0)) {
		case 0:
			lua_getglobal(L, "threads"); /* push threads table */
			lua_pushlightuserdata(L, thread); /* push pointer to Thread */
			lua_pushnil(L); /* pushes nil */
			lua_settable(L, -3); /* threads[s] = nil (pops s and thread) */
			lua_pop(L, 1); /* pop "threads" */
			
			remove_thread(&queue, thread);
			break;
		case LUA_YIELD: {
			lua_Number amount = luaL_checknumber(thread->L, -1);
			if(amount > 0)
				thread->now += amount;
			break;
		}
		case LUA_ERRRUN:
			fprintf(stderr, "runtime error in '%s'\n", thread->filename);
			remove_thread(&queue, thread);
			break;
		case LUA_ERRMEM:
			fprintf(stderr, "memory allocation error while running '%s'\n", thread->filename);
			remove_thread(&queue, thread);
			break;
		}
	}
	
	lua_close(L);
	
	return EXIT_SUCCESS;
}
