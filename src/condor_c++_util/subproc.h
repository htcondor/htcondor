/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
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

/*
  Create a subprocess and either send data to its stdin or read data
  from its stdout via a pipe.  The data connection is made available
  to the parent process as a FILE *, for use with stdio routines.  The
  functionality is quite similar to that offered by popen(), but the
  interface is in C++ style.  Also (and importantly), some versions of
  popen() aren't careful when they wait for the child process, and can
  grab and discard status information from other child processes not
  spawned by popen().  This class uses waitpid(), and will only grab
  the status of the correct child.

  The constructor intializes a SubProc instance with the name of the
  executable and command line arguments to be used.  The other
  parameter should be the string "r" if the parent wants to read data
  from the child, and "w" if the parent wants to write data to the
  child.  To actually execute the subprocess, use the run() function.
  To wait for the subprocess to complete use the terminate() function.
  Each instance of a SubProc object is designed to only run one
  copy of the process at a time, though an instance can be re-used
  for subsequent invocations of the same subprocess.  If you need
  multiple subprocesses running simultaneously, you could use an
  array or a list of SubProcess objects.
*/


#ifndef _SUBPROC_H
#define _SUBPROC_H

#include <stdio.h>

#if defined(__cplusplus)

class SubProc {
enum { IS_WRITE, IS_READ };
public:
	SubProc( const char *exec_name, const char *args, const char *rw );
	~SubProc();
	FILE *run();
	int terminate();
	void display();
	char *parse_output( const char *targ, const char *left, const char *right );
	char *get_stderr();
private:
	int		do_kill();
	int		do_wait();

	char	*name;
	char	*args;
	int 	direction;
	int		is_running;
	pid_t	pid;
	int		has_run;
	int		exit_status;
	int		saved_errno;
	FILE	*fp;
	FILE	*stderr_fp;
	char	err_buf[ 1024 ];
};
#endif

/*
  Simple "C" interface.  Here there is only one instance of the class,
  and only one invocation of the subprocess is possible.  The
  terminate_program() routine returns the exit status of the subprocess.
*/
#if defined(__cplusplus)
extern "C" {
#endif
	FILE *execute_program( const char *cmd, const char *args, const char *mode);
	int terminate_program();
#if defined(__cplusplus)
}
#endif

#endif
