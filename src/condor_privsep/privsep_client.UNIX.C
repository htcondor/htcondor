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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_privsep.h"
#include "condor_uid.h"
#include "condor_arglist.h"
#include "my_popen.h"
#include "privsep_open.h"

bool
privsep_enabled()
{
	static bool first_time = true;
	static bool answer;

	if (first_time) {
		answer = (param_boolean("PRIVSEP_ENABLED", false) && !is_root());
		first_time = false;
	}

#if defined(HPUX)
	if (answer) {
		EXCEPT("PRIVSEP_ENABLED not supported on this platform");
	}
#endif

	return answer;
}

bool
privsep_setup_exec_as_user(uid_t uid, gid_t gid, ArgList& args)
{
	char* privsep_executable = param("PRIVSEP_EXECUTABLE");
	if (privsep_executable == NULL) {
		dprintf(D_ALWAYS,
		        "privsep open: PRIVSEP_EXECUTABLE not defined\n");
		return false;
	}
	args.InsertArg(privsep_executable, 0);
	free(privsep_executable);
	args.InsertArg("3", 1);
	MyString tmp;
	tmp.sprintf("%u", (unsigned)uid);
	args.InsertArg(tmp.Value(), 2);
	tmp.sprintf("%u", (unsigned)gid);
	args.InsertArg(tmp.Value(), 3);

	return true;
}

// helper function for privsep_open
//
static int
receive_fd(int switchboard_fd)
{
	int received_fd;

	// set up a msghdr structure for our call to recvmsg
	//
	struct msghdr msg;

	// don't care about the address
	//
	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	// we should receive a single null byte of in-band data
	//
	char buf[1];
	struct iovec iov[1];
	iov[0].iov_base = buf;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

#if !defined(PRIVSEP_OPEN_USE_ACCRIGHTS)
	// control data (this is where we receive the FD)
	//
	char cmsg_buf[CMSG_LEN(sizeof(int))];
	struct cmsghdr* cmsg = (struct cmsghdr*)cmsg_buf;
	msg.msg_control = cmsg_buf;
	msg.msg_controllen = CMSG_LEN(sizeof(int));
#else
	// set up the variable received_fd to get the
	// file descriptor
	//
	msg.msg_accrights = (caddr_t)&received_fd;
	msg.msg_accrightslen = sizeof(int);
#endif

	// receive it!
	//
	int bytes = recvmsg(switchboard_fd, &msg, 0);
	if (bytes == -1) {
		dprintf(D_ALWAYS, "recvmsg error: %s\n", strerror(errno));
		return -1;
	}
	if (bytes == 0) {
		dprintf(D_ALWAYS,
		        "privsep_open: error: connection closed by server\n");
		return -1;
	}
	if (bytes != 1) {
		dprintf(D_ALWAYS,
		        "privsep_open: error: too many bytes received on socket\n");
		return -1;
	}
	if (*(char*)msg.msg_iov[0].iov_base != '\0') {
		dprintf(D_ALWAYS,
		        "privsep_open: protocol error: didn't receive null byte\n");
		return -1;
	}
#if !defined(PRIVSEP_OPEN_USE_ACCRIGHTS)
	if (msg.msg_controllen != CMSG_LEN(sizeof(int))) {
#else
	if (msg.msg_accrightslen != sizeof(int)) {
#endif
		dprintf(D_ALWAYS,
		        "privsep_open: protocol error: wrong control data size\n");
		return -1;
	}

	// holy shit, it worked!
	//
#if !defined(PRIVSEP_OPEN_USE_ACCRIGHTS)
	memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
#endif

	return received_fd;
}

int
privsep_open(uid_t uid, gid_t gid, const char* path, int flags, mode_t mode)
{
	ArgList switchboard_args;

	// make sure we have a binary we can use
	//
	char* privsep_executable = param("PRIVSEP_EXECUTABLE");
	if (privsep_executable == NULL) {
		dprintf(D_ALWAYS,
		        "privsep open: PRIVSEP_EXECUTABLE not defined\n");
		return -1;
	}
	switchboard_args.AppendArg(privsep_executable);
	free(privsep_executable);

	// create a connected pair of UNIX domain sockets to use for FD passing
	//
	int socket_fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, socket_fds) == -1) {
		dprintf(D_ALWAYS, "socketpair error: %s\n", strerror(errno));
		return -1;
	}

	// fork a new process in which to invoke the PrivSep Switchboard
	//
	pid_t child_pid = fork();
	if (child_pid == -1) {
		dprintf(D_ALWAYS, "fork error: %s\n", strerror(errno));
		return -1;
	}

	// child
	//
	if (child_pid == 0) {

		// close the FD for the parent's side of the socket pair
		//
		close(socket_fds[0]);

		// dup our side of the socket pair to FD 0
		// (which is where the privsep_open server expects it)
		//
		if (dup2(socket_fds[1], 0) == -1) {
			_exit(1);
		}

		// now close the original, since it is no longer needed
		//
		close(socket_fds[1]);

		// setup the arguments for the Switchboard's "open" operation

		// operation "7" means "open"
		//
		switchboard_args.AppendArg("7");

		// pass the target UID and GID
		//
		switchboard_args.AppendArg(uid);
		switchboard_args.AppendArg(gid);
	
		// the path to open, open flags, and mode
		//
		switchboard_args.AppendArg(path);
		switchboard_args.AppendArg(flags);
		switchboard_args.AppendArg(mode);

		char** args = switchboard_args.GetStringArray();

		// exec the privsep_open server
		//
		execv(args[0], args);

		// error occurred if we got here
		//
		_exit(2);
	}

	// close the FD for the child's side of the socket pair
	//
	close(socket_fds[1]);

	// attempt to receive the requested FD from the server
	//
	int fd = receive_fd(socket_fds[0]);

	// UNIX domain socket no longer needed
	//
	close(socket_fds[0]);

	// wait for child to exit
	//
	int status;
	if (waitpid(child_pid, &status, 0) == -1) {
		dprintf(D_ALWAYS, "privsep_open: waitpid error: %s\n", strerror(errno));
	}
	if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
		dprintf(D_ALWAYS,
		        "privsep_open: unexpected status from server: %d\n",
		        status);
	}

	// return the opened fd
	//
	return fd;
}

bool
privsep_create_dir(uid_t uid, gid_t gid, const char* pathname)
{
	ArgList privsep_mkdir_args;

	char* privsep_executable = param("PRIVSEP_EXECUTABLE");
	if (privsep_executable == NULL) {
		dprintf(D_ALWAYS,
		        "privsep_mkdir_dir: PRIVSEP_EXECUTABLE not defined\n");
		return false;
	}
	privsep_mkdir_args.AppendArg(privsep_executable);
	free(privsep_executable);
	privsep_mkdir_args.AppendArg(5);
	privsep_mkdir_args.AppendArg(uid);
	privsep_mkdir_args.AppendArg(gid);
	privsep_mkdir_args.AppendArg(pathname);

	MyString args_string;
	int retval = my_system(privsep_mkdir_args);

	if (retval != 0) {
		dprintf(D_ALWAYS,
		        "privsep_mkdir: unexpected return from switchboard: %d\n",
		        retval);
		return false;
	}

	return true;
}

bool
privsep_remove_dir(const char*)
{
	ASSERT(0);
	return false;
}

bool
privsep_chown_dir(uid_t uid, gid_t, const char* pathname)
{
	ArgList privsep_chown_args;

	char* privsep_executable = param("PRIVSEP_EXECUTABLE");
	if (privsep_executable == NULL) {
		dprintf(D_ALWAYS,
		        "privsep_chown_dir: PRIVSEP_EXECUTABLE not defined\n");
		return false;
	}
	privsep_chown_args.AppendArg(privsep_executable);
	free(privsep_executable);
	privsep_chown_args.AppendArg(6);
	privsep_chown_args.AppendArg(uid);
	privsep_chown_args.AppendArg(pathname);

	MyString args_string;
	int retval = my_system(privsep_chown_args);

	if (retval != 0) {
		dprintf(D_ALWAYS,
		        "privsep_chown_dir: unexpected return from switchboard: %d\n",
		        retval);
		return false;
	}

	return true;
}

bool
privsep_rename(uid_t uid,
               gid_t gid,
               const char* old_path,
               const char* new_path)
{
	ArgList privsep_rename_args;

	char* privsep_executable = param("PRIVSEP_EXECUTABLE");
	if (privsep_executable == NULL) {
		dprintf(D_ALWAYS,
		        "privsep_rename_dir: PRIVSEP_EXECUTABLE not defined\n");
		return false;
	}
	privsep_rename_args.AppendArg(privsep_executable);
	free(privsep_executable);
	privsep_rename_args.AppendArg(8);
	privsep_rename_args.AppendArg(uid);
	privsep_rename_args.AppendArg(gid);
	privsep_rename_args.AppendArg(old_path);
	privsep_rename_args.AppendArg(new_path);

	MyString args_string;
	int retval = my_system(privsep_rename_args);

	if (retval != 0) {
		dprintf(D_ALWAYS,
		        "privsep_rename_dir: unexpected return from switchboard: %d\n",
		        retval);
		return false;
	}

	return true;
}

bool
privsep_chmod(uid_t uid, gid_t gid, const char* path, mode_t mode)
{
	ArgList privsep_chmod_args;

	char* privsep_executable = param("PRIVSEP_EXECUTABLE");
	if (privsep_executable == NULL) {
		dprintf(D_ALWAYS,
		        "privsep_chmod_dir: PRIVSEP_EXECUTABLE not defined\n");
		return false;
	}
	privsep_chmod_args.AppendArg(privsep_executable);
	free(privsep_executable);
	privsep_chmod_args.AppendArg(9);
	privsep_chmod_args.AppendArg(uid);
	privsep_chmod_args.AppendArg(gid);
	privsep_chmod_args.AppendArg(path);
	privsep_chmod_args.AppendArg(mode);

	MyString args_string;
	int retval = my_system(privsep_chmod_args);

	if (retval != 0) {
		dprintf(D_ALWAYS,
		        "privsep_chmod_dir: unexpected return from switchboard: %d\n",
		        retval);
		return false;
	}

	return true;
}
