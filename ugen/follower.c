
#include "../ckv.h"
#include "ugen.h"

/* LIBRARY REGISTRATION */

int
open_ugen_follower(lua_State *L)
{
	(void) luaL_dostring(L,
	"Follower = {"
	" new = function(class, half_life)"
	"  return UGen.initialize_io({"
	"   last_value = 0.0,"
	"   decay = math.exp(math.log(0.5) / (half_life or (sample_rate / 16.))),"
	"   tick = function(self)"
	"    if not(now() == self.last_tick) then"
	"     local in_sample = math.abs(UGen.sum_inputs(self));"
	"     if in_sample > self.last_value then"
	"      self.last_value = in_sample"
	"     else"
	"      self.last_value = self.last_value * self.decay"
	"     end"
	"     self.last_tick = now();"
	"    end"
	"    return self.last_value"
	"   end"
	"  })"
	" end"
	"}"
	);
	
	return 0;
}
