/***************************************************************
 *
 * Copyright (C) 1990-2019, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_debug.h"
#include "subsystem_info.h"
#include "env.h"
#include "basename.h"
#include "condor_getcwd.h"
#include <time.h>
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "condor_distribution.h"
#include "condor_ver_info.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#endif
#include "match_prefix.h"

#include "directory.h"
#include "filename_tools.h"
#include <scheduler.h>
#include "condor_version.h"
#include <my_async_fread.h>
#include <submit_utils.h>
#include <param_info.h> // for BinaryLookup

#include "list.h"

#include <algorithm>
#include <string>
#include <set>

/* =============================== NOTE NOTE NOTE NOTE ! ====================================

  The JobFactory class in this file is a lightly modified copy of the one in the schedd
  It was modified to facilitate turning it into a simple test excutable so that that
  idea of late materialization can be tested and proven.

  !!! THIS IS NOT THE CANONICAL VERSION OF LATE MATERIALIZATION !!!

  The JobFactory class in the schedd should be considered the definitive version. If it
  differs any any material way from this file, then this file is wrong and should be updated.

  TODO: refactor the JobFactory in the schedd so that it can be used by the schedd and
  also by this test code.  Then we could delete 90% of this file...

 ============================================================================================ */

// if pause_code < 0, pause is permanent, if >= 3, cluster was removed
typedef enum {
	mmInvalid = -1, // some fatal error occurred, such as failing to load the submit digest.
	mmRunning = 0,
	mmHold = 1,
	mmNoMoreItems = 2,
	mmClusterRemoved = 3
} MaterializeMode;


bool verbose = false;


class JobFactory : public SubmitHash {

public:
	JobFactory(const char * name, int id);
	~JobFactory();

	//enum PauseCode { InvalidSubmit=-1, Running=0, Hold=1, NoMoreItems=2, ClusterRemoved=3, };

	// load the submit file/digest that was passed in our constructor
	int LoadDigest(MacroStream & ms, int cluster_id, StringList & items, std::string & errmsg);
	int LoadDigest(FILE* fp, int cluster_id, StringList & items, std::string & errmsg);
	// load the item data for the given row, and setup the live submit variables for that item.
	int LoadRowData(int row, std::string * empty_var_names = NULL);
	// returns true when the row data is not yet loaded, but might be available if you try again later.
	bool RowDataIsLoading(int row);

	bool IsResumable() { return paused >= mmRunning && paused < mmClusterRemoved; }
	int Pause(MaterializeMode pause_code) {
		if (IsResumable()) {
			if (pause_code && (pause_code > paused)) paused = pause_code;
		}
		return paused;
	}
	int Resume(MaterializeMode pause_code) {
		if (IsResumable()) {
			if (paused && pause_code == paused) paused = mmRunning;
		}
		return paused;
	}
	bool IsPaused() { return paused != mmRunning; }
	bool IsComplete() { return paused > mmHold; }
	int PauseMode() { return paused; }

	const char * Name() { return name ? name : "<empty>"; }
	int ID() const { return ident; }

	bool NoItems() const { return fea.foreach_mode == foreach_not; }
	int StepSize() const { return fea.queue_num; }
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
	int          ident;
	MaterializeMode paused; // 0 is not paused, non-zero is pause code.
	MACRO_SOURCE source;
	MyAsyncFileReader reader; // used when there is external foreach data
	SubmitForeachArgs fea;
	char emptyItemString[4];
	int cached_total_procs;

	// let these functions access internal factory data
	friend int AppendRowsToJobFactory(JobFactory *factory, char * buf, size_t cbbuf, std::string & remainder);
	friend bool CheckJobFactoryPause(JobFactory * factory, int want_pause);

	// we override this so that we can use an async foreach implementation.
	int  load_q_foreach_items(
		FILE* fp_submit, MACRO_SOURCE& source, // IN: submit file and source information, used only if the items are inline.
		SubmitForeachArgs & o,           // OUT: options & items from parsing the queue args
		std::string & errmsg);           // OUT: error message if return value is not 0
};

JobFactory::JobFactory(const char * _name, int id)
	: name(NULL)
	, ident(id)
	, paused(mmInvalid)
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
	if (! item) { item = emptyItemString; }

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

	// check for the use of US as a field separator
	// if we find one, then use that instead of the default token separator
	char * pus = strchr(data, '\x1F');
	if (pus) {
		char * pend = data + strlen(data);
		// remove trailing \r\n
		if (pend > data && pend[-1] == '\n') { *--pend = 0; }
		if (pend > data && pend[-1] == '\r') { *--pend = 0; }

		for (;;) {
			*pus = 0;
			// trim token separator and also trailing whitespace
			char * endp = pus - 1;
			while (endp >= data && (*endp == ' ' || *endp == '\t')) *endp-- = 0;
			if (! var) break;

			// advance to the next field and skip leading whitespace
			data = pus;
			if (data < pend) ++data;
			while (*data == ' ' || *data == '\t') ++data;
			pus = strchr(data, '\x1F');
			var = fea.vars.next();
			if (var) {
				set_live_submit_variable(var, data, false);
			}
			if (! pus) {
				// last field, use the terminating null for the remainder of items.
				pus = pend;
			}
		}
	} else {
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
	}

	if (empty_var_names) {
		fea.vars.rewind();
		empty_var_names->clear();
		while ((var = fea.vars.next())) {
			MACRO_ITEM* pitem = lookup_exact(var);
			if (! pitem || (pitem->raw_value != emptyItemString && 0 == strlen(pitem->raw_value))) {
				if (! empty_var_names->empty()) (*empty_var_names) += ",";
				(*empty_var_names) += var;
			}
		}
	}

	return loaded_row;
}

// returns true when the row data is not yet loaded, but might be available if you try again later.
bool JobFactory::RowDataIsLoading(int row)
{
	//PRAGMA_REMIND("TODO: keep only unused items in the item list.")

	if (fea.foreach_mode == foreach_from_async) {

		// put all of the items we have read so far into the item list
		std::string str;
		while (reader.output().readLine(str, false)) {
			trim(str);
			fea.items.append(str.c_str());
		}

		if (reader.done_reading()) {
			if (! reader.eof_was_read()) {
				std::string filename;
				dprintf(D_ALWAYS, "failed to read all of the item data from '%s', error %d\n", fea.items_filename.c_str(), reader.error_code());
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


int JobFactory::LoadDigest(FILE* fp, int cluster_id, StringList & items, std::string & errmsg)
{
	MacroStreamYourFile ms(fp, source);
	return LoadDigest(ms, cluster_id, items, errmsg);
}

int JobFactory::LoadDigest(MacroStream &ms, int cluster_id, StringList & items, std::string & errmsg)
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
			dprintf(D_MATERIALIZE | D_VERBOSE, "Digest for cluster %d has %d(%d) itemdata %s\n", cluster_id, fea.foreach_mode, rval, fea.items_filename.c_str());
			if (rval == 1) {
				rval = 0;
				// there is external foreach data.  it had better be from a file, because that's the only form we support
				if (fea.foreach_mode != foreach_from) {
					formatstr(errmsg, "foreach mode %d is not supported by schedd job factory", fea.foreach_mode);
					rval = -1;
				} else if (fea.items_filename.empty()) {
					// this is ok?
				} else if (fea.items_filename == "<" || fea.items_filename == "-") {
					formatstr(errmsg, "invalid filename '%s' for foreach from", fea.items_filename.c_str());
				} else if (reader.eof_was_read()) {
					// the reader was primed with itemdata sent over the wire...
				} else if (fea.items.number() > 0) {
					// we populated the itemdata already
					dprintf(D_MATERIALIZE | D_VERBOSE, "Digest itemdata for cluster %d already loaded. number=%d\n", cluster_id, fea.items.number());
				} else if (items.number() > 0) {
					dprintf(D_MATERIALIZE | D_VERBOSE, "Digest itemdata for cluster %d passed in. number=%d\n", cluster_id, items.number());
					fea.items.take_list(items);
					dprintf(D_MATERIALIZE | D_VERBOSE, "Moved %d items, input list now has %d items\n", fea.items.number(), items.number());
				} else {
					// before opening the item data file, we (may) want to impersonate the user
					if (rval == 0) {
						// setup an async reader for the itemdata
						rval = reader.open(fea.items_filename.c_str());
						if (rval) {
							formatstr(errmsg, "could not open item data file '%s', error = %d", fea.items_filename.c_str(), rval);
							dprintf(D_ALWAYS, "%s\n", errmsg.c_str());
						} else {
							rval = reader.queue_next_read();
							if (rval) {
								formatstr(errmsg, "could not initiate reading from item data file '%s', error = %d", fea.items_filename.c_str(), rval);
								dprintf(D_ALWAYS, "%s\n", errmsg.c_str());
							} else {
								fea.foreach_mode = foreach_from_async;
							}
						}
						dprintf(rval ? D_ALWAYS : (D_MATERIALIZE|D_VERBOSE), "reader.open() returned %d, mode=%d\n", rval, fea.foreach_mode);
					}
					// failed to open or start the item reader, so just close it and free the internal buffers.
					if (rval) { reader.clear(); }
				}
			}
		}
	}

	paused = rval ? mmInvalid : mmRunning;

	return rval;
}

// append a buffer of row data into job factories row data, the last item in the buffer may be incomplete
// in which case it should be returned in the remainder string - which will be prefixed to the first
// item the next time this function is called.
#if 1
int AppendRowsToJobFactory(StringList & items, char * buf, size_t cbbuf, std::string & remainder)
{
	int num_rows = 0;
	size_t off = 0;
	for (size_t ix = off; ix < cbbuf; ++ix) {
		if (buf[ix] == '\n') {
			remainder.append(buf + off, ix - off);
			items.append(remainder.data(), (int)remainder.size());
			remainder.clear();
			off = ix + 1;
			++num_rows;
		}
	}
	if (off < cbbuf) {
		remainder.append(buf + off, cbbuf - off);
	}
	dprintf(D_MATERIALIZE | D_VERBOSE, "Appended %d rows to factory from %d bytes. remain=%d, items=%d\n",
		num_rows, (int)cbbuf, (int)remainder.size(), items.number());
	return 0;
}
#else
int AppendRowsToJobFactory(JobFactory *factory, char * buf, size_t cbbuf, std::string & remainder)
{
	int num_rows = 0;
	size_t off = 0;
	for (size_t ix = off; ix < cbbuf; ++ix) {
		if (buf[ix] == '\n') {
			remainder.append(buf + off, ix - off);
			factory->fea.items.append(remainder.data(), (int)remainder.size());
			remainder.clear();
			off = ix + 1;
			++num_rows;
		}
	}
	if (off < cbbuf) {
		remainder.append(buf + off, cbbuf - off);
	}
	dprintf(D_MATERIALIZE | D_VERBOSE, "Appended %d rows to factory from %d bytes. remain=%d, items=%d\n",
		num_rows, (int)cbbuf, (int)remainder.size(), factory->fea.items.number());
	return 0;
}
#endif

// this function should behave (more or less) the same as the one in src/condor_schedd.V6/qmgmt_factory.cpp
//
// returns true if the factory changed state, false otherwise.
bool CheckJobFactoryPause(JobFactory * factory, int want_pause)
{
	if (! factory)
		return false;

	int paused = factory->IsPaused() ? 1 : 0;

	dprintf(D_MATERIALIZE | D_VERBOSE, "in CheckJobFactoryPause for job factory %d %s want_pause=%d is_paused=%d (%d)\n",
		factory->ID(), factory->Name(), want_pause, factory->IsPaused(), factory->PauseMode());

	// make sure that the desired mode is a valid one.
	// Also, we want to disallow setting the mode to  ClusterRemoved using this function.
	if (want_pause < mmInvalid) want_pause = mmInvalid;
	else if (want_pause > mmNoMoreItems) want_pause = mmNoMoreItems;

	// If the factory is in the correct meta-mode (either paused or not),
	// then we won't try and change the actual pause code - we just return true.
	// As a result of this test, a factory in mmInvalid or mmNoMoreItems state
	// can't be changed to mmHold or visa versa.
	if (paused == (want_pause != mmRunning)) {
		return true;
	}

	dprintf(D_MATERIALIZE, "CheckJobFactoryPause %s job factory %d %s code=%d\n",
		want_pause ? "Pausing" : "Resuming", factory->ID(), factory->Name(), want_pause);

	if (want_pause) {
		// we should only get here if the factory is in running state.
		// we pass the pause code through, but it will only change the state if the
		// new value is > than the old value, so we can change from running -> Hold and Hold -> NoMore but not the other way.
		return factory->Pause((MaterializeMode)want_pause) != mmRunning;
	} else {
		// only resume a natural hold, not a failure or end condition pause
		return factory->Resume(mmHold) == mmRunning;
	}
}

// this function should behave (more or less) the same as the one in src/condor_schedd.V6/qmgmt_factory.cpp
//
bool setJobFactoryPauseAndLog(JobFactory * factory, int pause_mode, int /*hold_code*/, const std::string & reason)
{
	ClassAd * cad = factory->get_cluster_ad();
	if (! cad) return false;

	cad->Assign(ATTR_JOB_MATERIALIZE_PAUSED, pause_mode);
	if (reason.empty() || pause_mode == mmRunning) {
		cad->Delete(ATTR_JOB_MATERIALIZE_PAUSE_REASON);
	} else {
		if (strchr(reason.c_str(), '\n')) {
			// make sure that the reason  has no embedded newlines because if it does,
			// it will cause the SCHEDD to abort when it rotates the job log.
			std::string msg(reason);
			std::replace(msg.begin(), msg.end(), '\n', ' ');
			std::replace(msg.begin(), msg.end(), '\r', ' ');
			cad->Assign(ATTR_JOB_MATERIALIZE_PAUSE_REASON, msg);
		} else {
			cad->Assign(ATTR_JOB_MATERIALIZE_PAUSE_REASON, reason);
		}
	}

	// make sure that the factory state is in sync with the pause mode
	CheckJobFactoryPause(factory, pause_mode);

	// log the change in pause state
	//const char * reason_ptr = reason.empty() ? NULL : reason.c_str();
	//scheduler.WriteFactoryPauseToUserLog(cad, hold_code, reason_ptr);
	return true;
}

static int factory_check_sub_file(void*, SubmitHash *, _submit_file_role, const char *, int)
{
	// possibly add verbose logging here?
	return 0;
}

// struct for a table mapping attribute names to a flag indicating that the attribute
// must only be in cluster ad or only in proc ad, or can be either.
//
typedef struct attr_force_pair {
	const char * key;
	int          forced; // -1=forced cluster, 0=not forced, 1=forced proc
	// a LessThan operator suitable for inserting into a sorted map or set
	bool operator<(const struct attr_force_pair& rhs) const {
		return strcasecmp(this->key, rhs.key) < 0;
	}
} ATTR_FORCE_PAIR;

// table defineing attributes that must be in either cluster ad or proc ad
// force value of 1 is proc ad, -1 is cluster ad.
// NOTE: this table MUST be sorted by case-insensitive attribute name
#define FILL(attr,force) { attr, force }
static const ATTR_FORCE_PAIR aForcedSetAttrs[] = {
	FILL(ATTR_CLUSTER_ID,         -1), // forced into cluster ad
	FILL(ATTR_JOB_SET_ID,         -1), // forced into cluster ad
	FILL(ATTR_JOB_SET_NAME,       -1), // forced into cluster ad
	FILL(ATTR_JOB_STATUS,         1),  // forced into proc ad
	FILL(ATTR_JOB_UNIVERSE,       -1), // forced into cluster ad
	FILL(ATTR_OWNER,              -1), // forced into cluster ad
	FILL(ATTR_PROC_ID,            1),  // forced into proc ad
};
#undef FILL

// returns non-zero attribute id and optional category flags for attributes
// that require special handling during SetAttribute.
static int IsForcedProcAttribute(const char *attr)
{
	const ATTR_FORCE_PAIR* found = NULL;
	found = BinaryLookup<ATTR_FORCE_PAIR>(
		aForcedSetAttrs,
		COUNTOF(aForcedSetAttrs),
		attr, strcasecmp);
	if (found) {
		return found->forced;
	}
	return 0;
}

int NewProcFromAd(FILE * out, const classad::ClassAd * ad, int ProcId, JobFactory * factory)
{
	ClassAd * ClusterAd = factory->get_cluster_ad();
	int ClusterId = factory->getClusterId();

	//fprintf(out, ATTR_PROC_ID "=%d\n", ProcId);
	//SendAttributeInt(ClusterId, ProcId, ATTR_PROC_ID, ProcId);

	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd(true, true);
	std::string buffer;

	// the EditedClusterAttrs attribute of the cluster ad is the list of attibutes where the value in the cluster ad
	// should win over the value in the proc ad because the cluster ad was edited after submit time.
	// we do this so that "condor_qedit <clusterid>" will affect ALL jobs in that cluster, not just those that have
	// already been materialized.
	classad::References clusterAttrs;
	if (ClusterAd->LookupString(ATTR_EDITED_CLUSTER_ATTRS, buffer)) {
		StringTokenIterator it(buffer, 40, ", \t\r\n");
		const std::string * attr;
		while ((attr = it.next_string())) { clusterAttrs.insert(*attr); }
	}

	for (auto it = ad->begin(); it != ad->end(); ++it) {
		const std::string & attr = it->first;
		const ExprTree * tree = it->second;
		if (attr.empty() || ! tree) {
			dprintf(D_ALWAYS, "ERROR: Null attribute name or value for job %d.%d\n", ClusterId, ProcId);
			return -1;
		}

		// check if attribute is forced to either cluster ad (-1) or proc ad (1)
		bool send_it = false;
		int forced = IsForcedProcAttribute(attr.c_str());
		if (forced) {
			send_it = forced > 0;
		} else if (clusterAttrs.find(attr) != clusterAttrs.end()) {
			send_it = false; // this is a forced cluster attribute
		} else {
			// If we aren't going to unconditionally send it, when we check if the value
			// matches the value in the cluster ad.
			// If the values match, don't add the attribute to this proc ad
			ExprTree *cluster_tree = ClusterAd->Lookup(attr);
			send_it = ! cluster_tree || ! (*tree == *cluster_tree || *tree == *SkipExprEnvelope(cluster_tree));
		}
		if (! send_it)
			continue;

		buffer.clear();
		unparser.Unparse(buffer, tree);
		if (buffer.empty()) {
			dprintf(D_ALWAYS, "ERROR: Null value for job %d.%d\n", ClusterId, ProcId);
			return -1;
		} else {
			fprintf(out, "%s=%s\n", attr.c_str(), buffer.c_str());
		}
	}

	return 0;
}


// return value is 0 for 'can't materialize now, try later'
// in which case retry_delay is set to indicate how long later should be
// retry_delay of 0 means we are done, either because of failure or because we ran out of jobs to materialize.
int  MaterializeNextFactoryJob(FILE* out, JobFactory * factory, int & retry_delay)
{
	// dummy up the parts of a JobQueueCluster that we use here.
	struct _jqc { JOB_ID_KEY jid; } jqc = { {factory->getClusterId(), -1 } };
	struct _jqc* ClusterAd = &jqc;

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
	int rval = factory->get_cluster_ad()->LookupInteger(ATTR_JOB_MATERIALIZE_NEXT_PROC_ID, next_proc_id) ? 0 : -1;
	if (rval < 0 || next_proc_id < 0) {
		dprintf(D_ALWAYS, "ERROR - " ATTR_JOB_MATERIALIZE_NEXT_PROC_ID " is not set, aborting materalize for cluster %d\n", ClusterAd->jid.cluster);
		setJobFactoryPauseAndLog(factory, mmInvalid, 0, ATTR_JOB_MATERIALIZE_PAUSED " is undefined");
		//factory->Pause(mmInvalid);
		//PRAGMA_REMIND("should this be durable?")
		//ClusterAd->Assign(ATTR_JOB_MATERIALIZE_PAUSED, mmInvalid);
		//ClusterAd->Assign(ATTR_JOB_MATERIALIZE_PAUSE_REASON,);
		return rval;
	}

	int step = next_proc_id % step_size;
	int item_index = 0;
	bool no_items = factory->NoItems();
	if (no_items) {
		if (next_proc_id >= step_size) {
			dprintf(D_MATERIALIZE | D_VERBOSE, "Materialize for cluster %d is done. has_items=%d, next_proc_id=%d, step=%d\n", ClusterAd->jid.cluster, !no_items, next_proc_id, step_size);
			// we are done
			factory->Pause(mmNoMoreItems);
			return 0;
		}
	} else {
		// item index is optional, if missing, the value is 0 and the item is the empty string.
		rval = factory->get_cluster_ad()->LookupInteger(ATTR_JOB_MATERIALIZE_NEXT_ROW, item_index) ? 0 : -1;
		if (rval < 0) {
			item_index = next_proc_id / step_size;
			if (item_index > 0 && next_proc_id != step_size) {
				// ATTR_JOB_MATERIALIZE_NEXT_ROW must exist in the cluster ad once we are done with proc 0
				dprintf(D_ALWAYS, "ERROR - " ATTR_JOB_MATERIALIZE_NEXT_ROW " is not set, aborting materialize for job %d.%d step=%d, row=%d\n", ClusterAd->jid.cluster, next_proc_id, step_size, item_index);
				setJobFactoryPauseAndLog(factory, mmInvalid, 0, ATTR_JOB_MATERIALIZE_NEXT_ROW " is undefined");
				//PRAGMA_REMIND("should this be durable?")
				//factory->Pause(mmInvalid);
				//ClusterAd->Assign(ATTR_JOB_MATERIALIZE_PAUSED, mmInvalid);
				//ClusterAd->Assign(ATTR_JOB_MATERIALIZE_PAUSE_REASON, ATTR_JOB_MATERIALIZE_NEXT_ROW " is undefined");
				// we are done
				return 0;
			}
			item_index = factory->FirstSelectedRow();
			dprintf(D_MATERIALIZE | D_VERBOSE, "FirstSelectedRow returned %d\n", item_index);
		}
	}
	if (item_index < 0) {
		// we are done
		dprintf(D_MATERIALIZE | D_VERBOSE, "Materialize for cluster %d is done. JobMaterializeNextRow is %d\n", ClusterAd->jid.cluster, item_index);
		factory->Pause(mmNoMoreItems);
		return 0;
	}
	int row = item_index;

	// if the row data is still loading, return 0 and a small value for the retry delay
	if (factory->RowDataIsLoading(row)) {
		retry_delay = 1;
		return 0;
	}

	JOB_ID_KEY jid(ClusterAd->jid.cluster, next_proc_id);
	dprintf(D_ALWAYS, "Trying to Materializing new job %d.%d step=%d row=%d\n", jid.cluster, jid.proc, step, row);

	const bool check_empty = true;
	const bool fail_empty = false;
	std::string empty_var_names;
	int row_num = factory->LoadRowData(row, check_empty ? &empty_var_names : NULL);
	if (row_num < row) {
		// we are done
		dprintf(D_MATERIALIZE | D_VERBOSE, "Materialize for cluster %d is done. LoadRowData returned %d for row %d\n", ClusterAd->jid.cluster, row_num, row);
		factory->Pause(mmNoMoreItems);
		return 0;
	}
	// report empty vars.. do we still want to do this??
	if (! empty_var_names.empty()) {
		if (fail_empty) {
			std::string msg;
			formatstr(msg, "job %d.%d row=%d : %s have empty values", jid.cluster, jid.proc, step, empty_var_names.c_str());
			dprintf(D_ALWAYS, "Failing Materialize of %s\n", msg.c_str());
			chomp(msg);
			setJobFactoryPauseAndLog(factory, mmInvalid, 0, msg);
			//factory->Pause(mmInvalid);
			//ClusterAd->Assign(ATTR_JOB_MATERIALIZE_PAUSED, mmInvalid);
			//ClusterAd->Assign(ATTR_JOB_MATERIALIZE_PAUSE_REASON, msg);
			return -1;
		}
	}

	factory->get_cluster_ad()->Assign(ATTR_JOB_MATERIALIZE_NEXT_PROC_ID, next_proc_id + 1);
	if (! no_items && (step + 1 == step_size)) {
		int next_row = factory->NextSelectedRow(row);
		dprintf(D_MATERIALIZE | D_VERBOSE, "NextSelectedRow returned %d\n", next_row);
		if (next_row != item_index) {
			factory->get_cluster_ad()->Assign(ATTR_JOB_MATERIALIZE_NEXT_ROW, next_row);
		}
	}

	// Calculate total submit procs taking the slice into acount. the refresh_in_ad bool will be set to
	// true only once in the lifetime of this instance of the factory
	bool refresh_in_ad = false;
	int total_procs = factory->TotalProcs(refresh_in_ad);
	if (refresh_in_ad) {
		factory->get_cluster_ad()->Assign(ATTR_TOTAL_SUBMIT_PROCS, total_procs);
		dprintf(D_MATERIALIZE | D_VERBOSE, "setting " ATTR_TOTAL_SUBMIT_PROCS " to %d\n", total_procs);
	}

	// have the factory make a job and give us a pointer to it.
	// note that this ia not a transfer of ownership, the factory still owns the job and will delete it
	const classad::ClassAd * job = factory->make_job_ad(jid, row, step, false, false, factory_check_sub_file, NULL);
	if (! job) {
		std::string msg;
		std::string txt(factory->error_stack()->getFullText()); if (txt.empty()) { txt = ""; }
		formatstr(msg, "failed to create ClassAd for Job %d.%d : %s", jid.cluster, jid.proc, txt.c_str());
		dprintf(D_ALWAYS, "ERROR: %s", msg.c_str());
		setJobFactoryPauseAndLog(factory, mmHold, CONDOR_HOLD_CODE::Unspecified, msg);
		//factory->Pause(mmHold);
		//ClusterAd->Assign(ATTR_JOB_MATERIALIZE_PAUSED, mmHold);
		//ClusterAd->Assign(ATTR_JOB_MATERIALIZE_PAUSE_REASON, msg);
		rval = -1; // failed to instantiate.
	} else {
		rval = NewProcFromAd(out, job, jid.proc, factory);
		factory->delete_job_ad();
	}
	if (rval < 0) {
		return rval; // failed instantiation
	}

	return 1; // successful instantiation.
}



void usage();
const char	*MyName = "job_factory";
int DashDryRun = 0;
int DumpSubmitHash = 0;
int DumpSubmitDigest = 0;
const char *DashDryRunOutName = NULL;

int
main( int argc, const char *argv[] )
{
	const char **ptr;
	const char *pcolon = NULL;
	const char *digest_file = NULL;
	const char *clusterad_file = NULL;
	const char *items_file = NULL;
	ClassAdFileParseType::ParseType clusterad_format = ClassAdFileParseType::Parse_long;
	std::string errmsg;

	set_mySubSystem( "SUBMIT", false, SUBSYSTEM_TYPE_SUBMIT );

	MyName = condor_basename(argv[0]);
	config();


	for( ptr=argv+1,argc--; argc > 0; argc--,ptr++ ) {
		if( ptr[0][0] == '-' ) {
			if (MATCH == strcmp(ptr[0], "-")) { // treat a bare - as a submit filename, it means read from stdin.
				digest_file = ptr[0];
			} else if (is_dash_arg_prefix(ptr[0], "verbose", 1)) {
				verbose = true;
			} else if (is_dash_arg_colon_prefix(ptr[0], "debug", &pcolon, 3)) {
				// dprintf to console
				dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
			} else if (is_dash_arg_colon_prefix(ptr[0], "clusterad", &pcolon, 1)) {
				if (!(--argc) || !(*(++ptr))) {
					fprintf(stderr, "%s: -clusterad requires another argument\n", MyName);
					exit(1);
				}
				clusterad_file = *ptr;
				if (pcolon) {
					clusterad_format = parseAdsFileFormat(++pcolon, ClassAdFileParseType::Parse_long);
				}
			} else if (is_dash_arg_prefix(ptr[0], "digest", 1)) {
				if (!(--argc) || !(*(++ptr))) {
					fprintf(stderr, "%s: -digest requires another argument\n", MyName);
					exit(1);
				}
				digest_file = *ptr;
			} else if (is_dash_arg_prefix(ptr[0], "items", 1)) {
				if (!(--argc) || !(*(++ptr))) {
					fprintf(stderr, "%s: -items requires another argument\n", MyName);
					exit(1);
				}
				items_file = *ptr;
			} else if (is_dash_arg_colon_prefix(ptr[0], "dry-run", &pcolon, 3)) {
				DashDryRun = 1;
				bool needs_file_arg = true;
				if (pcolon) { 
					needs_file_arg = false;
					StringList opts(++pcolon);
					for (const char * opt = opts.first(); opt; opt = opts.next()) {
						if (YourString(opt) == "hash") {
							DumpSubmitHash |= 0x100 | HASHITER_NO_DEFAULTS;
							needs_file_arg = true;
						} else if (YourString(opt) == "def") {
							DumpSubmitHash &= ~HASHITER_NO_DEFAULTS;
							needs_file_arg = true;
						} else if (YourString(opt) == "digest") {
							DumpSubmitDigest = 1;
							needs_file_arg = true;
						} else {
							int optval = atoi(opt);
							// if the argument is -dry:<number> and number is > 0x10,
							// then what we are actually doing triggering the unit tests.
							if (optval > 1) {  DashDryRun = optval; needs_file_arg = optval < 0x10; }
							else {
								fprintf(stderr, "unknown option %s for -dry-run:<opts>\n", opt);
								exit(1);
							}
						}
					}
				}
				if (needs_file_arg) {
					const char * pfilearg = ptr[1];
					if ( ! pfilearg || (*pfilearg == '-' && (MATCH != strcmp(pfilearg,"-"))) ) {
						fprintf( stderr, "%s: -dry-run requires another argument\n", MyName );
						exit(1);
					}
					DashDryRunOutName = pfilearg;
					--argc; ++ptr;
				}
			} else if (is_dash_arg_prefix(ptr[0], "help")) {
				usage();
				exit( 0 );
			} else {
				usage();
				exit( 1 );
			}
		} else {
			digest_file = *ptr;
		}
	}

	if ( ! clusterad_file) {
		fprintf(stderr, "\nERROR: no clusterad filename provided\n");
		exit(1);
	}

	FILE* file = NULL;
	bool close_file = false;
	if (MATCH == strcmp(clusterad_file, "-")) {
		file = stdin;
		close_file = false;
	} else {
		file = safe_fopen_wrapper_follow(clusterad_file, "rb");
		if (! file) {
			fprintf(stderr, "could not open %s: error %d - %s\n", clusterad_file, errno, strerror(errno));
			exit(1);
		}
		close_file = true;
	}

	CondorClassAdFileIterator adIter;
	if (! adIter.begin(file, close_file, clusterad_format)) {
		fprintf(stderr, "Unexpected error reading clusterad: %s\n", clusterad_file);
		if (close_file) { fclose(file); file = NULL; }
		exit(1);
	}
	file = NULL; // the adIter will close the clusterAd file handle when it is destroyed.
	close_file = false;

	int cluster_id = -1;
	ClassAd * clusterAd = adIter.next(NULL);
	if ( ! clusterAd || !clusterAd->LookupInteger(ATTR_CLUSTER_ID, cluster_id)) {
		fprintf(stderr, "ERROR: %s did not contain a job Cluster classad", clusterad_file);
		exit(1);
	}

	JobFactory * factory = new JobFactory(digest_file, cluster_id);

	StringList items;
	if (items_file) {
		file = safe_fopen_wrapper_follow(items_file, "rb");
		if (! file) {
			fprintf(stderr, "could not open %s: error %d - %s\n", items_file, errno, strerror(errno));
			exit(1);
		}

		// read the items and append them in 64k (ish) chunks to replicate the way the data is sent to the schedd by submit
		const size_t cbAlloc = 0x10000;
		char buf[cbAlloc];
		std::string remainder;
		size_t cbread;
		size_t off = 0;
		while ((cbread = fread(buf, 1, sizeof(buf), file)) > 0) {
			if (AppendRowsToJobFactory(items, buf, cbread, remainder) < 0) {
				fprintf(stderr, "ERROR: could not append %d bytes of itemdata to factory at offset %d\n", (int)cbread, (int)off);
				exit(1);
			}
			off += cbread;
		}

		fclose(file);
	}

	if (! digest_file) {
		fprintf(stderr, "\nERROR: no digest filename provided\n");
		exit(1);
	}
	file = safe_fopen_wrapper_follow(digest_file, "rb");
	if (! file) {
		fprintf(stderr, "could not open %s: error %d - %s\n", digest_file, errno, strerror(errno));
		exit(1);
	}
	close_file = true;

	// the SetJobFactory RPC does this
	if (factory->LoadDigest(file, cluster_id, items, errmsg) != 0) {
		fprintf(stderr, "\nERROR: failed to parse submit digest for factory %d : %s\n", factory->getClusterId(), errmsg.c_str());
		exit(1);
	}
	// It also does this where first_proc_id is an argument passed through the RPC and always 0 (for now)
	const int first_proc_id = 0;
	clusterAd->Assign(ATTR_JOB_MATERIALIZE_NEXT_PROC_ID, first_proc_id);

	// on CommitTransaction we attach the clusterAd to the factory
	factory->set_cluster_ad(clusterAd);

	FILE* out = stdout;
	bool free_out = false;
	if (DashDryRunOutName) {
		if (MATCH == strcmp(DashDryRunOutName, "-")) {
			out = stdout; free_out = false;
		} else {
			out = safe_fopen_wrapper_follow(DashDryRunOutName, "w");
			if (! out) {
				fprintf(stderr, "\nERROR: Failed to open file -dry-run output file (%s)\n", strerror(errno));
				exit(1);
			}
			free_out = true;
		}
	}

	if (DashDryRun) {
		// fprintf(out, "Dry-Run job(s)");
	}

	// ok, factory initialized, now materialize them jobs
	int num_jobs = 0;
	for (;;) {
		int retry_delay = 0; // will be set to non-zero when we should try again later.
		int rv = MaterializeNextFactoryJob(out, factory, retry_delay);
		if (rv >= 1) {
			// made a job.
			if (verbose) fprintf(out, "# %d jobs\n", rv);
			fprintf(out, "\n");
			num_jobs += rv;
		} else if (retry_delay > 0) {
			// we are delayed by itemdata loading, try again later
			if (verbose) fprintf(stdout, "# retry in %d seconds\n", retry_delay);
			if (retry_delay > 100) {
				fprintf(stdout, "# delay > 100 indicates factory paused, quitting\n");
				break;
			}
			sleep(retry_delay);
			continue;
		} else {
			if (verbose) { fprintf(stdout, "# no more jobs to materialize\n"); }
			break;
		}
		if (DashDryRun) fprintf(out, ".\n");
	}
	if (DashDryRun) fprintf(out, "%d job(s) dry-run to cluster %d.\n", num_jobs, cluster_id);

	if (free_out && out) {
		fclose(out); out = NULL;
	}
	return 0;
}

void usage()
{
	fprintf(stderr, "Usage: %s [options] -clusterad <classad-file> - | <digest-file>\n", MyName);
	fprintf(stderr, "\t-clusterad <classad-file>\tRead The ClusterAd <classad-file>\n");
	fprintf(stderr, "    [options] are\n");
	fprintf(stderr, "\t-digest <digest-file>\tRead Submit commands from <digest-file>\n");
	fprintf(stderr, "\t-items <items-file>\tRead Submit itemdata from <items-file>\n");
	fprintf(stderr, "\t-verbose\t\tDisplay verbose output, jobid and full job ClassAd\n");
	fprintf(stderr, "\t-debug  \t\tDisplay debugging output\n");

}

