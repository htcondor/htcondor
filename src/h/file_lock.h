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
#ifndef FILE_LOCK_H
#define FILE_LOCK_H

#include "condor_constants.h"   /* Need definition of BOOLEAN */

typedef enum { READ_LOCK, WRITE_LOCK, UN_LOCK } LOCK_TYPE;

#if defined(__cplusplus)

class FileLock {
public:
	FileLock( int fd, FILE *fp = NULL );
	~FileLock();

		// Read only access functions
	BOOLEAN		is_blocking();		// whether or not operations will block
	LOCK_TYPE	get_state();		// cur state READ_LOCK, WRITE_LOCK, UN_LOCK
	void		display();

		// Control functions
	void		set_blocking( BOOLEAN );	// set blocking TRUE or FALSE
	BOOLEAN		obtain( LOCK_TYPE t );		// get a lock
	BOOLEAN		release();					// release a lock

private:
	int			fd;			// File descriptor to deal with
	FILE		*fp;
	BOOLEAN		blocking;	// Whether or not to block
	LOCK_TYPE	state;		// Type of lock we are holding
};

#endif	/* cpluscplus */

#endif
