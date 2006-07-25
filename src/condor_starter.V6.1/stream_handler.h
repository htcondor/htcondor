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
#ifndef STREAM_HANDLER_H
#define STREAM_HANDLER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#define STREAM_BUFFER_SIZE 4096

class StreamHandler : public Service {
public:
	StreamHandler();

	bool Init( const char *filename, const char *streamname, bool is_output );
	int Handler( int fd );
	int GetJobPipe();

private:
	char	buffer[STREAM_BUFFER_SIZE];
	char	filename[_POSIX_PATH_MAX];
	char	streamname[_POSIX_PATH_MAX];
	bool	is_output;
	int	pipe_fds[2];
	int	job_pipe;
	int	handler_pipe;
	int	remote_fd;
	off_t	offset;
};


#endif
