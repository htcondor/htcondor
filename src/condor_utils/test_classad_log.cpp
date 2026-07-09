/***************************************************************
 *
 * Copyright (C) 2016, Condor Team, Computer Sciences Department,
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

// unit tests for the functions related to the MACRO_SET data structure
// used by condor's configuration and submit language.

#include "condor_common.h"
#include "condor_config.h"
#include "param_info.h"
#include "match_prefix.h"
#include "condor_random_num.h" // so we can force the random seed.
#include "filename_tools.h"
#include "directory_util.h"
#include "proc.h"
#include "classad_log.h"
#include "classad_collection.h"
//#include "condor_attributes.h"
//#include "condor_holdcodes.h"
#include <map>

int fail_count = 0;
bool dash_verbose2 = false;
std::string longest_log_line;

// accumulate a randomly ordered set of sizes into a vector that holds the top N sizes
// TODO: implement using binary lookup for insertion point?
static void top_n(std::vector<size_t> & top, size_t cb, size_t N) {
	for (size_t ix = 0; ix < N; ++ix) {
		if (ix > top.size()) {
			top.push_back(cb);
			return;
		}
		if (top[ix] == cb) {
			return;
		}
		if (top[ix] > cb) continue;
		if (top[ix] < cb) {
			top.insert(top.begin()+ix, cb);
			if (top.size() > N) { top.pop_back(); }
			break;
		}
	}
}

// An implementation of LoggableClassAdTable (which is used by the ClassadLog class).
// This implentations stores ads, and tracks the size of temporary LogRecord(s) that
// are used to populate it.
class ClassAdLogTableWithStats : public LoggableClassAdTable {
public:
	ClassAdLogTableWithStats() = default;
	virtual ~ClassAdLogTableWithStats() { clear(); };

	// before loading the job queue, we can tell it how to interpret the keys
	bool has_job_keys() { return has_job_id_keys; }
	void set_job_keys() { has_job_id_keys = true; }
	void set_string_keys() { has_job_id_keys = false; }
	void keep_removed_ads() { save_removed_ads = true; }
	void delete_removed_ads() { save_removed_ads = false; clear_removed(); }

	// Stash a single ad. This is used to delay deletion of a classad until
	// after remove is called, so we can choose to save it in a deletion collection.
	// The previous stashed ad or null is returned which the caller should delete.
	ClassAd * Stash(ClassAd* ad) {
		if ( ! save_removed_ads) return ad;
		ClassAd * tmp = stashed_ad;
		if (ad == stashed_ad) return nullptr;
		stashed_ad = ad;
		return tmp;
	}

	virtual bool lookup(const char * key, ClassAd*& out) {
		auto it = table.find(key);
		if (it != table.end()) {
			out = it->second;
			return true;
		}
		return false;
	}
	virtual bool remove(const char * key) {
		auto found = table.find(key);
		if (found != table.end()) {
			// the caller will have stashed the ad before calling remove on the key
			if (stashed_ad) {
				auto it = remove_table.emplace(key, stashed_ad);
				if (it.second) { stashed_ad = nullptr; } // we inserted the ad, so unstash it.
			}
			stats[key].tot.remove += 1;
			table.erase(found);
			return true;
		}
		return false;
	}
	virtual bool insert(const char * key, ClassAd * ad) {
		stats[key].tot.add += 1;
		auto it = table.emplace(key, ad); return it.second;
	}
	virtual void startIterations() { cur_iter=table.begin(); } // begin iterations
	virtual bool nextIteration(const char*& key, ClassAd*&ad) {
		if (cur_iter == table.end()) {
			key = nullptr;
			ad = nullptr;
			return false;
		}
		key = cur_iter->first.c_str();
		ad = cur_iter->second;
		++cur_iter;
		return true;
	}
	// statistics
	void set_attr(const char * key, const char * name, const char * value) {
		size_t cbh = strlen(key) + 3 + 2 + 1; // 3 digit OP, 3 spaces, 1 newline
		size_t cbn = strlen(name);
		size_t cbv = strlen(value);
		largest_attr = MAX(largest_attr,cbn);
		largest_value = MAX(largest_value,cbv);
		// global totals by key
		auto & st = stats[key];
		st.tot.assign += 1;
		st.tot.hdr_size += cbh;
		st.tot.attr_size += cbn;
		st.tot.value_size += cbv;
		// by key/attr
		auto & ast = st.by_attr[name];
		ast.assign += 1;
		ast.hdr_size += cbh;
		ast.attr_size += cbn;
		ast.value_size += cbv;
		ast.active_size = cbv; // used by dump_stats to calculate overwritten size
		// global by attr
		auto & bat = by_attr[name];
		bat.assign += 1;
		bat.hdr_size += cbh;
		bat.attr_size += cbn;
		bat.value_size += cbv;
	}
	std::string to_cluster_key(const std::string & key) {
		std::string ckey;
		ckey.reserve(key.size()+2);
		if (key[0] != '0') ckey = "0";
		size_t pos = key.find('.');
		ckey += key.substr(0,pos);
		ckey += ".-1";
		return ckey;
	}
	void get_record_name(const std::string & key, std::string & name, std::string & mytype) {
		name.clear();
		mytype.clear();
		if ( ! has_job_id_keys) {
			name = key;
			auto it = table.find(key);
			if (it == table.end()) { it = remove_table.find(key); }
			if (it != table.end()) {
				it->second->LookupString("Name", name);
				it->second->LookupString("MyType", mytype);
			}
			return;
		}
		auto it = table.find(key);
		if (it == table.end()) { it = remove_table.find(key); }
		if (it != table.end()) {
			if ( ! it->second->LookupString("Owner", name)) {
				it->second->LookupString("Name", name);
			}
			if (key == "0.0") {
				mytype = "<info>";
				name = "header";
			} else {
				it->second->LookupString("MyType", mytype);
			}
		}
	}
	void dump_stats(FILE* out, int num_clusters, int num_attrs, int num_users) {
		std::string name, mytype, ckey;

		basic_stats overall;
		basic_stats removed_ads;
		basic_stats overwritten_attrs;
		basic_stats removed_attrs;
		std::map<std::string, basic_stats> by_name{};
		std::map<std::string, basic_stats> by_cluster{};
		std::multimap<size_t, std::string> lines;
		size_t by_cluster_thresh=0;
		size_t by_attr_thresh=0;
		size_t by_name_thresh=0;

		for (auto & [key,st] : stats) {
			st.tot.count = 1; // so when we sum by cluster and by name, we count records
			overall += st.tot;
			ClassAd * ad = nullptr;
			if (lookup(key.c_str(),ad)) {
				for (const auto &[attr,ast] : st.by_attr) {
					if (ad->Lookup(attr)) {
						if (ast.value_size > ast.active_size) {
							overwritten_attrs.value_size += ast.value_size - ast.active_size;
						}
						overwritten_attrs.count += 1;
						if (ast.assign > 1) {
							int num_over = ast.assign-1;
							overwritten_attrs.remove += num_over;
							overwritten_attrs.hdr_size += (key.size()+3+2+1) * num_over;
							overwritten_attrs.attr_size += attr.size() * num_over;
						}
					} else { // attr was removed
						removed_attrs.count += 1;
						removed_attrs.remove += ast.assign;
						removed_attrs.hdr_size += ast.hdr_size;
						removed_attrs.attr_size += ast.attr_size;
						removed_attrs.value_size += ast.value_size;
					}
				}
			} else {
				removed_ads += st.tot;
			}

			get_record_name(key, name, mytype);
			if (has_job_id_keys) {
				if (name.empty() || key[0] != '0') {
					ckey = to_cluster_key(key);
					get_record_name(ckey, name, mytype);
					by_cluster[ckey] += st.tot;
				} else if (key.ends_with("-1")) {
					by_cluster[key] += st.tot;
				}
			} else {
				// TODO: create key groupings for accountant log
				//by_cluster[key] += st.tot;
			}
			by_name[name] += st.tot;
		}

		std::vector<size_t> top_by_cluster; top_by_cluster.push_back(0);
		for (auto const &[_,bat] : by_cluster) {
			size_t cb = bat.attr_size + bat.value_size;
			top_n(top_by_cluster, cb, num_clusters);
		}
		by_cluster_thresh = top_by_cluster.back();

		if (has_job_id_keys) {
			fprintf(out,     "KEY        \tNAME\tMYTYPE\t    ADD\t REMOVE\t  SET_ATTR\t ATTR_SIZE\tVALUE_SIZE\tHDR+ATTR+VALUE (MB)\n");
			fprintf(out, "--- TOP USAGE BY JOBLIST ---\n");
			for (auto const & [key,st] : stats) {
				size_t totsize = st.tot.attr_size + st.tot.value_size;
				const auto * ptot = &st.tot;
				const char * pkey = key.c_str();
				get_record_name(key, name, mytype);
				if (name.empty() && key[0] != '0') {
					if (dash_verbose2) {
						// get Owner from the cluster key
						ckey = to_cluster_key(key);
						get_record_name(ckey, name, mytype);
					} else {
						// just don't print proc ads at all
						continue;
					}
				} else if ( ! dash_verbose2 && key[0] == '0') {
					ckey = key.substr(1, key.find('.'));
					pkey = ckey.c_str();
					ptot = &by_cluster[key];
					totsize = ptot->attr_size + ptot->value_size;
					if (totsize < by_cluster_thresh) continue;
				}

				auto & tot = *ptot;
				double fltot = (double)(totsize + tot.hdr_size) / (1024*1024);
				fprintf(out, "%-10s\t%s\t%s\t%7d\t%7d\t%10d\t%10zu\t%10zu\t%10.1f\n",
					pkey, name.c_str(), mytype.c_str(),
					tot.add, tot.remove, tot.assign, tot.attr_size, tot.value_size, fltot);
			}
		} else if (by_cluster_thresh) {
			fprintf(out,     "KEY        \t\t\t    ADD\t REMOVE\t  SET_ATTR\t ATTR_SIZE\tVALUE_SIZE\tHDR+ATTR+VALUE (MB)\n");
			fprintf(out, "--- TOP USAGE BY KEY ---\n");
			for (auto const & [key,st] : stats) {
				size_t totsize = st.tot.attr_size + st.tot.value_size;
				if (totsize < by_cluster_thresh) continue;
				const auto * ptot = &st.tot;
				const char * pkey = key.c_str();
				get_record_name(key, name, mytype);

				auto & tot = *ptot;
				double fltot = (double)(totsize + tot.hdr_size) / (1024*1024);
				fprintf(out, "%-30s\t%7d\t%7d\t%10d\t%10zu\t%10zu\t%10.1f\n",
					pkey,
					tot.add, tot.remove, tot.assign, tot.attr_size, tot.value_size, fltot);
			}
		} else {
			fprintf(out,     "KEY        \t\t\t       \t       \t  SET_ATTR\t ATTR_SIZE\tVALUE_SIZE\tHDR+ATTR+VALUE (MB)\n");
			num_users = MAX(num_users, num_clusters);
		}

		// memory useage by attribute
		std::vector<size_t> top_by_attr; top_by_attr.push_back(0);
		for (auto const &[attr,bat] : by_attr) {
			size_t cb = bat.attr_size + bat.value_size;
			top_n(top_by_attr, cb, num_attrs);
		}
		by_attr_thresh = top_by_attr.back();

		fprintf(out, "\n--- TOP USAGE BY ATTRIBUTE ---\n");
		for (auto const &[attr,bat] : by_attr) {
			size_t totsize = bat.attr_size + bat.value_size;
			if ( ! dash_verbose2) {
				if (totsize < by_attr_thresh) continue;
			}
			size_t totcb = totsize + bat.hdr_size;
			double fltot = (double)(totcb) / (1024*1024);
			auto xx = lines.emplace(totcb, "");
			formatstr(xx->second, "%-46s\t%10d\t%10zu\t%10zu\t%10.1f", attr.c_str(), bat.assign, bat.attr_size, bat.value_size, fltot);
		}
		for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
			fprintf(out, "%s\n", it->second.c_str());
		}

		// memory useage by owner/project for job_queue and by key for accountant log
		std::vector<size_t> top_by_name; top_by_name.push_back(0);
		for (auto const &[own,bat] : by_name) {
			size_t cb = bat.attr_size + bat.value_size;
			top_n(top_by_name, cb, num_users);
		}
		by_name_thresh = top_by_name.back();

		fprintf(out, "\n--- TOP USAGE BY NAME ---\n");
		lines.clear();
		for (auto const &[own,bat] : by_name) {
			size_t totsize = bat.attr_size + bat.value_size;
			if ( ! dash_verbose2) {
				if (totsize < by_name_thresh) continue;
			}
			size_t totcb = totsize + bat.hdr_size;
			double fltot = (double)(totcb) / (1024*1024);
			auto xx = lines.emplace(totcb, "");
			formatstr(xx->second, "%-46s\t%10d\t%10zu\t%10zu\t%10.1f", own.c_str(), bat.assign, bat.attr_size, bat.value_size, fltot);
		}
		for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
			fprintf(out, "%s\n", it->second.c_str());
		}

		fprintf(out, "\n--- SUMMARY ----\n");
		basic_stats & st = overall;
		double fltot = (double)(st.attr_size + st.value_size + st.hdr_size) / (1024*1024);
		int num_removed = (int)remove_table.size();
		fprintf(out, "%-30s\t%7d\t%7d\t%10d\t%10zu\t%10zu\t%10.1f\n", "<overall>", st.count, num_removed, st.assign, st.attr_size, st.value_size, fltot);
		fprintf(out, "%-46s\t          \t%10zu\t%10zu\n", "<largest>", largest_attr, largest_value);
		st = removed_ads;
		fltot = (double)(st.attr_size + st.value_size + st.hdr_size) / (1024*1024);
		fprintf(out, "%-46s\t%10d\t%10zu\t%10zu\t%10.1f\n", "<removed_ads>", st.remove, st.attr_size, st.value_size, fltot);
		st = removed_attrs;
		fltot = (double)(st.attr_size + st.value_size + st.hdr_size) / (1024*1024);
		fprintf(out, "%-46s\t%10d\t%10zu\t%10zu\t%10.1f\n", "<removed_attrs>", st.remove, st.attr_size, st.value_size, fltot);
		st = overwritten_attrs;
		fltot = (double)(st.attr_size + st.value_size + st.hdr_size) / (1024*1024);
		fprintf(out, "%-46s\t%10d\t%10zu\t%10zu\t%10.1f\n", "<overwritten_attrs>", st.remove, st.attr_size, st.value_size, fltot);
	}

	ClassAd * is_removed_ad(const char * key)
	{
		auto it = remove_table.find(key);
		if (it != remove_table.end()) {
			return it->second;
		}
		return nullptr;
	}

	void clear_removed()
	{
		delete stashed_ad; stashed_ad = nullptr;
		for (auto & [_,ad] : remove_table) { delete ad; }
		remove_table.clear();
	}
	void clear()
	{
		for (auto & [_,ad] : table) { delete ad; }
		table.clear();
		stats.clear();
		by_attr.clear();
		largest_attr = largest_value = 0;
		clear_removed();
	}

	void chain_ads() {
		if ( ! has_job_id_keys) return;
		std::string ckey;
		for (auto &[key,ad] : table) {
			if (key[0] != '0') {
				ckey = to_cluster_key(key);
				auto it = table.find(ckey);
				if (it != table.end()) { ad->ChainToAd(it->second); }
			}
		}
		for (auto &[key,ad] : remove_table) {
			if (key[0] != '0') {
				ckey = to_cluster_key(key);
				auto it = table.find(ckey);
				if (it == table.end()) { it = remove_table.find(ckey); }
				if (it != table.end()) { ad->ChainToAd(it->second); }
			}
		}
	}
	void unchain_ads() {
		if ( ! has_job_id_keys) return;
		for (auto &[key,ad] : table) { ad->Unchain(); }
		for (auto &[key,ad] : remove_table) { ad->Unchain(); }
	}

	const std::map<std::string, ClassAd*> & ads() const { return table; }
	const std::map<std::string, ClassAd*> & removed_ads() const { return table; }

protected:
	bool has_job_id_keys{true};
	bool save_removed_ads{true};
	ClassAd * stashed_ad{nullptr};
	std::map<std::string, ClassAd*> table{};
	std::map<std::string, ClassAd*>::const_iterator cur_iter{};

	std::map<std::string, ClassAd*> remove_table{};

	// statistics
	struct basic_stats {
		int add{0};
		int remove{0};
		int assign{0};
		int count{0}; // used to count jobs in a cluster
		size_t hdr_size{0};
		size_t attr_size{0};
		size_t value_size{0};
		size_t active_size{0};
		basic_stats operator+=(const basic_stats & that) {
			count += that.count;
			add += that.add;
			remove += that.remove;
			assign += that.assign;
			hdr_size += that.hdr_size;
			attr_size += that.attr_size;
			value_size += that.value_size;
			active_size += that.active_size;
			return *this;
		}
	};
	struct stats_by_key {
		basic_stats tot;
		std::map<std::string, basic_stats, CaseIgnLTYourString> by_attr;
	};
	std::map<std::string, stats_by_key> stats{};
	std::map<std::string, basic_stats, CaseIgnLTYourString> by_attr{};
	size_t largest_attr{0};
	size_t largest_value{0};
};

#define FAST_LOG_SET_ATTRIBUTE 1

class LogSetAttributeFast : public LogRecord {
public:
	LogSetAttributeFast() { op_type = CondorLogOp_SetAttribute; };
	virtual ~LogSetAttributeFast() {
		delete key; key = nullptr;
#ifdef FAST_LOG_SET_ATTRIBUTE
		name = nullptr;
		value = nullptr;
#else
		delete name; name = nullptr;
		delete value; value = nullptr;
#endif
		delete value_expr; value = nullptr;
	};
	int Play(void *data_structure) {
		// data_structure should be derived from LoggableClassAdTable *
		ClassAdLogTableWithStats *table = (ClassAdLogTableWithStats *)data_structure;
		int rval;
		ClassAd *ad = 0;
		if ( ! table->lookup(key, ad))
			return -1;

		// Insert via cache saves ~20% memory, but not time.
		//if (ad->InsertViaCache(name, value)) {
		if (ad->AssignExpr(name, value)) {
			table->set_attr(key, name, value);
			rval = TRUE;
		} else {
			rval = FALSE;
		}
		return rval;
	}
	virtual char const *get_key() { return key; }
	char const *get_name() { return name ? name : ""; }
	char const *get_value() { return value ? value : ""; }
	ExprTree* get_expr() {
		if (value && *value && ! value_expr) {
			ParseClassAdRvalExpr(value, value_expr);
		}
		return value_expr;
	}

private:
	virtual int WriteBody(FILE* /*fp*/) {
		return -1;
	}
	virtual int ReadBody(FILE* fp)
	{
#ifdef FAST_LOG_SET_ATTRIBUTE
		int rval = 0;

		std::string_view line = readline_fast(fp);
		if (line.empty()) return -1;

		if (line.size() > longest_log_line.size()) {
			longest_log_line = line;
		}
	#if 1 // single allocation
		if (key) free(key);

		while ( ! line.empty() && isspace(line.front())) line.remove_prefix(1);
		while ( ! line.empty() && isspace(line.back())) line.remove_suffix(1);
		if (line.empty()) return -1;

		key = strdup(line.data());
		char * p = key;
		char* endp = p + line.size();

		while (p < endp && ! isspace(*p)) ++p;
		if (p >= endp) {
			return -1;
		}
		rval += (int)(p - key);
		*p++ = 0;
		while (isspace(*p)) ++p;

		name = p;
		while (p < endp && ! isspace(*p)) ++p;
		if (p >= endp) {
			return -1;
		}
		rval += (int)(p - name);
		*p++ = 0;
		while (isspace(*p)) ++p;

		if (p >= endp) {
			return -1;
		}
		value = p;
		rval += (int)(endp - value);
	#else // separate allocation per string
		char* p = const_cast<char*>(line.data());
		char* endp = p + line.size();
		while (isspace(*p)) ++p;
		const char *k = p;
		while (p < endp && ! isspace(*p)) ++p;
		if (p >= endp) {
			return -1;
		}
		rval += (int)(p - k);
		*p++ = 0;

		while (isspace(*p)) ++p;
		const char *a = p;
		while (p < endp && ! isspace(*p)) ++p;
		if (p >= endp) {
			return -1;
		}
		rval += (int)(p - a);
		*p++ = 0;

		while (isspace(*p)) ++p;
		if (p >= endp) {
			return -1;
		}
		rval += (int)(endp - p);

		value = strdup(p);
		key = strdup(k);
		name = strdup(a);
	#endif

		return rval;
#else
		int rval, rval1;

		if (key) free(key);
		rval1 = readword(fp, key);
		if (rval1 < 0) {
			return rval1;
		}

		if (name) free(name);
		rval = readword(fp, name);
		if (rval < 0) {
			return rval;
		}
		rval1 += rval;

		if (value) free(value);
		rval = readline(fp, value);
		if (rval < 0) {
			return rval;
		}

		return rval + rval1;
#endif
	}

	char *key{nullptr};
	char *name{nullptr};
	char *value{nullptr};
	ExprTree* value_expr{nullptr};
};

class LogDestroyClassAdSafe : public LogRecord {
public:
	LogDestroyClassAdSafe(const ConstructLogEntry & ctor_in = DefaultMakeClassAdLogTableEntry) : ctor(ctor_in) {
		op_type = CondorLogOp_DestroyClassAd;
	}
	virtual ~LogDestroyClassAdSafe() { free(key); key = nullptr; }
	int Play(void *data_structure);
	virtual char const *get_key() { return key; }

private:
	virtual int WriteBody(FILE* fp) {
		size_t r=fwrite(key, sizeof(char), strlen(key), fp);
		return (r < strlen(key)) ? -1 : (int)r;
	}
	virtual int ReadBody(FILE* fp) {
		free(key); key = nullptr;
		return readword(fp, key);
	}

	const ConstructLogEntry & ctor;
	char *key{nullptr};
};


class MyConstructJobQueueLogTableEntry : public ConstructLogEntry
{
public:
	MyConstructJobQueueLogTableEntry(ClassAdLogTableWithStats & _table) : table(_table) {}
	virtual ClassAd* New(const char * /*key*/, const char * /*mytype*/) const { return new ClassAd(); }
	virtual void Delete(ClassAd*& val) const { delete val; }
	virtual LogRecord * NewLogRec(int optype) const { 
		if (optype == CondorLogOp_SetAttribute) {
			return new LogSetAttributeFast();
		} else
		if (optype == CondorLogOp_DestroyClassAd) {
			return new LogDestroyClassAdSafe(*this);
		}
		return MakeLogRecord(optype, *this);
	}
	ClassAdLogTableWithStats & table;
};

int LogDestroyClassAdSafe::Play(void * data_structure) {
	ClassAdLogTableWithStats *table = (ClassAdLogTableWithStats *)data_structure;
	ClassAd *ad;

	if ( ! table->lookup(key, ad)) {
		return -1;
	}

	// we give the ad back to the table to stash, and delete what it returns
	ad = table->Stash(ad);
	ctor.Delete(ad);
	return table->remove(key) ? 0 : -1;
}

bool load_classad_log(ClassAdLogTableWithStats & classad_log, bool verbose, const char * filename)
{
	//unsigned long hist_seq=1;
	//time_t birthdate = 0;
	//bool is_clean = false;
	bool needs_clean = false;
	Transaction * active_transaction = NULL;
	MyConstructJobQueueLogTableEntry maker(classad_log);

#ifdef FAST_LOG_SET_ATTRIBUTE
	LogRecord::reserve_linebuf(1024*1024); // reserve a 1MB log record read buffer for LogSetAttributeFast
	//LogRecord::reserve_linebuf(128);
	FILE *log_fp = safe_fopen_no_create_follow(filename, "rb");
#else
	FILE *log_fp = safe_fopen_no_create_follow(filename, "r");
#endif
	if ( ! log_fp) {
		fprintf(stderr, "failed to open classad log %s, errno = %d\n", filename, errno);
		++fail_count;
		return false;
	}

	// Read all of the log records
	LogRecord *log_rec;
	unsigned long count = 0;
	unsigned long transaction_count = 0;
	long long next_log_entry_pos = 0;
	long long curr_log_entry_pos = 0;
	while ((log_rec = ReadLogEntry(log_fp, 1+count, InstantiateLogEntry, maker)) != 0) {
		curr_log_entry_pos = next_log_entry_pos;
		next_log_entry_pos = ftell(log_fp);
		count++;
		// fprintf(stdout, "%d %s\n", log_rec->get_op_type(), log_rec->get_key());
		switch (log_rec->get_op_type()) {
		case CondorLogOp_Error:
			// this is defensive, ought to be caught in InstantiateLogEntry()
			fprintf(stderr, "ERROR: in log %s transaction record %lu was bad (byte offset %lld)\n", filename, count, curr_log_entry_pos);
			fclose(log_fp); log_fp = NULL;
			delete active_transaction;
			++fail_count;
			return false;
			break;
		case CondorLogOp_BeginTransaction:
			// this file contains transactions, so it must not
			// have been cleanly shut down
			//is_clean = false;
			if (active_transaction) {
				fprintf(stderr, "Warning: Encountered nested transactions, log may be bogus...\n");
			} else {
				active_transaction = new Transaction();
				transaction_count += 1;
			}
			delete log_rec;
			break;
		case CondorLogOp_EndTransaction:
			if (!active_transaction) {
				fprintf(stderr, "Warning: Encountered unmatched end transaction, log may be bogus...\n");
			} else {
				active_transaction->Commit(NULL, NULL, &maker.table);
				delete active_transaction;
				active_transaction = NULL;
			}
			delete log_rec;
			break;
		case CondorLogOp_LogHistoricalSequenceNumber:
			if(count != 1) {
				fprintf(stderr, "Warning: Encountered historical sequence number after first log entry (entry number = %ld)\n",count);
			}
			//hist_seq = ((LogHistoricalSequenceNumber *)log_rec)->get_historical_sequence_number();
			//birthdate = ((LogHistoricalSequenceNumber *)log_rec)->get_timestamp();
			delete log_rec;
			break;
		default:
			if (active_transaction) {
				active_transaction->AppendLog(log_rec);
			} else {
				log_rec->Play(&maker.table);
				delete log_rec;
			}
		}
	}
	long long final_log_entry_pos = ftell(log_fp);
	if( next_log_entry_pos != final_log_entry_pos ) {
		// The log file has a broken line at the end so we _must_
		// _not_ write anything more into this log.
		// (Alternately, we could try to clear out the broken entry
		// and continue writing into this file, but since we are about to
		// rotate the log anyway, we may as well just require the rotation
		// to be successful.  In the case where rotation fails, we will
		// probably soon fail to write to the log file anyway somewhere else.)
		fprintf(stderr, "Detected unterminated log entry\n");
		needs_clean = true;
	}
	if (active_transaction) {	// abort incomplete transaction
		delete active_transaction;
		active_transaction = NULL;

		if( !needs_clean ) {
			// For similar reasons as with broken log entries above,
			// we need to force rotation.
			fprintf(stderr, "Detected unterminated transaction\n");
			needs_clean = true;
		}
	}

	fclose(log_fp); log_fp = nullptr;
	if (verbose) {
		fprintf(stdout, "\nLoaded %s : %zu log records (lines), %zu completed transactions\n", filename, (size_t)count, (size_t)transaction_count);
		//fprintf(stderr, "LogReader::linebuf_size = %zu\n", LogRecord::linebuf_size());
		//fprintf(stdout, "LongestRecord = %s\n", longest_log_line.c_str());
	}
	return true;
}


void testing_job_queue(ClassAdLogTableWithStats & /*job_queue*/, bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_job_queue ----\n\n");
	}
}

void testing_accountant(ClassAdLogTableWithStats & /*acct_log*/, bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_accountant ----\n\n");
	}
}

void Usage(const char * appname, FILE * out)
{
	const char * p = appname;
	while (*p) {
		if (*p == '/' || *p == '\\') appname = p+1;
		++p;
	}
	fprintf(out, "Usage: %s [<opts>] <source> [<queryopts> | -test[:<tests> ]\n"
		"  Where <opts> is one of\n"
		"    -help\t\tPrint this message\n"
		"    -verbose[:2}\tMore verbose output\n"
		"    -debug[:<flags>]\tSend log messages to stderr. default flags is D_FULLDEBUG\n"
		"\n  <source> is a classad transaction log file that is read\n"
		"    -job-queue <file>\tRead a job_queue.log file\n"
		"    -accountant <file>\tRead an Accoutantnew.log file\n"
		"\n  <queryopts> are queries run against the classad collection\n"
		"    -long[:fmt]\t\tOutput results in the given format\n"
		"    -raw\t\tOutput unmodified ads. Otherwise ads are chained and key attributes added.\n"
		"    -constraint <expr>\tOutput only ads which match the expression.\n"
		"\n  <tests> is one or more letters choosing specific subtests (future)\n"
		//"    j  job_queue tests\n"
		//"    a  accounting tests\n"
		"\n  This tool reads a classad transaction log and builds a collection of ads.\n"
		"  It then runs tests, runs a query and prints the results, or prints statistics.\n"
		"  Statistics are printed if no test or query is specified.\n"
		"  This program returns 0 if all tests/queries succeed, 1 if any tests fails.\n\n"
		, appname);
}

// runs all of the tests in non-verbose mode by default (i.e. printing only failures)
// individual groups of tests can be run by using the -t:<tests> option where <tests>
// is one or more of 
//   j   testing_job_queue
//   a   testing_accountant
//
int main( int /*argc*/, const char ** argv) {

	int test_flags = 0;
	const char * pcolon;
	bool other_arg = false;
	bool dash_verbose = false;
	bool dash_long = false;
	bool dash_raw_ads = false;
	const char * log_filename = nullptr;
	ClassAdLogTableWithStats classad_log;
	ClassAdFileParseType::ParseType dash_long_format = ClassAdFileParseType::Parse_auto;
	ConstraintHolder constr;

	// if we don't init dprintf, calls to it will be will be malloc'ed and stored
	// for later. this form of init will match the fewest possible issues.
	dprintf_config_tool_on_error("D_ERROR");
	dprintf_OnExitDumpOnErrorBuffer(stderr);

	for (int ii = 1; argv[ii]; ++ii) {
		const char *arg = argv[ii];
		if (is_dash_arg_colon_prefix(arg, "verbose", &pcolon, 1)) {
			dash_verbose = 1;
			if (pcolon && pcolon[1]=='2') { dash_verbose2 = true; }
		} else if (is_dash_arg_prefix(arg, "help", 1)) {
			Usage(argv[0], stdout);
			return 0;
		} else if (is_dash_arg_colon_prefix(arg, "test", &pcolon, 1)) {
			if (pcolon) {
				while (*++pcolon) {
					switch (*pcolon) {
					case 'j': test_flags |= 0x0111; break; // job_queue
					case 'a': test_flags |= 0x0222; break; // accountant
					}
				}
			} else {
				test_flags = 3;
			}
		} else if (is_dash_arg_prefix(arg, "job-queue", 3)) {
			log_filename = argv[ii+1];
			if (log_filename && (log_filename[0] != '-' || log_filename[1] != 0)) {
				++ii;
			} else {
				fprintf(stderr, "-job-queue requires a filename argument\n");
				return 1;
			}
			other_arg = true;
			classad_log.set_job_keys();
		} else if (is_dash_arg_prefix(arg, "accountant", 3)) {
			log_filename = argv[ii+1];
			if (log_filename && (log_filename[0] != '-' || log_filename[1] != 0)) {
				++ii;
			} else {
				fprintf(stderr, "-accountant requires a filename argument\n");
				return 1;
			}
			other_arg = true;
			classad_log.set_string_keys();
		} else if (is_dash_arg_colon_prefix(arg, "debug", &pcolon, 3)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		} else if (is_dash_arg_colon_prefix (arg, "long", &pcolon, 1)) {
			dash_long = 1;
			other_arg = true;
			if (pcolon) {
				for (const auto &opt: StringTokenIterator(++pcolon)) {
					if (YourString(opt) == "nosort") {
						// print_attrs_in_hash_order = true;
					} else {
						dash_long_format = parseAdsFileFormat(opt.c_str(), dash_long_format);
					}
				}
			}
		} else if (is_dash_arg_prefix(arg, "raw", 3)) {
			dash_raw_ads = true;
		} else if (is_dash_arg_prefix (arg, "constraint", 1)) {
			// make sure we have at least one more argument
			if ( ! argv[ii+1]) {
				fprintf( stderr, "Error: -constraint requires another parameter\n");
				exit(1);
			}
			const char * constraint = argv[++ii];
			if ( ! constr.parse(constraint)) {
				fprintf (stderr, "Error: Argument %d (%s) is not a valid constraint\n", ii, constraint);
				exit (1);
			}
		} else {
			fprintf(stderr, "unknown argument %s\n", arg);
			Usage(argv[0], stderr);
			return 1;
		}
	}
	// if no log file specified, just print usage and exit.
	if ( ! log_filename) { Usage(argv[0], stdout); return 1; }

	if ( ! test_flags && ! other_arg) test_flags = -1;

	ClassAdReconfig(); // register the compat classad functions. 

	if (log_filename) {
		if (load_classad_log(classad_log, dash_verbose, log_filename)) {
			// if no tests, just dump job queue stats
			if ( ! test_flags && ! dash_long) { 
				int num_clusters = 8;
				int num_attrs = 16;
				int num_users = 4;
				classad_log.dump_stats(stdout, num_clusters, num_attrs, num_users);
			}
		}
	}
	if (dash_long) {
		std::string record_type;
		CondorClassAdListWriter writer(dash_long_format);
		if ( ! dash_raw_ads) classad_log.chain_ads(); // in case this is a job queue, we need this so we can evaluate a constraint
		ExprTree * constraint = constr.Expr();
		for (const auto & [key,ad] : classad_log.ads()) {
			if (dash_raw_ads) {
				// don't modify the ads
			} else if (classad_log.has_job_keys()) {
				// job queue keys that start with 0 are not Proc ads (cluster, user, header)
				// User and Project have the correct MyType but header records are mis-tagged as Job records
				if (key[0] == '0') {
					if (key == "0.0") { ad->Assign("MyType", "Header"); }
				}
			} else {
				const char * rt = key.c_str();
				const char * nm = strchr(rt,'.');
				if (nm) {
					if (key.starts_with("Customer.")) {
						record_type = "Accounting";
					} else {
						record_type = key.substr(0, nm - rt);
					}
					ad->Assign("MyType", record_type);
					++nm;
				} else {
					nm = rt;
				}
				ad->Assign("Name", nm);
			}
			if (constraint && ! EvalExprBool(ad, constraint)) continue;
			if (dash_raw_ads) { fprintf(stdout, "# %s\n", key.c_str()); }
			writer.writeAd(*ad, stdout);
		}
		writer.writeFooter(stdout);
	}

	if (test_flags & 0x0001) testing_job_queue(classad_log, dash_verbose);
	//if (test_flags & 0x0002) testing_accountant(dash_verbose);

	dprintf_SetExitCode(fail_count > 0);
	return fail_count > 0;
}
