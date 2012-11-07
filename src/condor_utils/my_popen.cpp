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


#define _POSIX_SOURCE
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_arglist.h"
#include "my_popen.h"
#include "sig_install.h"
#include "env.h"
#include "../condor_privsep/condor_privsep.h"
#include "../condor_privsep/privsep_fork_exec.h"
#include "setenv.h"

#ifdef WIN32
typedef HANDLE child_handle_t;
#define INVALID_CHILD_HANDLE INVALID_HANDLE_VALUE;
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

static void add_child(FILE* fp, child_handle_t ch)
{
	struct popen_entry *pe = (struct popen_entry *)malloc(sizeof(struct popen_entry));
	MSC_SUPPRESS_WARNING_FIXME(6011) // Dereferencing a null pointer, malloc can return NULL.
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

/*

  FILE *my_popenv(char *const args[], const char *mode, int want_stderr);
  FILE *my_popen(ArgList &args, const char *mode, int want_stderr, Env *env_ptr);

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

*/

#ifdef WIN32

//////////////////////////////////////////////////////////////////////////
// Windows versions of my_popen / my_pclose 
//////////////////////////////////////////////////////////////////////////

extern "C" FILE *
my_popen(const char *const_cmd, const char *mode, int want_stderr)
{
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
		dprintf(D_ALWAYS, "my_popen: CreatePipe failed\n");
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
		CloseHandle(hParentPipe);
		CloseHandle(hChildPipe);
		dprintf(D_ALWAYS, "my_popen: CreateProcess failed\n");
		return NULL;
	}

	// don't care about child's primary thread handle
	// or child's ends of the pipes
	CloseHandle(pi.hThread);
	CloseHandle(hChildPipe);

	// convert pipe handle specified in mode into a FILE pointer
	fd = _open_osfhandle((long)hParentPipe, 0);
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
my_popen(ArgList &args, const char *mode, int want_stderr, Env *zkmENV)
{
	MyString cmdline, err;
	if (!args.GetArgsStringWin32(&cmdline, 0, &err)) {
		dprintf(D_ALWAYS, "my_popen: error making command line: %s\n", err.Value());
		return NULL;
	}
	//  maybe the following function should be extended by an Env* argument? Not sure...
	return my_popen(cmdline.Value(), mode, want_stderr);
}

extern "C" FILE *
my_popenv(const char *const args[], const char *mode, int want_stderr)
{
	// build the argument list
	ArgList arglist;
	for (int i = 0; args[i] != NULL; i++) {
		arglist.AppendArg(args[i]);
	}

	return my_popen(arglist, mode, want_stderr);
}

extern "C" int
my_pclose(FILE *fp)
{
	HANDLE hChildProcess;
	DWORD result;

	hChildProcess = remove_child(fp);

	fclose(fp);

	result = WaitForSingleObject(hChildProcess, INFINITE);
	if (result != WAIT_OBJECT_0) {
		dprintf(D_FULLDEBUG, "my_pclose: WaitForSingleObject failed\n");
		return -1;
	}
	if (!GetExitCodeProcess(hChildProcess, &result)) {
		dprintf(D_FULLDEBUG, "my_pclose: GetExitCodeProcess failed\n");
		return -1;
	}
	CloseHandle(hChildProcess);

	return result;
}

extern "C" int
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
#include <grp.h> // for setgroups

static int	READ_END = 0;
static int	WRITE_END = 1;

static FILE *
my_popenv_impl( const char *const args[],
                const char * mode,
                int want_stderr,
                uid_t privsep_uid,
				Env *env_ptr = 0)
{
	int	pipe_d[2], pipe_d2[2];
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

		/* Prepare for PrivSep if needed */
	PrivSepForkExec psforkexec;
	if ( privsep_uid != (uid_t)-1 ) {
		if (!psforkexec.init()) {
			dprintf(D_ALWAYS,
			        "my_popenv failure on %s\n",
			        args[0]);
			close(pipe_d[0]);
			close(pipe_d[1]);
			return NULL;
		}
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

		/* Create a new process */
	if( (pid=fork()) < 0 ) {
		dprintf(D_ALWAYS, "my_popenv: Failed to fork child, errno=%d (%s)\n",
				errno, strerror(errno));
			/* Clean up file descriptors */
		close( pipe_d[0] );
		close( pipe_d[1] );
		close( pipe_d2[0] );
		close( pipe_d2[1] );
		return NULL;
	}

		/* The child */
	if( pid == 0 ) {
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
			   we also want to drop any supplimental groups we're in.
			   we want to run this popen()'ed thing as our effective
			   uid/gid, dropping the real uid/gid.  all of these calls
			   will fail if we don't have a ruid of 0 (root), but
			   that's harmless.  also, note that we have to stash our
			   effective uid, then switch our euid to 0 to be able to
			   set our real uid/gid.
			   We wrap some of the calls in if-statements to quiet some
			   compilers that object to us not checking the return values.
			*/
		euid = geteuid();
		egid = getegid();
		if( seteuid( 0 ) ) { }
		setgroups( 1, &egid );
		if( setgid( egid ) ) { }
		if( setuid( euid ) ) _exit(ENOEXEC); // Unsafe?

			/* before we exec(), clear the signal mask and reset SIGPIPE
			   to SIG_DFL
			*/
		install_sig_handler(SIGPIPE, SIG_DFL);
		sigset_t sigs;
		sigfillset(&sigs);
		sigprocmask(SIG_UNBLOCK, &sigs, NULL);

			/* handle PrivSep if needed */
		MyString cmd = args[0];
		if ( privsep_uid != (uid_t)-1 ) {
			ArgList al;
			psforkexec.in_child(cmd, al);
			args = al.GetStringArray();			
		}

			/* set environment if defined */
		if (env_ptr) {
			char **m_unix_env = NULL;
			m_unix_env = env_ptr->getStringArray();
			execve(cmd.Value(), const_cast<char *const*>(args), m_unix_env );
		} else {
			execvp(cmd.Value(), const_cast<char *const*>(args) );
		}

			/* If we get here, inform the parent of our errno */
		char result_buf[10];
		int e = errno; // capture real errno

		int len = snprintf(result_buf, 10, "%d", errno);
		int ret = write(pipe_d2[1], result_buf, len);

			// Jump through some hoops just to use ret.
		if (ret <  1) {
			_exit( e );
		} else {
			_exit( e );
		}
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
		return NULL;
	}
		/* Handle case where exec fails */
	if (fscanf(fh, "%d", &exit_code) == 1) {
		fclose(fh);
		close(pipe_d[0]);
		close(pipe_d[1]);
		errno = exit_code;
		return NULL;
	}
	fclose(fh);

	if( parent_reads ) {
		close( pipe_d[WRITE_END] );
		retp = fdopen(pipe_d[READ_END],mode);
	} else {
		close( pipe_d[READ_END] );
		retp = fdopen(pipe_d[WRITE_END],mode);
	}
	add_child(retp, pid);

		/* handle PrivSep if needed */
	if ( privsep_uid != (uid_t)-1 ) {
		FILE* fp = psforkexec.parent_begin();
		privsep_exec_set_uid(fp, privsep_uid);
		privsep_exec_set_path(fp, args[0]);
		ArgList al;
		for (const char* const* arg = args; *arg != NULL; arg++) {
			al.AppendArg(*arg);
		}
		privsep_exec_set_args(fp, al);
		Env env;
		env.Import();
		privsep_exec_set_env(fp, env);
		privsep_exec_set_iwd(fp, ".");
		if (parent_reads) {
			privsep_exec_set_inherit_fd(fp, 1);
			if (want_stderr) {
				privsep_exec_set_inherit_fd(fp, 2);
			}
		}
		else {
			privsep_exec_set_inherit_fd(fp, 0);
		}
		if (!psforkexec.parent_end()) {
			dprintf(D_ALWAYS,
			        "my_popenv failure on %s\n",
			        args[0]);
			fclose(retp);
			return NULL;
		}
	}

	return retp;
}

extern "C" FILE *
my_popenv( const char *const args[],
           const char * mode,
           int want_stderr )
{
	return my_popenv_impl(args, mode, want_stderr, (uid_t)-1);
}

static FILE *
my_popen_impl(ArgList &args,
              const char *mode,
              int want_stderr,
              uid_t privsep_uid,
			  Env *env_ptr)
{
	char **string_array = args.GetStringArray();
	FILE *fp = my_popenv_impl(string_array, mode, want_stderr, privsep_uid, env_ptr);
	deleteStringArray(string_array);

	return fp;
}

FILE*
my_popen(ArgList &args, const char *mode, int want_stderr, Env *env_ptr)
{
	return my_popen_impl(args, mode, want_stderr, (uid_t)-1, env_ptr);
}

FILE*
privsep_popen(ArgList &args, const char *mode, int want_stderr, uid_t uid, Env *env_ptr)
{
	return my_popen_impl(args, mode, want_stderr, uid, env_ptr);
}

extern "C" int
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
#endif // !WIN32

extern "C" int
my_systemv(const char *const args[])
{
	FILE* fp = my_popenv(args, "w", FALSE);
	return (fp != NULL) ? my_pclose(fp) : -1;
}

int
my_system(ArgList &args, Env *env_ptr)
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
extern "C" int
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
extern "C" int
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
			   we also want to drop any supplimental groups we're in.
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
		setgroups( 1, &egid );
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
