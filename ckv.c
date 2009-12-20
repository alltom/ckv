
#include "ckv.h"

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	lua_State *L;
	L = luaL_newstate();
	if(L == NULL) {
		fprintf(stderr, "cannot init lua\n");
		return EXIT_FAILURE;
	}
	
	lua_gc(L, LUA_GCSTOP, 0);  /* stop collector during initialization */
	lua_pushcfunction(L, luaopen_string); lua_call(L, 0, 0);
	lua_pushcfunction(L, luaopen_table); lua_call(L, 0, 0);
	lua_pushcfunction(L, luaopen_math); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ckv); lua_call(L, 0, 0);
	lua_gc(L, LUA_GCRESTART, 0);
	
	switch(luaL_loadfile(L, argv[1])) {
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
	
	switch(lua_pcall(L, 0, LUA_MULTRET, 0)) {
	case LUA_ERRRUN:
		fprintf(stderr, "runtime error\n");
		return EXIT_FAILURE;
	case LUA_ERRMEM:
		fprintf(stderr, "memory allocation error while running\n");
		return EXIT_FAILURE;
	}
	
	lua_close(L);
	
	return EXIT_SUCCESS;
}
