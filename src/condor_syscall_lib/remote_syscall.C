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
#include "condor_constants.h"
#include "condor_sys.h"
#include "condor_io.h"

static char *_FileName_ = __FILE__;

ReliSock *syscall_sock;

static int RSCSock;
static int ErrSock;

int InDebugMode;

extern "C" {

ReliSock *
RSC_Init( int rscsock, int errsock )
{
	RSCSock     = rscsock;
	syscall_sock = new ReliSock();
	syscall_sock->attach_to_file_desc(rscsock);

	ErrSock = errsock;

	return( syscall_sock );
}

} /* extern "C" */
