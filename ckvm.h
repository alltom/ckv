
#ifndef CKVM_H
#define CKVM_H

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

typedef struct CKVM *CKVM;
typedef struct CKVM_Thread *CKVM_Thread;

typedef void (*ErrorCallback)(CKVM vm, const char *message);

CKVM ckvm_create(ErrorCallback err_callback);
lua_State *ckvm_global_state(CKVM vm);
CKVM_Thread ckvm_add_thread(CKVM vm, const char *filename);
void ckvm_remove_thrad(CKVM_Thread thread);
CKVM_Thread ckvm_get_thread(lua_State *L);
double ckvm_now(CKVM vm);
void ckvm_run_one(CKVM vm);
void ckvm_run_until(CKVM vm, double new_now);
void ckvm_run(CKVM vm); /* runs 'til all threads die or fall asleep */
int ckvm_running(CKVM vm);
void ckvm_destroy(CKVM vm);

#endif
