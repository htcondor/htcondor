#include "condor_common.h"

/* 
	Implementation of an easy name for this functionality since the 
	remote i/o and checkpointing libraries would like this functionality
	as well, but there are naming restrictions for functions that go into
	those libraries. 
*/

ssize_t full_read(int filedes, void *ptr, size_t nbyte)
{
	return _condor_full_read(filedes, ptr, nbyte);
}

ssize_t full_write(int filedes, void *ptr, size_t nbyte)
{
	return _condor_full_write(filedes, ptr, nbyte);
}
