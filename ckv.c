
#include "ckv.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
	const char *filename;
	lua_State *L;
} Thread;

typedef struct {
	int count, capacity;
	Thread *threads;
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

int
main(int argc, char *argv[])
{
	ThreadQueue threads;
	int num_scripts = argc - 1;
	int i;
	
	lua_State *L;
	L = luaL_newstate();
	if(L == NULL) {
		fprintf(stderr, "cannot init lua\n");
		return EXIT_FAILURE;
	}
	
	threads.count = 0;
	threads.capacity = 10;
	threads.threads = malloc(sizeof(Thread) * threads.capacity);
	if(threads.threads == NULL) {
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
		Thread thread;
		thread.filename = argv[1 + i];
		thread.L = lua_newthread(L);
		prepenv(thread.L);
		
		switch(luaL_loadfile(thread.L, thread.filename)) {
		case LUA_ERRSYNTAX:
			fprintf(stderr, "syntax error in '%s'\n", thread.filename);
			return EXIT_FAILURE;
		case LUA_ERRMEM:
			fprintf(stderr, "memory allocation error while loading '%s'\n", thread.filename);
			return EXIT_FAILURE;
		case LUA_ERRFILE:
			fprintf(stderr, "cannot open script '%s'\n", thread.filename);
			return EXIT_FAILURE;
		}
		
		threads.threads[i] = thread;
	}
	
	for(i = 0; i < num_scripts; i++) {
		Thread thread = threads.threads[i];
		int running = 1;
		
		while(running) {
			switch(lua_resume(thread.L, 0)) {
			case 0:
				fprintf(stderr, "'%s' finished\n\n", thread.filename);
				running = 0;
				break;
			case LUA_YIELD: {
				lua_Number amount = luaL_checknumber(thread.L, -1);
				fprintf(stderr, "'%s' yielded %f\n", thread.filename, amount);
				break;
			}
			case LUA_ERRRUN:
				fprintf(stderr, "runtime error in '%s'\n", thread.filename);
				running = 0;
				break;
			case LUA_ERRMEM:
				fprintf(stderr, "memory allocation error while running '%s'\n", thread.filename);
				running = 0;
				break;
			}
		}
	}
	
	lua_close(L);
	
	return EXIT_SUCCESS;
}
