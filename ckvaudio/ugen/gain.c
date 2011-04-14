
#include "ugen.h"

/* LIBRARY REGISTRATION */

int
open_ugen_gain(lua_State *L)
{
	(void) luaL_dostring(L,
	"function Gain(gain)"
	"  return {"
	"    last = 0.0,"
	"    gain = gain or 1.0,"
	"    tick = function(self)"
	"      self.last = UGen.sum_inputs(self) * self.gain;"
	"    end"
	"  }"
	"end;"
	"PassThru = Gain" /* alias for "Gain" is "PassThru" */
	);
	
	return 0;
}
