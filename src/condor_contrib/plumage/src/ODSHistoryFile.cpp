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

#include "mongo/client/dbclient.h"

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

const char DB_NAME[] = "condor.jobs";
extern mongo::DBClientConnection* db_client;

using namespace std;
using namespace mongo;
using namespace bson;
using namespace plumage::etl;

// cleans up the quoted values from the job log reader
static string trimQuotes(const char* str) {
    string val = str;

    size_t endpos = val.find_last_not_of("\\\"");
    if( string::npos != endpos ) {
        val = val.substr( 0, endpos+1 );
    }
    size_t startpos = val.find_first_not_of("\\\"");
    if( string::npos != startpos ) {
        val = val.substr( startpos );
    }

    return val;
}

ODSHistoryFile::ODSHistoryFile(const string name):
	m_name(name),
	m_stat(NULL),
	m_file(NULL)
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
        ExprTree *expr;
        const char *name;
        ad.ResetExpr();

        BSONObjBuilder bsonBuilder;
        int cluster, proc;
        while (ad.NextExpr(name,expr)) {

            if (!(expr = ad.Lookup(name))) {
                dprintf(D_FULLDEBUG, "Warning: failed to lookup attribute '%s' from ad\n", name);
                continue;
            }
            
            classad::Value value;
            ad.EvaluateExpr(expr,value);
            switch (value.GetType()) {
                // seems this covers expressions also
                case classad::Value::BOOLEAN_VALUE:
                    bool _bool;
                    ad.LookupBool(name,_bool);
                    bsonBuilder.append(name,_bool);
                    break;
                case classad::Value::INTEGER_VALUE:
                    int i;
                    ad.LookupInteger(name,i);
                    bsonBuilder.append(name,i);                    
                    if (0 == strcmp(name,ATTR_CLUSTER_ID)) {
                        cluster = i;
                    }
                    if (0 == strcmp(name,ATTR_PROC_ID)) {
                        proc = i;
                    }
                    break;
                case classad::Value::REAL_VALUE:
                    float f;
                    ad.LookupFloat(name,f);
                    bsonBuilder.append(name,f);
                    break;
                case classad::Value::STRING_VALUE:
                default:
                    bsonBuilder.append(name,trimQuotes(ExprTreeToString(expr)));
            }
            
        }
        try {
                BSONObj queryObj = BSONObjBuilder().append(ATTR_CLUSTER_ID, cluster).append(ATTR_PROC_ID, proc).obj();
                db_client->update(DB_NAME, queryObj, bsonBuilder.obj(), TRUE, FALSE);
            }
            catch(DBException& e) {
            dprintf(D_ALWAYS,"ODSMongodbWriter::writeClassAd caught DBException: %s\n", e.toString().c_str());
        }
        string last_err = db_client->getLastError();
        if (!last_err.empty()) {
            dprintf(D_ALWAYS,"mongodb getLastError: %s\n",last_err.c_str());
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
