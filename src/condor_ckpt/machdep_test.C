#define _POSIX_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include "image.h"

void test_func();

int		GlobalDataObj;
long	SomeDataAddr = (long)&GlobalDataObj;

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
