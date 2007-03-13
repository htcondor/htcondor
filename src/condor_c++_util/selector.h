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

#ifndef SELECTOR_H
#define SELECTOR_H

#include "condor_common.h"

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
	int select_retval();
	BOOLEAN	has_ready();
	BOOLEAN	timed_out();
	BOOLEAN	signalled();
	BOOLEAN failed();
	BOOLEAN fd_ready( int fd, IO_FUNC interest );
	void display();

private:
	static int _fd_select_size;
	static fd_set *cached_read_fds, *cached_save_read_fds;
	static fd_set *cached_write_fds, *cached_save_write_fds;
	static fd_set *cached_except_fds, *cached_save_except_fds;
	fd_set	*read_fds, *save_read_fds;
	fd_set	*write_fds, *save_write_fds;
	fd_set	*except_fds, *save_except_fds;
	int		fd_set_size;
	int		max_fd;
	BOOLEAN			timeout_wanted;
	struct timeval	timeout;
	SELECTOR_STATE	state;
	int		_select_retval;
};

#endif
