/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"

void * operator new(size_t size)
{
	return (void *)malloc(size);
}


void operator delete( void *to_free )
{
	if( to_free ) {
		(void)free( to_free );
	}
}

#if defined( __GNUC__ )
#	if (__GNUG__== 2 && (__GNUC_MINOR__ == 6 || __GNUC_MINOR__ == 7 || __GNUC_MINOR__ == 91 || __GNUC_MINOR__ == 95))
		extern "C" {
			void *__builtin_new( size_t );
			void __builtin_delete( void * );
			void *
			__builtin_vec_new (size_t sz)
			{
				return __builtin_new( sz );
			}

			void
			__builtin_vec_delete( void *to_free )
			{
				__builtin_delete( to_free );
			}
		}
#	endif
#endif

#if 0
/* This function is called by egcs when a pure virtual method is called.
   Since the user job may not be linking with egcs libraries, we need to
   provide our own version. */

#define MESSAGE "pure virtual method called\n"

extern "C" {
void
__pure_virtual()
{
	write (2, MESSAGE, sizeof (MESSAGE) - 1);
	_exit (1);
}
}
#endif
