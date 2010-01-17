
#include "../ckv.h"
#include "ugen.h"

/* LIBRARY REGISTRATION */

int
open_ugen_follower(lua_State *L)
{
	(void) luaL_dostring(L,
	"Follower = {"
	" new = function(class, half_life)"
	"  return {"
	"   decay = math.exp(math.log(0.5) / (half_life or (sample_rate / 16.))),"
	"   last = 0.0,"
	"   tick = function(self)"
	"    local in_sample = math.abs(UGen.sum_inputs(self));"
	"    if in_sample > self.last then"
	"     self.last = in_sample"
	"    else"
	"     self.last = self.last * self.decay"
	"    end"
	"   end"
	"  }"
	" end"
	"}"
	);
	
	return 0;
}
