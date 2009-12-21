
#ifndef ck_h
#define ck_h

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

typedef struct VM *VMPtr;
typedef struct Thread *ThreadPtr;

/* opens ckv library into a lua thread's env */
int open_ckv(lua_State *L);

ThreadPtr get_thread(lua_State *L); /* returns current thread */
double now(ThreadPtr thread); /* returns current thread time */
void fork_child(ThreadPtr parent); /* forks a child using function and args on the stack */
void fork_child_with_eval(ThreadPtr parent); /* forks a child evaluating the string argument */

#endif
