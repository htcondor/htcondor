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

 

#ifndef _MACHDEP_H
#define _MACHDEP_H

#if defined(ULTRIX42)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)();
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(ULTRIX43)

	extern "C" char *brk( char * );
	extern "C" char *sbrk( int );
	typedef void (*SIG_HANDLER)();
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(SUNOS41)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( ... );
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(SOLARIS2) || defined(Solaris)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( ... );
#	define SETJMP setjmp
#	define LONGJMP longjmp

#elif defined(OSF1)

	extern "C" int brk( void * );
#	include <sys/types.h>
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( int );
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(HPUX10)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
#	include <signal.h>
	typedef void (*SIG_HANDLER)( int, siginfo_t *, void * );
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(HPUX9)

	extern "C" int brk( const void * );
	extern "C" void *sbrk( int );
#	include <signal.h>
	typedef void (*SIG_HANDLER)( __harg );
#	define SETJMP _setjmp
#	define LONGJMP _longjmp

#elif defined(AIX32)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( int );
#	define SETJMP _setjmp
#	define LONGJMP _longjmp


#elif defined(IRIX) 

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( int );
#	define SETJMP setjmp
#	define LONGJMP longjmp

#elif defined(LINUX)

	extern "C" int brk( void *);
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( int );
#	define SETJMP setjmp
#	define LONGJMP longjmp
	extern "C" int kill( pid_t, int );

#else

#	error UNKNOWN PLATFORM

#endif

#endif
