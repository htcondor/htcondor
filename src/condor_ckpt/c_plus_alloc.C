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
