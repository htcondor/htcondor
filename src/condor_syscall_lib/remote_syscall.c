/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#define _POSIX_SOURCE

/* Solaris specific change ..dhaval 6/28 */
#if defined(Solaris)
#include "_condor_fix_types.h"
#endif

#include "condor_common.h"
#include "condor_debug.h"
#include <stdarg.h>
#include "condor_fix_assert.h"
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
