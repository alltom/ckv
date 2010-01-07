
#ifndef UGEN_H
#define UGEN_H

/* standard unit generators */
int open_ugen_delay(lua_State *L);
int open_ugen_follower(lua_State *L);
int open_ugen_gain(lua_State *L);
int open_ugen_noise(lua_State *L);
int open_ugen_sawosc(lua_State *L);
int open_ugen_sinosc(lua_State *L);
int open_ugen_sndin(lua_State *L);
int open_ugen_sqrosc(lua_State *L);
int open_ugen_step(lua_State *L);

/* custom unit generators */
/* (insert yours here) */

#endif
