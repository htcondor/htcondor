#define _POSIX_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "image.h"

void test_func();

int
main( int argc, char *argv )
{
	printf( "Data starts at 0x%X\n", data_start_addr() );
	printf( "Data ends at 0x%X\n", data_end_addr() );
	printf( "Stack starts at 0x%X\n", stack_start_addr() );
	printf( "Stack ends at 0x%X\n", stack_end_addr() );
	printf( "Stack grows %s\n", StackGrowsDown() ? "DOWN" : "UP" );
	printf( "Index of Stack Pointer in jmp_buf is %d\n", JmpBufSP_Index() );
	ExecuteOnTmpStk( test_func );
}

void
test_func()
{
	int		i;

	printf( "This should be a DATA address 0x%X\n", &i );
	exit( 0 );
}
