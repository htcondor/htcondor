/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#include "condor_common.h"
#include "condor_debug.h"
#include "selector.h"


void display_fd_set( char *msg, fd_set *set, int max );

int Selector::_fd_select_size = -1;

fd_set *Selector::cached_read_fds = NULL;
fd_set *Selector::cached_write_fds = NULL;
fd_set *Selector::cached_except_fds = NULL;

fd_set *Selector::cached_save_read_fds = NULL;
fd_set *Selector::cached_save_write_fds = NULL;
fd_set *Selector::cached_save_except_fds = NULL;

Selector::Selector()
{
#if defined(WIN32)
		// On Windows, we can't treat fd_set as an open-ended bit array.
		// fd_set can take up to FD_SETSIZE sockets (not any socket whose
		// fd is <= FD_SETSIZE) and that's it. Currently, we set
		// FD_SETSIZE to 1024. I'm not sure what we can do if we ever
		// have more than 1024 sockets to select on.
	fd_set_size = 1;
#else
	int nfdbits = 8 * sizeof(fd_set);
	fd_set_size = ( fd_select_size() + (nfdbits - 1) ) / nfdbits;
#endif

	if ( cached_read_fds ) {
		read_fds = cached_read_fds;
		write_fds = cached_write_fds;
		except_fds = cached_except_fds;

		save_read_fds = cached_save_read_fds;
		save_write_fds = cached_save_write_fds;
		save_except_fds = cached_save_except_fds;

		cached_read_fds = NULL;
		cached_write_fds = NULL;
		cached_except_fds = NULL;
		cached_save_read_fds = NULL;
		cached_save_write_fds = NULL;
		cached_save_except_fds = NULL;
	} else {
		read_fds = (fd_set *)calloc( fd_set_size, sizeof(fd_set) );
		write_fds = (fd_set *)calloc( fd_set_size, sizeof(fd_set) );
		except_fds = (fd_set *)calloc( fd_set_size, sizeof(fd_set) );

		save_read_fds = (fd_set *)calloc( fd_set_size, sizeof(fd_set) );
		save_write_fds = (fd_set *)calloc( fd_set_size, sizeof(fd_set) );
		save_except_fds = (fd_set *)calloc( fd_set_size, sizeof(fd_set) );
	}

	reset();
}

Selector::~Selector()
{
	if ( cached_read_fds == NULL ) {
		cached_read_fds = read_fds;
		cached_write_fds = write_fds;
		cached_except_fds = except_fds;

		cached_save_read_fds = save_read_fds;
		cached_save_write_fds = save_write_fds;
		cached_save_except_fds = save_except_fds;
	} else {
		free( read_fds );
		free( write_fds );
		free( except_fds );

		free( save_read_fds );
		free( save_write_fds );
		free( save_except_fds );
	}
}

void
Selector::reset()
{
	_select_retval = -2;
	state = VIRGIN;
	timeout_wanted = FALSE;
	max_fd = -1;
#if defined(WIN32)
	FD_ZERO( save_read_fds );
	FD_ZERO( save_write_fds );
	FD_ZERO( save_except_fds );
#else
	memset( save_read_fds, 0, fd_set_size * sizeof(fd_set) );
	memset( save_write_fds, 0, fd_set_size * sizeof(fd_set) );
	memset( save_except_fds, 0, fd_set_size * sizeof(fd_set) );
#endif
}

int
Selector::fd_select_size()
{
	if ( _fd_select_size < 0 ) {
#if defined(WIN32)
		// On Windows, fd_set is not a bit-array, but a structure that can
		// hold up to 1024 different file descriptors (not just file 
		// descriptors whose value is less than 1024). We set max_fd to
		// 1024 (FD_SETSIZE) as a reasonable approximation.
		_fd_select_size = FD_SETSIZE;
#elif defined(Solaris)
		// Solaris's select() can't handle fds greater than FD_SETSIZE.
		int max_fds = getdtablesize();
		if ( max_fds < FD_SETSIZE ) {
			_fd_select_size = max_fds;
		} else {
			_fd_select_size = FD_SETSIZE;
		}
#else
		_fd_select_size = getdtablesize();
#endif
	}
	return _fd_select_size;
}

void
Selector::add_fd( int fd, IO_FUNC interest )
{
	if ( fd < 0 || fd > fd_select_size() ) {
		EXCEPT( "Selector::add_fd(): fd %d outside valid range 0-%d",
				fd, _fd_select_size-1 );
	}

	switch( interest ) {

	  case IO_READ:
		FD_SET( fd, save_read_fds );
		break;

	  case IO_WRITE:
		FD_SET( fd, save_write_fds );
		break;

	  case IO_EXCEPT:
		FD_SET( fd, save_except_fds );
		break;

	}

	if( fd > max_fd ) {
		max_fd = fd;
	}
}

void
Selector::delete_fd( int fd, IO_FUNC interest )
{
	if ( fd < 0 || fd > fd_select_size() ) {
		EXCEPT( "Selector::delete_fd(): fd %d outside valid range 0-%d",
				fd, _fd_select_size-1 );
	}

	switch( interest ) {

	  case IO_READ:
		FD_CLR( fd, save_read_fds );
		break;

	  case IO_WRITE:
		FD_CLR( fd, save_write_fds );
		break;

	  case IO_EXCEPT:
		FD_CLR( fd, save_except_fds );
		break;

	}
}

void
Selector::set_timeout( time_t sec, long usec )
{
	timeout_wanted = TRUE;

	timeout.tv_sec = sec;
	timeout.tv_usec = usec;
}

void
Selector::set_timeout( timeval tv )
{
	timeout_wanted = TRUE;

	timeout = tv;
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

	memcpy( read_fds, save_read_fds, fd_set_size * sizeof(fd_set) );
	memcpy( write_fds, save_write_fds, fd_set_size * sizeof(fd_set) );
	memcpy( except_fds, save_except_fds, fd_set_size * sizeof(fd_set) );

	if( timeout_wanted ) {
		tp = &timeout;
	} else {
		tp = NULL;
	}

	nfds = select( max_fd + 1, 
				  (SELECT_FDSET_PTR) read_fds, 
				  (SELECT_FDSET_PTR) write_fds, 
				  (SELECT_FDSET_PTR) except_fds, 
				  tp );
	_select_retval = nfds;

	if( nfds < 0 ) {
#if !defined(WIN32)
		if( errno == EINTR ) {
			state = SIGNALLED;
			return;
		}
#endif
		state = FAILED;
		return;
	}

	if( nfds == 0 ) {
		state = TIMED_OUT;
	} else {
		state = FDS_READY;
	}
	return;
}

int
Selector::select_retval()
{
	return _select_retval;
}

BOOLEAN
Selector::fd_ready( int fd, IO_FUNC interest )
{
	if( state != FDS_READY && state != TIMED_OUT ) {
		EXCEPT(
			"Selector::fd_ready() called, but selector not in FDS_READY state"
		);
	}

	if ( fd < 0 || fd > fd_select_size() ) {
		return FALSE;
	}

	switch( interest ) {

	  case IO_READ:
		return FD_ISSET( fd, read_fds );
		break;

	  case IO_WRITE:
		return FD_ISSET( fd, write_fds );
		break;

	  case IO_EXCEPT:
		return FD_ISSET( fd, except_fds );
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
Selector::failed()
{
	return state == FAILED;
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

	  case FAILED:
		dprintf( D_ALWAYS, "State = FAILED\n" );
		break;
	}

	dprintf( D_ALWAYS, "max_fd = %d\n", max_fd );

	dprintf( D_ALWAYS, "Selection FD's\n" );
	display_fd_set( "\tRead", save_read_fds, max_fd );
	display_fd_set( "\tWrite", save_write_fds, max_fd );
	display_fd_set( "\tExcept", save_except_fds, max_fd );

	if( state == FDS_READY ) {
		dprintf( D_ALWAYS, "Ready FD's\n" );
		display_fd_set( "\tRead", read_fds, max_fd );
		display_fd_set( "\tWrite", write_fds, max_fd );
		display_fd_set( "\tExcept", except_fds, max_fd );
	}
	if( timeout_wanted ) {
		dprintf( D_ALWAYS,
			"Timeout = %ld.%06ld seconds\n", (long) timeout.tv_sec, 
			(long) timeout.tv_usec
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
