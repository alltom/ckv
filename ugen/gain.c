
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
	"      last_value = 0.0,"
	"      tick = function(self)"
	"        if not(now() == self.last_tick) then"
	"          self.last_value = UGen.sum_inputs(self);"
	"          self.last_tick = now();"
	"        end"
	"        return self.last_value"
	"      end"
	"    })"
	"  end"
	"} "
	"PassThru = Gain"
	);
	
	return 0;
}
