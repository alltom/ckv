
#ifndef ck_h
#define ck_h

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* open ckv-specific lua libraries */
int open_luabaselite(lua_State *L);
int open_ckvugen(lua_State *L);

/* pushes a global variable from the VM,
guaranteed not to have been overwritten by a ckv script */
void pushstdglobal(lua_State *L, const char *name);

typedef void (*AudioCallback)(double *outputBuffer, double *inputBuffer,
                              unsigned int nFrames,
                              double streamTime,
                              void *userData);
int start_audio(AudioCallback callback, int sample_rate, void *data);
void stop_audio(void);

#endif
