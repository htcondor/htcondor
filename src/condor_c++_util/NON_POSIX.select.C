/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
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

/* To remove warning in HPUX about implicit decl of select() ... */
#if defined (HPUX9)
#include <sys/time.h>
#endif

#include "_condor_fix_types.h"
#include "condor_common.h"
#include "condor_constants.h"
#include "condor_debug.h"
#include "condor_fix_timeval.h"

#include "selector.h"

/*
   In HPUX, the select call takes (int *) params, wheras SunOS, Solaris
   take (fd_set *) params.  We define an intermediate type to handle
   this.    -- Rajesh
*/

#if defined (HPUX9)
typedef int *SELECT_FDSET_PTR;
#else
typedef fd_set *SELECT_FDSET_PTR;
#endif

#if defined(IRIX53)
#	include<bstring.h>
#endif

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */


extern int errno;

void display_fd_set( char *msg, fd_set *set, int max );

Selector::Selector()
{
	state = VIRGIN;
	timeout_wanted = FALSE;
	max_fd = -1;
	FD_ZERO( &save_read_fds );
	FD_ZERO( &save_write_fds );
	FD_ZERO( &save_except_fds );
}

void
Selector::add_fd( int fd, IO_FUNC interest )
{
	switch( interest ) {

	  case IO_READ:
		FD_SET( fd, &save_read_fds );
		break;

	  case IO_WRITE:
		FD_SET( fd, &save_write_fds );
		break;

	  case IO_EXCEPT:
		FD_SET( fd, &save_except_fds );
		break;

	}

	if( fd > max_fd ) {
		max_fd = fd;
	}
}

void
Selector::delete_fd( int fd, IO_FUNC interest )
{
	switch( interest ) {

	  case IO_READ:
		FD_CLR( fd, &save_read_fds );
		break;

	  case IO_WRITE:
		FD_CLR( fd, &save_write_fds );
		break;

	  case IO_EXCEPT:
		FD_CLR( fd, &save_except_fds );
		break;

	}
}

void
Selector::set_timeout( clock_t ticks )
{
	static long clock_tick;

	if( ticks < 0 ) {
		ticks = 0;
	}

	timeout_wanted = TRUE;

	if( !clock_tick ) {
		clock_tick = sysconf( _SC_CLK_TCK );
	}

	timeout.tv_sec = ticks / clock_tick;
	timeout.tv_usec = ticks % clock_tick;
	timeout.tv_usec *= 1000000 / clock_tick;
}

void
Selector::unset_timeout()
{
	timeout_wanted = FALSE;
}

void
Selector::execute()
{
	int		nfds;
	struct timeval	*tp;

	read_fds = save_read_fds;
	write_fds = save_write_fds;
	except_fds = save_except_fds;

	if( timeout_wanted ) {
		tp = &timeout;
	} else {
		tp = NULL;
	}

	nfds = select( max_fd + 1, 
				  (SELECT_FDSET_PTR) &read_fds, 
				  (SELECT_FDSET_PTR) &write_fds, 
				  (SELECT_FDSET_PTR) &except_fds, 
				  tp );

	if( nfds < 0 ) {
		if( errno == EINTR ) {
			state = SIGNALLED;
			return;
		} else {
			EXCEPT( "select" );
		}
	}

	if( NFDS(nfds) == 0 ) {
		state = TIMED_OUT;
	} else {
		state = FDS_READY;
	}
}

BOOLEAN
Selector::fd_ready( int fd, IO_FUNC interest )
{
	if( state != FDS_READY ) {
		EXCEPT(
			"Selector::fd_ready() called, but selector not in FDS_READY state"
		);
	}

	switch( interest ) {

	  case IO_READ:
		return FD_ISSET( fd, &read_fds );
		break;

	  case IO_WRITE:
		return FD_ISSET( fd, &write_fds );
		break;

	  case IO_EXCEPT:
		return FD_ISSET( fd, &except_fds );
		break;

	}

		// Can never get here
	return FALSE;
}

BOOLEAN
Selector::timed_out()
{
	return state == TIMED_OUT;
}

BOOLEAN
Selector::signalled()
{
	return state == SIGNALLED;
}

BOOLEAN
Selector::has_ready()
{
	return state == FDS_READY;
}

void
Selector::display()
{

	switch( state ) {

	  case VIRGIN:
		dprintf( D_ALWAYS, "State = VIRGIN\n" );
		break;

	  case FDS_READY:
		dprintf( D_ALWAYS, "State = FDS_READY\n" );
		break;

	  case TIMED_OUT:
		dprintf( D_ALWAYS, "State = TIMED_OUT\n" );
		break;

	  case SIGNALLED:
		dprintf( D_ALWAYS, "State = SIGNALLED\n" );
		break;

	}

	dprintf( D_ALWAYS, "max_fd = %d\n", max_fd );

	dprintf( D_ALWAYS, "Selection FD's\n" );
	display_fd_set( "\tRead", &save_read_fds, max_fd );
	display_fd_set( "\tWrite", &save_write_fds, max_fd );
	display_fd_set( "\tExcept", &save_except_fds, max_fd );

	if( state == FDS_READY ) {
		dprintf( D_ALWAYS, "Ready FD's\n" );
		display_fd_set( "\tRead", &read_fds, max_fd );
		display_fd_set( "\tWrite", &write_fds, max_fd );
		display_fd_set( "\tExcept", &except_fds, max_fd );
	}
	if( timeout_wanted ) {
		dprintf( D_ALWAYS,
			"Timeout = %d.%06d seconds\n", timeout.tv_sec, timeout.tv_usec
		);
	} else {
		dprintf( D_ALWAYS, "Timeout not wanted\n" );
	}

}

void
display_fd_set( char *msg, fd_set *set, int max )
{
	int		i;

	dprintf( D_ALWAYS, "%s {", msg );
	for( i=0; i<=max; i++ ) {
		if( FD_ISSET(i,set) ) {
			dprintf( D_ALWAYS | D_NOHEADER, "%d ", i );
		}
	}
	dprintf( D_ALWAYS | D_NOHEADER, "}\n" );
}
