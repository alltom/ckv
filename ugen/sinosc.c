
#include "../ckv.h"
#include "ugen.h"
#include <math.h>

/* args: self */
static
int
ckv_sinosc_tick(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	
	lua_Number phase, freq, sample_rate, last_tick, last_value;
	
	lua_getfield(L, -1, "last_tick");
	last_tick = lua_tonumber(L, -1);
	lua_pop(L, 1);
	
	lua_getglobal(L, "now");
	lua_call(L, 0, 1);
	double tnow = lua_tonumber(L, -1);
	lua_pop(L, 1);
	
	if(tnow > last_tick) {
		lua_getfield(L, -1, "phase");
		phase = lua_tonumber(L, -1);
		lua_pop(L, 1);
		
		lua_getfield(L, -1, "freq");
		freq = lua_tonumber(L, -1);
		lua_pop(L, 1);
		
		lua_getglobal(L, "sample_rate");
		sample_rate = lua_tonumber(L, -1);
		lua_pop(L, 1);
		
		lua_pushnumber(L, tnow);
		lua_setfield(L, 1, "last_tick");
		
		last_value = sin(phase * M_PI * 2.0);
		phase += freq / sample_rate;
		while(phase < 0) phase += 1;
		while(phase > 1) phase -= 1;
		
		lua_pushnumber(L, last_value);
		lua_setfield(L, 1, "last_value");
		
		lua_pushnumber(L, phase);
		lua_setfield(L, 1, "phase");
		
		lua_pushnumber(L, last_value);
	} else {
		lua_getfield(L, 1, "last_value");
	}
	
	return 1;
}

static
const
luaL_Reg
ckvugen_sinosc[] = {
	{ "tick", ckv_sinosc_tick },
	{ NULL, NULL }
};

/* args: SinOsc (class), freq (amount) */
static
int
ckv_sinosc_new(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	double freq = lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 440;
	
	/* self */
	lua_createtable(L, 2 /* non-array */, 0 /* array */);
	
	/* self.phase = 0 */
	lua_pushnumber(L, 0);
	lua_setfield(L, -2, "phase");
	
	/* self.freq = freq */
	lua_pushnumber(L, freq);
	lua_setfield(L, -2, "freq");
	
	/* self.last_tick = -1 */
	lua_pushnumber(L, -1);
	lua_setfield(L, -2, "last_tick");
	
	/* add gain methods */
	luaL_register(L, NULL, ckvugen_sinosc);
	
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
open_ugen_sinosc(lua_State *L)
{
	lua_createtable(L, 0, 1 /* estimated number of functions */);
	lua_pushcfunction(L, ckv_sinosc_new); lua_setfield(L, -2, "new");
	lua_setglobal(L, "SinOsc"); /* pops */
	
	return 0;
}
