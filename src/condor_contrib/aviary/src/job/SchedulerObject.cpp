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

// condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_qmgr.h"
#include "../condor_schedd.V6/scheduler.h"

// local includes
#include "AviaryUtils.h"
#include "AviaryConversionMacros.h"
#include "SchedulerObject.h"
#include "Codec.h"

// Global Scheduler object, used for needReschedule
extern Scheduler scheduler;
extern char * Name;

using namespace aviary::job;
using namespace aviary::util;
using namespace aviary::codec;

SchedulerObject* SchedulerObject::m_instance = NULL;

SchedulerObject::SchedulerObject()
{
    m_pool = getPoolName();
	m_name = getScheddName();
    m_codec = new BaseCodec();
}

SchedulerObject::~SchedulerObject()
{
	delete m_codec;
}

SchedulerObject* SchedulerObject::getInstance()
{
    if (!m_instance) {
        m_instance = new SchedulerObject();
    }
    return m_instance;
}

void
SchedulerObject::update(const ClassAd &ad)
{
	MGMT_DECLARATIONS;

	m_stats.Pool = getPoolName();
	STRING(CondorPlatform);
	STRING(CondorVersion);
	TIME_INTEGER(DaemonStartTime);
	TIME_INTEGER(JobQueueBirthdate);
	STRING(Machine);
	INTEGER(MaxJobsRunning);
	INTEGER(MonitorSelfAge);
	DOUBLE(MonitorSelfCPUUsage);
	DOUBLE(MonitorSelfImageSize);
	INTEGER(MonitorSelfRegisteredSocketCount);
	INTEGER(MonitorSelfResidentSetSize);
	TIME_INTEGER(MonitorSelfTime);
	STRING(MyAddress);
    //TIME_INTEGER(MyCurrentTime);
	STRING(Name);
	INTEGER(NumUsers);
	STRING(MyAddress);
	INTEGER(TotalHeldJobs);
	INTEGER(TotalIdleJobs);
	INTEGER(TotalJobAds);
	INTEGER(TotalRemovedJobs);
	INTEGER(TotalRunningJobs);
	m_stats.System = m_stats.Machine;

	// debug
	if (IsFulldebug(D_FULLDEBUG)) {
		const_cast<ClassAd*>(&ad)->dPrint(D_FULLDEBUG|D_NOHEADER);
	}
}


bool
SchedulerObject::submit(AttributeMapType &jobAdMap, std::string &id, std::string &text)
{
	int cluster;
	int proc;

    if (!m_codec) {
        text = "Codec has not been initialized";
        return false;
    }

	// our mandatory set of attributes for a submit
	const char* required[] = {
				ATTR_JOB_CMD,
				ATTR_REQUIREMENTS,
				ATTR_OWNER,
				ATTR_JOB_IWD,
				NULL
				};

		// 1. Create transaction
	BeginTransaction();

		// 2. Create cluster
	if (-1 == (cluster = NewCluster())) {
		AbortTransaction();
		text = "Failed to create new cluster";
		return false;
	}

		// 3. Create proc
	if (-1 == (proc = NewProc(cluster))) {
		AbortTransaction();
		text = "Failed to create new proc";
		return false;
	}

		// 4. Submit job ad

		// Schema: (vanilla job)
		// Schedd demands - Owner, JobUniverse
		// To run - JobStatus, Requirements

		// Schedd excepts if no Owner
		// Schedd prunes on startup if no Owner or JobUniverse
		// Schedd won't run job without JobStatus
		// Job cannot match without Requirements
		// Shadow rejects jobs without an Iwd
		// Shadow: Job has no CondorVersion, assuming pre version 6.3.3
		// Shadow: Unix Vanilla job is pre version 6.3.3, setting 'TransferFiles = "NEVER"'
		// Starter won't run job without Cmd
		// Starter needs a valid Owner (local account name) if not using nobody
		// condor_q requires ClusterId (int), ProcId (int), QDate (int), RemoteUserCpu (float), JobStatus (int), JobPrio (int), ImageSize (int), Owner (str) and Cmd (str)

		// Schema: (vm job)
		// ShouldTransferFiles - unset by default, must be set

	ClassAd ad;
	int universe;

    // ShouldTransferFiles - unset by default, must be set
    // shadow will try to setup local transfer sandbox otherwise
    // without good priv
    ad.Assign(ATTR_SHOULD_TRANSFER_FILES, "NO");

	if (!m_codec->mapToClassAd(jobAdMap, ad, text)) {
		AbortTransaction();
		return false;
	}

	std::string missing;
	if (!checkRequiredAttrs(ad, required, missing)) {
		AbortTransaction();
		text = "Job ad is missing required attributes: " + missing;
		return false;
	}

		// EARLY SET: These attribute are set early so the incoming ad
		// has a change to override them.
	::SetAttribute(cluster, proc, ATTR_JOB_STATUS, "1"); // 1 = idle

		// Junk that condor_q wants, but really shouldn't be necessary
	::SetAttribute(cluster, proc, ATTR_JOB_REMOTE_USER_CPU, "0.0"); // float
	::SetAttribute(cluster, proc, ATTR_JOB_PRIO, "0");              // int
	::SetAttribute(cluster, proc, ATTR_IMAGE_SIZE, "0");            // int

	if (!ad.LookupInteger(ATTR_JOB_UNIVERSE, universe)) {
		char* uni_str = param("DEFAULT_UNIVERSE");
		if (!uni_str) {
			universe = CONDOR_UNIVERSE_VANILLA;
		}
		else {
			universe = CondorUniverseNumber(uni_str);
		}
		::SetAttributeInt(cluster, proc, ATTR_JOB_UNIVERSE, universe );
	}
	// more stuff - without these our idle stats are whack
	if ( universe != CONDOR_UNIVERSE_MPI && universe != CONDOR_UNIVERSE_PVM ) {
		::SetAttribute(cluster, proc, ATTR_MAX_HOSTS, "1");              // int
		::SetAttribute(cluster, proc, ATTR_MIN_HOSTS, "1");            // int
	}
	::SetAttribute(cluster, proc, ATTR_CURRENT_HOSTS, "0"); // int

	ExprTree *expr;
	const char *name;
	std::string value;
	ad.ResetExpr();
	while (ad.NextExpr(name,expr)) {

			// All these extra lookups are horrible. They have to
			// be there because the ClassAd may have multiple
			// copies of the same attribute name! This means that
			// the last attribute with a given name will set the
			// value, but the last attribute tends to be the
			// attribute with the oldest (wrong) value. How
			// annoying is that!
		if (!(expr = ad.Lookup(name))) {
			dprintf(D_ALWAYS, "Failed to lookup %s\n", name);

			AbortTransaction();
			text = "Failed to parse job ad attribute";
			return false;
		}

        value = ExprTreeToString(expr);
        ::SetAttribute(cluster, proc, name, value.c_str());
	}

		// LATE SET: These attributes are set late, after the incoming
		// ad, so they override whatever the incoming ad set.
	char buf[22]; // 22 is max size for an id, 2^32 + . + 2^32 + \0
	snprintf(buf, 22, "%d", cluster);
	::SetAttribute(cluster, proc, ATTR_CLUSTER_ID, buf);
	snprintf(buf, 22, "%d", proc);
	::SetAttribute(cluster, proc, ATTR_PROC_ID, buf);
	snprintf(buf, 22, "%ld", time(NULL));
	::SetAttribute(cluster, proc, ATTR_Q_DATE, buf);

		// Could check for some invalid attributes, e.g
		//  JobUniverse <= CONDOR_UNIVERSE_MIN or >= CONDOR_UNIVERSE_MAX

		// 5. Commit transaction
	CommitTransaction();


		// 6. Reschedule
	scheduler.needReschedule();


		// 7. Return identifier
		// TODO: dag ids?
	MyString tmp;
	//tmp.sprintf("%s#%d.%d", Name, cluster, proc);
	// we have other API compositions for job id and submission id
	// so let's return raw cluster.proc
	tmp.formatstr("%d.%d", cluster, proc);
	id = tmp.Value();

	return true;
}

bool
SchedulerObject::setAttribute(std::string key,
							  std::string name,
							  std::string value,
							  std::string &text)
{
	PROC_ID id = getProcByString(key.c_str());
	if (id.cluster <= 0 || id.proc < 0) {
		dprintf(D_FULLDEBUG, "SetAttribute: Failed to parse id: %s\n", key.c_str());
		text = "Invalid Id";
		return false;
	}

	if (isSubmissionChange(name.c_str())) {
		text = "Changes to submission name not allowed";
		return false;
	}

	if (isKeyword(name.c_str())) {
		text = "Attribute name is reserved: " + name;
		return false;
	}

	if (!isValidAttributeName(name,text)) {
		return false;
	}

		// All values are strings in the eyes of
		// ::SetAttribute. Their type is inferred when read from
		// the ClassAd log. It is important that the incoming
		// value is properly quoted to differentiate between EXPR
		// and STRING.
	if (::SetAttribute(id.cluster,
					   id.proc,
					   name.c_str(),
					   value.c_str())) {
		text = "Failed to set attribute " + name + " to " + value;
		return false;
	}

	return true;
}

bool
SchedulerObject::hold(std::string key, std::string &reason, std::string &text)
{
	PROC_ID id = getProcByString(key.c_str());
	if (id.cluster <= 0 || id.proc < 0) {
		dprintf(D_FULLDEBUG, "Hold: Failed to parse id: %s\n", key.c_str());
		text = "Invalid Id";
		return false;
	}

	if (!holdJob(id.cluster,
				 id.proc,
				 reason.c_str(),
				 true, // Always perform this action within a transaction
				 true, // Always notify the shadow of the hold
				 false, // Do not email the user about this action
				 false, // Do not email admin about this action
				 false // This is not a system related (internal) hold
				 )) {
		text = "Failed to hold job";
		return false;
	}

	return true;
}

bool
SchedulerObject::release(std::string key, std::string &reason, std::string &text)
{
	PROC_ID id = getProcByString(key.c_str());
	if (id.cluster <= 0 || id.proc < 0) {
		dprintf(D_FULLDEBUG, "Release: Failed to parse id: %s\n", key.c_str());
		text = "Invalid Id";
		return false;
	}

	if (!releaseJob(id.cluster,
					id.proc,
					reason.c_str(),
					true, // Always perform this action within a transaction
					false, // Do not email the user about this action
					false // Do not email admin about this action
					)) {
		text = "Failed to release job";
		return false;
	}

	return true;
}

bool
SchedulerObject::remove(std::string key, std::string &reason, std::string &text)
{
	PROC_ID id = getProcByString(key.c_str());
	if (id.cluster <= 0 || id.proc < 0) {
		dprintf(D_FULLDEBUG, "Remove: Failed to parse id: %s\n", key.c_str());
		text = "Invalid Id";
		return false;
	}

	if (!abortJob(id.cluster,
				  id.proc,
				  reason.c_str(),
				  true // Always perform within a transaction
				  )) {
		text = "Failed to remove job";
		return false;
	}

	return true;
}

bool
SchedulerObject::suspend(std::string key, std::string &/*reason*/, std::string &text)
{
	PROC_ID id = getProcByString(key.c_str());
	if (id.cluster <= 0 || id.proc < 0) {
		dprintf(D_FULLDEBUG, "Remove: Failed to parse id: %s\n", key.c_str());
		text = "Invalid Id";
		return false;
	}

	scheduler.enqueueActOnJobMyself(id,JA_SUSPEND_JOBS,true);

	return true;
}

bool
SchedulerObject::_continue(std::string key, std::string &/*reason*/, std::string &text)
{
	PROC_ID id = getProcByString(key.c_str());
	if (id.cluster <= 0 || id.proc < 0) {
		dprintf(D_FULLDEBUG, "Remove: Failed to parse id: %s\n", key.c_str());
		text = "Invalid Id";
		return false;
	}

	scheduler.enqueueActOnJobMyself(id,JA_CONTINUE_JOBS,true);

	return true;
}

