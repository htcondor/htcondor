/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include "condor_common.h"

#include "except.h"
#include "debug.h"
static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

extern int	errno;

/*
** Compatibility routine for systems which utilize various forms of the
** fcntl() call for this purpose.  Note that semantics are a bit different
** then the bsd type flock() in that a write lock (exclusive lock) can
** only be applied if the file is open for writing, and a read lock
** (shared lock) can only be applied if the file is open for reading.
*/
flock( fd, op )
int		fd;
int		op;
{
	struct flock	f;
	int				cmd;
	int				status;

	if( (op & LOCK_NB) == LOCK_NB ) {
		cmd = F_SETLK;		/* non-blocking */
	} else {
		cmd = F_SETLKW;		/* blocking */
	}

	f.l_start = 0;			/* flock only supports locking whole files */
	f.l_len = 0;
	f.l_whence = 0;
	f.l_pid = 0;

	if( op & LOCK_SH ) {			/* shared */
		f.l_type = F_RDLCK;
	} else if (op & LOCK_EX) {		/* exclusive */
		f.l_type = F_WRLCK;
	} else if (op & LOCK_UN ) {		/* unlock */
		f.l_type = F_UNLCK;
	} else {
		errno = EINVAL;
		return -1;
	}

	display_flock( &f );
	display_cmd( cmd );

	status =  fcntl( fd, cmd, &f );
	/* dprintf( D_ALWAYS, "return value is %d\n", status ); */
	return status;
}

#define CASE(vble,val) \
	case val: \
	/* dprintf( D_ALWAYS, "vble = val\n" ) */; \
	break

#define DEBUG(name,fmt) dprintf( D_ALWAYS, "name = fmt\n", name );
display_flock( f )
struct flock	*f;
{
	switch( f->l_type ) {
		CASE( l_type, F_RDLCK );
		CASE( l_type, F_WRLCK );
		CASE( l_type, F_UNLCK );
		default:
			dprintf( D_ALWAYS, "l_type is unknown (%d)\n", f->l_type );
			break;
	}

	/*
	DEBUG( f->l_whence, %d );
	DEBUG( f->l_start, %d );
	DEBUG( f->l_len, %d );
	DEBUG( f->l_pid, %d );
	*/
}


display_cmd( cmd )
int		cmd;
{
	switch( cmd ) {
		CASE( cmd, F_DUPFD );
		CASE( cmd, F_GETFD );
		CASE( cmd, F_SETFD );
		CASE( cmd, F_GETLK );
		CASE( cmd, F_SETLK );
		CASE( cmd, F_SETLKW );
		default:
			dprintf( D_ALWAYS, "cmd = UNKNOWN (%d)\n" );
			break;
	}
}
