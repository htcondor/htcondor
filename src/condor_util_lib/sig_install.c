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
#include <signal.h>
#include "condor_debug.h"

static char *_FileName_ = __FILE__;


typedef void (*SIG_HANDLER)();

void
install_sig_handler( int sig, SIG_HANDLER handler )
{
	struct sigaction act;

	act.sa_handler = handler;
	sigemptyset( &act.sa_mask );
	act.sa_flags = 0;

	if( sigaction(sig,&act,0) < 0 ) {
		EXCEPT( "sigaction" );
	}
}

void
unblock_signal( int sig)
{
    sigset_t    set;
    if ( sigprocmask(SIG_SETMASK,0,&set)  == -1 ) {
        EXCEPT("Error in reading procmask, errno = %d\n", errno);
    }
    sigdelset(&set, sig);
    if ( sigprocmask(SIG_SETMASK,&set, 0)  == -1 ) {
        EXCEPT("Error in setting procmask, errno = %d\n", errno);
    }
}	
