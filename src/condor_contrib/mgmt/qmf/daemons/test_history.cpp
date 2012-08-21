/*
 * Copyright 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "condor_common.h"

#include "condor_config.h"
#include "condor_debug.h"
#include "subsystem_info.h"

#include "HistoryProcessingUtils.h"
#include "qpid/agent/ManagementAgent.h"

using namespace qpid::management;

extern MyString m_path;
ManagementAgent::Singleton *singleton;

int
main(int /*argc*/, char **argv)
{
	Termlog = 1;
	dprintf_config("TOOL", get_param_functions());

	singleton = new ManagementAgent::Singleton();

	m_path = MyString(argv[1]);

		// On startup run 0 then 1, after put 0 and 1 on timers.
		// (0,1) should be on startup and periodic timer
		// (2) should be on timer, faster than (0,1)
	ProcessHistoryDirectory();
	ProcessOrphanedIndices();
	ProcessCurrentHistory();
	for (int i = 1; i <= 120; i++) {
		if (0 == i % 10) ProcessHistoryDirectory();
		if (0 == i % 20) ProcessOrphanedIndices();
		ProcessCurrentHistory();
		sleep(1);
	}

	return 0;
}


/*
int
main2(int argc, char **argv)
{
	Termlog = 1;
	dprintf_config("TOOL", get_param_functions());

	const char *file = NULL;
	const MyString path(argv[1]);

	StatWrapper stat_wrapper;
	const StatStructType *stat = NULL;

	Directory dir(path.Value());
	dir.Rewind();
	while ((file = dir.Next())) {
			// Skip all non-history files, e.g. history and history.*
		if (strncmp(file, "history.", 8) ||
			!strncmp(file + (strlen(file) - 4), ".idx", 4)) continue;

		if (stat_wrapper.Stat(path + DIR_DELIM_STRING + file)) {
			fprintf(stderr, "Failed to stat %s: %d (%s)\n", file,
					stat_wrapper.GetErrno(), strerror(stat_wrapper.GetErrno()));
			continue;
		}
		stat = stat_wrapper.GetBuf();
		fprintf(stdout,
				"name: %s\n"
				"inode: %lld\n"
				"size: %lld\n"
				"mtime sec: %ld\n"
				"ctime sec: %ld\n",
				file,
				stat->st_ino,
				stat->st_size,
				stat->st_mtime,  // st_mtimespec.tv_sec on OS X
				stat->st_ctime); // st_ctimespec.tv_sec on OS X

		HistoryFile h_file((path + DIR_DELIM_STRING + file).Value());
		CondorError errstack;
		if (!h_file.init(errstack)) {
			fprintf(stderr, "%s\n", errstack.getFullText().c_str());
			return 1;
		}
		errstack.clear();
		HistoryFile::HistoryEntriesTypeIterators ij = h_file.poll(errstack);
		for (HistoryFile::HistoryEntriesTypeIterator i = ij.first;
			 i != ij.second;
			 i++) {
			fprintf(stdout,
					"%ld %ld %s %d %d %d %d %d\n%s\n%s\n%s\n%s\n%s\n%s\n",
					(*i)->start, (*i)->stop, (*i)->id,
					(*i)->cluster, (*i)->proc,
					(*i)->q_date, (*i)->status, (*i)->entered_status,
					(*i)->submission,
					(*i)->cmd,
					(*i)->args1,
					(*i)->args2,
					(*i)->release_reason,
					(*i)->hold_reason);
		}
	}

	file = "history";
	if (stat_wrapper.Stat(path + DIR_DELIM_STRING + file)) {
		fprintf(stderr, "Failed to stat %s: %d (%s)\n", file,
				stat_wrapper.GetErrno(), strerror(stat_wrapper.GetErrno()));
		return 1;
	}
	stat = stat_wrapper.GetBuf();
	fprintf(stdout,
			"name: %s\n"
			"inode: %lld\n"
			"size: %lld\n"
			"mtime sec: %ld\n"
			"ctime sec: %ld\n",
			file,
			stat->st_ino,
			stat->st_size,
			stat->st_mtime,  // st_mtimespec.tv_sec on OS X
			stat->st_ctime); // st_ctimespec.tv_sec on OS X

	HistoryFile h_file((path + DIR_DELIM_STRING + file).Value());
	CondorError errstack;
	if (!h_file.init(errstack)) {
		fprintf(stderr, "%s\n", errstack.getFullText().c_str());
		return 1;
	}
	HistoryFile::HistoryEntriesTypeIterators poll = h_file.poll(errstack);
	int c = 0;
	while (c++ < 10) {
		errstack.clear();
		poll = h_file.poll(errstack);
		for (HistoryFile::HistoryEntriesTypeIterator i = poll.first;
			 i != poll.second;
			 i++) {
			fprintf(stdout,
					"%ld %ld %s %d %d %d %d %d\n%s\n%s\n%s\n%s\n%s\n%s\n",
					(*i).start, (*i).stop, (*i).id.c_str(),
					(*i).cluster, (*i).proc,
					(*i).q_date, (*i).status, (*i).entered_status,
					(*i).submission.c_str(),
					(*i).cmd.c_str(),
					(*i).args1.c_str(),
					(*i).args2.c_str(),
					(*i).release_reason.c_str(),
					(*i).hold_reason.c_str());
		}

		sleep(3);
	}

	return 0;
}
*/
