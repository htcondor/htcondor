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
#include "image.h"

void test_func();

int		GlobalDataObj;
long	SomeDataAddr = (long)&GlobalDataObj;

void
Suicide()
{
	exit(1);
}

int
main( int argc, char *argv )
{
	int		local_data_obj;
	long	some_stack_addr = (long)&local_data_obj;
	long	data_start, data_end;
	long	stack_start, stack_end;

	data_start = data_start_addr();
	data_end = data_end_addr();
	stack_start = stack_start_addr();
	stack_end = stack_end_addr();

	fprintf( stderr, "Data starts at 0x%lX\n", data_start );
	fprintf( stderr, "Data ends at 0x%lX\n", data_end );
	fprintf( stderr, "Typical Data addr at 0x%lX\n", SomeDataAddr );
	assert( data_start < data_end );
	assert( data_start < SomeDataAddr && SomeDataAddr < data_end );

	fprintf( stderr, "Stack starts at 0x%lX\n", stack_start );
	fprintf( stderr, "Stack ends at 0x%lX\n", stack_end );
	fprintf( stderr, "Typical Stack addr at 0x%lX\n", some_stack_addr );
	assert( stack_start < stack_end );
	assert( stack_start < some_stack_addr && some_stack_addr < stack_end );

	fprintf( stderr, "Stack grows %s\n", StackGrowsDown() ? "DOWN" : "UP" );
	fprintf( stderr, "Index of Stack Pointer in jmp_buf is %d\n", JmpBufSP_Index() );
	ExecuteOnTmpStk( test_func );

	fprintf( stderr, "ERROR test_func() returned\n" );
	return 1;
}

void
test_func()
{
	int		i;

	fprintf( stderr, "This should be a DATA address 0x%lX\n", &i );
	exit( 0 );
}
extern "C"
int SetSyscalls( int mode )
{
}
