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

/* Here is the version string - update before a public release */
static char* CondorVersionString = "$CondorVersion: 6.1.11 " __DATE__ " $";

/* 
   This is some wisdom from Cygnus's web page.  If you just try to use
   the "stringify" operator on a preprocessor directive, you'd get
   "PLATFORM", not "Intel Linux" (or whatever the value of PLATFORM
   is).  That's because the stringify operator is a special case, and
   the preprocessor isn't allowed to expand things that are passed to
   it.  However, by defining two layers of macros, you get the right
   behavior, since the first pass converts:

   xstr(PLATFORM) -> str(Intel Linux)

   and the next pass gives:

   str(Intel Linux) -> "Intel Linux"

   This is exactly what we want, so we use it.  -Derek Wright and Jeff
   Ballard, 12/2/99 

   Also, because the NT build system is totally different, we have to
   define the correct platform string right in here. :( -Derek 12/3/99 
*/

#if defined(WIN32)
#define PLATFORM INTEL-WINNT40
#endif

#define xstr(s) str(s)
#define str(s) #s

/* Here is the platform string.  You don't need to edit this */
static char* CondorPlatformString = "$CondorPlatform: " xstr(PLATFORM) " $";

extern "C" {

char*
CondorVersion( void )
{
	return CondorVersionString;
}

char*
CondorPlatform( void )
{
	return CondorPlatformString;
}

} /* extern "C" */

