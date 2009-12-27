
#include "ckv.h"

static int ckv_now(lua_State *L) {
	lua_pushnumber(L, now(get_thread(L)));
	return 1;
}

static int ckv_fork(lua_State *L) {
	fork_child(get_thread(L));
	return 0;
}

static int ckv_fork_eval(lua_State *L) {
	fork_child_with_eval(get_thread(L));
	return 0;
}

static int ckv_yield(lua_State *L) {
	return lua_yield(L, lua_gettop(L));
}

/* LIBRARY REGISTRATION */

static const luaL_Reg ckvlib[] = {
	{ "now", ckv_now },
	{ "fork", ckv_fork },
	{ "fork_eval", ckv_fork_eval },
	{ "yield", ckv_yield },
	{ NULL, NULL }
};

/* opens library */
int open_ckv(lua_State *L) {
	lua_pushvalue(L, LUA_GLOBALSINDEX);
	luaL_register(L, NULL, ckvlib);
	
	return 1;
}
