
#include "../ckv.h"
#include "ugen.h"

/* LIBRARY REGISTRATION */

int
open_ugen_sinosc(lua_State *L)
{
	(void) luaL_dostring(L,
	"SinOsc = {"
	"  new = function(class, freq)"
	"    return UGen.initialize_io({"
	"      phase = 0.0,"
	"      freq = freq or 440.0,"
	"      tick = function(self)"
	"        self.last = math.sin(self.phase * math.pi * 2.0);"
	"        self.phase = (self.phase + self.freq / sample_rate) % 1.0;"
	"      end,"
	"    })"
	"  end"
	"};"
	);
	
	return 0;
}

int
open_ugen_sqrosc(lua_State *L)
{
	(void) luaL_dostring(L,
	"SqrOsc = {"
	"  new = function(class, freq)"
	"    return UGen.initialize_io({"
	"      phase = 0.0,"
	"      freq = freq or 440.0,"
	"      tick = function(self)"
	"        if self.phase < 0.5 then"
	"          self.last = -1;"
	"        else"
	"          self.last = 1;"
	"        end"
	"        self.phase = (self.phase + self.freq / sample_rate) % 1.0;"
	"      end,"
	"    })"
	"  end"
	"};"
	);
	
	return 0;
}

int
open_ugen_sawosc(lua_State *L)
{
	(void) luaL_dostring(L,
	"SawOsc = {"
	"  new = function(class, freq)"
	"    return UGen.initialize_io({"
	"      phase = 0.0,"
	"      freq = freq or 440.0,"
	"      tick = function(self)"
	"        self.last = self.phase;"
	"        self.phase = (self.phase + self.freq / sample_rate) % 1.0;"
	"      end,"
	"    })"
	"  end"
	"};"
	);
	
	return 0;
}
