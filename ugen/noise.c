
#include "../ckv.h"
#include "ugen.h"

/* LIBRARY REGISTRATION */

int
open_ugen_noise(lua_State *L)
{
	(void) luaL_dostring(L,
	"Noise = {"
	"  new = function(class)"
	"    return UGen.initialize_io({"
	"      last_value = 0.0,"
	"      tick = function(self)"
	"        self.last_value = math.random() * 2.0 - 1.0;"
	"        self.last_tick = now();"
	"        return self.last_value;"
	"      end"
	"    })"
	"  end"
	"}"
	);
	
	return 0;
}
