
#include "../ckv.h"
#include "ugen.h"

/*
TODO: convert Gain to Lua
I created this unit generator in C to prove that it could
be done. There's not much point since Lua's compiler probably
produces better code, especially with LuaJIT.
*/

/* args: self */
static
int
ckv_gain_tick(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	
	lua_Number gain, last_tick, last_value;
	
	lua_getfield(L, -1, "last_tick");
	last_tick = lua_tonumber(L, -1);
	lua_pop(L, 1);
	
	lua_getglobal(L, "now");
	lua_call(L, 0, 1);
	double tnow = lua_tonumber(L, -1);
	lua_pop(L, 1);
	
	if(tnow > last_tick) {
		lua_getfield(L, -1, "gain");
		gain = lua_tonumber(L, -1);
		lua_pop(L, 1);
		
		lua_pushnumber(L, tnow);
		lua_setfield(L, 1, "last_tick");
		
		lua_getglobal(L, "UGen");
		lua_getfield(L, -1, "sum_inputs");
		lua_pushvalue(L, 1);
		lua_pushstring(L, "default");
		lua_call(L, 2, 1); /* UGen.sum_inputs(self, "default") */
		
		last_value = gain * lua_tonumber(L, -1);
		lua_pop(L, 1);
		
		lua_pushnumber(L, last_value);
		lua_setfield(L, 1, "last_value"); /* set self.last_value */
		
		lua_pop(L, 1); /* pop UGen */
		
		lua_pushnumber(L, last_value);
	} else {
		lua_getfield(L, 1, "last_value");
	}
	
	return 1;
}

static
const
luaL_Reg
ckvugen_gain[] = {
	{ "tick", ckv_gain_tick },
	{ NULL, NULL }
};

/* args: Gain (class), gain (amount) */
static
int
ckv_gain_new(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	double gain = lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 1;
	
	/* self */
	lua_createtable(L, 2 /* non-array */, 0 /* array */);
	
	/* self.gain = gain */
	lua_pushnumber(L, gain);
	lua_setfield(L, -2, "gain");
	
	/* self.last_tick = -1 */
	lua_pushnumber(L, -1);
	lua_setfield(L, -2, "last_tick");
	
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

int
open_ugen_gain(lua_State *L)
{
	lua_createtable(L, 0, 1 /* estimated number of functions */);
	lua_pushcfunction(L, ckv_gain_new); lua_setfield(L, -2, "new");
	lua_setglobal(L, "Gain"); /* pops */
	
	return 0;
}
