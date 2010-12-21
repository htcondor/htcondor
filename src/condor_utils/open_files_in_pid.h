#ifndef _OPEN_FILES_IN_PID_H_
#define _OPEN_FILES_IN_PID_H_

#include "condor_common.h"
#include "MyString.h"
#include <set>

/* Given a pid, find out what files are currently open by that pid.

	The files returned are to be fully qualified. 
	Under Linux, so says the man page, this will be always true. If this
	function is implemented on Solaris or elsewhere, as relative might might be
	put into the set. See the realpath(3) man page.
*/

std::set<MyString> open_files_in_pid(pid_t pid);

#endif
