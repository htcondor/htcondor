#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include <stdarg.h>
#include <assert.h>
#include "condor_syscall_mode.h"
#include "condor_xdr.h"
#include "condor_constants.h"
#include "condor_sys.h"

static char *_FileName_ = __FILE__;

static XDR_Write( int *iohandle, void *buf, size_t len );
static XDR_Read( int *iohandle, void *buf, size_t len );

static XDR xdr_data;
XDR *xdr_syscall = &xdr_data;

static int RSCSock;
static int ErrSock;
static int XDR_BUFSIZ = 4096;

int InDebugMode;


XDR *
RSC_Init( rscsock, errsock )
int rscsock, errsock;
{
	RSCSock     = rscsock;
	xdrrec_create( xdr_syscall, XDR_BUFSIZ, XDR_BUFSIZ, &RSCSock,
			XDR_Read, XDR_Write );

	ErrSock = errsock;

	return( xdr_syscall );
}

static
XDR_Read( int *iohandle, void *buf, size_t len )
{
	register int scm = SetSyscalls( SYS_LOCAL | SYS_RECORDED );
	int cnt;
	int	fd;

	if( InDebugMode ) {
		fd = RPL_SOCK;
	} else {
		fd = *iohandle;
	}

	dprintf(D_XDR, "XDR_Read: about to read(%d, 0x%x, %d)\n",
			fd, buf, len);

	cnt = read(fd, buf, len);
	if( cnt == 0 ) {
		dprintf(D_XDR, "XDR_Read: cnt was zero, returning -1\n");
		cnt = -1;
	}

	dprintf(D_XDR, "XDR_Read: cnt = %d\n", cnt);

	(void) SetSyscalls( scm );

	return( cnt );
}

static
XDR_Write( int *iohandle, void *buf, size_t len )
{
	register int scm = SetSyscalls( SYS_LOCAL | SYS_RECORDED );
	int cnt;
	int	fd;

	if( InDebugMode ) {
		fd = REQ_SOCK;
	} else {
		fd = *iohandle;
	}

	dprintf(D_XDR, "XDR_Write: about to write(%d, 0x%x, %d)\n",
			fd, buf, len);

	cnt = write( fd, buf, len );

	dprintf(D_XDR, "XDR_Write: cnt = %d\n", cnt);

	if( cnt != len ) {
		EXCEPT("write(%d, 0x%x, %d) returned %d", fd, buf, len, cnt);
	}

	(void) SetSyscalls( scm );

	return( cnt );
}
