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
#ifndef FIX_STRING_H
#define FIX_STRING_H

/*
  Some versions of string.h now include the function strdup(), which is
  functionally equivalent to the one in the condor_util_lib directory.
  This would be OK, if they had remembered to declare the single
  parameter as a "const" as is obviously intended.  Since they forgot,
  we have to hide that definition here and provide our own.  -- mike
*/

#if defined(LINUX) && !defined(__USE_GNU)
#define __USE_GNU
#endif

#define strdup _hide_strdup
#include <string.h>
#undef strdup

#if defined(LINUX)
#undef __USE_GNU
#endif

#if defined(__cplusplus)
extern "C" {
#endif
char *strdup( const char * );

#if defined(__cplusplus)
}
#endif

#if defined(OSF1)
/* On Digital Unix, basename() has the wrong prototype.  Normally,
   we'd "hide" it, but the system header files are already doing wierd
   things with it, like "#define basename _Ebasename", etc.  So, to
   effectively hide it, all we have to do is undef what the system
   header files have done. -Derek Wright 10/30/98 */
#undef basename
#endif


#endif

