#include "condor_common.h"
#include "condor_debug.h"
#include <stdarg.h>
#include <assert.h>
#include "condor_syscall_mode.h"
#include "condor_xdr.h"
#include "condor_constants.h"
#include "condor_sys.h"

static char *_FileName_ = __FILE__;


int CurrentSysCall;

extern XDR *xdr_syscall;

int
REMOTE_syscall( int syscall_num, ... )
{
	int		scm;
	int		terrno;
	int		rval;
	va_list ap;


	scm = SetSyscalls( SYS_LOCAL | SYS_MAPPED );
	va_start( ap, syscall_num );

	switch( syscall_num ) {

	  case SYS_open:
		{
			char	*path;
			int		access_mode;
			mode_t	file_mode;
			int		xdr_status;

			CurrentSysCall = CONDOR_open;

			path = va_arg( ap, char * );
			access_mode = va_arg( ap, int );
			if( access_mode & O_CREAT ) {
				file_mode = va_arg( ap, mode_t );
			} else {
				file_mode = 0;
			}

			dprintf( D_ALWAYS,
				"SYS_open: Path \"%s\", access_mode %d, file_mode 0%o\n",
				path, access_mode, file_mode
			);



			xdr_syscall->x_op = XDR_ENCODE;
			assert( xdr_int(xdr_syscall,&CurrentSysCall) );
			assert( xdr_string(xdr_syscall,&path,_POSIX_PATH_MAX) );
			assert( xdr_int(xdr_syscall,&access_mode) );
			assert( xdr_int(xdr_syscall,&file_mode) );
			assert( xdrrec_endofrecord(xdr_syscall,TRUE) );

			xdr_syscall->x_op = XDR_DECODE;
			assert( xdrrec_skiprecord(xdr_syscall) );
			assert( xdr_int(xdr_syscall,&rval) );
			if( rval < 0 ) {
				assert( xdr_int(xdr_syscall,&terrno) );
				errno = terrno;
			}
		break;
		}

	  case SYS_write:
		{
			int fd;
			void *buf;
			size_t len;

			CurrentSysCall = CONDOR_write;

			fd = va_arg( ap, int );
			buf = va_arg( ap, void * );
			len = va_arg( ap, size_t );
			fprintf( stderr,
				"SYS_write: fd = %d, buf = 0x%x, len = %d\n",
				fd, buf, len
			);

			xdr_syscall->x_op = XDR_ENCODE;
			assert( xdr_int(xdr_syscall,&CurrentSysCall) );
			assert( xdr_int(xdr_syscall,&fd) );
			assert( xdr_int(xdr_syscall,&len) );
			assert( xdr_bytes(xdr_syscall,&buf,&len,len) );
			assert( xdrrec_endofrecord(xdr_syscall,TRUE) );

			xdr_syscall->x_op = XDR_DECODE;
			assert( xdrrec_skiprecord(xdr_syscall) );
			assert( xdr_int(xdr_syscall,&rval) );
			if( rval < 0 ) {
				assert( xdr_int(xdr_syscall,&terrno) );
				errno = terrno;
			}
			break;
		}


	  default:
		scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
		fprintf(
			stderr,
			"Don't know how to do system call %d remotely - yet\n",
			syscall_num
		);
		rval =  -1;
	}

	SetSyscalls( scm );
	return rval;
}
