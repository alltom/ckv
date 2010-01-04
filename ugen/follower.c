
#include "../ckv.h"
#include "ugen.h"

/* LIBRARY REGISTRATION */

int
open_ugen_follower(lua_State *L)
{
	(void) luaL_dostring(L,
	"Follower = {"
	"  new = function(class)"
	"    return UGen.initialize_io({"
	"      last_value = 0.0,"
	"      a = 0.0001,"
	"      tick = function(self)"
	"        if not(now() == self.last_tick) then"
	"          self.last_value = self.last_value + self.a * (math.abs(UGen.sum_inputs(self)) - self.last_value);"
	"          self.last_tick = now();"
	"        end"
	"        return self.last_value"
	"      end"
	"    })"
	"  end"
	"}"
	);
	
	return 0;
}
