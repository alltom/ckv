
#include <stdlib.h>
#include <math.h>

#include "../../ckvm.h"
#include "ugen.h"


/*
the amount size of the array used for the delay line is
DELAY_BUFFER_PAD_FACTOR * LENGTH. when the array fills, all the
values are shifted over so only LENGTH remain. larger values mean
that the shifting happens less frequently, but will take longer.
I have not tweaked this value to find the perfect balance.
*/
#define DELAY_BUFFER_PAD_FACTOR (2)

typedef struct _Delay {
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
	delay = (Delay *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	last_value = 0.0;
	if(delay->ptr - delay->delay_length >= 0)
		last_value = delay->buffer[delay->ptr - delay->delay_length];
	
	/* get the latest input sample */
	ckvm_pushstdglobal(L, "UGen");
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
	
	/* add current sample to the delay line */
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
	Delay *delay = (Delay *)lua_touserdata(L, 1);
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
	
	delay = (Delay *)malloc(sizeof(struct _Delay));
	if(delay == NULL) {
		fprintf(stderr, "[ckv] memory error allocating Delay\n");
		return 0;
	}
	
	/* if they provided a delay length use that; otherwise default to about 100ms */
	if(lua_isnumber(L, 1)) {
		delay_amount = lua_tonumber(L, 1);
		if(delay_amount < 0)
			delay_amount = 0;
	} else {
		ckvm_pushstdglobal(L, "sample_rate");
		delay_amount = lua_tonumber(L, -1) / 10.0; /* ~100 ms */
		lua_pop(L, 1);
	}
	
	delay->delay_length = ceil(delay_amount);
	delay->size = delay_amount * DELAY_BUFFER_PAD_FACTOR;
	delay->ptr = 0;
	
	delay->buffer = (lua_Number *)malloc(sizeof(lua_Number) * delay->size);
	if(delay->buffer == NULL) {
		fprintf(stderr, "[ckv] memory error allocating Delay buffer\n");
		free(delay);
		return 0;
	}
	
	lua_createtable(L, 0 /* array */, 2 /* non-array */);             /* stack: delay */
	lua_pushlightuserdata(L, delay);                                  /* stack: delay, delay struct */
	lua_createtable(L, 0 /* array */, 1 /* non-array */);             /* stack: delay, delay struct, metatable */
	lua_pushcfunction(L, ckv_delay_release);                          /* stack: delay, delay struct, metatable, destructor */
	lua_setfield(L, -2, "__gc"); /* metatable[__gc] = destructor */   /* stack: delay, delay struct, metatable */
	lua_setmetatable(L, -2);                                          /* stack: delay, delay struct */
	lua_setfield(L, -2, "obj"); /* delay[obj] = delay struct */       /* stack: delay */
	
	luaL_register(L, NULL, ckvugen_delay); /* add delay methods (tick, etc) */
	
	/* initialize delay.last to 0.0 */
	lua_pushnumber(L, 0.0);
	lua_setfield(L, -2, "last");
	
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
