
#include "../ckv.h"
#include "ugen.h"

/* UGen HELPER FUNCTIONS */

/* args: self */
static
int
ckv_ugen_initialize_io(lua_State *L)
{
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
static
int
ckv_ugen_create_input(lua_State *L)
{
	const char *port;
	
	luaL_checktype(L, 1, LUA_TTABLE);
	port = luaL_checkstring(L, 2);
	
	/* self.inputs[name] = {}; */
	lua_getfield(L, 1, "inputs");
	lua_newtable(L);
	lua_setfield(L, -2, port);
	
	/* return self; */
	lua_pushvalue(L, 1);
	return 1;
}

/* args: self, port */
static
int
ckv_ugen_sum_inputs(lua_State *L)
{
	const char *port;
	double sample = 0;
	
	luaL_checktype(L, 1, LUA_TTABLE);
	port = lua_gettop(L) > 1 ? lua_tostring(L, 2) : "default";
	
	lua_getfield(L, 1, "inputs"); /* pushes self.inputs */
	lua_getfield(L, -1, port); /* pushes self.inputs[port] */
	
	/* enumerate the inputs */
	lua_pushnil(L); /* first key */
	while(lua_next(L, -2) != 0) {
		/* pairs are source (-2) -> true (-1) */
		lua_getfield(L, -2, "tick"); /* source.tick */
		lua_pushvalue(L, -3); /* source */
		lua_call(L, 1, 1); /* source.tick(source) */
		sample += lua_tonumber(L, -1);
		
		/* remove sample and source; keeps 'key' for next iteration */
		lua_pop(L, 2);
	}
	
	lua_pushnumber(L, sample);
	return 1;
}

/* CONNECT & DISCONNECT */

/* args: source1, dest1/source2, dest2/source3, ... */
static
int
ckv_connect(lua_State *L) {
	int i, source, dest;
	int nargs = lua_gettop(L);
	
	if(nargs < 2)
		return luaL_error(L, "connect() expects at least two arguments, received %d", nargs);
	
	for(i = 1; i < nargs; i++) {
		source = i;
		dest = i + 1;
		
		luaL_checktype(L, source, LUA_TTABLE);
		luaL_checktype(L, dest, LUA_TTABLE);
		
		/* dest.inputs["default"][source] = true; */
		lua_getfield(L, dest, "inputs");
		lua_getfield(L, -1, "default");
		lua_pushvalue(L, source); /* source */
		lua_pushboolean(L, 1);
		lua_settable(L, -3);
		
		lua_pop(L, 2); /* pop dest.inputs["default"] and dest.inputs */
	}
	
	return 0;
}

/* args: source, dest, port */
static
int
ckv_disconnect(lua_State *L)
{
	const char *port;
	
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TTABLE);
	port = lua_tostring(L, 3);
	
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
int
open_ckvugen(lua_State *L) {
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
	
	/* standard ugens */
	lua_pushcfunction(L, open_ugen_delay); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ugen_follower); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ugen_gain); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ugen_impulse); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ugen_noise); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ugen_sawosc); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ugen_sinosc); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ugen_sndin); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ugen_sqrosc); lua_call(L, 0, 0);
	lua_pushcfunction(L, open_ugen_step); lua_call(L, 0, 0);
	
	/* custom ugens */
	/* (put yours here) */
	
	/* blackhole */
	lua_getglobal(L, "Gain");
	lua_getfield(L, -1, "new");
	lua_pushvalue(L, -2); /* push Gain again (argument to new) */
	lua_call(L, 1, 1);
	lua_setglobal(L, "blackhole");
	lua_pop(L, 1); /* pop Gain */
	
	/* dac */
	lua_getglobal(L, "Gain");
	lua_getfield(L, -1, "new");
	lua_pushvalue(L, -2); /* push Gain again (argument to new) */
	lua_call(L, 1, 1);
	lua_pushvalue(L, -1); /* dup dac */
	lua_setglobal(L, "dac"); /* pops one */
	lua_setglobal(L, "speaker"); /* pops other */
	lua_pop(L, 1); /* pop Gain */
	
	/* adc */
	lua_getglobal(L, "Step");
	lua_getfield(L, -1, "new");
	lua_pushvalue(L, -2); /* push Step again (argument to new) */
	lua_call(L, 1, 1);
	lua_pushvalue(L, -1); /* dup adc */
	lua_setglobal(L, "adc"); /* pops one */
	lua_setglobal(L, "mic"); /* pops other */
	lua_pop(L, 1); /* pop Step */
	
	return 1; /* return globals */
}
