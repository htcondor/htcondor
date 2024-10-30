/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_arglist.h"
#include "my_popen.h"
#include "sig_install.h"
#include "env.h"
#include "setenv.h"

#ifndef WIN32
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include "largestOpenFD.h"
#endif

#ifdef WIN32
typedef HANDLE child_handle_t;
#define INVALID_CHILD_HANDLE INVALID_HANDLE_VALUE
#else
typedef pid_t child_handle_t;
#define INVALID_CHILD_HANDLE ((pid_t)-1)
#endif


/* We manage outstanding children with a simple linked list. */
struct popen_entry {
	FILE* fp;
	child_handle_t ch;
	struct popen_entry *next;
};
struct popen_entry *popen_entry_head = NULL;

// Several functions here have return values that we can't
// sensibly check.  To quiet our static analyzers, the functions write
// return values to this dummy variable, which is never read.
static int dummy_global = 0;

static void add_child(FILE* fp, child_handle_t ch)
{
	struct popen_entry *pe = (struct popen_entry *)malloc(sizeof(struct popen_entry));
	ASSERT(pe);
	pe->fp = fp;
	pe->ch = ch;
	pe->next = popen_entry_head;
	popen_entry_head = pe;
}

static child_handle_t remove_child(FILE* fp)
{
	struct popen_entry *pe = popen_entry_head;
	struct popen_entry **last_ptr = &popen_entry_head;
	while (pe != NULL) {
		if (pe->fp == fp) {
			child_handle_t ch = pe->ch;
			*last_ptr = pe->next;
			free(pe);
			return ch;
		}
		last_ptr = &(pe->next);
		pe = pe->next;
	}
	return INVALID_CHILD_HANDLE;
}

/* not currently used.
static struct popen_entry * find_child(FILE* fp)
{
	struct popen_entry *pe = popen_entry_head;
	while (pe != NULL) {
		if (pe->fp == fp) { return pe; }
		pe = pe->next;
	}
	return NULL;
}
*/



/*

  This is a popen(3)-like function that intentionally avoids
  calling out to the shell in order to limit what can be done for
  security reasons. Please note that this does not intend to behave
  in the same way as a normal popen, it exists as a convenience. 
  
  This function is careful in how it waits for its children's
  status so that it doesn't reap status information for other
  processes which the calling code may want to reap.

  However, we do *NOT* "eat" SIGCHLD, since a) SIGCHLD is
  probably blocked when this method is invoked, b) there are cases
  where our attempt to eat SIGCHLD might result in eating too many of
  them, and that's really bad, c) to try to do this right would be
  very complicated code in here and, d) the worst thing that happens
  is our caller gets and extra SIGCHLD, which, in the case of
  DaemonCore, just results in a D_FULLDEBUG and nothing more.  the
  potential harm of doing this wrong far outweighs the harm of this
  extra dprintf()... Derek Wright <wright@cs.wisc.edu> 2004-05-27

  my_popen() also has some functionality beyond popen() via various
  function parameters (most of which are optional):
      want_stderr - in mode "r", merge stdout and stderr
	  env_ptr - explicitly specify environment for child process
	  drop_privs - (unix only) set realuid equal to effective uid in child
	  write_data - (unix only) in mode "r", this null-terminated buffer will be immdiately
	      sent to the child process' stdin.  limited in size to 2k to prevent deadlock.
		  this allows my_popen() to both read and write data to a child process.
*/

#ifdef WIN32

//////////////////////////////////////////////////////////////////////////
// Windows versions of my_popen / my_pclose 
//////////////////////////////////////////////////////////////////////////

FILE *
my_popen(const char *const_cmd, const char *mode, int options)
{
	int want_stderr = (options & MY_POPEN_OPT_WANT_STDERR);
	int fail_quietly = (options & MY_POPEN_OPT_FAIL_QUIETLY);
	BOOL read_mode;
	SECURITY_ATTRIBUTES saPipe;
	HANDLE hReadPipe, hWritePipe;
	HANDLE hParentPipe, hChildPipe;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	char *cmd;
	BOOL result;
	int fd;
	FILE *retval;

	if (!mode)
		return NULL;
	read_mode = mode[0] == 'r';

	// use SECURITY_ATTRIBUTES to mark pipe handles as inheritable
	saPipe.nLength = sizeof(SECURITY_ATTRIBUTES);
	saPipe.lpSecurityDescriptor = NULL;
	saPipe.bInheritHandle = TRUE;

	// create the pipe (and mark the parent's end as uninheritable)
	if (CreatePipe(&hReadPipe, &hWritePipe, &saPipe, 0) == 0) {
		DWORD err = GetLastError();
		dprintf(D_ALWAYS, "my_popen: CreatePipe failed, err=%d\n", err);
		return NULL;
	}
	if (read_mode) {
		hParentPipe = hReadPipe;
		hChildPipe = hWritePipe;
	}
	else {
		hParentPipe = hWritePipe;
		hChildPipe = hReadPipe;
	}
	SetHandleInformation(hParentPipe, HANDLE_FLAG_INHERIT, 0);

	// initialize STARTUPINFO to set standard handles
	memset(&si, 0, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	if (read_mode) {
		si.hStdOutput = hChildPipe;
		if (want_stderr) {
			si.hStdError = hChildPipe;
		}
		else {
			si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		}
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	}
	else {
		si.hStdInput = hChildPipe;
		si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	}
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	// make call to CreateProcess
	cmd = strdup(const_cmd);
	result = CreateProcess(NULL,                   // determine app from cmd line
	                       cmd,                    // command line
	                       NULL,                   // process SA
	                       NULL,                   // primary thread SA
	                       TRUE,                   // inherit handles 
	                       CREATE_NEW_CONSOLE,     // creation flags
	                       NULL,                 // use our environment : given that this value used to be zkmENV (an Env*), maybe this function would want another argument?
	                       NULL,                   // use our CWD
	                       &si,                    // STARTUPINFO
	                       &pi);                   // receive PROCESS_INFORMATION
	free(cmd);
	if (result == 0) {
		DWORD err = GetLastError();
		CloseHandle(hParentPipe);
		CloseHandle(hChildPipe);
		if ( ! fail_quietly) { dprintf(D_ALWAYS, "my_popen: CreateProcess failed err=%d\n", err); }
		return NULL;
	}

	// don't care about child's primary thread handle
	// or child's ends of the pipes
	CloseHandle(pi.hThread);
	CloseHandle(hChildPipe);

	// convert pipe handle specified in mode into a FILE pointer
	fd = _open_osfhandle((intptr_t)hParentPipe, 0);
	if (fd == -1) {
		CloseHandle(hParentPipe);
		CloseHandle(pi.hProcess);
		dprintf(D_ALWAYS, "my_popen: _open_osfhandle failed\n");
		return NULL;
	}
	retval = _fdopen(fd, mode);
	if (retval == NULL) {
		CloseHandle(hParentPipe);
		CloseHandle(pi.hProcess);
		dprintf(D_ALWAYS, "my_popen: _fdopen failed\n");
		return NULL;
	}

	// save child's process handle (for pclose)
	add_child(retval, pi.hProcess);

	return retval;
}

FILE *
my_popen(const ArgList &args, const char *mode, int want_stderr, const Env *zkmENV, bool drop_privs, const char *write_data)
{
	/* drop_privs HAS NO EFFECT ON WINDOWS */
	/* write_data IS NOT YET IMPLEMENTED ON WINDOWS - we can do so when we need it */

	std::string cmdline;
	if (!args.GetArgsStringWin32(cmdline, 0)) {
		dprintf(D_ALWAYS, "my_popen: error making command line\n");
		return NULL;
	}
	if (write_data && write_data[0]) {
		dprintf(D_ALWAYS, "my_popen: error - write_data option not supported on Windows\n");
		return NULL;
	}
	//  maybe the following function should be extended by an Env* argument? Not sure...
	return my_popen(cmdline.c_str(), mode, want_stderr);
}

FILE *
my_popenv(const char *const args[], const char *mode, int options)
{
	// build the argument list
	ArgList arglist;
	for (int i = 0; args[i] != NULL; i++) {
		arglist.AppendArg(args[i]);
	}

	return my_popen(arglist, mode, options);
}

int
my_pclose_ex(FILE *fp, unsigned int timeout, bool kill_after_timeout)
{
	HANDLE hChildProcess;
	DWORD result;

	hChildProcess = remove_child(fp);

	fclose(fp);

	if (INVALID_CHILD_HANDLE == hChildProcess) {
		return MYPCLOSE_EX_NO_SUCH_FP;
	}

	if (timeout != 0xFFFFFFFF) {
		timeout *= 1000;
	}

	result = WaitForSingleObject(hChildProcess, timeout);
	if (result != WAIT_OBJECT_0) {
		dprintf(D_FULLDEBUG, "my_pclose: Child process has not exited after %u seconds\n", timeout);
		if (kill_after_timeout) {
			TerminateProcess(hChildProcess, -9);
			CloseHandle(hChildProcess);
			return MYPCLOSE_EX_I_KILLED_IT;
		}
	}
	if ( ! GetExitCodeProcess(hChildProcess, &result)) {
		dprintf(D_FULLDEBUG, "my_pclose: GetExitCodeProcess failed with error %d\n", GetLastError());
		result = MYPCLOSE_EX_STATUS_UNKNOWN;
	}
	CloseHandle(hChildProcess);

	return result;
}

// this is the backward compatible my_pclose function that NO CONDOR CODE SHOULD USE!
int my_pclose( FILE *fp )
{
	int rv = my_pclose_ex(fp, 0xFFFFFFFF, false);
	if (rv == MYPCLOSE_EX_NO_SUCH_FP || rv == MYPCLOSE_EX_I_KILLED_IT || rv == MYPCLOSE_EX_STATUS_UNKNOWN) {
		rv = -1; // set a backward compatible exit status.
	}
	return rv;
}

int
my_pclose( FILE *fp, unsigned int timeout, bool kill_after_timeout ) {
	int rv = my_pclose_ex(fp, timeout, kill_after_timeout );
	if (rv == MYPCLOSE_EX_NO_SUCH_FP || rv == MYPCLOSE_EX_I_KILLED_IT || rv == MYPCLOSE_EX_STATUS_UNKNOWN) {
		rv = -1; // set a backward compatible exit status.
	}
	return rv;
}

int
my_system(const char *cmd)
{
	FILE* fp = my_popen(cmd, "w", FALSE);
	return (fp != NULL) ? my_pclose(fp) : -1;
}

#else

//////////////////////////////////////////////////////////////////////////
// UNIX versions of my_popen(v) & my_pclose 
//////////////////////////////////////////////////////////////////////////

#include <fcntl.h> // for O_CLOEXEC

static int	READ_END = 0;
static int	WRITE_END = 1;

static FILE *
my_popenv_impl( const char *const args[],
                const char * mode,
                int options,
		const Env *env_ptr = 0,
		bool drop_privs = true,
		const char *write_data = NULL )
{
	int want_stderr = (options & MY_POPEN_OPT_WANT_STDERR);
	int fail_quietly = (options & MY_POPEN_OPT_FAIL_QUIETLY);
	int	pipe_d[2], pipe_d2[2];
	int pipe_writedata[2];
	int want_writedata = 0;
	int	parent_reads;
	uid_t	euid;
	gid_t	egid;
	pid_t	pid;
	FILE*	retp;

		/* Figure out who reads and who writes on the pipe */
	parent_reads = (mode[0] == 'r');

		/* Create the pipe */
	if( pipe(pipe_d) < 0 ) {
		dprintf(D_ALWAYS, "my_popenv: Failed to create the pipe, "
				"errno=%d (%s)\n", errno, strerror(errno));
		return NULL;
	}

		/* Create a pipe to detect execv failures */
	if ( pipe(pipe_d2) < 0) {
		dprintf(D_ALWAYS, "my_popenv: Failed to create the pre-exec pipe, "
				"errno=%d (%s)\n", errno, strerror(errno));
		close(pipe_d[0]);
		close(pipe_d[1]);
		return NULL;
	}
	int fd_flags;
	if ((fd_flags = fcntl(pipe_d2[1], F_GETFD, NULL)) == -1) {
		dprintf(D_ALWAYS, "my_popenv: Failed to get fd flags: errno=%d (%s)\n", errno, strerror(errno));
		close( pipe_d[0] );
		close( pipe_d[1] );
		close( pipe_d2[0] );
		close( pipe_d2[1] );
		return NULL;
	}
	if (fcntl(pipe_d2[1], F_SETFD, fd_flags | FD_CLOEXEC) == -1) {
		dprintf(D_ALWAYS, "my_popenv: Failed to set new fd flags: errno=%d (%s)\n", errno, strerror(errno));
		close( pipe_d[0] );
		close( pipe_d[1] );
		close( pipe_d2[0] );
		close( pipe_d2[1] );
		return NULL;
	}

// dprintf( D_FULLDEBUG, "my_popenv_impl() =" );
// for( int i = 0; args[i] != NULL; ++i ) {
//	dprintf( D_FULLDEBUG | D_NOHEADER, " |%s|", args[i] );
// }
// dprintf( D_FULLDEBUG | D_NOHEADER, "\n" );

		/* if parent reads and there is write data, create a pipe for that */
	if (parent_reads && write_data && write_data[0]) {
		if (strlen(write_data) > 2048) {
			/* Make sure data fits in pipe buffer to avoid deadlock */
			dprintf(D_ALWAYS,"my_popenv: Write data is too large, failing\n");
			close(pipe_d[0]);
			close(pipe_d[1]);
			close( pipe_d2[0] );
			close( pipe_d2[1] );
			return NULL;
		}
		if ( pipe(pipe_writedata) < 0 ) {
			dprintf(D_ALWAYS, "my_popenv: Failed to create the writedata pipe, "
					"errno=%d (%s)\n", errno, strerror(errno));
			close(pipe_d[0]);
			close(pipe_d[1]);
			close( pipe_d2[0] );
			close( pipe_d2[1] );
			return NULL;
		}
		want_writedata = 1;
	} else {
		pipe_writedata[0] = -1;
		pipe_writedata[1] = -1;
		want_writedata = 0;
	}

		/* Create a new process */
	if( (pid=fork()) < 0 ) {
		dprintf(D_ALWAYS, "my_popenv: Failed to fork child, errno=%d (%s)\n",
				errno, strerror(errno));
			/* Clean up file descriptors */
		close( pipe_d[0] );
		close( pipe_d[1] );
		close( pipe_d2[0] );
		close( pipe_d2[1] );
		close( pipe_writedata[0] );
		close( pipe_writedata[1] );
		return NULL;
	}

		/* The child */
	if( pid == 0 ) {

		/* Don't leak out fds from the parent to our child.
		 * Wish there was a more efficient way to do this, but
		 * this is how we do it in daemoncore CreateProcess...
		 * Of course, do not close stdin/out/err or the fds to
		 * the pipes we just created above.
		 */
		int limit = largestOpenFD();
		for (int jj=3; jj < limit; jj++) {
			if (jj != pipe_d[0] &&
				jj != pipe_d[1] &&
				jj != pipe_d2[0] &&
				jj != pipe_d2[1] &&
				jj != pipe_writedata[0] &&
				jj != pipe_writedata[1])
			{
				close(jj);
			}
		}

		close(pipe_d2[0]);

		if( parent_reads ) {
				/* Close stdin, dup pipe to stdout */
			close( pipe_d[READ_END] );
			bool close_pipe_end = false;
			if( pipe_d[WRITE_END] != 1 ) {
				dup2( pipe_d[WRITE_END], 1 );
				close_pipe_end = true;
			}
			if (want_stderr) {
				if ( pipe_d[WRITE_END] != 2 ) {
					dup2( pipe_d[WRITE_END], 2 );
				}
				else {
					close_pipe_end = false;
				}
			}
			if (close_pipe_end) {
				close(pipe_d[WRITE_END]);
			}
			if (want_writedata) {
				/* close write end of data pipe, dup read end to stdin */
				close( pipe_writedata[WRITE_END] );
				if( pipe_writedata[READ_END] != 0 ) {
					dup2( pipe_writedata[READ_END], 0 );
					close( pipe_writedata[READ_END] );
				}
			}
		} else {
				/* Close stdout, dup pipe to stdin */
			close( pipe_d[WRITE_END] );
			if( pipe_d[READ_END] != 0 ) {
				dup2( pipe_d[READ_END], 0 );
				close( pipe_d[READ_END] );
			}
		}
			/* to be safe, we want to switch our real uid/gid to our
			   effective uid/gid (shedding any privledges we've got).
			   Our supplemental groups should already be set properly
			   by the set_priv() code.
			   we want to run this popen()'ed thing as our effective
			   uid/gid, dropping the real uid/gid.  all of these calls
			   will fail if we don't have a ruid of 0 (root), but
			   that's harmless.  also, note that we have to stash our
			   effective uid, then switch our euid to 0 to be able to
			   set our real uid/gid.
			   We wrap some of the calls in if-statements to quiet some
			   compilers that object to us not checking the return values.
			*/
		if (drop_privs) {
			euid = geteuid();
			egid = getegid();
			if( seteuid( 0 ) ) { }
			if( setgid( egid ) ) { }

			// Some linux environment make it an error, to
			// setuid'ing to our current euid.  So don't check first
			
			if (getuid() != euid) {
				if (setuid(euid) < 0) {
					_exit(ENOEXEC);
				}
			}
		}

			/* before we exec(), clear the signal mask and reset SIGPIPE
			   to SIG_DFL
			*/
		install_sig_handler(SIGPIPE, SIG_DFL);
		sigset_t sigs;
		sigfillset(&sigs);
		sigprocmask(SIG_UNBLOCK, &sigs, NULL);

		std::string cmd = args[0];

			/* set environment if defined */
		if (env_ptr) {
			char **m_unix_env = NULL;
			m_unix_env = env_ptr->getStringArray();
			execve(cmd.c_str(), const_cast<char *const*>(args), m_unix_env );

				// delete the memory even though we're on our way out
				// if exec failed.
			deleteStringArray(m_unix_env);

		} else {
			execvp(cmd.c_str(), const_cast<char *const*>(args) );
		}

			/* If we get here, inform the parent of our errno */
		char result_buf[10];
		int e = errno; // capture real errno

		int len = snprintf(result_buf, 10, "%d", errno);

		// can't do anything with write's error code,
		// save it to global to quiet static analysis
		dummy_global = write(pipe_d2[1], result_buf, len);

		_exit( e );
	}

		/* The parent */
		/* First, wait until the exec is called - determine status */
	close(pipe_d2[1]);
	int exit_code;
	FILE *fh;
	if ((fh = fdopen(pipe_d2[0], "r")) == NULL) {
		dprintf(D_ALWAYS, "my_popenv: Failed to reopen file descriptor as file handle: errno=%d (%s)", errno, strerror(errno));
		close(pipe_d2[0]);
		close(pipe_d[0]);
		close(pipe_d[1]);
		close( pipe_writedata[0] );
		close( pipe_writedata[1] );

		/* Ensure child process is dead, then wait for it to exit */
		kill(pid, SIGKILL);
		while( waitpid(pid,NULL,0) < 0 && errno == EINTR ) {
			/* NOOP */
		}

		return NULL;
	}
		/* Handle case where exec fails */
	if (fscanf(fh, "%d", &exit_code) == 1) {
		fclose(fh);
		close(pipe_d[0]);
		close(pipe_d[1]);
		close( pipe_writedata[0] );
		close( pipe_writedata[1] );

		/* Ensure child process is dead, then wait for it to exit */
		kill(pid, SIGKILL);
		while( waitpid(pid,NULL,0) < 0 && errno == EINTR ) {
			/* NOOP */
		}

		if ( ! fail_quietly) {
			dprintf(D_ALWAYS, "my_popenv: Failed to exec %s, errno=%d (%s)\n",
				(args && args[0]) ? args[0] : "null",
				exit_code,
				strerror(exit_code));
		}
		errno = exit_code;
		return NULL;
	}
	fclose(fh);

	if( parent_reads ) {
		close( pipe_d[WRITE_END] );
		retp = fdopen(pipe_d[READ_END],mode);
		if (want_writedata) {
			close(pipe_writedata[READ_END]);
			if (write(pipe_writedata[WRITE_END],write_data,strlen(write_data)) < 0) {} // we don't care about return 
			close(pipe_writedata[WRITE_END]);
		}
	} else {
		close( pipe_d[READ_END] );
		retp = fdopen(pipe_d[WRITE_END],mode);
	}
	add_child(retp, pid);

	return retp;
}

FILE *
my_popenv( const char *const args[],
           const char * mode,
           int options )
{
	return my_popenv_impl(args, mode, options);
}

static FILE *
my_popen_impl(const ArgList &args,
              const char *mode,
              int options,
              const Env *env_ptr,
              bool drop_privs = true,
			  const char *write_data = NULL)
{
	char **string_array = args.GetStringArray();
	FILE *fp = my_popenv_impl(string_array, mode, options,
			env_ptr, drop_privs, write_data);
	deleteStringArray(string_array);

	return fp;
}

FILE*
my_popen(const ArgList &args, const char *mode, int options, const Env *env_ptr, bool drop_privs,
		 const char *write_data)
{
	return my_popen_impl(args, mode, options, env_ptr, drop_privs, write_data);
}

// this is the backward compatible my_pclose function that NO CONDOR CODE SHOULD USE!
int
my_pclose(FILE *fp)
{
	int			status;
	pid_t			pid;

		/* Pop the child off our list */
	pid = remove_child(fp);

		/* Close the pipe */
	(void)fclose( fp );

		/* Wait for child process to exit and get its status */
	while( waitpid(pid,&status,0) < 0 ) {
		if( errno != EINTR ) {
			status = -1;
			break;
		}
	}

		/* Now return status from child process */
	return status;
}

// returns true if waitpid succeed and status was set.
// status will be set to the exit status of the pid if true is returned
// (or to MYPCLOSE_EX_STATUS_UNKNOWN if exit status is not available)
static bool waitpid_with_timeout(pid_t pid, int *pstatus, time_t timeout)
{
	time_t begin_time = time(NULL);
	for(;;)
	{
		int rv = waitpid(pid,pstatus,WNOHANG);
		if (rv > 0) {
			return true;
		}
		if (rv < 0 && errno != EINTR) {
			*pstatus = MYPCLOSE_EX_STATUS_UNKNOWN;
			return true;
		}
		time_t now = time(NULL);
		if ((now - begin_time) >= timeout) {
			return false;
		}
		usleep(10);
	}
	return false;
}

int
my_pclose( FILE *fp, unsigned int timeout, bool kill_after_timeout ) {
	int rv = my_pclose_ex(fp, timeout, kill_after_timeout );
	if (rv == MYPCLOSE_EX_NO_SUCH_FP || rv == MYPCLOSE_EX_I_KILLED_IT || rv == MYPCLOSE_EX_STATUS_UNKNOWN) {
		rv = -1; // set a backward compatible exit status.
	}
	return rv;
}

int
my_pclose_ex(FILE *fp, unsigned int timeout, bool kill_after_timeout)
{
	int			status;
	pid_t			pid;

		/* Pop the child off our list */
	pid = remove_child(fp);

		/* Close the pipe */
	(void)fclose( fp );

	if (INVALID_CHILD_HANDLE == pid) {
		return MYPCLOSE_EX_NO_SUCH_FP;
	}

		/* Wait for child process to exit and get its status */
	if (waitpid_with_timeout(pid,&status,timeout)) {
		return status;
	}

	// we get here if the wait pid timed out, in that case
	// send a kill signal and wait for it to terminate
	status = MYPCLOSE_EX_STILL_RUNNING;
	if (kill_after_timeout) {
		kill(pid,SIGKILL);
		while (waitpid(pid,&status,0) < 0) {
			if (errno != EINTR) {
				break;
			}
		}
		status = MYPCLOSE_EX_I_KILLED_IT;
	}

		/* Now return status from child process */
	return status;
}


#endif // !WIN32

int
my_systemv(const char *const args[])
{
	FILE* fp = my_popenv(args, "w", FALSE);
	return (fp != NULL) ? my_pclose(fp) : -1;
}

int
my_system(const ArgList &args, const Env *env_ptr)
{
	FILE* fp = my_popen(args, "w", FALSE, env_ptr);
	return (fp != NULL) ? my_pclose(fp): -1;
}

#if !defined(WIN32)

/*
  my_spawnl() / my_spawnv() implementations (UNIX-only)
*/

pid_t ChildPid = 0;

/*
  This is similar to the UNIX system(3) call, except it doesn't invoke
  a shell.  This is much more of a fork/exec/wait call.  Perhaps you
  should think of it as the "spawn" call on old MS-DOG systems.

  It shares the child handling with my_popen(), which is why it's in
  the same source file.  See the comments for it for more details.

  Returns:
    -1: failure
    >0: Pid of child (wait == false)
    0: Success (wait == true)
*/
#define MAXARGS	32
int
my_spawnl( const char* cmd, ... )
{
	int		rval;
	int		argno = 0;

    va_list va;
	const char * argv[MAXARGS + 1];

	/* Convert the args list into an argv array */
    va_start( va, cmd );
	for( argno = 0;  argno < MAXARGS;  argno++ ) {
		const char	*p;
		p = va_arg( va, const char * );
		argv[argno] = p;
		if ( ! p ) {
			break;
		}
	}
	argv[MAXARGS] = NULL;
    va_end( va );

	/* Invoke the real spawnl to do the work */
    rval = my_spawnv( cmd, const_cast<char *const*>(argv));

	/* Done */
	return rval;
}

/*
  This is similar to the UNIX system(3) call, except it doesn't invoke
  a shell.  This is much more of a fork/exec/wait call.  Perhaps you
  should think of it as the "spawn" call on old MS-DOG systems.

  It shares the child handling with my_popen(), which is why it's in
  the same source file.  See the comments for it for more details.

  Returns:
    -1: failure
    >0 == Return status of child
*/
int
my_spawnv( const char* cmd, const char *const argv[] )
{
	int					status;
	uid_t euid;
	gid_t egid;

		/* Use ChildPid as a simple semaphore-like lock */
	if ( ChildPid ) {
		return -1;
	}

		/* Create a new process */
	ChildPid = fork();
	if( ChildPid < 0 ) {
		ChildPid = 0;
		return -1;
	}

		/* Child: create an ARGV array, exec the binary */
	if( ChildPid == 0 ) {
			/* to be safe, we want to switch our real uid/gid to our
			   effective uid/gid (shedding any privledges we've got).
			   Our supplemental groups should already be set properly
			   by the set_priv() code.
			   the whole point of my_spawn*() is that we want to run
			   something as our effective uid/gid, and we want to do
			   it safely.  all of these calls will fail if we don't
			   have a ruid of 0 (root), but that's harmless.  also,
			   note that we have to stash our effective uid, then
			   switch our euid to 0 to be able to set our real uid/gid.
			   We wrap some of the calls in if-statements to quiet some
			   compilers that object to us not checking the return values.
			*/
		euid = geteuid();
		egid = getegid();
		if( seteuid( 0 ) ) { }
		if( setgid( egid ) ) { }
		if( setuid( euid ) ) _exit(ENOEXEC); // Unsafe?

			/* Now it's safe to exec whatever we were given */
		execv( cmd, const_cast<char *const*>(argv) );
		_exit( ENOEXEC );		/* This isn't safe ... */
	}

		/* Wait for child process to exit and get its status */
	while( waitpid(ChildPid,&status,0) < 0 ) {
		if( errno != EINTR ) {
			status = -1;
			break;
		}
	}

		/* Now return status from child process */
	ChildPid = 0;
	return status;
}

#endif // ndef WIN32


// run a command and return its output in a buffer
// the program is killed if the timeout expires.
//
// returns NULL and sets exit_status to errno if the timeout expires
//
// returns a null-terminated malloc'd buffer and sets exit_status to
//   the programs exit status if the program ran to completion
char* run_command(time_t timeout, const ArgList &args, int options, const Env* env_ptr, int *exit_status)
{
	bool want_stderr = (options & RUN_COMMAND_OPT_WANT_STDERR) != 0;
	bool drop_privs =  0 == (options & RUN_COMMAND_OPT_USE_CURRENT_PRIVS);
	MyPopenTimer pgm;
	*exit_status = pgm.start_program(args, want_stderr, env_ptr, drop_privs);
	if (*exit_status < 0) {
		return NULL;
	}
	if (pgm.wait_for_exit(timeout, exit_status)) {
		pgm.close_program(1); // close fp wait 1 sec, then SIGKILL (does nothing if program already exited)
		char * out = pgm.output().Detach();
		return out ? out : strdup("");
	}
	pgm.close_program(1); // close fp, wait 1 sec, then SIGKILL (does nothing if program already exited)
	*exit_status = pgm.error_code();
	return NULL;
}

//////////////////////////////////////////////////
// MyPopenTimer methods
//////////////////////////////////////////////////


MyPopenTimer::~MyPopenTimer()
{
	clear();
}

void MyPopenTimer::clear()
{
	if (fp) {
		my_pclose_ex(fp, 5, false);
		fp = NULL;
	}
	status = 0;
	error = NOT_INTIALIZED;
	begin_time = 0;
	src.rewind();
	bytes_read = 0;
}

// run a program and begin capturing it's output
int MyPopenTimer::start_program (
	const ArgList &args,
	bool also_stderr,
	const Env* env_ptr /*=NULL*/,
	bool drop_privs /*=true*/,
	const char * stdin_data /*=NULL*/)
{
	if (fp) { return ALREADY_RUNNING; }

	//src.clear();
	status = 0;
	error = 0;

#ifdef WIN32
	const char * mode = "rb";
#else
	const char * mode = "r";
#endif

	int options = MY_POPEN_OPT_FAIL_QUIETLY;
	if (also_stderr) options |= MY_POPEN_OPT_WANT_STDERR;
	fp = my_popen(args, mode, options, env_ptr, drop_privs, stdin_data);
	if ( ! fp) {
		error = errno;
#ifdef WIN32
		// do a little error translation for windows to make sure that the caller can reasonable
		// distinguish 'the program does not exist' from other errors.
		DWORD err = GetLastError();
		if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
			error = ENOENT;
		}
#endif
		return error;
	}
#ifdef WIN32
	// no windows equivalent, 
#else
	int fd = fileno(fp);
	int flags = fcntl(fd, F_GETFL, 0);
	dummy_global = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
	begin_time = time(NULL);
	return 0;
}

const char * MyPopenTimer::error_str() const
{
	const char * errmsg = "";
	if (error == ETIMEDOUT) { errmsg = "Timed out waiting for program to exit"; }
	else if (error == NOT_INTIALIZED) { errmsg = "start_program was never called"; }
	else if (error) { errmsg = strerror(error); }
	return errmsg;
}

// capture program output until it exits or the timout expires
// returns true and the exit code if the program runs to completion.
// returns false if the timeout expired or there was an error reading the output.
bool MyPopenTimer::wait_for_exit(time_t timeout, int *exit_status)
{
	if (error && (error != ETIMEDOUT)) // if error was not a previous timeout, assume we cannot continue
		return false;
	if (read_until_eof(timeout) != 0)
		return false;

	*exit_status = status;
	return true;
}

// close the program, sending a SIGTERM now if it has not yet exited
// and a SIGKILL after wait_for_term if it still has not yet exited.
// returns  true if program exited on its own, false if it had be signalled.
bool MyPopenTimer::close_program(time_t wait_for_term)
{
	// Note: if program exits normally by itself within the alloted time,
	// fp will always be NULL when we get here because
	// read_until_eof called my_pclose at eof()
	if (fp) {
		status = my_pclose_ex(fp, (unsigned int)wait_for_term, true);
		run_time = (int)(time(NULL) - begin_time);
		fp = NULL;
	}
	return ! WIFSIGNALED(status);
}

// capture program output until the program exits or until timeout expires
// timeout is measured relative to the time that start_program was called.
// returns true if program runs to completion and output is captured
// false if error or timeout while reading the output.
const char*  MyPopenTimer::wait_for_output(time_t timeout)
{
	if (error && (error != ETIMEDOUT)) // if error was not a previous timeout, assume we cannot continue
		return NULL;
	if (read_until_eof(timeout) != 0)
		return NULL;
	return src.data() ? src.data() : "";
}

// helper function for read_until_eof
// takes an array of fixed-size buffers and a total size
// and squashes them into a single alloction and stores (or appends) that
// into the given MyStringCharSource
// TODO: make MyStringCharSource aware of the size of it's allocation so we an re-use instead of re-allocating.
// 
static void store_buffers (
	MyStringCharSource & src,
	char* bufs[],
	int cbBuf,   // size of each buffer in bufs
	int cbTot,   // total size to store (only the last buffer will be partially full)
	bool append) // append to src, (if false, replaces src)
{
	char * old = src.Detach();
	if ( ! old) append = false;

	if ( ! append && (cbTot < cbBuf)) {
		// if there is only one buffer, and we are not appending, we can just store it
		char * out = bufs[0];
		bufs[0] = NULL;
		out[cbTot] = 0;
		src.Attach(out);
		if (old) free(old);
		return;
	}

	int cbOld = append ? (int)strlen(old) : 0;
	char * out = (char*)malloc(cbOld+cbTot+1);
	ASSERT(out);

	int ix = 0;
	if (cbOld) {
		memcpy(out,old,cbOld);
		ix += cbOld;
	}
	int cbRemain = cbTot;
	for (size_t ii = 0; cbRemain > 0; ++ii) {
		int cb = MIN(cbRemain, cbBuf);
		memcpy(out+ix, bufs[ii], cb);
		ix += cb;
		cbRemain -= cb;
		free(bufs[ii]);
		bufs[ii] = NULL;
	}
	// make sure it is null terminated
	out[cbTot] = 0;

	// attach our new buffer to output source, and free the old buffer (if any)
	src.Attach(out);
	if (old) free(old);
}


// returns error, 0 on success
int MyPopenTimer::read_until_eof(time_t timeout)
{
	if ( ! fp)
		return error;

#ifdef WIN32
	// consider using a thread with: BOOL WINAPI CancelSynchronousIo(HANDLE hThread);
#else
	struct pollfd fdt;
	fdt.fd = fileno(fp);
	fdt.events = POLLIN;
	fdt.revents = 0;
#endif

	std::vector<char*> bufs;
	int cbTot = 0;
	int ix = 0;
	const int cbBuf = 0x2000;
	char * buffer = (char*)calloc(1, cbBuf);
	for(;;) {
		bool wait_for_hotness = false;
		int cb = (int)fread(buffer+ix, 1, cbBuf-ix, fp);
		if (cb > 0) {
			// Otherwise, buffer should contain cb bytes.
			cbTot += cb;
			ix += cb;
			if (ix >= cbBuf) {
				bufs.push_back(buffer);
				buffer = (char*)calloc(1, cbBuf);
				ix = 0;
			}
		} else if (cb < 0) {
			if (errno == EAGAIN) {
				// there is no data to be read, right now, just wait a bit.
				wait_for_hotness = true;
			} else {
				error = errno;
				break;
			}
		} else {
			ASSERT(cb == 0);
			if (feof(fp)) {
				// got end of file, so we can close the program and capture it's return value
				time_t elapsed_time = (time(NULL) - begin_time);
				time_t remain_time = elapsed_time < timeout ? timeout - elapsed_time : 0;
				status = my_pclose_ex(fp, remain_time, true);
				run_time = (int)(time(NULL) - begin_time);
				fp = NULL;
				error = 0;
				break;
			}
			wait_for_hotness = true;
		}

		time_t now = time(NULL);
		time_t elapsed_time = now - begin_time;
		if (elapsed_time >= timeout) {
			error = ETIMEDOUT;
			break;
		}

		if (wait_for_hotness) {
			#ifdef WIN32
				//PRAGMA_REMIND("use PeekNamedPipe to check for pipe hotness...")
				sleep(2);
			#else
				int wait_time = timeout - (int)elapsed_time;
				int rv = poll(&fdt, 1, wait_time*1000);
				if(rv == 0) {
					error = ETIMEDOUT;
					break;
				} else if (rv > 0) {
					// new data can be read.
				}
			#endif
		}
	}
	bufs.push_back(buffer);

	// Now turn it into a single large buffer.
	if (cbTot > 0) {
		store_buffers(src, &bufs[0], cbBuf, cbTot, bytes_read > 0);
		bytes_read += cbTot;
	} else {
		free(buffer);
	}

	return error;
}

