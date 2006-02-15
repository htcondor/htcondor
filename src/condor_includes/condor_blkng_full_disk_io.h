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
#ifndef _CONDOR_BLKNG_FULL_DISK_IO_H
#define _CONDOR_BLKNG_FULL_DISK_IO_H

/* This file implements blocking read/write operations on a regular
	files only.  The main purpose is to read/write however many
	bytes you actually specified ans these functions will try very
	hard to do just that. */

BEGIN_C_DECLS

/* These functions are the actual implementation, and must be used if
	you want this functionality in the remote i/o or condor_checkpointing
	libraries. Be warned, however, that they do require libc to be mapped
	correctly. */
ssize_t _condor_full_read(int fd, void *ptr, size_t nbytes);
ssize_t _condor_full_write(int fd, void *ptr, size_t nbytes);

/* For generic Condor code not in the above libraries, use these functions */
ssize_t full_read(int filedes, void *ptr, size_t nbyte);
ssize_t full_write(int filedes, void *ptr, size_t nbyte);

END_C_DECLS

#endif
