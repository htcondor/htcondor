
#ifndef CONDOR_ERROR_H
#define CONDOR_ERROR_H

#include "condor_common.h"

/*
These error functions may be called from within the syscall or ckpt libs, and report errors with different failure semantics. All of them offer printf-like formatting.
	_condor_warning - report a message and continue executing
	_condor_error_retry - report a message and kill the process, but allow for the possibility of rolling back and retrying.
	_condor_error_fatal - report a message and kill the process, but terminate the job with no possibility of rollback.
*/

BEGIN_C_DECLS

void _condor_warning( char *format, ... );
void _condor_error_retry( char *format, ... );
void _condor_error_fatal( char *format, ... );

END_C_DECLS

#endif
