
#include "ugen.h"

/* ugens to load */
lua_CFunction ugens[] = {
	open_ugen_delay,
	open_ugen_follower,
	open_ugen_gain,
	open_ugen_impulse,
	open_ugen_noise,
	open_ugen_pulseosc,
	open_ugen_sawosc,
	open_ugen_sinosc,
	open_ugen_sndin,
	open_ugen_sqrosc,
	open_ugen_step,
	open_ugen_triosc,
	NULL
};


/* UGen HELPER FUNCTIONS */

/* args: ugen, port */
static
int
ckv_ugen_sum_inputs(lua_State *L)
{
	const char *port;
	int ugen = 1;
	double sample = 0;
	
	luaL_checktype(L, ugen, LUA_TTABLE);
	port = lua_gettop(L) > 1 ? lua_tostring(L, 2) : "default";
	
	lua_getfield(L, LUA_REGISTRYINDEX, "ugen_graph");
	lua_getfield(L, -1, "conns");
	
	lua_pushvalue(L, ugen);
	lua_rawget(L, -2);
	
	if(lua_isnil(L, -1)) {
		/* nothing connected to this ugen */
		lua_pushnumber(L, 0);
		return 1;
	}
	
	lua_getfield(L, -1, port);
	if(lua_isnil(L, -1)) {
		/* nothing connected to this port */
		lua_pushnumber(L, 0);
		return 1;
	}
	
	/* enumerate the inputs */
	lua_pushnil(L); /* first key */
	while(lua_next(L, -2) != 0) {
		/* pairs are source (-2) -> connection count (-1) */
		int connection_count = lua_tonumber(L, -1);
		
		lua_getfield(L, -2, "last"); /* source.last */
		sample += lua_tonumber(L, -1) * connection_count;
		
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
	int i, graph, connect, source, dest;
	int nargs = lua_gettop(L);
	
	if(nargs < 2)
		return luaL_error(L, "connect() expects at least two arguments, received %d", nargs);
	
	lua_getfield(L, LUA_REGISTRYINDEX, "ugen_graph");
	graph = lua_gettop(L);
	lua_getfield(L, -1, "connect");
	connect = lua_gettop(L);
	
	/*
	if they provided a function (constructor) instead of a table (ex: connect(Gain, speaker)),
	call the function and connect the return value instead
	*/
	if(lua_type(L, 1) == LUA_TFUNCTION) {
		lua_pushvalue(L, 1);
		lua_call(L, 0, 1);
		lua_replace(L, 1);
	}
	
	for(i = 1; i < nargs; i++) {
		source = i;
		dest = i + 1;
		
		if(lua_type(L, dest) == LUA_TFUNCTION) {
			lua_pushvalue(L, dest);
			lua_call(L, 0, 1);
			lua_replace(L, dest);
		}
		
		luaL_checktype(L, source, LUA_TTABLE);
		luaL_checktype(L, dest, LUA_TTABLE);
		
		lua_pushvalue(L, connect);
		lua_pushvalue(L, graph); /* self */
		lua_pushvalue(L, source);
		lua_pushvalue(L, dest);
		lua_pushstring(L, "default"); /* port */
		lua_call(L, 4, 0);
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
	if(lua_gettop(L) < 3)
		port = "default";
	else
		port = lua_tostring(L, 3);
	
	lua_getfield(L, LUA_REGISTRYINDEX, "ugen_graph");
	
	lua_getfield(L, -1, "disconnect");
	lua_pushvalue(L, -2); /* self */
	lua_pushvalue(L, 1);
	lua_pushvalue(L, 2);
	lua_pushstring(L, port);
	lua_call(L, 4, 0);
	
	return 0;
}

/* LIBRARY REGISTRATION */

static
void
open_ugen_graph(lua_State *L)
{
	lua_createtable(L, 0 /* array */, 6 /* non-array */);
	
	lua_newtable(L);
	lua_setfield(L, -2, "conns");
	
	(void) luaL_dostring(L,
	"return function(self, source, dest, port) \n"
	"  local conns = self.conns \n"
	"  self.queue = nil \n"
	"  if conns[dest] == nil then \n"
	"    conns[dest] = {} \n"
	"    conns[dest][port] = {} \n"
	"    conns[dest][port][source] = 1 \n"
	"  elseif conns[dest][port] == nil then \n"
	"    conns[dest][port] = {} \n"
	"    conns[dest][port][source] = 1 \n"
	"  elseif conns[dest][port][source] == nil then \n"
	"    conns[dest][port][source] = 1 \n"
	"  else \n"
	"    conns[dest][port][source] = conns[dest][port][source] + 1 \n"
	"  end \n"
	"end"
	);
	lua_setfield(L, -2, "connect");
	
	(void) luaL_dostring(L,
	"return function(self, source, dest, port) \n"
	"  local conns = self.conns \n"
	"  self.queue = nil \n"
	"  if conns[dest] and conns[dest][port] then \n"
	"    local port = conns[dest][port] \n"
	"    if port[source] and port[source] > 1 then \n"
	"      port[source] = port[source] - 1 \n"
	"    else \n"
	"      port[source] = nil \n"
	"    end \n"
	"  end \n"
	"end"
	);
	lua_setfield(L, -2, "disconnect");
	
	(void) luaL_dostring(L,
	"return function(self, ugen) \n"
	"  local conns = self.conns \n"
	"  local inputs = {} \n"
	"  if conns[ugen] then \n"
	"    for port,iugens in pairs(conns[ugen]) do \n"
	"      for iugen,count in pairs(iugens) do \n"
	"        inputs[iugen] = count \n"
	"      end \n"
	"    end \n"
	"  end \n"
	"  return inputs \n"
	"end"
	);
	lua_setfield(L, -2, "gather_inputs");
	
	(void) luaL_dostring(L,
	"return function(self, sinks) \n"
	"  local conns = self.conns \n"
	"  local queue = {} \n"
	"  local deduped = {} \n"
	"  local seen = {} \n"
	"  for k,ugen in pairs(sinks) do \n"
	"    queue[#queue + 1] = ugen \n"
	"  end \n"
	"  for i,ugen in ipairs(queue) do \n"
	"    for iugen,count in pairs(self:gather_inputs(ugen)) do \n"
	"      queue[#queue + 1] = iugen \n"
	"    end \n"
	"  end \n"
	"  for i = #queue, 1, -1 do \n"
	"    if not seen[queue[i]] then \n"
	"      deduped[#deduped + 1] = queue[i] \n"
	"    end \n"
	"    seen[queue[i]] = true \n"
	"  end \n"
	"  return deduped \n"
	"end"
	);
	lua_setfield(L, -2, "create_ugen_queue");
	
	(void) luaL_dostring(L,
	"return function(self, sinks) \n"
	"  if not self.queue then \n"
	"    self.queue = self:create_ugen_queue(sinks) \n"
	"  end \n"
	"  for i,ugen in ipairs(self.queue) do \n"
	"    ugen:tick() \n"
	"  end \n"
	"end"
	);
	lua_setfield(L, -2, "tick_all");
	
	lua_setfield(L, LUA_REGISTRYINDEX, "ugen_graph");
}

/* opens ckv library */
int
open_ckvugen(lua_State *L)
{
	lua_CFunction *fn;
	
	/* UGen */
	lua_createtable(L, 0, 3 /* 3 functions in "UGen" */);
	lua_pushcfunction(L, ckv_ugen_sum_inputs); lua_setfield(L, -2, "sum_inputs");
	lua_setglobal(L, "UGen");
	
	/* connect & disconnect */
	lua_pushcfunction(L, ckv_connect); lua_setglobal(L, "connect");
	lua_pushcfunction(L, ckv_connect); lua_setglobal(L, "c");
	lua_pushcfunction(L, ckv_disconnect); lua_setglobal(L, "disconnect");
	lua_pushcfunction(L, ckv_disconnect); lua_setglobal(L, "d");
	
	open_ugen_graph(L);
	
	/* ugens */
	for(fn = ugens; *fn != NULL; fn++) {
		lua_pushcfunction(L, *fn);
		lua_call(L, 0, 0);
	}
	
	/* blackhole */
	lua_getglobal(L, "Gain");
	lua_call(L, 0, 1);
	lua_setglobal(L, "blackhole");
	
	/* dac */
	lua_getglobal(L, "Gain");
	lua_call(L, 0, 1);
	lua_pushvalue(L, -1); /* dup dac */
	lua_setglobal(L, "dac"); /* pops one */
	lua_setglobal(L, "speaker"); /* pops other */
	
	/* adc (microphone/audio input) */
	lua_getglobal(L, "Step");
	lua_call(L, 0, 1);
	lua_pushvalue(L, -1); /* dup adc */
	lua_setglobal(L, "adc"); /* pops one */
	lua_setglobal(L, "mic"); /* pops other */
	
	return 1; /* return globals */
}
