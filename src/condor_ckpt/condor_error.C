
#include "condor_common.h"
#include "condor_syscall_mode.h"
#include "condor_sys.h"
#include "condor_debug.h"
#include "image.h"

#define BUFFER_SIZE 1024

/* here are the remote syscalls we use in this file */
extern "C" int REMOTE_CONDOR_report_error(char *text);


static char text[BUFFER_SIZE];

static void _condor_message( char *text )
{
	if( MyImage.GetMode()==STANDALONE ) {
		int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
		syscall( SYS_write, 2, "Condor: ", 8 );
		syscall( SYS_write, 2, text, strlen(text) );
		syscall( SYS_write, 2, "\n", 1 );
		SetSyscalls(scm);
	} else {
		REMOTE_CONDOR_report_error( text );
		dprintf(D_ALWAYS,text);
	}
}

extern "C" void _condor_warning( char *format, ... )
{
	va_list	args;
	va_start(args,format);
	strcpy(text,"Warning: ");
	vsprintf( &text[strlen(text)], format, args );
	_condor_message(text);
	va_end(args);
}

extern "C" void _condor_error_retry( char *format, ... )
{
	va_list	args;
	va_start(args,format);
	strcpy(text,"Error: ");
	vsprintf( &text[strlen(text)], format, args );
	_condor_message(text);
	Suicide();
	va_end(args);
}

extern "C" void _condor_error_fatal( char *format, ... )
{
	va_list	args;
	va_start(args,format);
	strcpy(text,"Error: ");
	vsprintf( &text[strlen(text)], format, args );
	_condor_message(text);
	exit(-1);
	va_end(args);
}
