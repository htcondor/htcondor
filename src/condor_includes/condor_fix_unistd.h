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
#ifndef FIX_UNISTD_H
#define FIX_UNISTD_H

/**********************************************************************
** Stuff that needs to be hidden or defined before unistd.h is
** included on various platforms. 
**********************************************************************/

#if defined(IRIX)
#if !defined(_SYS_SELECT_H)
typedef struct fd_set fd_set;
#endif
#define _save_SGIAPI _SGIAPI
#undef _SGIAPI
#define _SGIAPI 1
#define _save_XOPEN4UX _XOPEN4UX
#undef _XOPEN4UX
#define _XOPEN4UX 1
#define _save_XOPEN4 _XOPEN4
#undef _XOPEN4
#define _XOPEN4 1
#define __vfork fork
#define _save_BSD_COMPAT _BSD_COMPAT
#undef _BSD_COMPAT
#endif /* IRIX */

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

#if defined(OSF1)
/* Here we make certain that pipe is not defined to _Epipe().  Why?
 * Because systems running DUX 4.0D which boot over the network will
 * fail upon calling _Epipe() for any user except root (like condor).
 * Yes, this was a pain in my ass to figure out.  Then we give a 
 * prototype for pipe, since we only have one for _Epipe()... -Todd
 */
#	if defined(pipe)
#		undef pipe   /* we do not want pipe defined to _Epipe() ! */
#		if defined(__cplusplus)
extern "C" {
#		endif
int pipe(int fildes[2]);
#if 	defined(__cplusplus)
}
#		endif
#	endif
#endif

#if defined(IRIX)
#	undef _SGIAPI
#	define _SGIAPI _save_SGIAPI
#	undef _save_SGIAPI
#	undef _XOPEN4UX
#	define _XOPEN4UX _save_XOPEN4UX
#	undef _save_XOPEN4UX
#	undef _XOPEN4
#	define _XOPEN4 _save_XOPEN4
#	undef _save_XOPEN4
#	undef __vfork
#	undef _BSD_COMPAT
#	define _BSD_COMPAT _save_BSD_COMPAT
#	undef _save_BSD_COMPAT
#endif


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

#if defined(SUNOS41) 
	typedef unsigned long ssize_t;
	ssize_t read( int, void *, size_t );
	ssize_t write( int, const void *, size_t );
#endif

#if defined(SUNOS41)
	int symlink( const char *, const char * );
	void *sbrk( ssize_t );
	int gethostname( char *, int );
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
