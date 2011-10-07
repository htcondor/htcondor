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

#include "ODSMongodbOps.h"

// condor includes
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "directory.h"

// c++ includes
#include <string>

// global includes
#include <libgen.h>

// local includes
#include "ODSHistoryFile.h"
#include "ODSHistoryUtils.h"

const char DB_NAME[] = "condor_jobs.history";

using namespace std;
using namespace mongo;
using namespace bson;
using namespace plumage::etl;

ODSHistoryFile::ODSHistoryFile(const string name):
	m_name(name),
	m_stat(NULL),
	m_file(NULL),
	m_writer(NULL)
{
}

ODSHistoryFile::~ODSHistoryFile()
{
	cleanup();
}

void
ODSHistoryFile::cleanup()
{
	if (m_file) { fclose(m_file); m_file = NULL; }
	if (m_stat) { free(m_stat); m_stat = NULL; }
}

ODSHistoryFile::ODSHistoryFile(const ODSHistoryFile &base)
{
	m_file = NULL;
	m_stat = NULL;

	*this = base;
}

ODSHistoryFile &
ODSHistoryFile::operator=(const ODSHistoryFile &base)
{
	if (this != &base) {
		(*this).m_name = base.m_name;

		cleanup();

		// Don't just copy the stat and FILE* members, initialize them
		CondorError errstack;
		if (!init(errstack)) {
			// XXX: Should throw an exception here
			dprintf ( D_ALWAYS, "ODSHistoryFile::operator=: %s\n",
					errstack.getFullText(true));		
		}
	}

	return *this;
}

bool
ODSHistoryFile::init(CondorError &errstack)
{
	StatWrapper stat_wrapper;

	if (stat_wrapper.Stat(m_name.c_str())) {
		errstack.pushf("ODSHistoryFile::init", 1,
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
		errstack.pushf("ODSHistoryFile::init", 2,
					   "%s: not a regular file\n",
					   m_name.c_str());
		return false;
	}

	m_file = safe_fopen_wrapper(m_name.c_str(), "r");
	if (NULL == m_file) {
		errstack.pushf("ODSHistoryFile::init", 4,
					   "Failed to fopen %s: %d (%s)\n",
					   m_name.c_str(), errno, strerror(errno));
		return false;
	}
	
	m_writer = new ODSMongodbOps(DB_NAME);
    if (!m_writer->init("localhost")) {
        errstack.pushf("ODSHistoryFile::init", 5,
                       "Unable to init ODS writer\n");
        return false;
    }

	return true;
}

bool
ODSHistoryFile::getId(long unsigned int &id)
{
	if (!m_stat) return false;

	id = m_stat->st_ino;
	return true;
}

ODSHistoryFile::ODSHistoryEntryTypeIterators
ODSHistoryFile::poll(CondorError &/*errstack*/)
{
	// Record the end of the current set of records, used to
	// return just the newly read records
	int size = m_entries.size();

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
			dprintf(D_ALWAYS, "ODSHistoryFile::poll: malformed ad, skipping\n");
			continue;
		}
		if (empty) {
			empty = 0;
			dprintf(D_ALWAYS, "ODSHistoryFile::poll: empty ad, skipping\n");
			continue;
		}

        // TODO: write the ad to db here
        BSONObjBuilder key;
        int cluster, proc;
        if (ad.LookupInteger(ATTR_CLUSTER_ID,cluster)) {
            key.append(ATTR_CLUSTER_ID, cluster);
        }
        if (ad.LookupInteger(ATTR_PROC_ID,proc)) {
            key.append(ATTR_PROC_ID, proc);
        }

        if (!m_writer->updateAd(key,&ad)) {
            dprintf(D_ALWAYS, "ODSHistoryFile::poll: unable to write history job ad to ODS for '%d.%d'\n");
        }
        else {
            dprintf(D_FULLDEBUG, "ODSHistoryFile::poll: recorded job class ad for '%d.%d'\n", cluster,proc);
        }
        
		// record these so we know where we are?
		HistoryEntry entry;
		entry.file = m_name;
		entry.start = start;
		entry.stop = stop;

		m_entries.push_back(entry);
	}

		// Return iterators for newly read records
	return ODSHistoryFile::ODSHistoryEntryTypeIterators(m_entries.begin() + size,
													m_entries.end());
}
