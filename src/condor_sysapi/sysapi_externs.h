#ifndef SYSAPI_EXTERNS_H
#define SYSAPI_EXTERNS_H

/* this header file can be included by C and C++ files, so make sure it
   understands that fact.

   so use the defined(_cplusplus) directive to ensure that works 
*/

#if defined(__cplusplus)
#include "string_list.h"
#endif

/* the extern declarations are placed here instead of sysapi.h because I want
   them to be completely internal to the sysapi. Everything inside the
   sysapi should include this, while everything outside of the sysapi
   only includes sysapi.h, which is merely function prototypes 

   All of these variables are declared in reconfig.h 
*/

/* needed by idle_time.C and last_x_event.c */
#if defined(__cplusplus)
extern StringList *_sysapi_console_devices;
#endif
extern int _sysapi_last_x_event;

/* needed by free_fs_blocks.c */
extern int _sysapi_reserve_afs_cache;
extern int _sysapi_reserve_disk;

/* needed by idle_time.C */
extern int _sysapi_startd_has_bad_utmp;

#endif
