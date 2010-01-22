
#include "ugen.h"
#include <stdlib.h>
#include <math.h>

#define DELAY_BUFFER_PAD_FACTOR (2)

typedef struct Delay {
	double *buffer;
	int delay_length;
	int ptr;
	int size;
} Delay;

/* args: self */
static
int
ckv_delay_tick(lua_State *L)
{
	Delay *delay;
	lua_Number last_value, sample;
	int i;
	
	luaL_checktype(L, 1, LUA_TTABLE);
	
	lua_getfield(L, -1, "obj");
	delay = lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	last_value = 0.0;
	if(delay->ptr - delay->delay_length >= 0)
		last_value = delay->buffer[delay->ptr - delay->delay_length];
	
	/* get the latest input sample */
	pushstdglobal(L, "UGen");
	lua_getfield(L, -1, "sum_inputs");
	lua_pushvalue(L, 1);
	lua_call(L, 1, 1);
	sample = lua_tonumber(L, -1);
	lua_pop(L, 2); /* pop sample and UGen */
	
	/* if we've reached the end of the buffer, move everything to the beginning */
	if(delay->ptr == delay->size) {
		for(i = 0; i < delay->delay_length; i++)
			delay->buffer[i] = delay->buffer[delay->size - 1 - delay->delay_length + i];
		delay->ptr = delay->delay_length;
	}
	
	delay->buffer[delay->ptr++] = sample;
	
	lua_pushnumber(L, last_value);
	lua_setfield(L, 1, "last");
	
	return 0;
}

/* args: delay obj */
static
int
ckv_delay_release(lua_State *L)
{
	Delay *delay = lua_touserdata(L, 1);
	free(delay->buffer);
	free(delay);
	
	return 0;
}

static
const
luaL_Reg
ckvugen_delay[] = {
	{ "tick", ckv_delay_tick },
	{ NULL, NULL }
};

/* args: Delay (class), filename */
static
int
ckv_delay_new(lua_State *L)
{
	Delay *delay;
	lua_Number delay_amount = 0;
	
	delay = malloc(sizeof(Delay));
	if(delay == NULL) {
		fprintf(stderr, "[ckv] memory error allocating Delay\n");
		return 0;
	}
	
	if(lua_isnumber(L, 1)) {
		delay_amount = lua_tonumber(L, 1);
		if(delay_amount < 0)
			delay_amount = 0;
	} else {
		pushstdglobal(L, "sample_rate");
		delay_amount = lua_tonumber(L, -1) / 10.0; /* ~100 ms */
		lua_pop(L, 1);
	}
	
	delay->delay_length = ceil(delay_amount);
	delay->size = delay_amount * DELAY_BUFFER_PAD_FACTOR;
	delay->ptr = 0;
	
	delay->buffer = malloc(sizeof(lua_Number) * delay->size);
	if(delay->buffer == NULL) {
		fprintf(stderr, "[ckv] memory error allocating Delay buffer\n");
		free(delay);
		return 0;
	}
	
	/* self */
	lua_createtable(L, 0 /* array */, 2 /* non-array */);
	
	/* set GC routine, then */
	/* self.obj = delay* */
	lua_pushlightuserdata(L, delay);
	lua_createtable(L, 0 /* array */, 1 /* non-array */); /* new metatable */
	lua_pushcfunction(L, ckv_delay_release);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);
	lua_setfield(L, -2, "obj");
	
	/* add delay methods */
	luaL_register(L, NULL, ckvugen_delay);
	
	return 1; /* return self */
}

/* LIBRARY REGISTRATION */

int
open_ugen_delay(lua_State *L)
{
	lua_pushcfunction(L, ckv_delay_new);
	lua_setglobal(L, "Delay");
	
	return 0;
}
