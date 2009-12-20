
#ifndef ck_h
#define ck_h

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* opens ckv library into a lua thread's env */
int open_ckv(lua_State *L);

double now(void); /* returns current thread time */
void fork_child(lua_State *L); /* forks a child using function and args on the stack */
void fork_child_with_eval(lua_State *L); /* forks a child evaluating the string argument */

#endif
