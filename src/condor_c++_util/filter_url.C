/* 
** Copyright 1995 by Miron Livny and Jim Pruyne
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
** Author:  Jim Pruyne
**
*/ 

#include <string.h>
#include <sys/wait.h>
#include "condor_common.h"
#include "condor_fix_wait.h"
#include "url_condor.h"

#define READ_END 0
#define WRITE_END 1

static int
makeargv( int *argc, char *argv[], char *line )
{
	int		count, instring;
	char	*ptr;

	count = 0;
	instring = 0;
	for( ptr=line; *ptr; ptr++ ) {
		if( isspace(*ptr) ) {
			instring = 0;
			*ptr = '\0';
		}
		else if( !instring ) {
			argv[count++] = ptr;
			instring = 1;
		}
	}
	argv[count] = 0;
	*argc = count;
}

/* A filer url is of the form "filter:program {|URL}" */

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

	pipe_char = strchr(name, '|');
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
			makeargv(&argc, argv, (char *) name);
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
