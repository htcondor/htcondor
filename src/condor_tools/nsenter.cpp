/***************************************************************
 *
 * Copyright (C) 1990-2020, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "match_prefix.h"
#include <termios.h>
#include <unistd.h>
#include <grp.h>
#include <signal.h>
#include <algorithm>

// condor_nsenter
// 
// This is a replacement for the linux nsenter command to launch a
// shell inside a container.  We need this when running 
// condor_ssh_to_job to a job that has been launched inside singularity
// Docker jobs use docker exec to enter the container, but there is no
// equivalent in singularity.  Standard nsenter isn't sufficient, as it
// does not create a pty for the shell, and we need different command
// line arguments to enter a non-setuid singularity container.  This
// is harder because the starter can't tell if singularity is setuid easily.
//
// The architecture for ssh-to-job to a contained job is to land in an sshd
// the starter forks *outside* the container.  This is important because we
// never want to assume anything about the container, even that there is an sshd
// inside it.  After the sshd starts, a script runs which runs condor_docker_enter
// (even for singularity), which connects to a Unix Domain Socket, passes 
// stdin/out/err to the starter, which passes those to condor_nsenter, which 
// is runs as root.  Rootly privilege is required to enter a setuid namespace
// so this is how we acquire.
//
// condor_nsenter enters the namespace, taking care to try to enter the user
// namespace, which is only set up for non-setuid singularity, and drops
// privileges to the uid and gid passed on the command line.  It sets up
// a pty for the shell to use, so all interactive processses will work.
//
// A final problem is environment variables.  Singularity, especially when
// managing GPUs, sets some nvidia-related environment variables that the
// condor starter doesn't know about.  So, condor_nsenter extracts the environment
// variables from the contained job process, and sets them in the shell
// it spawns.

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] condor_* ....\n",cmd);
	fprintf(stderr,"Where options are:\n");
	fprintf(stderr,"-t target_pid:\n");
	fprintf(stderr,"-S user_id:\n");
	fprintf(stderr,"-G group_id:\n");
	fprintf(stderr,"    -help              Display options\n");
}


// Before we exit, we need to reset the pty back to normal
// if we put it in raw mode

bool pty_is_raw = false;
struct termios old_tio;
void reset_pty_and_exit(int signo) {
	if (pty_is_raw)
		tcsetattr(0, TCSAFLUSH, &old_tio);
	exit(signo);
}

int main( int argc, char *argv[] )
{
	std::string condor_prefix;
	pid_t pid = 0;
	uid_t uid = 0;
	gid_t gid = 0;

	// parse command line args
	for( int i=1; i<argc; i++ ) {
		if(is_arg_prefix(argv[i],"-help")) {
			usage(argv[0]);
			exit(1);
		}

		// target pid to enter
		if(is_arg_prefix(argv[i],"-t")) {
			pid = atoi(argv[i + 1]);
			i++;
		}

		// uid to switch to
		if(is_arg_prefix(argv[i],"-S")) {
			uid = atoi(argv[i + 1]);
			i++;
		}

		// gid to switch to
		if(is_arg_prefix(argv[i],"-G")) {
			gid = atoi(argv[i + 1]);
			i++;
		}
	}

	if (pid < 1) {
		fprintf(stderr, "missing -t argument > 1\n");
		exit(1);
	}	

	if (uid == 0) {
		fprintf(stderr, "missing -S argument > 1\n");
		exit(1);
	}

	if (gid == 0) {
		fprintf(stderr, "missing -G argument > 1\n");
		exit(1);
	}

	// slurp the enviroment out of our victim
	std::string env;
	std::string envfile;
	formatstr(envfile, "/proc/%d/environ", pid);
	
	int e = open(envfile.c_str(), O_RDONLY);
	if (e < 0) {
		fprintf(stderr, "Can't open %s %s\n", envfile.c_str(), strerror(errno));
		exit(1);
	}
	char buf[512];
	int bytesRead;
	while ((bytesRead = read(e, &buf, 512)) > 0) {
		env.append(buf, bytesRead);
	}
	close(e);
	
	// If the environment contains a string with the value of the
	// path to the container's scratch directory outside the container
	// replace it with the path inside the container.
	
	std::string outsidePath;
	if (getenv("_CONDOR_SCRATCH_DIR_OUTSIDE_CONTAINER")) {
		outsidePath = getenv("_CONDOR_SCRATCH_DIR_OUTSIDE_CONTAINER");
	}  
	std::string insidePath; 
	if (getenv("_CONDOR_SCRATCH_DIR")) {
		insidePath = getenv("_CONDOR_SCRATCH_DIR");
	}  
	if (outsidePath.length() > 0) {
		size_t pos = 0;
		do {
			pos = env.find(outsidePath, pos);
			if (pos != std::string::npos) {
				env.replace(pos, outsidePath.length(), insidePath);
				pos += insidePath.length();
			}	
		} while (pos != std::string::npos);
	}
	// we probably replaced the value of _CONDOR_SCRATCH_DIR_OUTSIDE_CONTAINER
	// with the inside value, so set that back
	size_t pos = 0;
	pos = env.find("_CONDOR_SCRATCH_DIR_OUTSIDE_CONTAINER=", pos);
	if (pos != std::string::npos) {
		size_t eqpos = env.find("=", pos);
		eqpos++;
		env.replace(eqpos, insidePath.length(), outsidePath);  
	}

	// make a vector to hold all the pointers to env entries
	std::vector<const char *> envp;

	// copy DISPLAY from outside to inside.  sshd has set this,
	// it is the only one from the outside we need on the inside,
	// and one way that an ssh-to-job shell is different than the job.
	std::string display;
	if (getenv("DISPLAY")) {
		formatstr(display, "DISPLAY=%s", getenv("DISPLAY"));
		envp.push_back(display.c_str());
	}

	// the first one
	envp.push_back(env.c_str());
	auto it = env.cbegin();
	while (env.cend() != (it = std::find(it, env.cend(), '\0'))) {
		// skip past null terminator
		it++;	
		if (& (*it)  != nullptr) {
			envp.push_back(& (*it));
		}
	}
	envp.push_back(nullptr);
	envp.push_back(nullptr);

	std::string filename;

	// start changing namespaces.  Note that once we do this, things
	// get funny in this process
	formatstr(filename, "/proc/%d/ns/uts", pid);
	int fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Can't open uts namespace: %d %s\n", errno, strerror(errno));
		exit(1);
	}
	int r = setns(fd, 0);
	close(fd);
	if (r < 0) {
		// This means an unprivileged singularity, most likely
		// need to set user namespace instead.
		formatstr(filename, "/proc/%d/ns/user", pid);
		fd = open(filename.c_str(), O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "Can't open user namespace: %d %s\n", errno, strerror(errno));
			exit(1);
		}
		r = setns(fd, 0);
		close(fd);
		if (r < 0) {
			fprintf(stderr, "Can't setns to user namespace: %s\n", strerror(errno));
			exit(1);
		}
	}

	// now the pid namespace
	formatstr(filename, "/proc/%d/ns/pid", pid);
	fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Can't open pid namespace: %s\n", strerror(errno));
		exit(1);
	}
	r = setns(fd, 0);
	close(fd);
	if (r < 0) {
		fprintf(stderr, "Can't setns to pid namespace: %s\n", strerror(errno));
		exit(1);
	}

	// finally the mnt namespace
	formatstr(filename, "/proc/%d/ns/mnt", pid);
	fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Can't open mnt namespace: %s\n", strerror(errno));
		exit(1);
	}
	r = setns(fd, 0);
	close(fd);
	if (r < 0) {
		fprintf(stderr, "Can't setns to mnt namespace: %s\n", strerror(errno));
		exit(1);
	}

	setgroups(0, nullptr);

	// order matters!
	r = setgid(gid);
	if (r < 0) {
		fprintf(stderr, "Can't setgid to %d\n", gid);
		exit(1);
	}
	r = setuid(uid);
	if (r < 0) {
		fprintf(stderr, "Can't setuid to %d\n", uid);
		exit(1);
	}

	struct winsize win;
	ioctl(0, TIOCGWINSZ, &win);

	// now the pty handling
	int masterPty = -1;
	int workerPty = -1;
	masterPty = open("/dev/ptmx", O_RDWR);
	unlockpt(masterPty);

	if (masterPty < 0) {
		fprintf(stderr, "Can't open master pty %s\n", strerror(errno));
		exit(1);
	} else {
		workerPty = open(ptsname(masterPty), O_RDWR);
		if (workerPty < 0) {
			fprintf(stderr, "Can't open worker pty %s\n", strerror(errno));
			exit(1);
		}
	}
	int childpid = fork();
	if (childpid == 0) {
	
		// in the child -- 

		close(0);
		close(1);
		close(2);
		close(masterPty);

		dup2(workerPty, 0);
		dup2(workerPty, 1);
		dup2(workerPty, 2);

		if (getenv("_CONDOR_SCRATCH_DIR")) {
			int r = chdir(getenv("_CONDOR_SCRATCH_DIR"));
			if (r < 0) {
				printf("Cannot chdir to %s: %s\n", getenv("_CONDOR_SCRATCH_DIR"), strerror(errno));
			}
 		}

		// make this process group leader so shell job control works
		setsid();

		// Make the pty the controlling terminal
		ioctl(workerPty, TIOCSCTTY, 0);

		// and make it the process group leader
		tcsetpgrp(workerPty, getpid());
 
		// and set the window size properly
		ioctl(0, TIOCSWINSZ, &win);

		// Finally, launch the shell
		execle("/bin/sh", "/bin/sh", "-l", "-i", nullptr, envp.data());
 
		// Only get here if exec fails
		fprintf(stderr, "exec failed %d\n", errno);
		exit(errno);

	} else {

		// the parent
		fd_set readfds, writefds, exceptfds;
		bool keepGoing = true;

		// put the pty in raw mode
		struct termios tio;
		tcgetattr(0, &tio);
		pty_is_raw = true;
		old_tio = tio;
		tio.c_iflag &= ~(ICRNL);
		tio.c_oflag &= ~(OPOST);
		tio.c_cflag |= (CS8);
		tio.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
		tio.c_cc[VMIN] = 1;
		tio.c_cc[VTIME] = 0;
		tcsetattr(0, TCSAFLUSH, &tio);
		
		struct sigaction handler;
		struct sigaction oldhandler;
		handler.sa_handler = reset_pty_and_exit;
		handler.sa_flags   = 0;
		sigemptyset(&handler.sa_mask);


		sigaction(SIGCHLD, &handler, &oldhandler);

		while (keepGoing) {
			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
			FD_ZERO(&exceptfds);

			FD_SET(0, &readfds);
			FD_SET(masterPty, &readfds);

			select(masterPty + 1, &readfds, &writefds, &exceptfds, nullptr);

			if (FD_ISSET(masterPty, &readfds)) {
				char buf[4096];
				int r = read(masterPty, buf, 4096);
				if (r > 0) {	
					int ret = write(1, buf, r);
					if (ret < 0) {
						reset_pty_and_exit(0);
					}
				} else {
					keepGoing = false;
				}
			}

			if (FD_ISSET(0, &readfds)) {
				char buf[4096];
				int r = read(0, buf, 4096);
				if (r > 0) {	
					int ret = write(masterPty, buf, r);
					if (ret < 0) {
						reset_pty_and_exit(0);
					}
				} else {
					keepGoing = false;
				}
			}
		}
		int status;
		waitpid(childpid, &status, 0);	
	}
	return 0;
}

