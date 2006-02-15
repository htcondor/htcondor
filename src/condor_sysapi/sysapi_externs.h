/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
extern int _sysapi_last_x_event;

/* needed by free_fs_blocks.c */
extern int _sysapi_reserve_afs_cache;
extern int _sysapi_reserve_disk;

/* needed by idle_time.C */
extern int _sysapi_startd_has_bad_utmp;

/* needed by test.C */
extern int _sysapi_config;

/* needed by ncpus.c */
extern int _sysapi_ncpus;

/* needed by phys_mem.c */
extern int _sysapi_memory;
extern int _sysapi_reserve_memory;

/* needed by ckptpltfrm.c */
extern char* _sysapi_ckptpltfrm;

#endif /* SYSAPI_EXTERNS_H */
