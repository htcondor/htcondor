/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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

#include "condor_common.h"

#include "debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

debug_level_t debug_level    = DEBUG_NORMAL;
char *        debug_progname = NULL;

/*--------------------------------------------------------------------------*/
int debug_printf (debug_level_t level, const char *fmt, ...) {
  int ret = 0;
  if (DEBUG_LEVEL(level)) {
    va_list args;
    va_start (args, fmt);
    ret = vprintf (fmt, args);
    va_end (args);
	debug_fflush();
  }
  return ret;
}

/*--------------------------------------------------------------------------*/
void debug_println (debug_level_t level, const char *fmt, ...) {
  if (DEBUG_LEVEL(level)) {
    printf ("%s: ", debug_progname);
    {
      va_list args;
      va_start (args, fmt);
      vprintf (fmt, args);
      va_end (args);
    }
    printf ("\n");
    if (DEBUG_LEVEL(DEBUG_DEBUG_1)) debug_fflush();
  }
}

/*--------------------------------------------------------------------------*/
void debug_error (int error, debug_level_t level, const char *fmt, ...) {
  if (DEBUG_LEVEL(level)) {
    printf ("%s: ", debug_progname);
    {
      va_list args;
      va_start (args, fmt);
      vprintf (fmt, args);
      va_end (args);
    }
    printf ("\n");
    if (DEBUG_LEVEL(DEBUG_DEBUG_1)) debug_fflush();
  }
  DC_Exit(error);
  /* exit(error); */
}

/*--------------------------------------------------------------------------*/
void debug_perror (int error, debug_level_t level, const char *s) {
  if (DEBUG_LEVEL(level)) {
    perror (s);
    DC_Exit(error);
    /* exit(error); */
  }
}
