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

#include "condor_attributes.h"

#include "directory.h"

#include "HistoryFile.h"

#include "HistoryProcessingUtils.h"

#include <libgen.h> // dirname

#include <string>

#define HISTORY_INDEX_SUFFIX ".idx"
using std::string;


HistoryFile::HistoryFile(const string name):
	m_name(name),
	m_stat(NULL),
	m_file(NULL),
	m_index(NULL)
{
}

HistoryFile::~HistoryFile()
{
	cleanup();
}

void
HistoryFile::cleanup()
{
	m_entries.clear();
	if (m_file) { fclose(m_file); m_file = NULL; }
	if (m_index) { fclose(m_index); m_index = NULL; }
	if (m_stat) { free(m_stat); m_stat = NULL; }
}

HistoryFile::HistoryFile(const HistoryFile &base)
{
	m_file = NULL;
	m_index = NULL;
	m_stat = NULL;

	*this = base;
}

HistoryFile &
HistoryFile::operator=(const HistoryFile &base)
{
	if (this != &base) {
		(*this).m_name = base.m_name;

		cleanup();

		// Don't just copy the stat and FILE* members, initialize them
		CondorError errstack;
		if (!init(errstack)) {
			// XXX: Should throw an exception here
			dprintf ( D_ALWAYS, "HistoryFile::operator=: %s\n",
					errstack.getFullText(true).c_str());		
		}
	}

	return *this;
}

bool
HistoryFile::init(CondorError &errstack)
{
	StatWrapper stat_wrapper;

	if (stat_wrapper.Stat(m_name.c_str())) {
		errstack.pushf("HistoryFile::init", 1,
					   "Failed to stat %s: %d (%s)\n",
					   m_name.c_str(),
					   stat_wrapper.GetErrno(),
					   strerror(stat_wrapper.GetErrno()));
		return false;
	}

	m_stat = (StatStructType *) malloc(sizeof(StatStructType));
	ASSERT(m_stat);
	memcpy(m_stat, stat_wrapper.GetBuf(), sizeof(StatStructType));
	if (!S_ISREG(m_stat->st_mode)) {
		errstack.pushf("HistoryFile::init", 2,
					   "%s: not a regular file\n",
					   m_name.c_str());
		return false;
	}

	m_file = safe_fopen_wrapper(m_name.c_str(), "r");
	if (NULL == m_file) {
		errstack.pushf("HistoryFile::init", 4,
					   "Failed to fopen %s: %d (%s)\n",
					   m_name.c_str(), errno, strerror(errno));
		return false;
	}

	// Store the index in history.INO.idx to handle renames
	MyString tmp;
	char *buf = strdup(m_name.c_str());
	ASSERT(buf);
	long unsigned int id;
	ASSERT(getId(id));
	tmp.formatstr("%s%shistory.%ld%s", dirname(buf), DIR_DELIM_STRING, id, HISTORY_INDEX_SUFFIX);
	m_index_name = tmp.Value();
	free(buf);

	const char *mode = "r+";

	if (stat_wrapper.Stat(m_index_name.c_str())) {
			// Not being available is acceptable
		if (ENOENT != stat_wrapper.GetErrno()) {
			errstack.pushf("HistoryFile::init", 1,
						   "Failed to stat %s: %d (%s)\n",
						   m_index_name.c_str(),
						   stat_wrapper.GetErrno(),
						   strerror(stat_wrapper.GetErrno()));
			return false;
		}

		mode = "w+";
 	}

	const StatStructType *stat = stat_wrapper.GetBuf();
	if (!S_ISREG(stat->st_mode)) {
		errstack.pushf("HistoryFile::init", 2,
					   "%s: not a regular file\n",
					   m_index_name.c_str());
		return false;
	}

	m_index = safe_fopen_wrapper(m_index_name.c_str(), mode);
	if (NULL == m_index) {
		errstack.pushf("HistoryFile::init", 4,
					   "Failed to fopen %s: %d (%s)\n",
					   m_index_name.c_str(), errno, strerror(errno));
		return false;
	}

	return true;
}

bool
HistoryFile::getId(long unsigned int &id)
{
	if (!m_stat) return false;

	id = m_stat->st_ino;
	return true;
}

HistoryFile::HistoryEntriesTypeIterators
HistoryFile::poll(CondorError &/*errstack*/)
{
	// Record the end of the current set of records, used to
	// return just the newly read records
	int size = m_entries.size();

	// Load from the index
	CondorError ignored_errstack;
	if (!pollIndex(ignored_errstack)) {
		dprintf(D_FULLDEBUG, "%s\n", ignored_errstack.getFullText(true).c_str());		
	}

	// Seek to the end of the last known record
	if (!m_entries.empty()) {
		HistoryEntry entry = m_entries.back();
		fseek(m_file, entry.stop, SEEK_SET);
	}

	int end, error, empty;
	end = 0;
	while (!end) {
		long int start, stop;

		start = ftell(m_file);

		// NOTE: If the ClassAd parsing completes without finding
		// the delimiter the result is end=1 and empty=0. This
		// should be sufficient for identifying incomplete
		// records that are otherwise valid.
		ClassAd ad(m_file, "***", end, error, empty);

		stop = ftell(m_file);

		if (end) continue;
		if (error) {
			error = 0;
			dprintf(D_ALWAYS, "HistoryFile::poll: malformed ad, skipping\n");
			continue;
		}
		if (empty) {
			empty = 0;
			dprintf(D_ALWAYS, "HistoryFile::poll: empty ad, skipping\n");
			continue;
		}

		int cluster, proc, q_date, status, entered_status;
		char *global_id = NULL, *cmd = NULL, *args1 = NULL, *args2 = NULL;
		char *release_reason = NULL, *hold_reason = NULL;
		char *submission = NULL;
		char *owner = NULL;
		if (!ad.LookupInteger(ATTR_CLUSTER_ID, cluster)) {
			dprintf(D_ALWAYS, "HistoryFile::poll: no %s, skipping\n",
					ATTR_CLUSTER_ID);
			continue;
		}
		if (!ad.LookupInteger(ATTR_PROC_ID, proc)) {
			dprintf(D_ALWAYS, "HistoryFile::poll: no %s, skipping\n",
					ATTR_PROC_ID);
			continue;
		}
		if (!ad.LookupInteger(ATTR_Q_DATE, q_date)) {
			dprintf(D_ALWAYS, "HistoryFile::poll: no %s, skipping: %d.%d\n",
					ATTR_Q_DATE, cluster, proc);
			continue;
		}
		if (!ad.LookupInteger(ATTR_JOB_STATUS, status)) {
			dprintf(D_ALWAYS, "HistoryFile::poll: no %s, skipping: %d.%d\n",
					ATTR_JOB_STATUS, cluster, proc);
			continue;
		}
		if (!ad.LookupInteger(ATTR_ENTERED_CURRENT_STATUS, entered_status)) {
			dprintf(D_ALWAYS, "HistoryFile::poll: no %s, skipping: %d.%d\n",
					ATTR_ENTERED_CURRENT_STATUS, cluster, proc);
			continue;
		}
		if (!ad.LookupString(ATTR_GLOBAL_JOB_ID, &global_id)) {
			dprintf(D_ALWAYS, "HistoryFile::poll: no %s, skipping: %d.%d\n",
					ATTR_GLOBAL_JOB_ID, cluster, proc);
			continue;
		}
		if (!ad.LookupString(ATTR_JOB_CMD, &cmd)) {
			dprintf(D_ALWAYS, "HistoryFile::poll: no %s, skipping: %d.%d\n",
					ATTR_JOB_CMD, cluster, proc);
			continue;
		}
		ad.LookupString(ATTR_JOB_ARGUMENTS1, &args1);
		ad.LookupString(ATTR_JOB_ARGUMENTS2, &args2);
		ad.LookupString(ATTR_RELEASE_REASON, &release_reason);
		ad.LookupString(ATTR_HOLD_REASON, &hold_reason);
		ad.LookupString(ATTR_JOB_SUBMISSION, &submission);
		ad.LookupString(ATTR_OWNER, &owner);

		// Write an index record
		fprintf(m_index,
				"%ld %ld %s %d %d %d %d %d\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
				start, stop, global_id,
				cluster, proc,
				q_date, status, entered_status,
				submission ? submission : "",
				owner ? owner : "",
				cmd,
				args1 ? args1 : "",
				args2 ? args2 : "",
				release_reason ? release_reason : "",
				hold_reason ? hold_reason : "");

		HistoryEntry entry;

		entry.file = m_name;
		entry.start = start;
		entry.stop = stop;
		entry.id = global_id;
		entry.cluster = cluster;
		entry.proc = proc;
		entry.q_date = q_date;
		entry.status = status;
		entry.entered_status = entered_status;
		if (submission) entry.submission = submission;
		if (owner) entry.owner = owner;
		entry.cmd = cmd;
		if (args1) entry.args1 = args1;
		if (args2) entry.args2 = args2;
		if (release_reason) entry.release_reason = release_reason;
		if (hold_reason) entry.hold_reason = hold_reason;

		m_entries.push_back(entry);

        free(global_id);free(cmd);free(args1); free(args2);
        free(release_reason); free(hold_reason);
        free(submission);
        free(owner);
	}

		// Return iterators for newly read records
	return HistoryFile::HistoryEntriesTypeIterators(m_entries.begin() + size,
													m_entries.end());
}

/**
 * Index format:
 *  start(%ld) stop(%ld) id(%s) cluster(%d) proc(%d) q_date(%d) status(%d) entered_status(%d)
 *  submission(%s)
 *  cmd(%s)
 *  args1(%s)
 * 	args2(%s)
 *  release_reason(%s)
 *  hold_reason(%s)
 */
bool
HistoryFile::pollIndex(CondorError &errstack)
{
	while (1) {
		int code;
		HistoryEntry entry;

		entry.file = m_name;
		char *id = NULL;
		code = fscanf(m_index, "%ld %ld %as %d %d %d %d %d",
					  &entry.start, &entry.stop, &id,
					  &entry.cluster, &entry.proc,
					  &entry.q_date, &entry.status, &entry.entered_status);
		if (code != 8) {
			// Ok, at the end of the file, we're done
			// code check is to handle truncated final entry
			if (feof(m_index) && code < 0) {
				break;
			}

			// XXX: EINTR?
			errstack.pushf("HistoryFile::pollIndex", 6,
						   "Failed to fscanf HistoryEntry from %s, "
						   "rc=%d: %d (%s)\n",
						   m_index_name.c_str(), code, errno, strerror(errno));

			return false;
		}

		MyString buf;

		buf.readLine(m_index); // consume newline

#define BUF_SET(dest,attr)											\
		if (buf.readLine(m_index)) {								\
			buf.trim();												\
			if (!buf.IsEmpty()) {									\
				dest = buf.Value();									\
			}														\
		} else {													\
			errstack.pushf("HistoryFile::pollIndex", 7,				\
						   "Failed to read %s's %s from %s\n",		\
						   id, attr, m_index_name.c_str());			\
			return false;											\
		}

		BUF_SET(entry.submission, ATTR_JOB_SUBMISSION);
		BUF_SET(entry.owner, ATTR_OWNER);
		BUF_SET(entry.cmd, ATTR_JOB_CMD);
		BUF_SET(entry.args1, ATTR_JOB_ARGUMENTS1);
		BUF_SET(entry.args2, ATTR_JOB_ARGUMENTS2);
		BUF_SET(entry.release_reason, ATTR_RELEASE_REASON);
		BUF_SET(entry.hold_reason, ATTR_HOLD_REASON);

		entry.id = id;
		free(id); id = NULL;

		m_entries.push_back(entry);
	}

	return true;
}
