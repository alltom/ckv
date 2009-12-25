
#ifndef ck_h
#define ck_h

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

typedef struct VM *VMPtr;
typedef struct Thread *ThreadPtr;

/* opens ckv-specific lua libraries */
int open_ckv(lua_State *L);
int open_ckvugen(lua_State *L);

ThreadPtr get_thread(lua_State *L); /* returns current thread */
void get_lua_thread(lua_State *L, ThreadPtr thread); /* pushes lua object for current thread */
double now(ThreadPtr thread); /* returns current thread time */
void fork_child(ThreadPtr parent); /* forks a child using function and args on the stack */
void fork_child_with_eval(ThreadPtr parent); /* forks a child evaluating the string argument */

typedef void (*AudioCallback)( double *outputBuffer, double *inputBuffer,
                               unsigned int nFrames,
                               double streamTime,
                               void *userData );
int start_audio(AudioCallback callback, void *data);
void stop_audio(void);

#endif
