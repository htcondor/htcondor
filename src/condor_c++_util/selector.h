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

#ifndef SELECTOR_H
#define SELECTOR_H

#include <sys/times.h>
#include "condor_fdset.h"



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
