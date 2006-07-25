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

#ifndef _MACHDEP_H
#define _MACHDEP_H

#if defined(Solaris)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( ... );
#	define SETJMP setjmp
#	define LONGJMP longjmp

#elif defined(OSF1)

	extern "C" int brk( void * );
#	include <sys/types.h>
# ifndef DUX5
	extern "C" void *sbrk( int );
# endif
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

#endif /* _MACHDEP_H */
