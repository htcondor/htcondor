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
