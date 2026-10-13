/* Shared by the offline system libraries (runtime_services.c, runtime_online.c,
 * runtime_dialogs.c): their error codes and the small helpers several of them export. */
#pragma once
#include "runtime.h"
#include <stdio.h>

#define USER_ID 1
#define ORBIS_OK 0
#define NP_SIGNED_OUT ((int32_t)0x80550006)

/* Unique ids for handles, contexts and requests (any library). */
int runtime_offline_id(void);
static inline int new_id(void) { return runtime_offline_id(); }
static inline void note(const char *what) { printf("Runtime: %s\n",what); }
static inline ABI int32_t lib_init_id(void) { return new_id(); }
static inline ABI int32_t ok_void(void) { return 0; }
static inline ABI int32_t np_signed_out(void) { return NP_SIGNED_OUT; }
