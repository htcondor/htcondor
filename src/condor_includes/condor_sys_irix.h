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
#ifndef CONDOR_SYS_IRIX_H
#define CONDOR_SYS_IRIX_H


#define _XOPEN_SOURCE 1
#define _BSD_COMPAT 1

/* While we want _BSD_TYPES defined, we can't just define it ourself,
   since we include rpc/types.h later, and that defines _BSD_TYPES
   itself without any checks.  So, instead, we'll just include
   <rpc/types.h> as our first header, to make sure we get BSD_TYPES.
   This also includes <sys/types.h>, so we don't need to include that
   ourselves anymore. */
#include <rpc/types.h>

/******************************
** unistd.h
******************************/
#define __vfork fork
#define _save_BSD_COMPAT _BSD_COMPAT
#undef _BSD_COMPAT
#include <unistd.h>
#undef __vfork
#define _BSD_COMPAT _save_BSD_COMPAT
#undef _save_BSD_COMPAT

/* Some prototypes we should have but did not pick up from unistd.h */
BEGIN_C_DECLS
extern int getpagesize(void);
END_C_DECLS

/* Want stdarg.h before stdio.h so we get GNU's va_list defined */
#include <stdarg.h>

/******************************
** stdio.h
******************************/
#include <stdio.h>
/* These aren't defined if _POSIX_SOURCE is defined. */
FILE *popen (const char *command, const char *type);
int pclose(FILE *stream);


/******************************
** signal.h
******************************/
#define SIGTRAP 5
#define SIGBUS 10
#define _save_NO_ANSIMODE _NO_ANSIMODE
#undef _NO_ANSIMODE
#define _NO_ANSIMODE 1
#undef _SGIAPI
#define _SGIAPI 1
#include <signal.h>
/* We also want _NO_ANSIMODE and _SGIAPI defined to 1 for sys/wait.h,
   math.h and limits.h */  
#include <sys/wait.h>    
#include <math.h>
#include <limits.h>
#undef _NO_ANSIMODE
#define _NO_ANSIMODE _save_NO_ANSIMODE
#undef _save_NO_ANSIMODE
#undef _SGIAPI
#define _SGIAPI _save_SGIAPI
#undef _save_SGIAPI

#include <sys/select.h>

/* Need these to get statfs and friends defined */
#include <sys/stat.h>
#include <sys/statfs.h>

/* Needed to get TIOCNOTTY defined */
#include <sys/ttold.h>


/****************************************
** Condor-specific system definitions
****************************************/
#define HAS_U_TYPES				1
#define NO_VOID_SIGNAL_RETURN	1


#endif /* CONDOR_SYS_IRIX_H */
