/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


 

#include "condor_common.h"
#include "condor_debug.h"
#include "selector.h"
#include "condor_threads.h"

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
	_select_errno = 0;
	state = VIRGIN;
	timeout_wanted = FALSE;
	timeout.tv_sec = timeout.tv_usec = 0;

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

	if (IsDebugLevel(D_DAEMONCORE)) {
		dprintf(D_FULLDEBUG, "selector %p resetting\n", this);
	}
}

int
Selector::fd_select_size()
{
	if ( _fd_select_size < 0 ) {
#if defined(WIN32)
		// On Windows, fd_set is not a bit-array, but a structure that can
		// hold up to 1024 different file descriptors (not just file 
		// descriptors whose value is less than 1024). We set max_fds to
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

/*
 * Returns a newly-allocated null-terminated string describing the fd
 * (filename or pipe/socket info).  Currently only implemented on Linux,
 * implemented via /proc/.
 */
char* 
describe_fd(int fd) {
#if defined(LINUX)
#define PROC_BUFSIZE 32
#define LINK_BUFSIZE 256
  char proc_buf[PROC_BUFSIZE];
  char link_buf[LINK_BUFSIZE + 1];
  ssize_t end;
  
  memset(link_buf, '\0', LINK_BUFSIZE);

  snprintf(proc_buf, PROC_BUFSIZE, "/proc/self/fd/%d", fd);
  end = readlink(proc_buf, link_buf, LINK_BUFSIZE);

  if (end == -1) { 
    goto cleanup; 
  }

  link_buf[end] = '\0';
  return strdup(link_buf);
 cleanup:
#else
  if (fd) {}
#endif
  return strdup("");
}

void
Selector::add_fd( int fd, IO_FUNC interest )
{
	// update max_fd (the highest valid index in fd_set's array) and also
        
	// make sure we're not overflowing our fd_set
	// On Windows, we have to check the individual fd_set to see if it's
	// full.
	if( fd > max_fd ) {
		max_fd = fd;
	}
#if !defined(WIN32)
	if ( fd < 0 || fd >= fd_select_size() ) {
		EXCEPT( "Selector::add_fd(): fd %d outside valid range 0-%d",
				fd, _fd_select_size-1 );
	}
#endif


	if(IsDebugLevel(D_DAEMONCORE)) {
		char *fd_description = describe_fd(fd);

		dprintf(D_FULLDEBUG, "selector %p adding fd %d (%s)\n",
				this, fd, fd_description);

		free(fd_description);
	}

	switch( interest ) {

	  case IO_READ:
#if defined(WIN32)
		if ( save_read_fds->fd_count >= fd_select_size() ) {
			EXCEPT( "Selector::add_fd(): read fd_set is full" );
		}
#endif
		FD_SET( fd, save_read_fds );
		break;

	  case IO_WRITE:
#if defined(WIN32)
		if ( save_write_fds->fd_count >= fd_select_size() ) {
			EXCEPT( "Selector::add_fd(): write fd_set is full" );
		}
#endif
		FD_SET( fd, save_write_fds );
		break;

	  case IO_EXCEPT:
#if defined(WIN32)
		if ( save_except_fds->fd_count >= fd_select_size() ) {
			EXCEPT( "Selector::add_fd(): except fd_set is full" );
		}
#endif
		FD_SET( fd, save_except_fds );
		break;

	}
}

void
Selector::delete_fd( int fd, IO_FUNC interest )
{
#if !defined(WIN32)
	if ( fd < 0 || fd >= fd_select_size() ) {
		EXCEPT( "Selector::delete_fd(): fd %d outside valid range 0-%d",
				fd, _fd_select_size-1 );
	}
#endif

	if (IsDebugLevel(D_DAEMONCORE)) {
		dprintf(D_FULLDEBUG, "selector %p deleting fd %d\n", this, fd);
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

		// select() ignores its first argument on Windows. We still track
		// max_fd for the display() functions.
	start_thread_safe("select");
	nfds = select( max_fd + 1, 
				  (SELECT_FDSET_PTR) read_fds, 
				  (SELECT_FDSET_PTR) write_fds, 
				  (SELECT_FDSET_PTR) except_fds, 
				  tp );
	stop_thread_safe("select");
	_select_retval = nfds;

	if( nfds < 0 ) {
		_select_errno = errno;
#if !defined(WIN32)
		if( errno == EINTR ) {
			state = SIGNALLED;
			return;
		}
#endif
		state = FAILED;
		return;
	}
	_select_errno = 0;

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

int
Selector::select_errno()
{
	return _select_errno;
}

BOOLEAN
Selector::fd_ready( int fd, IO_FUNC interest )
{
	if( state != FDS_READY && state != TIMED_OUT ) {
		EXCEPT(
			"Selector::fd_ready() called, but selector not in FDS_READY state"
		);
	}

#if !defined(WIN32)
	// on UNIX, make sure the value of fd makes sense
	//
	if ( fd < 0 || fd >= fd_select_size() ) {
		return FALSE;
	}
#endif

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
	bool try_dup = ( (FAILED == state) &&  (EBADF == _select_errno) );
	display_fd_set( "\tRead", save_read_fds, max_fd, try_dup );
	display_fd_set( "\tWrite", save_write_fds, max_fd, try_dup );
	display_fd_set( "\tExcept", save_except_fds, max_fd, try_dup );

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
display_fd_set( const char *msg, fd_set *set, int max, bool try_dup )
{
	int		i, count;

	dprintf( D_ALWAYS, "%s {", msg );
	for( i=0, count=0; i<=max; i++ ) {
		if( FD_ISSET(i,set) ) {
			count++;

			dprintf( D_ALWAYS | D_NOHEADER, "%d", i );

			if( try_dup ) {
#			  if defined( UNIX )
				int newfd = dup( i );
				if ( newfd >= 0 ) {
					close( newfd );
				}
				else if ( EBADF == errno ) {
					dprintf( D_ALWAYS | D_NOHEADER, "<EBADF> " );
				}
				else {
					dprintf( D_ALWAYS | D_NOHEADER, "<%d> ", errno );
				}
#			  endif
			}

			dprintf( D_ALWAYS | D_NOHEADER, " " );
		}
	}
	dprintf( D_ALWAYS | D_NOHEADER, "} = %d\n", count );
}
