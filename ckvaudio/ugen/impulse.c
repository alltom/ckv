
#include "ugen.h"

/* LIBRARY REGISTRATION */

int
open_ugen_impulse(lua_State *L)
{
	(void) luaL_dostring(L,
	"function Impulse()"
	"  return {"
	"    next = 0.0,"
	"    tick = function(self)"
	"      self.last = self.next;"
	"      self.next = 0.0;"
	"    end"
	"  }"
	"end"
	);
	
	return 0;
}
