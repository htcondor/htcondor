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
#define _POSIX_SOURCE

#include "condor_common.h"

#if defined(ALLOC_DEBUG)

static int     CallsToNew;
static int     CallsToDelete;

void * operator new(size_t size)
{
	CallsToNew += 1;
	return (void *)malloc(size);
}

void operator delete( void *to_free )
{
	CallsToDelete += 1;
	if( to_free ) {
		(void)free( to_free );
	}
}

void
clear_alloc_stats()
{
	CallsToNew = 0;
	CallsToDelete = 0;
}

void
print_alloc_stats()
{
    printf( "Calls to new = %d, Calls to delete = %d\n",
        CallsToNew, CallsToDelete
    );
}

void
print_alloc_stats( const char * msg )
{
    printf( "%s: Calls to new = %d, Calls to delete = %d\n",
        msg, CallsToNew, CallsToDelete
    );
}

#else

void print_alloc_stats() { }
void print_alloc_stats( const char *foo ) { }
void clear_alloc_stats() { }

#endif

