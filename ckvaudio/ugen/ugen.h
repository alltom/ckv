
#ifndef UGEN_H
#define UGEN_H

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

/* standard unit generators */
/* these functions add their respective
   unit generator constructors to the
   global namespace */
int open_ugen_delay(lua_State *L);
int open_ugen_follower(lua_State *L);
int open_ugen_gain(lua_State *L);
int open_ugen_impulse(lua_State *L);
int open_ugen_noise(lua_State *L);
int open_ugen_pulseosc(lua_State *L);
int open_ugen_sawosc(lua_State *L);
int open_ugen_sinosc(lua_State *L);
int open_ugen_sndin(lua_State *L);
int open_ugen_sqrosc(lua_State *L);
int open_ugen_step(lua_State *L);
int open_ugen_triosc(lua_State *L);

/* custom unit generators */
/* (insert yours here) */

#endif
