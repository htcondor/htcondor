#include "syscall_numbers.h"
#include "condor_syscall_mode.h"

#if defined( SYS_send )
int
send( int socket, char *msg, int length, int flags )
{
	int	rval;

	if( LocalSysCalls() ) {
		rval = syscall( SYS_send, socket, msg, length, flags );
	} else {
		rval = REMOTE_syscall( CONDOR_send, socket, msg, length, flags );
	}

	return rval;
}
#endif
