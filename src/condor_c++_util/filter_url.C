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

 

#include "condor_common.h"
#include "url_condor.h"
#include "util_lib_proto.h"		// for mkargv() proto

#define READ_END 0
#define WRITE_END 1

/** A filer url is of the form "filter:program {|URL}" **/

static
int condor_open_filter( const char *name, int flags, size_t n_bytes )
{
	char	*pipe_char;
	int		child_fd = -1;;
	int		pipe_fds[2];
	int		rval;
	int		child_pid;
	int		grand_child_pid;
	int		return_fd = -1;
	int		status;
	int		argc;
	char	*argv[20];

	pipe_char = (char *)strchr((const char *)name, '|');
	if (pipe_char) {
		*pipe_char = '\0';
		pipe_char++;
		child_fd = open_url(pipe_char, flags, n_bytes);
		if (child_fd < 0) {
			return child_fd;
		}
	}

	rval = pipe(pipe_fds);
	if (rval < 0) {
		return rval;
	}
	
	child_pid = fork();

	if (child_pid) {
		/* The parent */
		if (flags == O_WRONLY) {
			return_fd = pipe_fds[WRITE_END];
			close(pipe_fds[READ_END]);
		} else if (flags == O_RDONLY) {
			return_fd = pipe_fds[READ_END];
			close(pipe_fds[WRITE_END]);
		}


		waitpid(child_pid, &status, 0);
	} else {
		/* The child */
		grand_child_pid = fork();
		if (grand_child_pid) {
			/* Still the child */
			exit(0);
		} else {
			/* The Grandchild */
			if (flags == O_WRONLY) {
				close(0);
				dup(pipe_fds[READ_END]);
				close(pipe_fds[READ_END]);
				close(pipe_fds[WRITE_END]);
				if (child_fd != -1) {
					close(1);
					dup(child_fd);
					close(child_fd);
				}
			} else if (flags == O_RDONLY) {
				close(1);
				dup(pipe_fds[WRITE_END]);
				close(pipe_fds[WRITE_END]);
				close(pipe_fds[READ_END]);
				if (child_fd != -1) {
					close(0);
					dup(child_fd);
					close(child_fd);
				}
			}

			// cast discards const
			mkargv(&argc, argv, (char *) name);
			execv(argv[0], argv);
			fprintf(stderr, "execv(%s) failed!!!!\n", argv[0]);
			exit(-1);
		}
	}

	if (pipe_char) {
		*pipe_char = '|';
	}
	return return_fd;
}


void
init_filter()
{
	static URLProtocol	*FILTER_URL = 0;

	if (FILTER_URL == 0) {
	    FILTER_URL = new URLProtocol("filter", "CondorFilterUrl", 
					 condor_open_filter);
	}
}
