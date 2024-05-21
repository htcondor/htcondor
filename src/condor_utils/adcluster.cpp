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
#include "adcluster.h"
#include "string_list.h"

template <class K> class AdKeySet {
public:
	bool contains(const K & key) const { return keys.find(key) != keys.end(); }
	bool empty() const { return keys.empty(); }
	int  count() const { return (int)keys.size(); }
	void print(std::string & buf, int cmax);
	void insert(K &key) { keys.insert(key); }
	void erase(K &key) { keys.erase(key); }
private:
	std::set<K> keys;
};

template <> void AdKeySet<std::string>::print(std::string & ids, int cmax)
{
	if (cmax <= 0) return;

	size_t start = ids.size();
	for (auto it = keys.begin(); it != keys.end(); ++it) {
		if (ids.size() > start) ids += " ";
		if (--cmax < 0) { ids += "..."; break; }
		ids += *it;
	}
}

template <> void AdKeySet<ClassAd *>::print(std::string & ids, int cmax)
{
	if (cmax <= 0) return;
	size_t start = ids.size();
	for (auto it = keys.begin(); it != keys.end(); ++it) {
		if (ids.size() > start) ids += " ";
		if (--cmax < 0) { ids += "..."; break; }
		ClassAd * item = *it;
		char id[32]; snprintf(id, sizeof(id), "%p", item);
		ids += id;
	}
}

template <class K> AdCluster<K>::AdCluster()
	: next_id(1)
	, significant_attrs(NULL)
	, get_ad_key(NULL)
{
}

template <class K> AdCluster<K>::~AdCluster()
{
	clear();
	if (significant_attrs) free(const_cast<char*>(significant_attrs));
	significant_attrs = NULL;
	//get_ad_key = NULL;
}

template <class K> void AdCluster<K>::clear()
{
	cluster_map.clear();
	cluster_use.clear();
	//cluster_gone.clear();
	next_id = 1;
}

template <class K> bool AdCluster<K>::setSigAttrs(const char* new_sig_attrs, bool free_input_attrs, bool replace_attrs)
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
			new_sig_attrs = NULL;
		}
		return false;
	}

	//PRAGMA_REMIND("tj: is it worth checking to see if the significant attrs only changed order?")

	const char * free_attrs = NULL;

	if (replace_attrs || ! significant_attrs) {
		// Create significant_attrs from new_sig_attrs
		free_attrs = significant_attrs; // remember to free this later
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
			free_attrs = significant_attrs; // free this later
			significant_attrs = attrs.print_to_string();
		} else if (free_input_attrs) {
			free_attrs = new_sig_attrs; // free this later
		}
	}

	// free whatever list of attrs we don't need anymore
	if (free_attrs) { free(const_cast<char*>(free_attrs)); free_attrs = NULL; }

	// the SIGNIFICANT_ATTRIBUTES setting changed, purge our
	// state.
	if (sig_attrs_changed || next_id_exhausted) {
		clear();
	}

	return sig_attrs_changed;
}

template <class K> int AdCluster<K>::getClusterid(ClassAd & ad, bool expand_refs, std::string * final_list)
{
	int cur_id = -1;

	// we want to summarize ad into a string "signature"
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
		ExprTree * tree = ad.Lookup(*attr);
		sigset.push_back(tree);
		if (expand_refs && tree) {
			ad.GetInternalReferences(tree, exattrs, false);
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
				ExprTree * tree = ad.Lookup(*it);
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
	AdSigidMap::iterator it;
	it = cluster_map.find(signature);
	if (it != cluster_map.end()) {
		cur_id = it->second;
	}
	else {
		cur_id = next_id++;
		cluster_map.insert(AdSigidMap::value_type(signature,cur_id));
	}

	if (get_ad_key) {
		K key = get_ad_key(ad);
#if 1
		// update the cluster_use map
		cluster_use[cur_id].insert(key);
#else
		// update the cluster_use and cluster_gone maps
		auto jit = cluster_use.find(cur_id);
		if (jit != cluster_use.end() && ! jit->second.contains(key)) {
			int old_id = jit->first;
			if (old_id == cur_id) {
				jit->second.insert(key);
			} else {
				jit->second.erase(key);
				if (jit->second.empty()) { cluster_gone.insert(old_id); }
				cluster_use[cur_id].insert(key);
			}
		} else {
			cluster_use[cur_id].insert(key);
		}
#endif
	}

	return cur_id;
}

template <class K> bool AdAggregationResults<K>::rewind()
{
	results_returned = 0;
	pause_position.clear();
	it = ac.cluster_map.begin();
	return it != ac.cluster_map.end();
}

// pause iterator, remember the key of the current item, when we resume
// we will pick back up at that point.
template <class K> void AdAggregationResults<K>::pause()
{
	pause_position.clear();
	if (it != ac.cluster_map.end()) {
		pause_position = it->first;
	}
}

template <class K> ClassAd * AdAggregationResults<K>::next()
{
	// if we have already returned the limit of results, then return NULL to signal end.
	if (result_limit >= 0 && results_returned >= result_limit) {
		return NULL;
	}

	// if we are resuming from a paused state, we don't have a valid iterator
	// so we have to find the the element we paused at or the first one after it.
	if ( ! pause_position.empty()) {
		it = ac.cluster_map.lower_bound(pause_position);
		pause_position.clear();
	}

	// in case we never enter the loop, clear our 'current' ad here.
	ad.Clear();

	// we may have to look at multiple items in order to find one to return
	while (it != ac.cluster_map.end()) {

		ad.Clear();

		// the autocluster key (or signature), is a string containing key value
		// pairs separated by \n. So we can easily turn it into a classad.
		StringTokenIterator iter(it->first, "\n");
		const char * line;
		while ((line = iter.next())) {
			int r = ad.Insert(line);
			if (!r) {
				dprintf(D_FULLDEBUG, "Cannot process autocluster attribute %s\n", line);
			}
		}
		ad.InsertAttr(attrId, it->second);

		if (ac.get_ad_key) {
			int cMembers = 0;
			auto jit = ac.cluster_use.find(it->second);
			if (jit != ac.cluster_use.end()) {
				AdKeySet<K> & jids = jit->second;
				cMembers = jids.count();
				int cret = this->return_key_limit;
				if (cret > 0 && cMembers > 0) {
					std::string members;
					jids.print(members, cret);
					ad.InsertAttr(attrMembers, members);
				}
			}
			ad.InsertAttr(attrCount, cMembers);
		}


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

// forced template instantiation
template class AdCluster<ClassAd*>;
template class AdCluster<std::string>;
template class AdAggregationResults<ClassAd*>;
template class AdAggregationResults<std::string>;

