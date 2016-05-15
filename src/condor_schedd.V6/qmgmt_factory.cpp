/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_io.h"
#include "string_list.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_daemon_core.h"

#include "basename.h"
#include "qmgmt.h"
#include "condor_qmgr.h"
#include "log.h"
#include "classad_collection.h"
#include "prio_rec.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "condor_adtypes.h"
#include "spooled_job_files.h"
#include "scheduler.h"	// for shadow_rec definition
#include "dedicated_scheduler.h"
#include "condor_email.h"
#include "condor_universe.h"
#include "globus_utils.h"
#include "env.h"
#include "condor_classad.h"
#include "condor_ver_info.h"
#include "condor_string.h" // for strnewp, etc.
#include "utc_time.h"
#include "condor_crontab.h"
#include "forkwork.h"
#include "condor_open.h"
#include "ickpt_share.h"
#include "classadHistory.h"
#include "directory.h"
#include "filename_tools.h"
#include "spool_version.h"
#include "condor_holdcodes.h"
#include "nullfile.h"
#include "condor_url.h"
#include "classad/classadCache.h"
#include <param_info.h>
#include "condor_version.h"
#include "submit_utils.h"

#if defined(HAVE_DLOPEN) || defined(WIN32)
#include "ScheddPlugin.h"
#endif

#if defined(HAVE_GETGRNAM)
#include <sys/types.h>
#include <grp.h>
#endif

extern int NewProc2(int cluster_id, int proc_id); // in qmgmt


class JobFactory : public SubmitHash {

public:
	JobFactory(const char * subfile);
	~JobFactory();

	enum PauseCode { InvalidSubmit=-1, Running=0, ClusterRemoved=1, };

	int LoadSubmit(std::string & errmsg);
	int SetNexProcId(int id) { highest_proc_id = MAX(id, highest_proc_id); return highest_proc_id; }
	int NextProcId() { return ++highest_proc_id; }
	int Pause(PauseCode pause_code) { if (pause_code && ! paused) paused = pause_code; return paused; }
	int Resume(PauseCode pause_code) { if (paused && pause_code == paused) paused = Running; return paused; }
	bool IsPaused() { return paused != Running; }
	const char * Name() { return submit_file ? submit_file : "<empty>"; }

protected:
	const char * submit_file;
	FILE * fp_submit;
	int    highest_proc_id;
	PauseCode    paused; // 0 is not paused, non-zero is pause code.
	MACRO_SOURCE source;
	SubmitForeachArgs fea;
};

JobFactory::JobFactory(const char * subfile)
	: submit_file(NULL)
	, fp_submit(NULL)
	, highest_proc_id(-1)
	, paused(InvalidSubmit)
{
	memset(&source, 0, sizeof(source));
	this->init();
	setScheddVersion(CondorVersion());
	insert_source(subfile, source);
	submit_file = macro_source_filename(source, SubmitMacroSet);
}

JobFactory::~JobFactory()
{
	if (fp_submit) {
		fclose(fp_submit);
		fp_submit = NULL;
	}
}

int  CommitJobFactoryProcId(JobFactory * factory, JOB_ID_KEY & jid)
{
	if ( ! factory)
		return -1;
	PRAGMA_REMIND("verify jid.cluster matches the factory cluster")
	return factory->SetNexProcId(jid.proc);
}

JobFactory * MakeJobFactory(JobQueueJob* job, const char * submit_file)
{
	JobFactory * factory = new JobFactory(submit_file);
	std::string errmsg = "";
	int rval = factory->LoadSubmit(errmsg);
	if (rval) {
		dprintf(D_ALWAYS, "failed to load JobFactory submit file %s : %s\n", submit_file, errmsg.c_str());
	} else {
		factory->set_cluster_ad(job);
	}
	return factory;
}

void DestroyJobFactory(JobFactory * factory)
{
	delete factory;
}

static int factory_check_sub_file(void*, SubmitHash *, _submit_file_role, const char *, int)
{
	// do we want to do anything here when loading factory jobs
	return 0;
}

bool is_forced_proc_attr(const char * name, bool & send)
{
	static const char * const attrs[] = {
		ATTR_JOB_STATUS,
		NULL, // items below this are NEVER send to ProcAd, items above are Always
		ATTR_PROC_ID,
		ATTR_OWNER,
	};
	send = true;
	for (size_t ii = 0; ii < COUNTOF(attrs); ++ii) {
		if ( ! attrs[ii]) { send = false; continue; }
		if (MATCH == strcasecmp(name, attrs[ii]))
			return true;
	}
	return false;
}

// send the differences between a Job classad and it's associated ClusterAd to the job queue.
static int send_proc_ad (ClassAd * job, int ProcId, JobQueueJob * ClusterAd, SetAttributeFlags_t flags)
{
	int ClusterId = ClusterAd->jid.cluster;
	ExprTree *tree = NULL;
	const char *attr;
	std::string buffer;

	NewProc2(ClusterId, ProcId);

	int rval = SetAttributeInt(ClusterId, ProcId, ATTR_PROC_ID, ProcId, flags | SetAttribute_LateInstantiation);
	if (rval < 0) {
		dprintf(D_ALWAYS, "ERROR: Failed to set ProcId=%d for job %d.%d (%d)\n", ProcId, ClusterId, ProcId, errno );
		return rval;
	}

	job->ResetExpr();
	while( job->NextExpr(attr, tree) ) {
		if ( ! attr || ! tree) {
			dprintf(D_ALWAYS, "ERROR: Null attribute name or value for job %d.%d\n", ClusterId, ProcId );
			return -1;
		}

		bool send_it = false;
		if ( ! is_forced_proc_attr(attr, send_it)) {
			// If we aren't going to unconditionally send it, when we check if the value
			// matches the value in the cluster ad.
			// If the values match, don't add the attribute to this proc ad
			ExprTree *cluster_tree = ClusterAd->LookupExpr(attr);
			send_it = ! cluster_tree || ! (*tree == *cluster_tree);
		}
		if ( ! send_it)
			continue;

		buffer.clear();
		const char * rhstr = ExprTreeToString(tree, buffer);
		if ( ! rhstr ) { 
			dprintf(D_ALWAYS, "ERROR: Null value for job %d.%d\n", ClusterId, ProcId );
			return -1;
		} else {
			rval = SetAttribute(ClusterId, ProcId, attr, rhstr, flags | SetAttribute_LateInstantiation);
			if (rval < 0) {
				dprintf(D_ALWAYS, "ERROR: Failed to set %s=%s for job %d.%d (%d)\n", attr, rhstr, ClusterId, ProcId, errno );
				return rval;
			}
		}
	}

	return 0;
}

int  PauseJobFactory(JobFactory * factory, int pause_code)
{
	if ( ! factory)
		return -1;
	dprintf(D_ALWAYS, "Pausing job factory %s code=%d\n", factory->Name(), pause_code);
	return factory->Pause(pause_code < 0 ? JobFactory::InvalidSubmit : JobFactory::ClusterRemoved);
}

int  ResumeJobFactory(JobFactory * factory, int pause_code)
{
	if ( ! factory)
		return -1;
	int rval = factory->Resume(pause_code < 0 ? JobFactory::InvalidSubmit : JobFactory::ClusterRemoved);
	dprintf(D_ALWAYS, "Attempted to Resume job factory %s code=%d resumed=%d\n", factory->Name(), pause_code, rval==0);
	return rval;
}

int  MaterializeNextFactoryJob(JobFactory * factory, JobQueueJob * ClusterAd, int /*exiting_proc_id*/)
{
	if (factory->IsPaused()) {
		return 0;
	}

	int proc_id_limit = 0, step_size = 1;
	if ( ! ClusterAd->LookupInteger("JobFactoryNumProcs", proc_id_limit) ||
		ClusterAd->NumProcs() >= proc_id_limit) {
		return 0; // nothing to instantiate.
	}

	if ( ! ClusterAd->LookupInteger("JobFactoryStepSize", step_size)) { step_size = 1; }

	JOB_ID_KEY jid(ClusterAd->jid.cluster, factory->NextProcId());

	dprintf(D_ALWAYS, "Materializing new job %d.%d\n", jid.cluster, jid.proc);

	PRAGMA_REMIND("here we would stuff the hashtable with live submit variable values for the next job.")

	int step = jid.proc % step_size;
	int row = jid.proc / step_size;

	ClassAd * job = factory->make_job_ad(jid, row, step, false, false, factory_check_sub_file, NULL);
	if ( ! job) {
		return -1; // failed to instantiate.
	}

	int rval = send_proc_ad(job, jid.proc, ClusterAd, 0);
	factory->delete_job_ad();
	if (rval < 0) {
		return rval;
	}
	return 1; // successful instantiation.
}

// Check to see if this is a queue statement, if it is, return a pointer to the queue arguments.
// 
static const char * is_queue_statement(const char * line)
{
	const int cchQueue = sizeof("queue")-1;
	if (starts_with_ignore_case(line, "queue") && (0 == line[cchQueue] || isspace(line[cchQueue]))) {
		const char * pqargs = line+cchQueue;
		while (*pqargs && isspace(*pqargs)) ++pqargs;
		return pqargs;
	}
	return NULL;
}

int JobFactory::LoadSubmit(std::string & errmsg)
{
	if (fp_submit) {
		fclose(fp_submit);
		fp_submit = NULL;
	}

	FILE* fp_submit = safe_fopen_wrapper_follow(submit_file,"r");
	if ( ! fp_submit ) {
		formatstr(errmsg, "Failed to open factory submit file (%s)", strerror(errno));
		paused = InvalidSubmit;
		return errno;
	}

	char * qline = NULL;
	int rval = parse_file_up_to_q_line(fp_submit, source, errmsg, &qline);
	if (rval == 0 && qline) {
		const char * pqargs = is_queue_statement(qline);
		rval = parse_q_args(pqargs, fea, errmsg);
	}

	paused = rval ? InvalidSubmit : Running;

	return rval;
}
