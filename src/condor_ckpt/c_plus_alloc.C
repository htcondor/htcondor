#define _POSIX_SOURCE
#include <sys/types.h>
#include <stdlib.h>


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
