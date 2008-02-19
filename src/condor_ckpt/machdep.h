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

#elif defined(LINUX)

	extern "C" int brk( void *);

#	if defined(I386)
	extern "C" void *sbrk( int );
#	elif defined(X86_86)
	extern "C" void *sbrk( long int );
#	endif 

	typedef void (*SIG_HANDLER)( int );
#	define SETJMP setjmp
#	define LONGJMP longjmp
	extern "C" int kill( pid_t, int );

#else

#	error UNKNOWN PLATFORM

#endif

#endif /* _MACHDEP_H */
