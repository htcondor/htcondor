/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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

#endif /* SYSAPI_EXTERNS_H */
