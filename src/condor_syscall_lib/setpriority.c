#include "syscall_numbers.h"
#include "condor_syscall_mode.h"

#if defined( SYS_setpriority )
int
setpriority( int which, int who, int priority )
{
	int	rval;

	if( LocalSysCalls() ) {
		rval = syscall( SYS_setpriority, which, who, priority );
	} else {
		rval = REMOTE_syscall( CONDOR_setpriority, which, who, priority );
	}

	return rval;
}
#endif
