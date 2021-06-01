#include "condor_common.h"
#include "directory.h"
#include "condor_debug.h"
#include "stl_string_utils.h"

#ifdef LINUX
bool isChildOf(const char *subdir, pid_t parent);
#endif
// Given a parent pid, find the first child pid of that parent pid
// by groveling through /proc

pid_t
findChildProc(pid_t parent) {
#ifdef LINUX
	Directory d("/proc");

	const char *subdir;
	while ((subdir = d.Next())) {
		// skip over all the non-pid subdirs of /proc (and 1)
		int pid = atoi(subdir);
		if (pid < 2) {
			continue;
		}
		if (isChildOf(subdir, parent)) {
			return pid;
		}
		
	}
	return -1;
#else
	return -1;
#endif
}

#ifdef LINUX
bool
isChildOf(const char *subdir, pid_t parent) {
	int fd;
	std::string fileName;

	char buf[512];
	buf[0] = '\0';

	formatstr(fileName, "/proc/%s/stat", subdir);

	fd = safe_open_wrapper(fileName.c_str(), O_RDONLY, 0666);
	if (fd < 0) {
		return false;
	}

	ssize_t num_read = read(fd, buf, 511);
	if (num_read < 5) { // minimum # of bytes for legal stat line
		close(fd);
		return false;
	}
	close(fd);

	int pid = 0;
	int ppid = -1;

	// Format of /proc/self/stat is
	// pid program_name State ppid
	int matched = sscanf(buf, "%d%*s%*s%d", &pid, &ppid);
	if ((ppid == parent) && (matched > 0)) {
		return true;
	}
	return false;
}
#endif
