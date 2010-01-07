
#include "../ckv.h"
#include "ugen.h"

/* LIBRARY REGISTRATION */

int
open_ugen_impulse(lua_State *L)
{
	(void) luaL_dostring(L,
	"Impulse = {"
	"  new = function(class)"
	"    return UGen.initialize_io({"
	"      next = 0.0,"
	"      tick = function(self)"
	"        self.last_value = self.next;"
	"        self.next = 0.0;"
	"        return self.last_value;"
	"      end"
	"    })"
	"  end"
	"}"
	);
	
	return 0;
}
