/* This file implements the ability for a process to find the currently open
	files by another (or self) process */

#include "condor_common.h"
#include "condor_debug.h"
#include "MyString.h"
#include "directory.h"
#include "stat_wrapper.h"
#include <set>
#include "open_files_in_pid.h"

using namespace std;

set<MyString> open_files_in_pid(pid_t pid)
{
	set<MyString> open_file_set;

#if defined(LINUX)

	MyString file;
	MyString tmp;
	char f[PATH_MAX];

	// Dig around in the proc file system looking for open files for the 
	// specified pid. This is Linux only, for now.

	tmp.formatstr("/proc/%lu/fd", (long unsigned) pid);
	Directory fds(tmp.Value());

	// If a file is open multiple times, that's fine, we only record it once.
	while(fds.Next()) {
		file = fds.GetFullPath();

		// Get the name of the file to which the link points.
		file = realpath(file.Value(), f);
		if (file == NULL) {
			continue;
		}

		if (file == "." || file == "..") {
			continue;
		}

		open_file_set.insert(file);

		dprintf(D_ALWAYS, "open_files(): Found file -> %s\n", file.Value());
	}

#else
	if (pid) {} // Fight compiler warnings!
	EXCEPT("open_files(): Only available for LINUX!");

#endif

	return open_file_set;
}

