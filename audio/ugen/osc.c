
#include "ugen.h"

/* LIBRARY REGISTRATION */

int
open_ugen_sinosc(lua_State *L)
{
	(void) luaL_dostring(L,
	"function SinOsc(freq)"
	"  return {"
	"    last = 0.0,"
	"    phase = 0.0,"
	"    freq = freq or 440.0,"
	"    tick = function(self)"
	"      self.last = math.sin(self.phase * math.pi * 2.0);"
	"      self.phase = (self.phase + self.freq / sample_rate) % 1.0;"
	"    end,"
	"  }"
	"end"
	);
	
	return 0;
}

int
open_ugen_sqrosc(lua_State *L)
{
	(void) luaL_dostring(L,
	"function SqrOsc(freq)"
	"  return {"
	"    last = 0.0,"
	"    phase = 0.0,"
	"    freq = freq or 440.0,"
	"    tick = function(self)"
	"      if self.phase < 0.5 then"
	"        self.last = -1;"
	"      else"
	"        self.last = 1;"
	"      end"
	"      self.phase = (self.phase + self.freq / sample_rate) % 1.0;"
	"    end,"
	"  }"
	"end"
	);
	
	return 0;
}

int
open_ugen_sawosc(lua_State *L)
{
	(void) luaL_dostring(L,
	"function SawOsc(freq)"
	"  return {"
	"    last = 0.0,"
	"    phase = 0.0,"
	"    freq = freq or 440.0,"
	"    tick = function(self)"
	"      self.last = self.phase;"
	"      self.phase = (self.phase + self.freq / sample_rate) % 1.0;"
	"    end,"
	"  }"
	"end"
	);
	
	return 0;
}
