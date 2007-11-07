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

#define xstr(s) str(s)
#define str(s) #s

#if defined(WIN32)
#define PLATFORM INTEL-WINNT50
#endif

/* Via configure, one may have specified a particular buildid string to use
	in the version string. So honor that request here. */
#if defined(BUILDID)
#define BUILDIDSTR " BuildID: " xstr(BUILDID)
#else
#define BUILDIDSTR ""
#endif

/* Here is the version string - update before a public release */
/* --- IMPORTANT!  THE FORMAT OF THE VERSION STRING IS VERY STRICT
   BECAUSE IT IS PARSED AT RUNTIME.  DO NOT ALTER THE FORMAT OR ENTER
   ANYTHING EXTRA BEFORE THE DATE.  IF YOU WISH TO ADD EXTRA INFORMATION,
   DO SO _AFTER_ THE BUILDIDSTR AND BEFORE THE TRAILING '$' CHARACTER.
   EXAMPLES:
       $CondorVersion: 6.1.11 " __DATE__ BUILDIDSTR " WinNTPreview $ [OK]
	   $CondorVersion: 6.1.11 WinNTPreview " __DATE__ BUILDIDSTR " $ [WRONG!!!]
   Any questions?  See Todd or Derek.  Note: if you mess it up, DaemonCore
   will EXCEPT at startup time.  
*/

static char* CondorVersionString = "$CondorVersion: 6.9.5 " __DATE__ BUILDIDSTR " PRE-RELEASE-UWCS $";

/* Here is the platform string.  You don't need to edit this */
static char* CondorPlatformString = "$CondorPlatform: " xstr(PLATFORM) " $";

extern "C" {

const char*
CondorVersion( void )
{
	return CondorVersionString;
}

const char*
CondorPlatform( void )
{
	return CondorPlatformString;
}

} /* extern "C" */

