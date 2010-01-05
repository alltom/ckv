
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
	"      last_value = 0.0,"
	"      tick = function(self)"
	"        if not (now() == self.last_tick) then"
	"          self.last_value = math.sin(self.phase * math.pi * 2.0);"
	"          self.phase = (self.phase + self.freq / sample_rate) % 1.0;"
	"          self.last_tick = now();"
	"        end"
	"        return self.last_value;"
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
	"      last_value = 0.0,"
	"      tick = function(self)"
	"        if not (now() == self.last_tick) then"
	"          if self.phase < 0.5 then"
	"            self.last_value = -1;"
	"          else"
	"            self.last_value = 1;"
	"          end"
	"          self.phase = (self.phase + self.freq / sample_rate) % 1.0;"
	"          self.last_tick = now();"
	"        end"
	"        return self.last_value;"
	"      end,"
	"    })"
	"  end"
	"};"
	);
	
	return 0;
}
