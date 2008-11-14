/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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


 

#include "condor_common.h"
#include "url_condor.h"
#include "util_lib_proto.h"		// for mkargv() proto

#define READ_END 0
#define WRITE_END 1

/** A filer url is of the form "filter:program {|URL}" **/

static
int condor_open_filter( const char *name, int flags )
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
		child_fd = open_url(pipe_char, flags);
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

			char *name_cpy = strdup( name );
			mkargv(&argc, argv, name_cpy);
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
