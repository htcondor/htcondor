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
#include "autocluster.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "classad/classadCache.h" // for CachedExprEnvelope
#include "qmgmt.h"
#include "schedd_stats.h" // for schedd_runtime_probe

// this is a placeholder for a future class that will compactly hold a set of jobs
// by taking into account the fact that it is common for only the cluster to be significant
// and that consecutive cluster id's are often owned by the same user.
class JobIdSet {
public:
	bool contains(const JOB_ID_KEY & jid) const { return jobs.find(jid) != jobs.end(); }
	bool empty() { return jobs.empty(); }
	void rewind() { it = jobs.begin(); }
	int  count() { return (int)jobs.size(); }
	bool next(JOB_ID_KEY &jid) { if (it == jobs.end()) return false; jid = *it; ++it; return true; }
	bool first(JOB_ID_KEY &jid) { if (jobs.empty()) return false; jid = *(jobs.begin()); return true; }
	bool last(JOB_ID_KEY &jid)  { if (jobs.empty()) return false; jid = *(jobs.rbegin()); return true; }
	void insert(JOB_ID_KEY &jid) { jobs.insert(jid); }
	void erase(JOB_ID_KEY &jid) { jobs.erase(jid); }
private:
	std::set<JOB_ID_KEY> jobs;
	std::set<JOB_ID_KEY>::const_iterator it;
};

JobCluster::JobCluster()
	: next_id(1)
	, significant_attrs(NULL)
#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP
	, keep_job_ids(false)
#endif
{
}

JobCluster::~JobCluster()
{
	clear();
	if (significant_attrs) free(const_cast<char*>(significant_attrs));
	significant_attrs = NULL;
}

void JobCluster::clear()
{
	cluster_map.clear();
#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP
	cluster_use.clear();
	cluster_gone.clear();
#endif
	next_id = 1;
}

bool JobCluster::setSigAttrs(const char* new_sig_attrs, bool free_input_attrs, bool replace_attrs)
{
	if ( ! new_sig_attrs) {
		if (replace_attrs) {
			clear();
			if (significant_attrs) {
				free(const_cast<char*>(significant_attrs));
				significant_attrs = NULL;
				return true;
			}
		}
		return false;
	}

		// If we are in danger of running out of IDs, clear all auto clusters
		// and reset next_id so we can reclaim unused IDs.
	bool next_id_exhausted = next_id > INT_MAX/2;
	bool sig_attrs_changed = false;

	if (significant_attrs && ! next_id_exhausted && (MATCH == strcasecmp(new_sig_attrs,significant_attrs))) {
		if (free_input_attrs) {
			free(const_cast<char*>(new_sig_attrs));
		}
		return false;
	}

	//PRAGMA_REMIND("tj: is it worth checking to see if the significant attrs only changed order?")

	if (replace_attrs || ! significant_attrs) {
		// Create significant_attrs from new_sig_attrs
		free(const_cast<char*>(significant_attrs));
		if (free_input_attrs) {
			significant_attrs = new_sig_attrs;
		} else {
			significant_attrs = strdup(new_sig_attrs);
		}
		sig_attrs_changed = true;
	} else {
		// Merge everything in new_sig_attrs into our existing
		// significant_attrs.  Take note if significant_attrs changed,
		// since we need to return this info to our caller.
		StringList attrs(significant_attrs);
		StringList new_attrs(new_sig_attrs);
		sig_attrs_changed = attrs.create_union(new_attrs,true);
		if (sig_attrs_changed) {
			free(const_cast<char*>(significant_attrs));
			significant_attrs = attrs.print_to_string();
		}
		if (free_input_attrs) {
			free(const_cast<char*>(new_sig_attrs));
		}
	}

	// the SIGNIFICANT_ATTRIBUTES setting changed, purge our
	// state.
	if (sig_attrs_changed || next_id_exhausted) {
		clear();
	}

	return sig_attrs_changed;
}

#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP

// lookup the autocluster for a job (assumes job.autocluster_id is valid)
JobCluster::JobIdSetMap::iterator JobCluster::find_job_id_set(JobQueueJob & job)
{
	// use the job's autocluster_id field to quickly lookup the old JobIdSetMap
	// if that set map actually still contains the job, return it.
	JobIdSetMap::iterator it = cluster_use.find(job.autocluster_id);
	if (it != cluster_use.end() && it->second.contains(job.jid)) {
		return it;
	}
	return cluster_use.end();
}

// scan all autoclusters for a given jobid
/* not used, and can be expensive
JobCluster::JobIdSetMap::iterator JobCluster::brute_force_find_job_id_set(const JOB_ID_KEY & jid)
{
	JobIdSetMap::iterator it;
	for (it = cluster_use.begin(); it != cluster_use.end(); ++it) {
		if (it->second.contains(jid)) {
			break;
		}
	}
	return it;
}
*/

// free up unused autoclusters, if brute_force flags is used, then all autoclusters are checked to
// see if they refer to any jobs that are still in the queue before deleting. otherwise only
// autoclusters that have been added to the cluster_gone list are removed.
//
void JobCluster::collect_garbage(bool brute_force) // free the deleted clusters
{
	if (cluster_gone.empty() && ! brute_force)
		return;

	// scan the cluster collection, checking to see if there are no longer any referring jobs.
	JobSigidMap::iterator it;
	for (it = cluster_map.begin(); it != cluster_map.end(); /*advance at bottom of loop!*/) {
		bool gone = cluster_gone.find(it->second) != cluster_gone.end();
		if (brute_force || gone) {
			// found a deleted cluster. but we should double check to see that it's really unused.
			JobIdSetMap::iterator jit = cluster_use.find(it->second);
			if (jit != cluster_use.end()) {
				gone = false;
				if (brute_force) {
					gone = true;
					JOB_ID_KEY jid;
					jit->second.rewind();
					while (jit->second.next(jid)) {
						if (GetJobAd(jid)) {
							gone = false;
							break;
						}
					}
					if (gone) { cluster_use.erase(jit); }
				}
			}
		}
		// advance here so that we can erase the previous entry if needed.
		JobSigidMap::iterator last = it++;
		if (gone) { cluster_map.erase(last); }
	}
	cluster_gone.clear();
}

#endif

extern int    last_autocluster_classad_cache_hit;

int JobCluster::getClusterid(JobQueueJob & job, bool expand_refs, std::string * final_list)
{
	int cur_id = -1;

	// we want to summarize job into a string "signature"
	// the signature will consist of "key1=val1\nkey2=val2\n"
	// for each of the keys in the significant_attrs list and (if expand_refs is true)
	// the keys that the significant_attrs values refer to that are internal references.
	// the order of the keys in the signature will be the same as the order specified in significant_attrs
	// followed by the expanded keys in case-insensitive alpha order.

	// first put build a set of class ad values, one for each significant attribute
	//
	classad::References exattrs;   // expanded attribs if requested
	std::vector<ExprTree*> sigset; // significant values, including expanded attribs if requested

	// walk significant attributes list and fetch values for each attrib
	// also fetch internal references if requested.
	StringTokenIterator list(significant_attrs);
	const std::string * attr;
	while ((attr = list.next_string())) {
		ExprTree * tree = job.Lookup(*attr);
		sigset.push_back(tree);
		if (expand_refs && tree) {
			// bare attribute references that are not resolved locally show up as external refs
			// but they may be something like JobMachineAttrs that get added to the job later.
			// Also When something like Requirements has MY.foo, foo shows up as external ref foo when
			// the job does not have a foo.  So we get the fully qualified external refs and then
			// delete those that have a "." in them which gets rid of the unambiguously external refs.
			// (Also we can't tolerate dotted attribute names in the significant attrs list)
			job.GetExternalReferences(tree, exattrs, true);
			for (auto it = exattrs.begin(); it != exattrs.end();) { // c++ 20 has erase_if, but we can't use it
				auto tmp = it++;
				if (tmp->find_first_of('.') != std::string::npos) { exattrs.erase(tmp); }
			}
			// now add in the internal refs
			job.GetInternalReferences(tree, exattrs, false);
		}
	}

	// if there are expanded refs, walk the expanded attribs list and fetch values for any
	// that have not already been fetched.
	if (expand_refs) {
		if ( ! exattrs.empty()) {
			// remove expanded attrs that already appear in the significant_attrs list
			list.rewind();
			while ((attr = list.next_string())) {
				classad::References::iterator it = exattrs.find(*attr);
				if (it != exattrs.end()) {
					exattrs.erase(it);
				}
			}
			for (classad::References::iterator it = exattrs.begin(); it != exattrs.end(); ++it) {
				ExprTree * tree = job.Lookup(*it);
				sigset.push_back(tree);
			}
		}
	}

	// sigset now contains the values of all the attributes we need,
	// significant attibutes are first, followed by expanded attributes
	// we build a signature essentially by printing it all out in one big string
	//
	bool need_sep = false; // true after the first item, (when we need to print separators)
	std::string signature;
	signature.reserve(strlen(significant_attrs) + exattrs.size()*20 + sigset.size()*20); // make a guess as to how much space the signature will take.

	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true, true );

	// first put the pre-defined significant attrs in the sig
	list.rewind();
	int ix = 0;
	while ((attr = list.next_string())) {
		ExprTree * tree = sigset[ix];
		signature += *attr;
		signature += " = ";
		if (tree) { unp.Unparse(signature, tree); }
		signature += '\n';
		if (final_list) {
			if (need_sep) { (*final_list) += ','; }
			final_list->append(*attr);
			need_sep = true;
		}
		++ix;
	}

	// now put out the expanded attribs (if any)
	for (classad::References::iterator it = exattrs.begin(); it != exattrs.end(); ++it) {
		ExprTree * tree = sigset[ix];
		signature += *it;
		signature += " = ";
		if (tree) { unp.Unparse(signature, tree); }
		signature += '\n';
		if (final_list) {
			if (need_sep) { (*final_list) += ','; }
			final_list->append(*it);
			need_sep = true;
		}
		++ix;
	}

	// now check the signature against the current cluster map
	// and either return the matching cluster id, or a new cluster id.
	JobSigidMap::iterator it;
	it = cluster_map.find(signature);
	if (it != cluster_map.end()) {
		cur_id = it->second;
	}
	else {
		cur_id = next_id++;
		cluster_map.insert(JobSigidMap::value_type(signature,cur_id));
	}

#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP
	if (keep_job_ids) {
		if (true) {
			JobIdSetMap::iterator jit = find_job_id_set(job);
			if (jit != cluster_use.end()) {
				int old_id = jit->first;
				if (old_id != cur_id) {
					jit->second.erase(job.jid);
					if (jit->second.empty()) { cluster_gone.insert(old_id); }
					cluster_use[cur_id].insert(job.jid);
				}
			} else {
				cluster_use[cur_id].insert(job.jid);
			}
		}
	}
#endif
	return cur_id;
}

// AutoCluster is an instance of JobCluster with additional semantics because it can pull
// it's config from params as well as from input.
//
//
AutoCluster::AutoCluster()
	: sig_attrs_came_from_config_file(false)
{
#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP
	this->keep_job_ids = true;
#endif
}


AutoCluster::~AutoCluster()
{
}

// configure or expand the significant attribute set.
bool AutoCluster::config(const classad::References &basic_attrs, const char* significant_target_attrs)
{
	bool sig_attrs_changed = false;
	char *new_sig_attrs =  param ("SIGNIFICANT_ATTRIBUTES");
	const std::string * attr; // used in various loops
	std::string auto_target_attrs; // this is used when NULL is passed in and we want to fake up an "input" set of attrs

	dprintf(D_FULLDEBUG,
		"AutoCluster:config(%s) invoked\n",
		significant_target_attrs ? significant_target_attrs : "(null)" );

		// Handle the case where our significant attributes came from
		// the config file, and now the user removed them from the
		// config file.  In this case, we want to wipe the slate clean.
	if (sig_attrs_came_from_config_file && !new_sig_attrs) {
		sig_attrs_changed = true;
		sig_attrs_came_from_config_file = false;
	}

	if (new_sig_attrs) {
		if ( ! sig_attrs_came_from_config_file) {
			// in this case, we were autocomputing sig attrs, and now
			// they are set in the config file -- so wipe the slate clean.
			sig_attrs_changed = true;
		}
		sig_attrs_came_from_config_file = true;
	} else if ( ! significant_target_attrs && significant_attrs) {
		// in this case, we have an cached lifetime set of significant attributes
		// but have been called with NULL (this happens on reconfig).
		// we want to check of there have been any changes that require we clean the cache.

		// check various things to see if the current set of significant attributes should be cleaned
		bool cache_needs_cleaning = false;

		// Check to see if one of the banned attrs is in the current set of significant attributes
		char * rm_attrs = param("REMOVE_SIGNIFICANT_ATTRIBUTES");
		if (rm_attrs) {
			classad::References banned_attrs;
			StringTokenIterator rt(rm_attrs);
			while ((attr = rt.next_string())) { banned_attrs.insert(*attr); }
			free(rm_attrs);

			// determine if the current cached set of significant attrs contains a banned attr
			// and if it does, set the cache_needs_cleaning flag
			StringTokenIterator it(significant_attrs);
			while ((attr = it.next_string())) {
				if (banned_attrs.find(*attr) != banned_attrs.end()) {
					cache_needs_cleaning = true;
					break;
				}
			}
		}

		// check to see if there are new basic attributes that should be added to the list.
		if ( ! cache_needs_cleaning) {
			classad::References cur_attrs;
			StringTokenIterator st(significant_attrs);
			while ((attr = st.next_string())) { cur_attrs.insert(*attr); }
			for (classad::References::const_iterator it = basic_attrs.begin(); it != basic_attrs.end(); ++it) {
				if (cur_attrs.find(*it) == cur_attrs.end()) {
					cache_needs_cleaning = true;
					break;
				}
			}
		}

		// if we need to add or remove attrs from the cached significant attributues
		// we will do that by copying the cache to a temp variable and pretending that it is the input list
		// Then we delete the cache and fall down into the code below that will adjust the input list
		// and merge it into the (now empty) cache.
		if (cache_needs_cleaning) {
			auto_target_attrs = significant_attrs;
			significant_target_attrs = auto_target_attrs.c_str();
			// delete the cache
			free(const_cast<char*>(significant_attrs));
			significant_attrs = NULL;
		}
	}


		// Always use what the user specifies in the config file.
		// If the user did not specify anything, then we want to use
		// the significant attrs passed to us (that likely originated
		// from the matchmaker).
	if ( ! new_sig_attrs && significant_target_attrs) {
		sig_attrs_came_from_config_file = false;

		// init set of required attrs from the basic attrs.
		classad::References required_attrs;
		for (classad::References::const_iterator it = basic_attrs.begin(); it != basic_attrs.end(); ++it) {
			required_attrs.insert(*it);
		}

		// If the configuration specifies a whitelist of attributes, add them to the required attrs list
		std::string other_attrs;
		if (param(other_attrs, "ADD_SIGNIFICANT_ATTRIBUTES")) {
			StringTokenIterator it(other_attrs);
			while ((attr = it.next_string())) { required_attrs.insert(*attr); }
		}

		// walk the input string, removing attributes from the required_attrs set if the are already
		// in the input, we do this because we want to preserve the order of the input significant attrs
		StringTokenIterator list(significant_target_attrs);
		while ((attr = list.next_string())) { 
			if (required_attrs.size() <= 0) break;
			classad::References::iterator it = required_attrs.find(*attr);
			if (it != required_attrs.end()) {
				required_attrs.erase(it);
			}
		}

		// If the configuration specifies a blacklist of attributes,
		// build up a set of banned attrs.
		classad::References banned_attrs;
		if (param(other_attrs, "REMOVE_SIGNIFICANT_ATTRIBUTES")) {
			StringTokenIterator it(other_attrs);
			while ((attr = it.next_string())) {
				if ( ! required_attrs.empty()) {
					classad::References::iterator it = required_attrs.find(*attr);
					if (it != required_attrs.end()) {
						required_attrs.erase(it);
						continue; // no need to add it to the banned list we already know it's not in the input string.
					}
				}
				banned_attrs.insert(*attr);
			}
		}


		// set new_sig_attrs to the union of the input attrs with the banned attrs remove
		// and the required attrs appended.
		if (required_attrs.size() > 0 || banned_attrs.size() > 0) {
			std::string attrs;
			attrs.reserve(strlen(significant_target_attrs) + required_attrs.size()*30);

			// list still contains the unmodified input significant_target_attrs
			// so we can just iterate them again looking for banned attrs.
			list.rewind();
			while ((attr = list.next_string())) { 
				if ( ! banned_attrs.empty()) {
					classad::References::iterator it = banned_attrs.find(*attr);
					if (it != banned_attrs.end()) {
						continue; // skip this one
					}
				}
				attrs.append(attr->c_str());
				attrs.append(" ");
			}

			// now append the required attrs that were not banned and not already in the input list.
			for (classad::References::iterator it = required_attrs.begin(); it != required_attrs.end(); ++it) {
				attrs.append(it->c_str());
				attrs.append(" ");
			}
			trim(attrs);
			new_sig_attrs = strdup(attrs.c_str());
		} else {
			// input has all of the required attrs, we can just copy it.
			new_sig_attrs = strdup(significant_target_attrs);
		}

	}

	bool replace_attrs = sig_attrs_came_from_config_file;
	bool changed = this->setSigAttrs(new_sig_attrs, true, replace_attrs);
	if (changed) {
		sig_attrs_changed = true;
	} else if (sig_attrs_changed) {
		//PRAGMA_REMIND("tj: do we need to clear state here anyway?")
		sig_attrs_changed = false;
	}

	if (sig_attrs_changed) {
		dprintf(D_ALWAYS,
			"AutoCluster:config() significant attributes changed to %s\n",
			significant_attrs ? significant_attrs : "(null)");
	} else {
		dprintf(D_FULLDEBUG,
			"AutoCluster:config() significant attributes unchanged\n");
	}

	return sig_attrs_changed;
}

void AutoCluster::mark()
{
	cluster_in_use.clear();
}

void AutoCluster::sweep()
{
	JobSigidMap::iterator it,next;
	for( it = cluster_map.begin();
		 it != cluster_map.end();
		 it = next )
	{
		next = it;
		next++; // avoid invalid iterator if we delete this element

		int id = it->second;
		JobClusterIDs::iterator in_use;
		in_use = cluster_in_use.find(id);
		if (in_use == cluster_in_use.end()) {
				// found an entry to remove.
			dprintf(D_FULLDEBUG,"removing auto cluster id %d\n",id);
			cluster_map.erase( it );
		}
	}
}

extern double last_autocluster_runtime;
extern bool   last_autocluster_make_sig;
extern int    last_autocluster_type;

int AutoCluster::getAutoClusterid(JobQueueJob *job)
{
	std::string final_list;
	int cur_id = -1;

		// first check if condor_config file even desires this
		// functionality...
	if ( ! significant_attrs /*&& current_aggregations.empty()*/) {
		return -1;
	}

	_condor_auto_accum_runtime<double> rt(last_autocluster_runtime);
	last_autocluster_make_sig = false;
	last_autocluster_type |= 2;


	job->LookupInteger(ATTR_AUTO_CLUSTER_ID, cur_id);
	if (cur_id != -1)  {
			// we've previously figured it out...

			// tag it as touched
		cluster_in_use.insert(cur_id);
		return cur_id;
	}

	last_autocluster_make_sig = true;

	cur_id = this->getClusterid(*job, true, &final_list);
	if( cur_id < 0 ) {
			// We've wrapped around MAX_INT!
			// In config() we take steps to avoid this unlikely condition.
		EXCEPT("Auto cluster IDs exhausted! (allocated %d)",cur_id);
	}

		// mark as in-use for the mark/sweep code
	cluster_in_use.insert(cur_id);

		// put the new auto cluster id into the job ad to cache it.
	job->Assign(ATTR_AUTO_CLUSTER_ID,cur_id);
	job->autocluster_id = cur_id;

		// for some nice feedback, place the final list of attrs used to create this
		// signature into the job ad.
		// the ATTR_AUTO_CLUSTER_ATTRS attribute is also used by SetAttribute()
		// in qmgmt -- if any of the attrs used to create the signature are
		// changed, then SetAttribute() will delete the ATTR_AUTO_CLUSTER_ID, since
		// the signature needs to be recomputed as it may have changed.
	job->Assign(ATTR_AUTO_CLUSTER_ATTRS, final_list);

	return cur_id;
}

bool AutoCluster::preSetAttribute(JobQueueJob &job, const char * attr, const char * /*value*/, int /*flags*/)
{
	// If any of the attrs used to create the signature are
	// changed, then delete the ATTR_AUTO_CLUSTER_ID, since
	// the signature needs to be recomputed as it may have changed.
	// Note we do this whether or not the transaction is committed - that
	// is ok, and actually is probably more efficient than hitting disk.
	ExprTree * expr = job.Lookup(ATTR_AUTO_CLUSTER_ATTRS);
	if (expr) {
		std::string tmp;
		const char * sigAttrs = NULL;
		if (ExprTreeIsLiteralString(expr, sigAttrs)) {
		} else if (job.LookupString(ATTR_AUTO_CLUSTER_ATTRS, tmp)) {
			// we don't expect this to be an expression rather than a string
			// but if it is, we should still be able to handle it.
			sigAttrs = tmp.c_str();
		} else {
			// is not a string and does not evaluate to one.
			return false;
		}

		if (is_attr_in_attr_list(attr, sigAttrs)) {
			removeFromAutocluster(job);
			return true;
		}
	}
	return false;
}

void AutoCluster::removeFromAutocluster(JobQueueJob &job)
{
	if (job.autocluster_id >= 0) {
#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP
		JobIdSetMap::iterator it = cluster_use.find(job.autocluster_id);
		if (it != cluster_use.end()) {
			it->second.erase(job.jid);
		}
#endif
		job.Delete(ATTR_AUTO_CLUSTER_ID);
		job.Delete(ATTR_AUTO_CLUSTER_ATTRS);
		job.autocluster_id = -1;
	}
}

#ifdef ALLOW_ON_THE_FLY_AGGREGATION

class aggregate_jobs_args {
public:
	aggregate_jobs_args(JobCluster* _jc=NULL, classad::ExprTree * _constr=NULL) : pjc(_jc), constraint(_constr) {}
	JobCluster* pjc;
	classad::ExprTree * constraint;
};

// callback for WalkJobQueue that builds an on-the-fly autocluster set.
int aggregate_jobs(JobQueueJob *job, const JOB_ID_KEY & /*jid*/, void * pv)
{
	aggregate_jobs_args * pargs = (aggregate_jobs_args*)pv;
	JobCluster* pjc = pargs->pjc;
	if (pargs->constraint && ! EvalExprBool(job, pargs->constraint)) {
		// if there is a constraint, and it doesn't evaluate to true, skip this job.
		return 0;
	}
	pjc->getClusterid(*job, true, NULL);
	return 0;
}

#endif

JobAggregationResults * AutoCluster::aggregateOn(
	bool use_default,
	const char * projection,
	int          result_limit,
	classad::ExprTree * constraint)
{
	if (use_default) {
		return new JobAggregationResults(*this, projection, result_limit, constraint, true);
	}
#ifdef ALLOW_ON_THE_FLY_AGGREGATION

	JobCluster * pjc = new JobCluster();
	return new JobAggregationResults(*pjc, projection, result_limit, constraint);
#else
	return NULL;
#endif
}

bool JobAggregationResults::compute()
{
#ifdef ALLOW_ON_THE_FLY_AGGREGATION
	if (is_def_autocluster)
		return true;

	jc.keepJobIds(true);
	jc.setSigAttrs(projection.c_str(), false, true);

	aggregate_jobs_args agg_args(&jc, constraint);

	schedd_runtime_probe runtime;
	WalkJobQueue3(
		// aggregate_jobs as lambda (ignores constraint, assumes pv is pjc
		// [](JobQueueJob *job, const JOB_ID_KEY &, void * pv) -> int { ((JobCluster*)pv)->getClusterid(*job, true, NULL); return 0; },
		aggregate_jobs,
		&agg_args,
		runtime);

	dprintf(D_ALWAYS, "Spent %.3f sec aggregating on '%s'\n", runtime.Total(), projection.c_str());
#endif
	return true;
}

bool JobAggregationResults::rewind()
{
	results_returned = 0;
	pause_position.clear();
	it = jc.cluster_map.begin();
	return it != jc.cluster_map.end();
}

// pause iterator, remember the key of the current item, when we resume
// we will pick back up at that point.
void JobAggregationResults::pause()
{
	pause_position.clear();
	if (it != jc.cluster_map.end()) {
		pause_position = it->first;
	}
}

ClassAd * JobAggregationResults::next()
{
	// if we have already returned the limit of results, then return NULL to signal end.
	if (result_limit >= 0 && results_returned >= result_limit) {
		return NULL;
	}

	// if we are resuming from a paused state, we don't have a valid iterator
	// so we have to find the the element we paused at or the first one after it.
	if ( ! pause_position.empty()) {
		it = jc.cluster_map.lower_bound(pause_position);
		pause_position.clear();
	}

	// in case we never enter the loop, clear our 'current' ad here.
	ad.Clear();

	// we may have to look at multiple items in order to find one to return
	while (it != jc.cluster_map.end()) {

		ad.Clear();

		// the autocluster key (or signature), is a string containing key value
		// pairs separated by \n. So we can easily turn it into a classad.
		StringTokenIterator iter(it->first, 100, "\n");
		const char * line;
		while ((line = iter.next())) {
			(void) ad.Insert(line);
		}
		if (this->is_def_autocluster) {
			ad.Assign(ATTR_AUTO_CLUSTER_ID,it->second);
		} else {
			ad.Assign("Id",it->second);
		}
	#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP
		int cJobs = 0;
		JobCluster::JobIdSetMap::iterator jit = jc.cluster_use.find(it->second);
		if (jit != jc.cluster_use.end()) {
			JobIdSet & jids = jit->second;
			cJobs = jids.count();

			int cret = this->return_jobid_limit;
			if (cret > 0 && cJobs > 0) {
				char buf[PROC_ID_STR_BUFLEN];
			#if 1
				JOB_ID_KEY jidFirst, jidLast, jid;
				jids.last(jidLast);
				jids.rewind();
				jids.next(jidFirst);
				cret -= 2;
				std::string ids;
				ProcIdToStr(jidFirst.cluster, jidFirst.proc, buf);
				ids = buf;
				while (jids.next(jid)) {
					if (jid == jidLast) break;
					if (--cret < 0) { ids += " ..."; break; }
					ProcIdToStr(jid.cluster, jid.proc, buf);
					ids += " ";
					ids += buf;
				}
				if (!(jidLast == jidFirst)) {
					ids += " ";
					ProcIdToStr(jidLast.cluster, jidLast.proc, buf);
					ids += buf;
				}
				ad.Assign("JobIds", ids);
			#else
				JOB_ID_KEY jidFirst, jidLast, jidMin, jidMax;
				bool got_first = false;

				jids.rewind();
				JOB_ID_KEY jid;
				std::string ids;
				while (jids.next(jid)) {
					if ( ! got_first) { jidMin = jidMax = jidFirst = jid; got_first = true; }
					jidLast = jid;
					if (jid < jidMin) jidMin = jid;
					if (jidMax < jid) jidMax = jid;
					if (--cret < 0) {
						if (!ids.empty()) ids += ",...";
						break;
					}
					ProcIdToStr(jid.cluster, jid.proc, buf);
					if ( ! ids.empty()) ids += ",";
					ids += buf;
				}
				if ( ! ids.empty()) { ad.Assign("JobIds", ids); }

				// HACK!! also return range of associated jobid's
				while (jids.next(jid)) {
					jidLast = jid;
					if (jid < jidMin) jidMin = jid;
					if (jidMax < jid) jidMax = jid;
				}

				ProcIdToStr(jidMin.cluster, jidMin.proc, buf);
				ids = buf;
				if (jidMin < jidMax) {
					ProcIdToStr(jidMax.cluster, jidMax.proc, buf);
					ids += ","; ids += buf;
				}
				if (jidFirst < jidMin || jidMin < jidFirst) {
					ProcIdToStr(jidFirst.cluster, jidFirst.proc, buf);
					ids += " ,"; ids += buf;
				}
				if (jidLast < jidMax || jidMax < jidLast) {
					ProcIdToStr(jidLast.cluster, jidLast.proc, buf);
					ids += " ,"; ids += buf;
				}
				ad.Assign("JidRange", ids);
			#endif
			}
		}
		ad.Assign("JobCount", cJobs);
	#endif

		// advance to the next item
		++it;

		// if there is a constraint, then only return the ad if it matches the constraint.
		if (constraint) {
			if ( ! EvalExprBool(&ad, constraint))
				continue;
		}

		// return a pointer to the ad.
		++results_returned;
		return &ad;
	}

	return NULL;
}

