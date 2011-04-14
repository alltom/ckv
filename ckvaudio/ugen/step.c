
#include "ugen.h"

/* LIBRARY REGISTRATION */

int
open_ugen_step(lua_State *L)
{
	(void) luaL_dostring(L,
	"function Step()"
	"  return {"
	"    last = 0.0,"
	"    next = 0.0,"
	"    tick = function(self)"
	"      self.last = self.next;"
	"    end"
	"  }"
	"end"
	);
	
	return 0;
}
