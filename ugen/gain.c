
#include "../ckv.h"
#include "ugen.h"

/* LIBRARY REGISTRATION */

int
open_ugen_gain(lua_State *L)
{
	(void) luaL_dostring(L,
	"Gain = {"
	"  new = function(class, gain)"
	"    return UGen.initialize_io({"
	"      gain = gain or 1.0,"
	"      tick = function(self)"
	"        self.last = UGen.sum_inputs(self) * self.gain;"
	"      end"
	"    })"
	"  end"
	"} "
	"PassThru = Gain"
	);
	
	return 0;
}
