/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

typedef enum {
	IO_READ, IO_WRITE, IO_EXCEPT
} IO_FUNC;

typedef enum {
	VIRGIN, FDS_READY, TIMED_OUT, SIGNALLED
} SELECTOR_STATE;

class Selector {
public:
	Selector();
	void add_fd( int fd, IO_FUNC interest );
	void delete_fd( int fd, IO_FUNC interest );
	void set_timeout( clock_t ticks );
	void unset_timeout();
	void	execute();
	BOOLEAN	has_ready();
	BOOLEAN	timed_out();
	BOOLEAN	signalled();
	BOOLEAN fd_ready( int fd, IO_FUNC interest );
	void display();

private:
	fd_set	read_fds, save_read_fds;
	fd_set	write_fds, save_write_fds;
	fd_set	except_fds, save_except_fds;
	int		max_fd;
	BOOLEAN			timeout_wanted;
	struct timeval	timeout;
	SELECTOR_STATE	state;
};

#endif
