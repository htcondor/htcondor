
#include "condor_common.h"
#include "condor_syscall_mode.h"
#include "condor_debug.h"
#include "condor_sys.h"

#define BUFFER_SIZE 1024

static char text[BUFFER_SIZE];

void _condor_warning( char *format, ... )
{
	int scm;
	va_list	args;
	va_start(args,format);

	vsprintf( text, format, args );

	dprintf(D_ALWAYS,"Condor Warning: %s\n",text);
	REMOTE_syscall( CONDOR_report_error, text );

	va_end(args);
}

void _condor_error_retry( char *format, ... )
{
	va_list	args;
	va_start(args,format);

	vsprintf( text, format, args );

	dprintf(D_ALWAYS,"Condor Error: %s - Will rollback and retry\n",text);

	kill( getpid(), SIGKILL );

	va_end(args);
}

void _condor_error_fatal( char *format, ... )
{
	int scm;
	va_list	args;
	va_start(args,format);

	vsprintf( text, format, args );

	dprintf(D_ALWAYS,"Condor Error: %s\n",text);
	REMOTE_syscall( CONDOR_report_error, text );

	exit(-1);

	va_end(args);
}
