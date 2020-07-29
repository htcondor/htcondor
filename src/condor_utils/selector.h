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


#ifndef SELECTOR_H
#define SELECTOR_H

#include "condor_common.h"

#ifdef CONDOR_HAVE_POLL
#define SELECTOR_USE_POLL 1
#endif

#ifdef SELECTOR_USE_POLL
#include <poll.h>
#else
// We define stubs for pollfd so we don't have to sprinkle our
// code with ifdef's
struct fake_pollfd {
	int   fd;         /* file descriptor */
	short events;     /* requested events */
	short revents;    /* returned events */
};
#endif

class Selector {
public:
	Selector();
	~Selector();
	static int fd_select_size();

	enum IO_FUNC {
		IO_READ, IO_WRITE, IO_EXCEPT
	};

	enum SELECTOR_STATE {
		VIRGIN, FDS_READY, TIMED_OUT, SIGNALLED, FAILED
	};

	void reset();
	void add_fd( int fd, IO_FUNC interest );
	void delete_fd( int fd, IO_FUNC interest );
	void set_timeout( time_t sec, long usec = 0 );
	void set_timeout( timeval tv );
	void unset_timeout();
	void execute();
	int select_retval() const;
	int select_errno() const;
	bool has_ready();
	bool timed_out();
	bool signalled();
	bool failed();
	bool fd_ready( int fd, IO_FUNC interest );
	void display();

private:

	void init_fd_sets();

	enum SINGLE_SHOT {
		SINGLE_SHOT_VIRGIN, SINGLE_SHOT_OK, SINGLE_SHOT_SKIP
	};

	static int _fd_select_size;
	fd_set	*read_fds, *save_read_fds;
	fd_set	*write_fds, *save_write_fds;
	fd_set	*except_fds, *save_except_fds;
	int		fd_set_size;
	int		max_fd;
	bool	timeout_wanted;
	struct timeval	timeout;
	SELECTOR_STATE	state;
	int		_select_retval;
	int		_select_errno;

	SINGLE_SHOT m_single_shot;
#ifdef SELECTOR_USE_POLL
	struct pollfd m_poll;
#else
	struct fake_pollfd m_poll;
#endif
};

void display_fd_set( const char *msg, fd_set *set, int max,
					 bool try_dup = false);

#endif
