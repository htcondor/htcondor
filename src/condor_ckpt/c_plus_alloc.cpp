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
