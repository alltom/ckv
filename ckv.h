
#ifndef ck_h
#define ck_h

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

typedef struct VM *VMPtr;

/* opens ckv-specific lua libraries */
int open_ckvbaselite(lua_State *L);
int open_ckvugen(lua_State *L);

typedef void (*AudioCallback)( double *outputBuffer, double *inputBuffer,
                               unsigned int nFrames,
                               double streamTime,
                               void *userData );
int start_audio(AudioCallback callback, int sample_rate, void *data);
void stop_audio(void);

#endif
