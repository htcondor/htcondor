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

//condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_qmgr.h"
#include "set_user_priv_from_ad.h"
#include "stat_info.h"
#include "stl_string_utils.h"

// C++ includes
// enable for debugging classad to ostream
// watch out for unistd clash
//#include <sstream>

//local includes
#include "JobServerObject.h"
#include "AviaryConversionMacros.h"
#include "AviaryUtils.h"
#include "Codec.h"
#include "JobServerJobLogConsumer.h"
#include "Globals.h"

using namespace std;
using namespace aviary::query;
using namespace aviary::util;
using namespace aviary::codec;

JobServerObject* JobServerObject::m_instance = NULL;

JobServerObject::JobServerObject ()
{
	m_name = getScheddName();
	m_pool = getPoolName();
	m_codec = new BaseCodec;
}

JobServerObject::~JobServerObject()
{
	delete m_codec;
}

JobServerObject* JobServerObject::getInstance()
{
    if (!m_instance) {
        m_instance = new JobServerObject();
    }
    return m_instance;
}

void
JobServerObject::update ( const ClassAd &ad )
{
    MGMT_DECLARATIONS;

    m_stats.Pool = getPoolName();
    STRING ( CondorPlatform );
    STRING ( CondorVersion );
    TIME_INTEGER ( DaemonStartTime );
//  TIME_INTEGER(JobQueueBirthdate);
    STRING ( Machine );
//  INTEGER(MaxJobsRunning);
    INTEGER ( MonitorSelfAge );
    DOUBLE ( MonitorSelfCPUUsage );
    DOUBLE ( MonitorSelfImageSize );
    INTEGER ( MonitorSelfRegisteredSocketCount );
    INTEGER ( MonitorSelfResidentSetSize );
    TIME_INTEGER ( MonitorSelfTime );
    STRING ( MyAddress );
    //TIME_INTEGER(MyCurrentTime);
    STRING ( Name );
//  INTEGER(NumUsers);
    STRING ( PublicNetworkIpAddr );
//  INTEGER(TotalHeldJobs);
//  INTEGER(TotalIdleJobs);
//  INTEGER(TotalJobAds);
//  INTEGER(TotalRemovedJobs);
//  INTEGER(TotalRunningJobs);

    m_stats.System = m_stats.Machine;
}

Job*
getValidKnownJob(const char* key, AviaryStatus &_status) {

	// #1: is it even a proper "cluster.proc"?
	PROC_ID id = getProcByString(key);
	if (id.cluster < 0 || id.proc < 0) {
		formatstr (_status.text, "Invalid job id '%s'",key);
		dprintf(D_FULLDEBUG, "%s\n", _status.text.c_str());
		_status.type = AviaryStatus::FAIL;
		return NULL;
	}

	// #2 is it anywhere in our job map?
    JobCollectionType::const_iterator element = g_jobs.find(key);
    if ( g_jobs.end() == element ) {
		formatstr (_status.text, "Unknown local job id '%s'",key);
		dprintf(D_FULLDEBUG, "%s\n", _status.text.c_str());
		_status.type = AviaryStatus::NO_MATCH;
		return NULL;
    }

	return (*element).second;
}

bool JobServerObject::getStatus(const char* key, int& job_status, AviaryStatus &_status) {
	Job* job = NULL;
	if (!(job = getValidKnownJob(key,_status))) {
		return false;
	}

	job_status = job->getStatus();

	_status.type = AviaryStatus::A_OK;
	return true;
}

bool JobServerObject::getSummary(const char* key, JobSummaryFields& _summary, AviaryStatus &_status) {
	Job* job = NULL;
	if (!(job = getValidKnownJob(key,_status))) {
		return false;
	}

    ClassAd classAd;
    job->getSummary ( classAd );
    // little cheat for ad problems with history lookups
    string str;
    if ( classAd.LookupString("JOB_AD_ERROR", str) )
    {
		formatstr(_status.text,"Error obtaining ClassAd for job '%s'; ",key);
		_status.text += str;
		dprintf(D_ALWAYS,"%s\n",_status.text.c_str());
        return false;
    }

	// return the limited attributes
    classAd.LookupString(ATTR_JOB_CMD,_summary.cmd);
	classAd.LookupString(ATTR_JOB_ARGUMENTS1,_summary.args1);
	classAd.LookupString(ATTR_JOB_ARGUMENTS2,_summary.args2);
	classAd.LookupString(ATTR_HOLD_REASON,_summary.hold_reason);
	classAd.LookupString(ATTR_RELEASE_REASON,_summary.release_reason);
	classAd.LookupString(ATTR_REMOVE_REASON,_summary.remove_reason);
	classAd.LookupString(ATTR_JOB_SUBMISSION,_summary.submission_id);
	classAd.LookupString(ATTR_OWNER,_summary.owner);
	classAd.LookupInteger(ATTR_Q_DATE,_summary.queued);
	classAd.LookupInteger(ATTR_ENTERED_CURRENT_STATUS,_summary.last_update);
	_summary.status = job->getStatus();

	_status.type = AviaryStatus::A_OK;
    return true;
}

bool
JobServerObject::getJobAd ( const char* key, AttributeMapType& _map, AviaryStatus &_status)
{
	Job* job = NULL;
	if (!(job = getValidKnownJob(key,_status))) {
		return false;
	}
    // call Job::getFullAd and use utils to populate the map
    ClassAd classAd;
    job->getFullAd ( classAd );
    // little cheat for ad problems with history lookups
    string str;
    if ( classAd.LookupString("JOB_AD_ERROR", str) )
    {
		formatstr(_status.text,"Error obtaining ClassAd for job '%s'; ",key);
		_status.text += str;
		dprintf(D_ALWAYS,"%s\n",_status.text.c_str());
    }

    // return all the attributes in the ClassAd
    if ( !m_codec->classAdToMap ( classAd, _map  ) )
    {
		formatstr(_status.text,"Error mapping info for job '%s'; ",key);
		dprintf(D_ALWAYS,"%s\n",_status.text.c_str());
        return false;
    }

    // debug
//    if (IsFulldebug(D_FULLDEBUG)) {
//        classAd.dPrint(D_FULLDEBUG|D_NOHEADER);
//        std::ostringstream oss;
//        oss << _map;
//        dprintf(D_FULLDEBUG|D_NOHEADER, oss.str().c_str());
//    }

	_status.type = AviaryStatus::A_OK;
    return true;
}

bool
JobServerObject::fetchJobData(const char* key,
					   const UserFileType ftype,
					   std::string& fname,
					   int max_bytes,
					   bool from_end,
					   int& fsize,
					   std::string &data,
					   AviaryStatus &_status)
{
	int32_t start;
	int32_t end;
	priv_state prev_priv_state;
	int fd = -1;
	int count;
	int length;
	int whence;
	char *buffer;
	bool fetched;
	Job* job = NULL;

	if (!(job =getValidKnownJob(key,_status))) {
		return false;
	}


	ClassAd ad;
	string str;
	job->getFullAd ( ad );
	if ( ad.LookupString("JOB_AD_ERROR", str)  ) {
		formatstr(_status.text,"Error checking ClassAd for user priv on job '%s'; ",key);
		_status.text += str;
		dprintf(D_ALWAYS,"%s\n",_status.text.c_str());
		return false;
	}
	
	// find out what the actual file is from classad lookup
	switch (ftype) {
		case ERR:
			if ( !ad.LookupString(ATTR_JOB_ERROR, fname)  ) {
				formatstr (_status.text,  "No error file for job '%s'",key);
				dprintf(D_ALWAYS,"%s\n", _status.text.c_str());
				return false;
			}
			break;
		case LOG:
			if ( !ad.LookupString(ATTR_ULOG_FILE, fname)  ) {
				formatstr (_status.text,  "No log file for job '%s'",key);
				dprintf(D_ALWAYS,"%s\n", _status.text.c_str());
				return false;
			}
			break;
		case OUT:
			if ( !ad.LookupString(ATTR_JOB_OUTPUT, fname)  ) {
				formatstr (_status.text,  "No output file for job '%s'",key);
				dprintf(D_ALWAYS,"%s\n", _status.text.c_str());
				return false;
			}
			break;
		default:
			// ruh-roh...asking for a file type we don't know about
			formatstr (_status.text,  "Unknown file type for job '%s'",key);
			dprintf(D_ALWAYS,"%s\n", _status.text.c_str());
			return false;
	}

	prev_priv_state = set_user_priv_from_ad(ad);

	StatInfo the_file(fname.c_str());
	if (the_file.Error()) {
		formatstr (_status.text, "Error opening requested file '%s', error %d",fname.c_str(),the_file.Errno());
        dprintf(D_FULLDEBUG,"%s\n", _status.text.c_str());
        // don't give up yet...maybe it's IWD+filename
        string iwd_path;
        if ( !ad.LookupString(ATTR_JOB_IWD, iwd_path)  ) {
            formatstr (_status.text,  "No IWD found for job '%s'",key);
            dprintf(D_ALWAYS,"%s\n", _status.text.c_str());
            return false;
        }
        StatInfo the_iwd_file(iwd_path.c_str(),fname.c_str());
        if (the_iwd_file.Error()) {
            formatstr (_status.text,  "No output file for job '%s'",key);
            dprintf(D_ALWAYS,"%s\n", _status.text.c_str());
            return false;
        }
        else {
            fsize = the_iwd_file.GetFileSize();
            fname = the_iwd_file.FullPath();
        }
    }
    else {
        fsize = the_file.GetFileSize();
    }

	// we calculate these based on file size
	if (from_end) {
		end = fsize;
		start = end - max_bytes;
	}
	else {
		start = 0;
		end = max_bytes;
	}

	// start >= 0, end >= 0 :: lseek(start, SEEK_SET), read(end - start)
	//  end < start :: error, attempt to read backwards
	// start >= 0, end < 0 :: error, don't know length
	// start < 0, end > 0 :: attempt to read off end of file, end = 0
	// start < 0, end <= 0 :: lseek(start, SEEK_END), read(abs(start - end))
	//  end < start :: error, attempt to read backwards

	// TODO: redundant checks given above
	if ((start >= 0 && end >= 0 && end < start) ||
		(start >= 0 && end < 0) ||
		(start < 0 && end <= 0 && end < start)) {
		_status.text = "Invalid start and end values";
		return false;
	}

	// Instead of reading off the end of the file, read to the
	// end of it
	if (start < 0 && end > 0) {
		end = 0;
	}

	if (start >= 0) {
		whence = SEEK_SET;
		length = end - start;
	} else {
		whence = SEEK_END;
		length = abs(start - end);
	}

	// TODO: Sanity check that length isn't too big?
	buffer = new char[length + 1];

	if (-1 != (fd = safe_open_wrapper(fname.c_str(),
									  O_RDONLY | _O_BINARY,
									  0))) {
			// If we are seeking from the end of the file, it is
			// possible that we will try to seek off the front of the
			// file. To avoid this, we check the file's size and set
			// the max length that we are able to seek
		if (SEEK_END == whence) {
			struct stat buf;
			if (-1 != fstat(fd, &buf)) {
				if (buf.st_size < abs(start)) {
					start = -buf.st_size;
				}
			} // if fstat fails, we just continue having made an effort
		}

		if (-1 != lseek(fd, start, whence)) {				
			if (-1 == (count = full_read(fd, buffer, length))) {
				_status.text = "Failed to read from " + fname;
				fetched = false;
			} else {
					// Terminate the string.
				buffer[count] = '\0';

				data = buffer;
				fetched = true;
			}

			close(fd); // assume closed on failure?
		} else {
			_status.text = "Failed to seek in " + fname;
			fetched = false;
		}
	} else {
		_status.text = "Failed to open " + fname;
		fetched = false;
	}

	set_priv(prev_priv_state);

	delete [] buffer;

	if (fetched) {
		_status.type = AviaryStatus::A_OK;
	}
	return fetched;
}
