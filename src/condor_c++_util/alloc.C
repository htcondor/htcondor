#define _POSIX_SOURCE

#include "condor_common.h"

static int     CallsToNew;
static int     CallsToDelete;

#if defined(ALLOC_DEBUG)

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

