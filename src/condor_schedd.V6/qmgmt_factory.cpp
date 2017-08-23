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
#include "set_user_priv_from_ad.h"
#include "my_async_fread.h"

#if defined(HAVE_DLOPEN) || defined(WIN32)
#include "ScheddPlugin.h"
#endif

#if defined(HAVE_GETGRNAM)
#include <sys/types.h>
#include <grp.h>
#endif


class JobFactory : public SubmitHash {

public:
	JobFactory(const char * name, int id);
	~JobFactory();

	enum PauseCode { InvalidSubmit=-1, Running=0, Hold=1, NoMoreItems=2, ClusterRemoved=3, };

	// load the submit file/digest that was passed in our constructor
	int LoadDigest(MacroStream & ms, std::string & errmsg);
	// load the item data for the given row, and setup the live submit variables for that item.
	int LoadRowData(int row, std::string * empty_var_names=NULL);
	// returns true when the row data is not yet loaded, but might be available if you try again later.
	bool RowDataIsLoading(int row);

	bool IsResumable() { return paused >= Running && paused < ClusterRemoved; }
	int Pause(PauseCode pause_code) { 
		if (IsResumable()) {
			if (pause_code && (pause_code > paused)) paused = pause_code;
		}
		return paused;
	}
	int Resume(PauseCode pause_code) {
		if (IsResumable()) {
			if (paused && pause_code == paused) paused = Running;
		}
		return paused;
	}
	bool IsPaused() { return paused != Running; }
	bool IsComplete() { return paused > Hold; }
	const char * Name() { return name ? name : "<empty>"; }
	int ID() { return ident; }
	int PauseMode() { return paused; }

	bool NoItems() { return fea.foreach_mode == foreach_not; }
	int StepSize() { return fea.queue_num; }
	// advance from the input row until the next selected row. return  < 0 if no more selected rows.
	int NextSelectedRow(int row) {
		if (fea.foreach_mode == foreach_not) return -1;
		int num_rows = fea.items.number();
		while (++row < num_rows) {
			if (fea.slice.selected(row, num_rows)) {
				return row;
			}
		}
		return -1;
	}
	// returns the first row selected by the slice (if any)
	int FirstSelectedRow() {
		if (fea.foreach_mode == foreach_not) return 0;
		int num_rows = (fea.foreach_mode == foreach_from_async) ? INT_MAX : fea.items.number();
		if (num_rows <= 0) return -1;
		if (fea.slice.selected(0, num_rows))
			return 0;
		return NextSelectedRow(0);
	}

	// calculate the number of rows selected by the slice
	int TotalProcs(bool & changed_value) {
		changed_value = false;
		if (cached_total_procs == -42) {
			int selected_rows = 1;
			if (fea.foreach_mode != foreach_not) {
				int num_rows = fea.items.number();
				selected_rows = 0;
				for (int row = 0; row < num_rows; ++row) {
					if (fea.slice.selected(row, num_rows)) {
						++selected_rows;
					}
				}
			}
			changed_value = true;
			cached_total_procs = StepSize() * selected_rows;
		}
		return cached_total_procs;
	}

protected:
	const char * name;
#ifdef HOLDS_DIGEST_FILE_OPEN
	//FILE * fp_digest;
#endif
	int          ident;
	PauseCode    paused; // 0 is not paused, non-zero is pause code.
	MACRO_SOURCE source;
	MyAsyncFileReader reader; // used when there is external foreach data
	SubmitForeachArgs fea;
	char emptyItemString[4];
	int cached_total_procs;

	// let these functions access internal factory data
	friend JobFactory * MakeJobFactory(int cluster_id, const char * submit_digest_text);
	friend JobFactory * MakeJobFactory(JobQueueCluster* job, const char * submit_digest_filename);

	// we override this so that we can use an async foreach implementation.
	int  load_q_foreach_items(
		FILE* fp_submit, MACRO_SOURCE& source, // IN: submit file and source information, used only if the items are inline.
		SubmitForeachArgs & o,           // OUT: options & items from parsing the queue args
		std::string & errmsg);           // OUT: error message if return value is not 0
};

JobFactory::JobFactory(const char * _name, int id)
	: name(NULL)
#ifdef HOLDS_DIGEST_FILE_OPEN
	, fp_digestX(NULL)
#endif
	, ident(id)
	, paused(InvalidSubmit)
	, cached_total_procs(-42)
{
	memset(&source, 0, sizeof(source));
	this->init();
	setScheddVersion(CondorVersion());
	// add digestfile into string pool, and store that pointer in the class
	insert_source(_name, source);
	name = macro_source_filename(source, SubmitMacroSet);
	// make sure that the string buffer for empty items is really empty.
	memset(emptyItemString, 0, sizeof(emptyItemString));
}

JobFactory::~JobFactory()
{
#ifdef HOLDS_DIGEST_FILE_OPEN
	if (fp_digest) { fclose(fp_digest); fp_digest = NULL; }
#endif
}

// called in CommitTransaction after the commit
//
int PostCommitJobFactoryProc(JobQueueCluster * cluster, JobQueueJob * /*job*/)
{
	if ( ! cluster || ! cluster->factory)
		return -1;
	//PRAGMA_REMIND("verify jid.cluster matches the factory cluster.  do we need to do anything here?")
	return 0;
}


// called by DecrementClusterSize to ask if the job factory wants
// to defer cleanup of the cluster.
//
bool JobFactoryAllowsClusterRemoval(JobQueueCluster * cluster)
{
	if ( ! cluster || ! cluster->factory)
		return true;
	if (cluster->factory->IsComplete()) {
		return true;
	}

	return false;
}

// Make a job factory for a job object that has been submitted, but not yet committed.
// this is the normal case for condor_submit from 8.7.3 onward.
class JobFactory * MakeJobFactory(int cluster_id, const char * submit_digest_text)
{
	JobFactory * factory = new JobFactory(NULL, cluster_id);

	MacroStreamMemoryFile ms(submit_digest_text, -1, factory->source);

	std::string errmsg = "";
	int rval = factory->LoadDigest(ms, errmsg);
	if (rval) {
		dprintf(D_ALWAYS, "failed to parse submit digest for factory %d : %s\n", cluster_id, errmsg.c_str());
		delete factory;
		factory = NULL;
	}

	return factory;
}

// Make a job factory for a Job object that exists, this entry point is used when
// the submit digest is a file on disk - either because condor_submit put it there, or
// because we are restarting. 
JobFactory * MakeJobFactory(JobQueueCluster* job, const char * submit_digest_file)
{

	JobFactory * factory = new JobFactory(submit_digest_file, job->jid.cluster);

	// Starting the 8.7.3 The digest file may be in the spool directory and owned by condor
	// or in the user directory and owned by the user (the 8.7.1 model). We will handle
	// both cases by first trying to open the file as condor, and if that fails, try impersonating
	// the user.
	bool restore_priv = false;
	priv_state priv = PRIV_UNKNOWN;

	FILE* fp = safe_fopen_wrapper_follow(submit_digest_file, "r");
	// if we can't open the file, try impersonating the user
	if ( ! fp) {
		restore_priv = true;
		priv = set_user_priv_from_ad(*job);
		fp = safe_fopen_wrapper_follow(submit_digest_file, "r");
	}

	std::string errmsg = "";
	int rval;
	if ( ! fp) {
		formatstr(errmsg, "Failed to open factory submit digest : %s", strerror(errno));
		rval = errno ? errno : -1;
	} else {
		MacroStreamYourFile ms(fp, factory->source);
		rval = factory->LoadDigest(ms, errmsg);
		fclose(fp);
		fp = NULL;
	}

	if (restore_priv) {
		set_priv(priv);
		uninit_user_ids();
	}

	if (rval) {
		dprintf(D_ALWAYS, "failed to load job factory %d submit digest %s : %s\n", job->jid.cluster, submit_digest_file, errmsg.c_str());
		delete factory;
		factory = NULL;
	} else {
		factory->set_cluster_ad(job);
	}
	return factory;
}

void AttachJobFactoryToCluster(JobFactory * factory, JobQueueCluster * cad)
{
	ASSERT( ! cad->factory);
	cad->factory = factory;
	factory->set_cluster_ad(cad);
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


int  PauseJobFactory(JobFactory * factory, int pause_code)
{
	if ( ! factory)
		return -1;
	dprintf(D_ALWAYS, "Pausing job factory %d %s code=%d\n", factory->ID(), factory->Name(), pause_code);
	JobFactory::PauseCode code = pause_code < 0 ? JobFactory::InvalidSubmit : JobFactory::Hold;
	if (pause_code >= 3) code = JobFactory::ClusterRemoved;
	return factory->Pause(code);
}

int  ResumeJobFactory(JobFactory * factory, int pause_code)
{
	if ( ! factory)
		return -1;
	JobFactory::PauseCode code = pause_code < 0 ? JobFactory::InvalidSubmit : JobFactory::Hold;
	if (pause_code >= 3) code = JobFactory::ClusterRemoved;
	int rval = factory->Resume(code);
	dprintf(D_ALWAYS, "Attempted to Resume job factory %d %s code=%d resumed=%d\n", factory->ID(), factory->Name(), pause_code, rval==0);
	return rval;
}

// reutrns true if cluster has a non-paused, non-complete factory
bool CanMaterializeJobs(JobQueueCluster * cad)
{
	if ( ! cad || ! cad->factory) return false;
	// since IsComplete implies IsPaused with a special pause code, we can just check the pause state here...
	return ! cad->factory->IsPaused();
}

bool JobFactoryIsComplete(JobQueueCluster * cad)
{
	if ( ! cad || ! cad->factory) return true;
	return cad->factory->IsComplete();
}
bool JobFactoryIsRunning(JobQueueCluster * cad)
{
	if ( ! cad || ! cad->factory) return true;
	return cad->factory->PauseMode() == JobFactory::Running;
}


// returns true if the factory changed state, false otherwise.
bool CheckJobFactoryPause(JobFactory * factory, int want_pause)
{
	if ( ! factory)
		return false;

	int paused = factory->IsPaused() ? 1 : 0;

	dprintf(D_MATERIALIZE, "in CheckJobFactoryPause for job factory %d %s want_pause=%d is_paused=%d (%d)\n",
			factory->ID(), factory->Name(), want_pause, factory->IsPaused(), factory->PauseMode());

	if (paused == want_pause) {
		return true;
	}

	dprintf(D_MATERIALIZE, "CheckJobFactoryPause %s job factory %d %s code=%d\n",
			want_pause ? "Pausing" : "Resuming", factory->ID(), factory->Name(), want_pause);

	if (want_pause) {
		// we should only get here if the factory is in running state.
		return factory->Pause(JobFactory::Hold) != JobFactory::Running;
	} else {
		// this will only resume a natural hold, not a failure or end condition pause
		return factory->Resume(JobFactory::Hold) == JobFactory::Running;
	}
}

int  MaterializeNextFactoryJob(JobFactory * factory, JobQueueCluster * ClusterAd, int & retry_delay)
{
	retry_delay = 0;
	if (factory->IsPaused()) {
		// if the factory is paused but resumable, return a large value for the retry delay.
		// in practice, this value really means "don't use a timer to retry, use a transaction trigger on the pause state to retry"
		if (factory->IsResumable()) { retry_delay = 300; }
		dprintf(D_MATERIALIZE, "in MaterializeNextFactoryJob for cluster=%d, Factory is paused (%d)\n", ClusterAd->jid.cluster, factory->PauseMode());
		return 0;
	}

	dprintf(D_MATERIALIZE | D_VERBOSE, "in MaterializeNextFactoryJob for cluster=%d, Factory is running\n", ClusterAd->jid.cluster);

	int step_size = factory->StepSize();
	if (step_size <= 0) {
		dprintf(D_ALWAYS, "WARNING - step size is %d for job materialization of cluster %d, using 1 instead\n", step_size, ClusterAd->jid.cluster);
		step_size = 1;
	}

// ATTR_JOB_MATERIALIZE_ITEM_COUNT   "JobMaterializeItemCount"
// ATTR_JOB_MATERIALIZE_STEP_SIZE    "JobMaterializeStepSize"
// ATTR_JOB_MATERIALIZE_NEXT_PROC_ID "JobMaterializeNextProcId"

	int next_proc_id = 0;
	int rval = GetAttributeInt(ClusterAd->jid.cluster, ClusterAd->jid.proc, ATTR_JOB_MATERIALIZE_NEXT_PROC_ID, &next_proc_id);
	if (rval < 0 || next_proc_id < 0) {
		dprintf(D_ALWAYS, "ERROR - " ATTR_JOB_MATERIALIZE_NEXT_PROC_ID " is not set, aborting materalize for cluster %d\n", ClusterAd->jid.cluster);
		return rval;
	}

	int step = next_proc_id % step_size;
	int item_index = 0;
	bool no_items = factory->NoItems();
	if (no_items) {
		if (next_proc_id >= step_size) {
			dprintf(D_MATERIALIZE | D_VERBOSE, "Materialize for cluster %d is done. has_items=%d, next_proc_id=%d, step=%d\n", ClusterAd->jid.cluster, !no_items, next_proc_id, step_size);
			// we are done
			factory->Pause(JobFactory::NoMoreItems);
			return 0;
		}
	} else {
		// item index is optional, if missing, the value is 0 and the item is the empty string.
		rval = GetAttributeInt(ClusterAd->jid.cluster, ClusterAd->jid.proc, ATTR_JOB_MATERIALIZE_NEXT_ROW, &item_index);
		if (rval < 0) {
			item_index = next_proc_id / step_size;
			if (item_index > 0 && next_proc_id != step_size) {
				// ATTR_JOB_MATERIALIZE_NEXT_ROW must exist in the cluster ad once we are done with proc 0
				dprintf(D_ALWAYS, "ERROR - " ATTR_JOB_MATERIALIZE_NEXT_ROW " is not set, aborting materialize for job %d.%d step=%d, row=%d\n", ClusterAd->jid.cluster, next_proc_id, step_size, item_index);
				// we are done
				return 0;
			}
			item_index = factory->FirstSelectedRow();
		}
	}
	if (item_index < 0) {
		// we are done
		dprintf(D_MATERIALIZE | D_VERBOSE, "Materialize for cluster %d is done. JobMaterializeNextRow is %d\n", ClusterAd->jid.cluster, item_index);
		factory->Pause(JobFactory::NoMoreItems);
		return 0; 
	}
	int row  = item_index;
	
	// if the row data is still loading, return 0 and a small value for the retry delay
	if (factory->RowDataIsLoading(row)){
		retry_delay = 1;
		return 0;
	}

	JOB_ID_KEY jid(ClusterAd->jid.cluster, next_proc_id);
	dprintf(D_ALWAYS, "Trying to Materializing new job %d.%d step=%d row=%d\n", jid.cluster, jid.proc, step, row);

	bool check_empty = true;
	bool fail_empty = false;
	std::string empty_var_names;
	int row_num = factory->LoadRowData(row, check_empty ? &empty_var_names : NULL);
	if (row_num < row) {
		// we are done
		dprintf(D_MATERIALIZE | D_VERBOSE, "Materialize for cluster %d is done. LoadRowData returned %d for row %d\n", ClusterAd->jid.cluster, row_num, row);
		factory->Pause(JobFactory::NoMoreItems);
		return 0; 
	}
	// report empty vars.. do we still want to do this??
	if ( ! empty_var_names.empty()) {
		if (fail_empty) {
			dprintf(D_ALWAYS, "Failing Materialize of job %d.%d row=%d because %s have empty values\n", jid.cluster, jid.proc, step, empty_var_names.c_str());
			return -1;
		}
	}

	bool already_in_transaction = InTransaction();
	if ( ! already_in_transaction) {
		BeginTransaction();
	}

	SetAttributeInt(ClusterAd->jid.cluster, ClusterAd->jid.proc, ATTR_JOB_MATERIALIZE_NEXT_PROC_ID, next_proc_id+1);
	if ( ! no_items && (step+1 == step_size)) {
		int next_row = factory->NextSelectedRow(row);
		if (next_row != item_index) {
			SetAttributeInt(ClusterAd->jid.cluster, ClusterAd->jid.proc, ATTR_JOB_MATERIALIZE_NEXT_ROW, next_row);
		}
	}

	// Calculate total submit procs taking the slice into acount. the refresh_in_ad bool will be set to
	// true only once in the lifetime of this instance of the factory
	bool refresh_in_ad = false;
	int total_procs = factory->TotalProcs(refresh_in_ad);
	if (refresh_in_ad) {
		SetAttributeInt(ClusterAd->jid.cluster, ClusterAd->jid.proc, ATTR_TOTAL_SUBMIT_PROCS, total_procs);
	}

	// have the factory make a job and give us a pointer to it.
	// note that this ia not a transfer of ownership, the factory still owns the job and will delete it
	const classad::ClassAd * job = factory->make_job_ad(jid, row, step, false, false, factory_check_sub_file, NULL);
	if ( ! job) {
		std::string errmsg(factory->error_stack()->getFullText());
		dprintf(D_ALWAYS, "ERROR: factory->make_job_ad() for %d.%d failed : %s\n", jid.cluster, jid.proc, errmsg.c_str());
		rval = -1; // failed to instantiate.
	} else {
		rval = NewProcFromAd(job, jid.proc, ClusterAd, 0);
		factory->delete_job_ad();
	}
	if (rval < 0) {
		if ( ! already_in_transaction) {
			AbortTransaction();
		}
		return rval; // failed instantiation
	}

	if( !already_in_transaction ) {
		rval = CommitTransaction();
		if (rval < 0) {
			dprintf(D_ALWAYS, "CommitTransaction() Failed for job %d.%d rval=%d\n", jid.cluster, jid.proc, rval);
			return rval;
		}
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

#if 0 // this is obsolete
int JobFactory::load_q_foreach_items(
	FILE* fp_submit, MACRO_SOURCE& source, // IN: submit file and source information, used only if the items are inline.
	SubmitForeachArgs & o,                 // OUT: options & items from parsing the queue args
	std::string & errmsg)                  // OUT: error message if return value is not 0
{
	// TODO: make this setup an asych foreach reader
	return SubmitHash::load_q_foreach_items(fp_submit, source, o, errmsg);
}
#endif

int JobFactory::LoadDigest(MacroStream &ms, std::string & errmsg)
{
	char * qline = NULL;
	int rval = parse_up_to_q_line(ms, errmsg, &qline);
	if (rval == 0 && qline) {
		const char * pqargs = is_queue_statement(qline);
		rval = parse_q_args(pqargs, fea, errmsg);
		if (rval == 0) {
			// establish live buffers for $(Cluster) and $(Process), and other loop variables
			// Because the user might already be using these variables, we can only set the explicit ones
			// unconditionally, the others we must set only when not already set by the user.
			set_live_submit_variable(SUBMIT_KEY_Cluster, LiveClusterString);
			set_live_submit_variable(SUBMIT_KEY_Process, LiveProcessString);
	
			if (fea.vars.isEmpty()) {
				set_live_submit_variable("item", emptyItemString);
			} else {
				for (const char * var = fea.vars.first(); var != NULL; var = fea.vars.next()) {
					set_live_submit_variable(var, emptyItemString, false);
				}
			}

			// optimize the submit hash for lookups if we inserted anything.  we expect this to happen only once.
			this->optimize();

			// load the inline foreach data, and check to see if there is external foreach data
			rval = load_inline_q_foreach_items(ms, fea, errmsg);
			if (rval == 1) {
				rval = 0;
				// there is external foreach data.  it had better be from a file, because that's the only form we support
				if (fea.foreach_mode != foreach_from) {
					formatstr(errmsg, "foreach mode %d is not supported by schedd job factory", fea.foreach_mode);
					rval = -1;
				} else if (fea.items_filename.empty()) {
					// this is ok?
				} else if (fea.items_filename == "<" || fea.items_filename == "=") {
					formatstr(errmsg, "invalid filename '%s' for foreach from", fea.items_filename.Value());
				} else {
					// setup an async reader for the itemdata
					rval = reader.open(fea.items_filename.c_str());
					if (rval) {
						formatstr(errmsg, "could not open item data file '%s', error = %d\n", fea.items_filename.Value(), rval);
					} else {
						rval = reader.queue_next_read();
						if (rval) {
							formatstr(errmsg, "could not initiate reading from item data file '%s', error = %d\n", fea.items_filename.Value(), rval);
						} else {
							fea.foreach_mode = foreach_from_async;
						}
					}
					// failed to open or start the item reader, so just close it and free the internal buffers.
					if (rval) { reader.clear(); }
				}
			}
		}
	}

	paused = rval ? InvalidSubmit : Running;

	return rval;
}

// returns true when the row data is not yet loaded, but might be available if you try again later.
bool JobFactory::RowDataIsLoading(int row)
{
	//PRAGMA_REMIND("TODO: keep only unused items in the item list.")

	if (fea.foreach_mode == foreach_from_async) {

		// put all of the items we have read so far into the item list
		MyString str;
		while (reader.output().readLine(str, false)) {
			str.trim();
			fea.items.append(str.c_str());
		}

		if (reader.done_reading()) {
			if ( ! reader.eof_was_read()) {
				std::string filename;
				dprintf(D_ALWAYS, "failed to read all of the item data from '%s', error %d\n", fea.items_filename.Value(), reader.error_code());
			}
			// we have read all we are going to, so close the reader now.
			reader.close();
			reader.clear();
			fea.foreach_mode = foreach_from;
		} else if (fea.items.number() < row) {
			// not done reading, and also haven't read this row. return true that row data is still loading.
			reader.check_for_read_completion();
			return true;
		}
	}

	return false;
}

// set live submit variables for the row data for the given row. If empty_var_names is not null
// it is set to the names of the item varibles that have empty values. (so warnings or errors can be given)
int JobFactory::LoadRowData(int row, std::string * empty_var_names /*=NULL*/)
{
	ASSERT(fea.foreach_mode != foreach_not || row == 0);

	const char* token_seps = ", \t";
	const char* token_ws = " \t";

	int loaded_row = row;
	char * item = emptyItemString;
	if (fea.foreach_mode != foreach_not) {
		loaded_row = 0;
		item = fea.items.first();
		//PRAGMA_REMIND("tj: need a stable iterator that keeps track of current pos, but can also seek.")
		for (int ix = 1; ix <= row; ++ix) {
			item = fea.items.next();
			if (item) {
				loaded_row = ix;
			} else {
				break;
			}
		}
	}

	// If there are loop variables, destructively tokenize item and stuff the tokens into the submit hashtable.
	if ( ! item) { item = emptyItemString; }

	if (fea.vars.isEmpty()) {
		set_live_submit_variable("item", item, true);
		return loaded_row;
	}


	// set the first loop variable unconditionally, we set it initially to the whole item
	// we may later truncate that item when we assign fields to other loop variables.
	fea.vars.rewind();
	char * var = fea.vars.next();
	char * data = item;
	set_live_submit_variable(var, data, false);

	// if there is more than a single loop variable, then assign them as well
	// we do this by destructively null terminating the item for each var
	// the last var gets all of the remaining item text (if any)
	while ((var = fea.vars.next())) {
		// scan for next token separator
		while (*data && ! strchr(token_seps, *data)) ++data;
		// null terminate the previous token and advance to the start of the next token.
		if (*data) {
			*data++ = 0;
			// skip leading separators and whitespace
			while (*data && strchr(token_ws, *data)) ++data;
			set_live_submit_variable(var, data, false);
		}
	}

	if (empty_var_names) {
		fea.vars.rewind();
		empty_var_names->clear();
		while ((var = fea.vars.next())) {
			MACRO_ITEM* pitem = lookup_exact(var);
			if ( ! pitem || (pitem->raw_value != emptyItemString && 0 == strlen(pitem->raw_value))) {
				if ( ! empty_var_names->empty()) (*empty_var_names) += ",";
				(*empty_var_names) += var;
			}
		}
	}

	return loaded_row;
}


