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
#ifndef FILE_LOCK_H
#define FILE_LOCK_H

typedef enum { READ_LOCK, WRITE_LOCK, UN_LOCK } LOCK_TYPE;

#if defined(__cplusplus)

class FileLock {
public:
	FileLock( int fd, FILE *fp = NULL );
	~FileLock();

		// Read only access functions
	bool		is_blocking();		// whether or not operations will block
	LOCK_TYPE	get_state();		// cur state READ_LOCK, WRITE_LOCK, UN_LOCK
	void		display();

		// Control functions
	void		set_blocking( bool );	// set blocking TRUE or FALSE
	bool		obtain( LOCK_TYPE t );		// get a lock
	bool		release();					// release a lock

private:
	int			fd;			// File descriptor to deal with
	FILE		*fp;
	bool		blocking;	// Whether or not to block
	LOCK_TYPE	state;		// Type of lock we are holding
};

#endif	/* cpluscplus */

#endif
