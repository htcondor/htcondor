
#include "condor_common.h"
#include "condor_syscall_mode.h"
#include "condor_sys.h"
#include "condor_debug.h"
#include "image.h"

#define BUFFER_SIZE 1024

static char text[BUFFER_SIZE];
static int warning_mode=1;

extern "C" void _condor_warning_mode_set( int onoff )
{
	warning_mode = onoff;
}


static void _condor_message( char *text, int kind )
{
	if( MyImage.GetMode()==STANDALONE ) {
		int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
		syscall( SYS_write, 2, "Condor: ", 8 );
		syscall( SYS_write, 2, text, strlen(text) );
		syscall( SYS_write, 2, "\n", 1 );
		SetSyscalls(scm);
	} else {
		REMOTE_syscall( kind, text );
		dprintf(D_ALWAYS,text);
	}
}

extern "C" void _condor_warning( char *format, ... )
{
	va_list	args;
	va_start(args,format);
	if(!warning_mode) return;
	strcpy(text,"Warning: ");
	vsprintf( &text[strlen(text)], format, args );
	_condor_message(text,CONDOR_report_error);
	va_end(args);
}

extern "C" void _condor_error_retry( char *format, ... )
{
	va_list	args;
	va_start(args,format);
	strcpy(text,"Error: ");
	vsprintf( &text[strlen(text)], format, args );
	_condor_message(text,CONDOR_report_error);
	Suicide();
	va_end(args);
}

extern "C" void _condor_error_fatal( char *format, ... )
{
	va_list	args;
	va_start(args,format);
	strcpy(text,"Error: ");
	vsprintf( &text[strlen(text)], format, args );
	_condor_message(text,CONDOR_report_error);
	exit(-1);
	va_end(args);
}
