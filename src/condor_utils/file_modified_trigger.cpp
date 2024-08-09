/******************************************************************************
 *
 * Copyright (C) 1990-2018, Condor Team, Computer Sciences Department,
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
 ******************************************************************************/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined( LINUX )
#include <sys/inotify.h>
#endif /* defined( LINUX ) */

#ifndef WIN32
#include <poll.h>
#endif

#include "utc_time.h"
#include "file_modified_trigger.h"


FileModifiedTrigger::FileModifiedTrigger( const std::string & f ) :
	filename( f ), initialized( false ), dont_close_statfd(false), statfd_is_pipe(false),
#ifdef LINUX
	inotify_fd(-1), inotify_initialized( false ),
#endif
	statfd( -1 ), lastSize( 0 )
{
	if (filename == "-") {
		dont_close_statfd = true;
		statfd = fileno(stdin);
	#ifdef WIN32
		intptr_t oshandle = _get_osfhandle(statfd);
		if (oshandle >= 0 && FILE_TYPE_PIPE == GetFileType((HANDLE)oshandle)) {
			statfd_is_pipe = true;
		}
	#else
	#endif
		initialized = true;
		return; // we are watching stdin...
	}

	statfd = open( filename.c_str(), O_RDONLY );
	if( statfd == -1 ) {
		dprintf( D_ALWAYS, "FileModifiedTrigger( %s ): open() failed: %s (%d).\n", filename.c_str(), strerror(errno), errno );
		return;
	}

	initialized = true;
}

FileModifiedTrigger::~FileModifiedTrigger() {
	releaseResources();
}

void
FileModifiedTrigger::releaseResources() {
#if defined( LINUX )
	if( inotify_initialized && inotify_fd != -1 ) {
		close( inotify_fd );
		inotify_fd = -1;
	}
	inotify_initialized = false;
#endif /* defined( LINUX ) */

	if( initialized && statfd != -1 ) {
		if ( ! dont_close_statfd) close( statfd );
		statfd = -1;
	}
	initialized = false;
}

//
// We would like to support the Linux and Windows -specific nonpolling
// solutions for detecting changes to the log file, but inotify blocks
// forever on network filesystems, and there's no Linux API which will
// let us know that ahead of time.  Instead, we use a polling approach
// on all platforms, but instead of sleeping on some platforms, we use
// inotify with the sleep duration as its timeout.
//
// On Linux, this means that notify_or_sleep() "sleeps" in poll(), and
// if the inotify fd goes hot, uses read_inotify_events() to clear the
// fd for the next time.
//

#if defined( LINUX )

int
FileModifiedTrigger::read_inotify_events( void ) {
	// Magic from 'man inotify'.
	char buf[ sizeof(struct inotify_event) + NAME_MAX + 1 ]
		__attribute__ ((aligned(__alignof__(struct inotify_event))));

	while( true ) {
		ssize_t len = read( inotify_fd, buf, sizeof( buf ) );
		if( len == -1 && errno != EAGAIN ) {
			dprintf( D_ALWAYS, "FileModifiedTrigger::read_inotify_events(%s): failed to ready from inotify fd.\n", filename.c_str() );
			return -1;
		}

		// We're done reading events for now.
		if( len <= 0 ) { return 1; }

		char * ptr = buf;
		for( ; ptr < buf + len; ptr += sizeof(struct inotify_event) + ((struct inotify_event *)ptr)->len ) {
			const struct inotify_event * event = (struct inotify_event *)ptr;
			if(! (event->mask & IN_MODIFY) ) {
				dprintf( D_ALWAYS, "FileModifiedTrigger::read_inotify_events(%s): inotify gave me an event I didn't ask for.\n", filename.c_str() );
				return -1;
			}
		}

		// We don't worry about partial reads because we're only watching
		// one file for one event type, and the kernel will coalesce
		// identical events, so we should only ever see one. Nonetheless,
		// we'll verify here that we read only complete events.
		if( ptr != buf + len ) {
			dprintf( D_ALWAYS, "FileModifiedTrigger::read_inotify_events(%s): partial inotify read.\n", filename.c_str() );
			return -1;
		}
	}

	return 1;
}

int
FileModifiedTrigger::notify_or_sleep( int timeout_in_ms ) {
	if(! inotify_initialized) {
#if defined( IN_NONBLOCK )
		inotify_fd = inotify_init1( IN_NONBLOCK );
#else
		inotify_fd = inotify_init();
		int flags = fcntl(inotify_fd, F_GETFL, 0);
		fcntl(inotify_fd, F_SETFL, flags | O_NONBLOCK);
#endif /* defined( IN_NONBLOCK ) */
		if( inotify_fd == -1 ) {
			dprintf( D_ALWAYS, "FileModifiedTrigger( %s ): inotify_init() failed: %s (%d).\n", filename.c_str(), strerror(errno), errno );
			return -1;
		}

		int wd = inotify_add_watch( inotify_fd, filename.c_str(), IN_MODIFY );
		if( wd == -1 ) {
			dprintf( D_ALWAYS, "FileModifiedTrigger( %s ): inotify_add_watch() failed: %s (%d).\n", filename.c_str(), strerror( errno ), errno );
			close(inotify_fd);
			return -1;
		}

		inotify_initialized = true;
	}

	struct pollfd pollfds[1];
	pollfds[0].fd = inotify_fd;
	pollfds[0].events = POLLIN;
	pollfds[0].revents = 0;

	int events = poll( pollfds, 1, timeout_in_ms );
	switch( events ) {
		case -1:
			return -1;

		case 0:
			return 0;

		default:
			if( pollfds[0].revents & POLLIN ) {
				return read_inotify_events();
			} else {
				dprintf( D_ALWAYS, "FileModifiedTrigger::wait(): inotify returned an event I didn't ask for.\n" );
				return -1;
			}
	}
}

#else

#ifdef WIN32
int ms_sleep(int ms) { Sleep(ms); return 0; }
#else
int ms_sleep(int ms) { return usleep((useconds_t)ms * 1000); }
#endif

int
FileModifiedTrigger::notify_or_sleep( int timeout_in_ms ) {
	if (statfd_is_pipe) {
	#ifdef WIN32
		intptr_t oshandle = _get_osfhandle(statfd);
		if (oshandle < 0) {
			// fall through to sleep
		} else {
			if (WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)oshandle, timeout_in_ms)) {
				return 0;
			} else {
				return -1; // timeout or error
			}
		}
	#else
		struct pollfd fds;
		fds.fd = statfd;
		fds.events = POLLIN;
		fds.revents = 0;

		int rv = poll(&fds, 1, timeout_in_ms);
		if (rv > 0 || rv < 0) { rv = -1; }
		return rv;
	#endif
	}
	return ms_sleep( timeout_in_ms );
}

#endif /* defined( LINUX ) */

int
FileModifiedTrigger::wait( int timeout_in_ms ) {
	if(! initialized) {
		return -1;
	}

	struct timeval deadline;
	condor_gettimestamp( deadline );

	deadline.tv_sec += timeout_in_ms / 1000;
	deadline.tv_usec += (timeout_in_ms % 1000) * 1000;
	if( deadline.tv_usec >= 1'000'000 ) {
		deadline.tv_sec += 1;
		deadline.tv_usec = deadline.tv_usec % 1'000'000;
	}

	while( true ) {
		struct stat statbuf;
		int rv = fstat( statfd, & statbuf );
		if( rv != 0 ) {
			dprintf( D_ALWAYS, "FileModifiedTrigger::wait(): fstat() failure on previously-valid fd: %s (%d).\n", strerror(errno), errno );
			return -1;
		}

		bool changed = statbuf.st_size != lastSize;
		lastSize = statbuf.st_size;
		if( changed ) { return 1; }

		int waitfor = 5000;
		if( timeout_in_ms >= 0 ) {
			struct timeval now;
			condor_gettimestamp( now );

			if( deadline.tv_sec < now.tv_sec ) { return 0; }
			else if( deadline.tv_sec == now.tv_sec &&
				deadline.tv_usec < now.tv_usec ) { return 0; }
			waitfor = ((deadline.tv_sec - now.tv_sec) * 1000) +
						((deadline.tv_usec - now.tv_usec) / 1000);
			if( waitfor > 5000 ) { waitfor = 5000; }
		}

		int events = notify_or_sleep( waitfor );
		if( events == 1 ) { return 1; }
		if( events == 0 ) { continue; }
		return -1;
	}

}
