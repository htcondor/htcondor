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

#include "JobServerObject.h"

#include "ArgsJobServerGetJobAd.h"
#include "ArgsJobServerFetchJobData.h"

#include "stdint.h"

#include "condor_attributes.h"

#include "condor_debug.h"

#include "condor_qmgr.h"

#include "set_user_priv_from_ad.h"

#include "MgmtConversionMacros.h"

#include "JobServerJobLogConsumer.h"

#include "Globals.h"
#include "PoolUtils.h"

// from src/management
#include "Utils.h"

#include <sstream>

using namespace com::redhat::grid;
using namespace qmf::com::redhat::grid;
using namespace qpid::management;


JobServerObject::JobServerObject ( ManagementAgent *agent, const char* _name, ObjectId& _oid )
{
    mgmtObject = new JobServer ( agent, this );
    mgmtObject->set_schedulerRef(_oid);

    // By default the jobserver will be persistent.
    bool _lifetime = param_boolean("QMF_IS_PERSISTENT", true);
    agent->addObject ( mgmtObject, _name, _lifetime);
}


JobServerObject::~JobServerObject()
{
    if ( mgmtObject )
    {
        mgmtObject->resourceDestroy();
    }
}


void
JobServerObject::update ( const ClassAd &ad )
{
    MGMT_DECLARATIONS;

    mgmtObject->set_Pool(GetPoolName());

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

    mgmtObject->set_System ( mgmtObject->get_Machine() );
}


Manageable::status_t
JobServerObject::GetJobAd ( std::string key,
                          Variant::Map &_map,
                          std::string &text)
{
    JobCollectionType::const_iterator element = g_jobs.find(key.c_str());
    if ( g_jobs.end() == element )
    {
        text = "Unknown Job Id";
        return STATUS_USER + 0;
    }

    // call Job::GetFullAd and use management utils to populate the map
    ClassAd classAd;
    ( *element ).second->GetFullAd ( classAd );
    // little cheat for ad problems with history lookups
    if ( classAd.LookupString("JOB_AD_ERROR", text) )
    {
        return STATUS_USER + 1;
    }

    // return all the attributes in the ClassAd
    if ( !PopulateVariantMapFromAd ( classAd, _map  ) )
    {
        text = "Error retrieving Job data";
        return STATUS_USER + 2;
    }

    // debug
//    if (DebugFlags & D_FULLDEBUG) {
//        classAd.dPrint(D_FULLDEBUG|D_NOHEADER);
//        std::ostringstream oss;
//        oss << _map;
//        dprintf(D_FULLDEBUG|D_NOHEADER, oss.str().c_str());
//    }

    return STATUS_OK;
}

Manageable::status_t
JobServerObject::FetchJobData(std::string key,
					   std::string &file,
					   int32_t start,
					   int32_t end,
					   std::string &data,
					   std::string &text)
{
	priv_state prev_priv_state;
	int fd = -1;
	int count;
	int length;
	int whence;
	char *buffer;
	Manageable::status_t status;

	PROC_ID id = getProcByString(key.c_str());
	if (id.cluster < 0 || id.proc < 0) {
		dprintf(D_FULLDEBUG, "FetchJobData: Failed to parse id: %s\n", key.c_str());
		text = "Invalid Job Id";
		return STATUS_USER + 0;
	}

		// start >= 0, end >= 0 :: lseek(start, SEEK_SET), read(end - start)
		//  end < start :: error, attempt to read backwards
		// start >= 0, end < 0 :: error, don't know length
		// start < 0, end > 0 :: attempt to read off end of file, end = 0
		// start < 0, end <= 0 :: lseek(start, SEEK_END), read(abs(start - end))
		//  end < start :: error, attempt to read backwards

	if ((start >= 0 && end >= 0 && end < start) ||
		(start >= 0 && end < 0) ||
		(start < 0 && end <= 0 && end < start)) {
		text = "Invalid start and end values";
		return STATUS_USER + 10;
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

		// XXX: Sanity check that length isn't too big?

	buffer = new char[length + 1];

	// scan our job map to see if this is even a known proc id
	JobCollectionType::const_iterator element = g_jobs.find(key.c_str());
	if ( g_jobs.end() == element ) {
		dprintf(D_ALWAYS,
				"Fetch method called on '%d.%d', which does not exist\n",
				id.cluster, id.proc);
		return STATUS_UNKNOWN_OBJECT;
	}

	ClassAd ad;
	( *element ).second->GetFullAd ( ad );
	if ( ad.LookupString("JOB_AD_ERROR", text)  ) {
		dprintf(D_ALWAYS,
				"Error checking ClassAd for user priv on key = '%d.%d'\n",
				id.cluster, id.proc);
		return STATUS_USER + 7;
	}

	prev_priv_state = set_user_priv_from_ad(ad);

	if (-1 != (fd = safe_open_wrapper(file.c_str(),
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
				text = "Failed to read from " + file;
				status = STATUS_USER + 3;
			} else {
					// Terminate the string.
				buffer[count] = '\0';

				data = buffer;
				status = STATUS_OK;
			}

			close(fd); // assume closed on failure?
		} else {
			text = "Failed to seek in " + file;
			status = STATUS_USER + 2;
		}
	} else {
		text = "Failed to open " + file;
		status = STATUS_USER + 1;
	}

	set_priv(prev_priv_state);

	delete []buffer;

	return status;
}

ManagementObject *
JobServerObject::GetManagementObject ( void ) const
{
    return mgmtObject;
}


Manageable::status_t
JobServerObject::ManagementMethod ( uint32_t methodId,
                                    Args &args,
                                    std::string &text )
{
    switch ( methodId )
    {
        case qmf::com::redhat::grid::JobServer::METHOD_ECHO:
			if (!param_boolean("QMF_MANAGEMENT_METHOD_ECHO", false)) return STATUS_NOT_IMPLEMENTED;

            return STATUS_OK;
        case qmf::com::redhat::grid::JobServer::METHOD_GETJOBAD:
            return GetJobAd ( ( ( ArgsJobServerGetJobAd & ) args ).i_Id,
                           ( ( ArgsJobServerGetJobAd & ) args ).o_JobAd,
                           text );
	case qmf::com::redhat::grid::JobServer::METHOD_FETCHJOBDATA:
            return FetchJobData ( ( ( ArgsJobServerFetchJobData & ) args ).i_Id,
                           ( ( ArgsJobServerFetchJobData & ) args ).i_File,
			   ( ( ArgsJobServerFetchJobData & ) args ).i_Start,
			   ( ( ArgsJobServerFetchJobData & ) args ).i_End,
			   ( ( ArgsJobServerFetchJobData & ) args ).o_Data,
                           text );
    }

    return STATUS_NOT_IMPLEMENTED;
}

bool
JobServerObject::AuthorizeMethod(uint32_t methodId, Args& args, const std::string& userId) {
	dprintf(D_FULLDEBUG, "AuthorizeMethod: checking '%s'\n", userId.c_str());
	if (0 == userId.compare("cumin")) {
		return true;
	}
	return false;
}
