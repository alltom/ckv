
#ifndef ck_h
#define ck_h

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

int open_ckv(lua_State *L);
double now(void);
void fork_child(lua_State *L);

#endif
