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

#ifndef FIX_UNISTD_H
#define FIX_UNISTD_H

/**********************************************************************
** Stuff that needs to be hidden or defined before unistd.h is
** included on various platforms. 
**********************************************************************/

#if defined(LINUX)
#	define idle _hide_idle
#	if ! defined(_BSD_SOURCE)
#		define _BSD_SOURCE
#		define CONDOR_BSD_SOURCE
#	endif
#endif

#if defined(LINUX) && defined(GLIBC)
#	define profil _hide_profil
#	define daemon _hide_daemon
#endif


/**********************************************************************
** Actually include the file
**********************************************************************/
#include <unistd.h>


/**********************************************************************
** Clean-up
**********************************************************************/

#if defined(LINUX)
#   undef idle
#	if defined( CONDOR_BSD_SOURCE ) 
#		undef CONDOR_BSD_SOURCE
#		undef _BSD_SOURCE
#	endif
#endif


#if defined(LINUX) && defined(GLIBC)
#	undef profil
#	undef daemon
#endif


/**********************************************************************
** Things that should be defined in unistd.h that for whatever reason
** aren't defined on various platforms, or that we hid explicitly.
**********************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

#if defined(Solaris)
	int gethostname( char *, int );
	long gethostid();
	int getpagesize();
	int getdtablesize();
	int getpriority( int, id_t );
	int setpriority( int, id_t, int );
#if defined(Solaris27) || defined(Solaris28)
	int utimes( const char*, const struct timeval* );
#else
	int utimes( const char*, struct timeval* );
#endif /* Solaris 27 */
	int getdomainname( char*, size_t );
#endif

#if defined(LINUX) && defined(GLIBC)
	int profil( char*, int, int, int );
#endif

#if defined(__cplusplus)
}
#endif

#endif /* FIX_UNISTD_H */
