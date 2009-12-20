
#include "ckv.h"

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	lua_State *l;
	l = luaL_newstate();
	if(l == NULL) {
		fprintf(stderr, "cannot init lua\n");
		return EXIT_FAILURE;
	}
	
	/* luaL_openlibs(l); */
	/*
	luaopen_base (for the basic library),
	luaopen_package (for the package library),
	luaopen_string (for the string library),
	luaopen_table (for the table library),
	luaopen_math (for the mathematical library),
	luaopen_io (for the I/O library),
	luaopen_os (for the Operating System library),
	luaopen_debug
	*/
	lua_pushcfunction(l, luaopen_string); lua_call(l, 0, 0);
	lua_pushcfunction(l, luaopen_table); lua_call(l, 0, 0);
	lua_pushcfunction(l, luaopen_math); lua_call(l, 0, 0);
	
	/* load ckv lib */
	lua_pushcfunction(l, open_ckv); lua_call(l, 0, 0);
	
	printf("in C\n\n");
	
	switch(luaL_loadfile(l, argv[1])) {
	case LUA_ERRSYNTAX:
		fprintf(stderr, "syntax error\n");
		return EXIT_FAILURE;
	case LUA_ERRMEM:
		fprintf(stderr, "memory allocation error while loading\n");
		return EXIT_FAILURE;
	case LUA_ERRFILE:
		fprintf(stderr, "cannot open script\n");
		return EXIT_FAILURE;
	}
	
	switch(lua_pcall(l, 0, LUA_MULTRET, 0)) {
	case LUA_ERRRUN:
		fprintf(stderr, "runtime error\n");
		return EXIT_FAILURE;
	case LUA_ERRMEM:
		fprintf(stderr, "memory allocation error while running\n");
		return EXIT_FAILURE;
	}
	
	printf("\nin C again\n");
	
	lua_close(l);
	
	return EXIT_SUCCESS;
}
