
#include "ckv.h"

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
	lua_settable(L, -2);
	
	return 0;
}

/* GAIN */

/* args: self */
static int ckv_gain_tick(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	
	lua_Number last_tick, last_value;
	
	lua_getfield(L, -1, "last_tick");
	last_tick = lua_tonumber(L, -1);
	lua_pop(L, 1);
	
	lua_getfield(L, -1, "last_value");
	last_value = lua_tonumber(L, -1);
	lua_pop(L, 1);
	
	double tnow = now(get_thread(L));
	if(tnow > last_tick) {
		lua_pushnumber(L, tnow);
		lua_setfield(L, 1, "last_tick");
		
		lua_getglobal(L, "UGen");
		lua_getfield(L, -1, "sum_inputs");
		lua_pushvalue(L, 1);
		lua_pushstring(L, "default");
		lua_call(L, 2, 1); /* UGen.sum_inputs(self, "default") */
		
		last_value = lua_tonumber(L, -1);
		lua_setfield(L, 1, "last_value");
		lua_pop(L, 1); /* pop UGen */
		
		lua_pushnumber(L, last_value);
	} else {
		lua_pushnumber(L, last_value);
	}
	
	return 1;
}

static const luaL_Reg ckvugen_gain[] = {
	{ "tick", ckv_gain_tick },
	{ NULL, NULL }
};

/* args: Gain (class), gain (amount) */
static int ckv_gain_new(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	double gain = lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 1;
	
	/* self */
	lua_createtable(L, 2 /* non-array */, 0 /* array */);
	
	/* self.gain = gain */
	lua_pushnumber(L, gain);
	lua_setfield(L, -2, "gain");
	
	/* add gain methods */
	luaL_register(L, NULL, ckvugen_gain);
	
	/* UGen.initialize_io(self) */
	lua_getfield(L, LUA_GLOBALSINDEX, "UGen");
	lua_getfield(L, -1, "initialize_io");
	lua_pushvalue(L, -3); /* self */
	lua_call(L, 1, 0);
	lua_pop(L, 1); /* pop UGen */
	
	return 1; /* return self */
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
	
	/* Gain */
	lua_createtable(L, 0, 1 /* estimated number of functions */);
	lua_pushcfunction(L, ckv_gain_new); lua_setfield(L, -2, "new");
	lua_setglobal(L, "Gain"); /* pops */
	
	/* connect & disconnect */
	lua_pushvalue(L, LUA_GLOBALSINDEX);
	lua_pushcfunction(L, ckv_connect); lua_setfield(L, -2, "connect");
	lua_pushcfunction(L, ckv_disconnect); lua_setfield(L, -2, "disconnect");
	lua_pop(L, 1);
	
	/* dac */
	lua_getglobal(L, "Gain");
	lua_getfield(L, -1, "new");
	lua_pushvalue(L, -2); /* push Gain again (argument to new) */
	lua_call(L, 1, 1);
	lua_setglobal(L, "dac");
	lua_pop(L, 1); /* pop Gain */
	
	return 1; /* return globals */
}
