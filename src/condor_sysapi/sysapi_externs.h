/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


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

   All of these variables are declared in reconfig.C 
*/

/* needed by idle_time.C and last_x_event.c */
#if defined(__cplusplus)
extern StringList *_sysapi_console_devices;
#endif

#ifndef EXTERN_C_BEGIN
#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif
#endif

EXTERN_C_BEGIN
extern time_t _sysapi_last_x_event;

/* needed by free_fs_blocks.c */
extern int _sysapi_reserve_afs_cache;
extern int _sysapi_reserve_disk;

/* needed by idle_time.C */
extern int _sysapi_startd_has_bad_utmp;

/* needed by test.C */
extern int _sysapi_config;

/* needed by ncpus.c */
extern int _sysapi_ncpus;
extern int _sysapi_max_ncpus;

/* needed by phys_mem.c */
extern int _sysapi_memory;
extern int _sysapi_reserve_memory;

/* needed by ckptpltfrm.c */
extern char* _sysapi_ckptpltfrm;

/* needed by load_avg.c */
extern int _sysapi_getload;

#ifdef LINUX
extern int _sysapi_count_hyperthread_cpus;
#endif


EXTERN_C_END

#endif /* SYSAPI_EXTERNS_H */
