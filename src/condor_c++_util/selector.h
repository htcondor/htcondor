/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
