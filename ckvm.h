
#ifndef CKVM_H
#define CKVM_H

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

/*

The CKVM (CKV (virtual) Machine) encapsulates a Lua virtual machine whose
state is accessible via ckvm_global_state().

*/

typedef struct _CKVM *CKVM; /* ckv virtual machine */
typedef struct _CKVM_Thread *CKVM_Thread; /* ckv threads ("shreds") */

typedef void (*ErrorCallback)(CKVM vm, const char *message); /* callback type for receiving vm error messages */

CKVM ckvm_create(ErrorCallback err_callback);
void ckvm_destroy(CKVM vm);

lua_State *ckvm_global_state(CKVM vm); /* get ckvm's global lua state */

CKVM_Thread ckvm_add_thread_from_file(CKVM vm, const char *filename); /* add script at the given path */
CKVM_Thread ckvm_add_thread_from_string(CKVM vm, const char *script); /* add script with the given source */
void ckvm_remove_thread(CKVM_Thread thread);
CKVM_Thread ckvm_get_thread(lua_State *L);

double ckvm_now(CKVM vm); /* ckv's current "now" (in samples) */

void ckvm_run_one(CKVM vm); /* run the next thread, then return */
void ckvm_run_until(CKVM vm, double new_now); /* run the vm until new_now; when it returns, "now" will be exactly new_now */
void ckvm_run(CKVM vm); /* runs 'til all threads die or fall asleep */
int ckvm_running(CKVM vm); /* is the vm running? */

void ckvm_pushstdglobal(lua_State *L, const char *name); /* fetches the VM-global with the given name and pushes it on L's stack */

void ckvm_push_new_scheduler(lua_State *L, double rate); /* pushes a scheduler with the given rate onto L's stack */
int ckvm_set_scheduler_rate(lua_State *L, int stack_index, double rate); /* sets the rate of the scheduler at the given stack position; returns a positive value if successful */
double ckvm_get_scheduler_rate(lua_State *L, int stack_index); /* gets the rate of the scheduler at the given stack index */

#endif
