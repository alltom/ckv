
#include "../ckv.h"
#include "ugen.h"

/* UGen HELPER FUNCTIONS */

/* args: self */
static int ckv_ugen_initialize_io(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	
	/* self.inputs = {}; */
	lua_newtable(L);
	lua_setfield(L, 1, "inputs");
	
	/* UGen.create_input(self, "default"); */
	lua_getfield(L, LUA_GLOBALSINDEX, "UGen");
	lua_getfield(L, -1, "create_input"); /* function */
	lua_pushvalue(L, 1); /* self */
	lua_pushstring(L, "default");
	lua_call(L, 2, 0);
	
	lua_pushvalue(L, 1); /* self */
	return 1; /* return self */
}

/* args: self, name */
static int ckv_ugen_create_input(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	const char *port = luaL_checkstring(L, 2);
	
	/* self.inputs[name] = {}; */
	lua_getfield(L, 1, "inputs");
	lua_newtable(L);
	lua_setfield(L, -2, port);
	
	/* return self; */
	lua_pushvalue(L, 1);
	return 1;
}

/* args: self, port */
static int ckv_ugen_sum_inputs(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	const char *port = luaL_checkstring(L, 2);
	
	lua_getfield(L, 1, "inputs"); /* pushes self.inputs */
	lua_getfield(L, -1, port); /* pushes self.inputs[port] */
	
	double sample = 0;
	
	/* enumerate the inputs */
	lua_pushnil(L); /* first key */
	while(lua_next(L, -2) != 0) {
		/* pairs are source (-2) -> true (-1) */
		lua_getfield(L, -2, "tick"); /* source.tick */
		lua_pushvalue(L, -3); /* source */
		lua_call(L, 1, 1); /* source.tick(source) */
		sample += lua_tonumber(L, -1);
		lua_pop(L, 1);
		
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(L, 1);
	}
	
	lua_pushnumber(L, sample);
	return 1;
}

/* CONNECT & DISCONNECT */

/* args: source, dest, port */
static int ckv_connect(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TTABLE);
	const char *port = lua_tostring(L, 3);
	
	/* dest.inputs[port or "default"][source] = true; */
	lua_getfield(L, 2, "inputs");
	lua_getfield(L, -1, port == NULL ? "default" : port);
	lua_pushvalue(L, 1); /* source */
	lua_pushboolean(L, 1);
	lua_settable(L, -3);
	
	return 0;
}

/* args: source, dest, port */
static int ckv_disconnect(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TTABLE);
	const char *port = lua_tostring(L, 3);
	
	/* dest.inputs[port or "default"][source] = true; */
	lua_getfield(L, 2, "inputs");
	lua_getfield(L, -1, port == NULL ? "default" : port);
	lua_pushvalue(L, 1); /* source */
	lua_pushnil(L);
	lua_settable(L, -3);
	
	return 0;
}

/* LIBRARY REGISTRATION */

/* opens ckv library */
int open_ckvugen(lua_State *L) {
	/* UGen */
	lua_createtable(L, 0, 3 /* estimated number of functions */);
	lua_pushcfunction(L, ckv_ugen_initialize_io); lua_setfield(L, -2, "initialize_io");
	lua_pushcfunction(L, ckv_ugen_create_input);  lua_setfield(L, -2, "create_input");
	lua_pushcfunction(L, ckv_ugen_sum_inputs);    lua_setfield(L, -2, "sum_inputs");
	lua_setglobal(L, "UGen");
	
	/* connect & disconnect */
	lua_pushvalue(L, LUA_GLOBALSINDEX);
	lua_pushcfunction(L, ckv_connect); lua_setfield(L, -2, "connect");
	lua_pushcfunction(L, ckv_disconnect); lua_setfield(L, -2, "disconnect");
	lua_pop(L, 1);
	
	/* UGens */
	lua_pushcfunction(L, open_ugen_gain); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ugen_sinosc); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ugen_sndin); lua_call(L, 0, 0);
	
	/* dac */
	lua_getglobal(L, "Gain");
	lua_getfield(L, -1, "new");
	lua_pushvalue(L, -2); /* push Gain again (argument to new) */
	lua_call(L, 1, 1);
	lua_pushvalue(L, -1); /* dup dac */
	lua_setglobal(L, "dac"); /* pops one */
	lua_setglobal(L, "speaker"); /* pops other */
	lua_pop(L, 1); /* pop Gain */
	
	return 1; /* return globals */
}
