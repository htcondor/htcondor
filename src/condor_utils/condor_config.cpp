/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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


/*

  This file implements config(), the function all daemons call to
  configure themselves.  It takes an optional argument which
  determines if config should be quiet or verbose on errors.  It
  defaults to verbose error reporting.

  There's also an entry point, config_host() where you pass in a
  string that should be filled in for HOSTNAME.  This is only used by
  special arguments to condor_config_val used by condor_init to
  bootstrap the installation process.

  When looking for the global config source, config() checks the
  "CONDOR_CONFIG" environment variable to find its location.  If that
  doesn't exist, we look in the following locations:

      1) ~/.condor/     # if not started as root
      2) /etc/condor/
      3) /usr/local/etc/
      4) ~condor/

  If none of the above locations contain a config source, config()
  prints an error message and exits.

  In each "global" config source, a list of "local" config files, or a single
  cmd whose output is to be piped in as the configuration settings can be
  specified.  If a cmd is specified, it is processed and its output used
  as the configuration data.  If a file or list of files is specified, each
  file given in the list is read and processed in order.  These lists can
  be used to specify both platform-specific config files and machine-specific
  config files, in addition to a single, pool-wide, platform-independent
  config file.

*/

#include "condor_common.h"
#include "condor_debug.h"
#include "pool_allocator.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "my_hostname.h"
#include "ipv6_hostname.h"
#include "condor_version.h"
#include "util_lib_proto.h"
#include "my_username.h"
#ifdef WIN32
#	include "ntsysinfo.WINDOWS.h"		// for WinNT getppid
#	include <locale.h>
#endif
#include "directory.h"			// for StatInfo
#include "condor_distribution.h"
#include "condor_environ.h"
#include "setenv.h"
#include "condor_uid.h"
#include "condor_mkstemp.h"
#include "basename.h"
#include "subsystem_info.h"
#include "param_info.h"
#include "param_info_tables.h"
#include "condor_regex.h"
#include "filename_tools.h"
#include "which.h"
#include "classad_helpers.h"
#include "CondorError.h"
#include "../condor_sysapi/sysapi.h"
#include <algorithm> // for std::sort


// define this to keep param who's values match defaults from going into the runtime param table.
#define DISCARD_CONFIG_MATCHING_DEFAULT
// define this to parse for #opt:newcomment/#opt:oldcomment to decide commenting rules
#define PARSE_CONFIG_TO_DECIDE_COMMENT_RULES

// Function prototypes
bool real_config(const char* host, int wantsQuiet, int config_options, const char * root_config);
//int Read_config(const char*, int depth, MACRO_SET& macro_set, int, bool, const char * subsys, std::string & errmsg);
bool Test_config_if_expression(const char * expr, bool & result, std::string & err_reason, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx);
//bool is_piped_command(const char* filename);
bool is_valid_command(const char* cmdToExecute);
void init_tilde();
void fill_attributes();
void check_domain_attributes();
void reinsert_specials(const char* host);
void process_config_source(const char*, int depth, const char*, const char*, int);
void process_locals( const char*, const char*);
void process_directory( const char* dirlist, const char* host);
static int  process_dynamic_configs();
void do_smart_auto_use(int options);

static const char* find_global(int options, std::string & config_file);
static const char* find_file(const char*, const char*, int config_options, std::string & config_file);

// pull from config.cpp
void param_default_set_use(const char * name, int use, MACRO_SET & set);


// Global variables
static MACRO_DEFAULTS ConfigMacroDefaults = { 0, NULL, NULL };
static MACRO_SET ConfigMacroSet = {
	0, 0,
	/* CONFIG_OPT_WANT_META | CONFIG_OPT_KEEP_DEFAULT | */ 0,
	0, NULL, NULL, ALLOCATION_POOL(), std::vector<const char*>(), &ConfigMacroDefaults, NULL };
const MACRO_SOURCE DetectedMacro = { true,  false, 0, -2, -1, -2 };
//const MACRO_SOURCE DefaultMacro  = { true,  false, 1, -2, -1, -2 };
const MACRO_SOURCE EnvMacro      = { false, false, 2, -2, -1, -2 };
const MACRO_SOURCE WireMacro     = { false, false, 3, -2, -1, -2 };

#ifdef _POOL_ALLOCATOR

// set the initial size of the system allocation for an empty allocation hunk
// this function is private to the pool, and not for external use.
//
void _allocation_hunk::reserve(int cb)
{
	if (this->pb != NULL && cb <= (this->cbAlloc - this->ixFree))
		return;

	if ( ! this->pb) {
#if 0 //def WIN32
		this->pb = (char*)VirtualAlloc(NULL, cb, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
		this->pb = (char*)malloc(cb);
#endif
		this->cbAlloc = cb;
	} else {
		// can't reserve memory if the hunk already has an allocation
	}
}

// release all pool memory back to the system.
//
void _allocation_pool::clear()
{
	for (int ii = 0; ii < this->cMaxHunks; ++ii) {
		if (ii > this->nHunk) break;
#if 0 //def WIN32
		if (this->phunks[ii].pb) { VirtualFree(this->phunks[ii].pb, this->phunks[ii].cbAlloc, MEM_RELEASE); }
#else
		if (this->phunks[ii].pb) { free(this->phunks[ii].pb); }
#endif
		this->phunks[ii].pb = NULL;
		this->phunks[ii].cbAlloc = 0;
		this->phunks[ii].ixFree = 0;
	}
	delete [] this->phunks;
	this->phunks = NULL;
	this->cMaxHunks = 0;
	this->nHunk = 0;
}

// swap the contents of one pool with another, this is done
// as part of the pool compaction process
//
void _allocation_pool::swap(struct _allocation_pool & other)
{
	int tmp_cMaxHunks =  this->cMaxHunks;
	int tmp_nHunk = this->nHunk;
	ALLOC_HUNK * tmp_phunks = this->phunks;
	this->cMaxHunks = other.cMaxHunks;
	this->nHunk = other.nHunk;
	this->phunks = other.phunks;
	other.cMaxHunks = tmp_cMaxHunks;
	other.nHunk = tmp_nHunk;
	other.phunks = tmp_phunks;
}

// calculate memory usage of the pool
//
// return value is memory usage
// number of system allocations is returned as cHunks
// sum of free space in all of the hunks is returned as cbFree.
//
int  _allocation_pool::usage(int & cHunks, int & cbFree)
{
	int cb = 0;
	cHunks = 0;
	cbFree = 0;
	for (int ii = 0; ii < this->cMaxHunks; ++ii) {
		if (ii > this->nHunk) break;
		ALLOC_HUNK * ph = &this->phunks[ii];
		if ( ! ph->cbAlloc || ! ph->pb)
			continue;
		++cHunks;
		cb += ph->ixFree;
		cbFree += (ph->cbAlloc - ph->ixFree) & ~(this->alignment()-1);
	}
	return cb;
}

// allocate a hunk of memory from the pool, and return a pointer to it.
//
char * _allocation_pool::consume(int cb, int cbAlign)
{
	if ( ! cb) return NULL;
	cbAlign = MAX(cbAlign, this->alignment());
	int cbConsume = (cb + cbAlign-1) & ~(cbAlign-1);
	if (cbConsume <= 0) return NULL;

	// if this is a virgin pool, give it a default reserve of 4 Kb
	if ( ! this->cMaxHunks || ! this->phunks) {
		this->cMaxHunks = 1;
		this->nHunk = 0;
		this->phunks = new ALLOC_HUNK[this->cMaxHunks];
		int cbAlloc = MAX(this->def_first_alloc(), cbConsume); // default first allocation is 4k
		this->phunks[0].reserve(cbAlloc);
	}

	ALLOC_HUNK * ph = NULL;
	int cbFree = 0;
	if (this->nHunk < this->cMaxHunks) {
		ph = &this->phunks[this->nHunk];
		// reduce calculated free space by the aligment requirement
		// this presumes withat ph->pb is always sufficiently aligned, which it should be
		// since we get it from malloc
		cbFree = ph->cbAlloc - ((ph->ixFree + cbAlign-1) & ~(cbAlign-1));
	}

	// do we need to allocate more hunks to service this request?
	//
	if (cbConsume > cbFree) {

		// if the current hunk has no allocation, give it a default allocation
		// equal to the previous hunk allocation * 2
		if (ph && ! ph->pb) {
			int cbAlloc = (this->nHunk > 0) ? (this->phunks[this->nHunk-1].cbAlloc * 2) : (16 * 1024);
			cbAlloc = MAX(cbAlloc, cbConsume);
			ph->reserve(cbAlloc);
		} else if (this->nHunk+1 >= this->cMaxHunks) {
			// if the hunk vector is fully populated, double it's size and transfer
			// the existing hunks into the new vector.
			ASSERT(this->nHunk+1 == this->cMaxHunks);
			ALLOC_HUNK * pnew = new ALLOC_HUNK[this->cMaxHunks*2];
			if ( ! pnew)
				return NULL;

			// transfer ownership of the existing hunks to the new hunkyness.
			for (int ii = 0; ii < this->cMaxHunks; ++ii) {
				pnew[ii] = this->phunks[ii];
				this->phunks[ii].pb = NULL;
			}
			delete [] this->phunks;
			this->phunks = pnew;
			this->cMaxHunks *= 2;
		}

		// if the current hunk has no allocation, give it a default allocation
		// equal to the previous hunk allocation * 2
		ph = &this->phunks[this->nHunk];
		if ( ! ph->pb) {
			int cbAlloc = (this->nHunk > 0) ? (this->phunks[this->nHunk-1].cbAlloc * 2) : (16 * 1024);
			cbAlloc = MAX(cbAlloc, cbConsume);
			ph->reserve(cbAlloc);
			SAL_assume(ph->pb != NULL);
		}

		// if the requested allocation doesn't fit in the current hunk, make a new hunk
		if (((ph->ixFree + cbAlign-1) & ~(cbAlign-1)) + cbConsume > ph->cbAlloc) {
			int cbAlloc = MAX(ph->cbAlloc * 2, cbConsume);
			ph = &this->phunks[++this->nHunk];
			ph->reserve(cbAlloc);
		}
	}

	// we want to align the return pointer as well as the size of the returned allocation
	// ixStart give an aligned return pointer. we zero fill before it if is not the same as ixFree
	// then zero fill at the end if we resized the requested allocation to align the size.
	int ixStart = (ph->ixFree + cbAlign-1) & ~(cbAlign-1);
	if (ixStart > ph->ixFree) memset(ph->pb + ph->ixFree, 0, ixStart - ph->ixFree);
	char * pb = ph->pb + ixStart;
	if (cbConsume > cb) memset(pb+cb, 0, cbConsume - cb);
	ph->ixFree = ixStart + cbConsume;
	return pb;
}

// copy arbitrary data into the pool and return a pointer to the copy
const char * _allocation_pool::insert(const char * pbInsert, int cbInsert)
{
	if ( ! pbInsert || ! cbInsert) return NULL;
	char * pb = this->consume(cbInsert, 1);
	if (pb) memcpy(pb, pbInsert, cbInsert);
	return pb;
}

// copy a single null terminate string into the pool and return a pointer to the copy
const char * _allocation_pool::insert(const char * psz)
{
	if ( ! psz) return NULL;
	int cb = (int)strlen(psz);
	if ( ! cb) return "";
	return this->insert(psz, cb+1);
}

// check to see if a given pointer is a pointer into the allocation pool
//
bool _allocation_pool::contains(const char * pb)
{
	if ( ! pb || ! this->phunks || ! this->cMaxHunks)
		return false;
	for (int ii = 0; ii < this->cMaxHunks; ++ii) {
		if (ii > this->nHunk) break;

		ALLOC_HUNK * ph = &this->phunks[ii];
		if ( ! ph->cbAlloc || ! ph->pb || ! ph->ixFree)
			continue;

		// if this address is within the allocation of this hunk, then
		// the pool contains this pointer.
		if (pb >= ph->pb && (int)(pb - ph->pb) < ph->ixFree)
			return true;
	}
	return false;
}

// make sure that the pool contains at least cbReserve in contiguous free space
void _allocation_pool::reserve(int cbReserve)
{
	// for now, just consume some memory, and then free it back to the pool
	this->free_everything_after(this->consume(cbReserve, 1));
}

// compact the pool, leaving at least this much free space.
void _allocation_pool::compact(int cbLeaveFree)
{
	if ( ! this->phunks || ! this->cMaxHunks)
		return;

	const int dont_bother_size = MAX(this->alignment(), 32); // don't bother freeing if savings < this.
	for (int ii = 0; ii < this->cMaxHunks; ++ii) {
		if (ii > this->nHunk) break;
		ALLOC_HUNK * ph = &this->phunks[ii];
		if (ph->pb && (ph->cbAlloc - ph->ixFree) > dont_bother_size) {
			int cbToFree = (ph->cbAlloc - ph->ixFree) & ~(this->alignment()-1);
			cbLeaveFree -= cbToFree;
			if (cbLeaveFree >= 0) {
				cbToFree = 0;
			} else {
				cbToFree = -cbLeaveFree & ~(this->alignment()-1);
				cbLeaveFree = 0;
			}
			if (cbToFree > dont_bother_size) {
				void * pb = realloc(ph->pb, ph->ixFree);
				ASSERT(pb == ph->pb);
				ph->cbAlloc = ph->ixFree;
			}
		}
	}
}

// free an allocation and everything allocated after it.
// may fail if pb is not the most recent allocation.
void _allocation_pool::free_everything_after(const char * pb)
{
	if ( ! pb || ! this->phunks || this->nHunk >= this->cMaxHunks) return;
	ALLOC_HUNK * ph = &this->phunks[this->nHunk];
	size_t cbUnwind = (ph->pb + ph->ixFree) - pb;
	if (cbUnwind > 0 && cbUnwind <= (size_t)ph->ixFree)
		ph->ixFree -= (int)cbUnwind;
}


#endif // _POOL_ALLOCATOR

static char* tilde = NULL;
static bool have_config_source = true;
static bool continue_if_no_config = false; // so condor_who won't exit if no config found.
extern bool condor_fsync_on;

std::string global_config_source;
std::vector<std::string> local_config_sources;
std::string user_config_source; // which of the files in local_config_sources is the user file


static void init_macro_eval_context(MACRO_EVAL_CONTEXT &ctx)
{
	ctx.init(get_mySubSystem()->getName(), 2);
	if (ctx.subsys && ! ctx.subsys[0]) ctx.subsys = NULL;
	ctx.localname = get_mySubSystem()->getLocalName();
	if (ctx.localname && ! ctx.localname[0]) ctx.localname = NULL;
}

bool config_continue_if_no_config(bool contin)
{
	bool old_contin = continue_if_no_config;
	continue_if_no_config = contin;
	return old_contin;
}

void config_dump_sources(FILE * fh, const char * sep)
{
	for (int ii = 0; ii < (int)ConfigMacroSet.sources.size(); ++ii) {
		fprintf(fh, "%s%s", ConfigMacroSet.sources[ii], sep);
	}
}

const char* config_source_by_id(int source_id)
{
	if (source_id >= 0 && source_id < (int)ConfigMacroSet.sources.size())
		return ConfigMacroSet.sources[source_id];
	// these are aliases used by param_names_for_summary for sorting the sources by priority
	if (source_id == summary_env_source_id) { return config_source_by_id(EnvMacro.id); }
	if (source_id == summary_wire_source_id) { return config_source_by_id(WireMacro.id); }
	return NULL;
}

void config_dump_string_pool(FILE * fh, const char * sep)
{
	int cEmptyStrings = 0;
	ALLOCATION_POOL * ap = &ConfigMacroSet.apool;
	for (int ii = 0; ii < ap->cMaxHunks; ++ii) {
		if (ii > ap->nHunk) break;
		ALLOC_HUNK * ph = &ap->phunks[ii];
		if ( ! ph->cbAlloc || ! ph->pb)
			continue;
		const char * psz = ph->pb;
		const char * pszEnd = ph->pb + ph->ixFree;
		while (psz < pszEnd) {
			int cch = (int)strlen(psz);
			if (cch > 0) {
				fprintf(fh, "%s%s", psz, sep);
			} else {
				++cEmptyStrings;
			}
			psz += cch+1;
		}
	}
	if (cEmptyStrings > 0) {
		fprintf(fh, "! %d empty strings found\n", cEmptyStrings);
	}
}

/* 
  A convenience function that calls param() then inserts items from the value
  into the given vector if they are not already there
*/
bool param_and_insert_unique_items(const char * param_name, std::vector<std::string> & items, bool case_sensitive /*=false*/)
{
	int num_inserts = 0;
	std::string value;
	if (param(value, param_name)) {
		for (auto& item: StringTokenIterator(value)) {
			if (case_sensitive) {
				if (contains(items, item)) continue;
			} else {
				if (contains_anycase(items, item)) continue;
			}
			items.emplace_back(item);
			++num_inserts;
		}
	}
	return num_inserts > 0;
}

/* 
  A convenience function that calls param() then inserts items from the value
  into the given classad:References set.  Useful whenever a param knob contains
  a string list of ClassAd attribute names, e.g. IMMUTABLE_JOB_ATTRS.
  Return true if given param name was found, false if not.
*/
bool
param_and_insert_attrs(const char * param_name, classad::References & attrs)
{
	auto_free_ptr value(param(param_name));
	if (value) {
		add_attrs_from_string_tokens(attrs, value);
		return true;
	}
	return false;
}

// Function implementations

void
config_fill_ad( ClassAd* ad, const char *prefix )
{
	const char * subsys = get_mySubSystem()->getName();
	std::vector<std::string> reqdAttrs;
	std::string param_name;

	if( !ad ) return;

	if ( ( NULL == prefix ) && get_mySubSystem()->hasLocalName() ) {
		prefix = get_mySubSystem()->getLocalName();
	}

	// <SUBSYS>_ATTRS is the proper form, set the string list from entries in the config value
	param_name = subsys; param_name += "_ATTRS";
	param_and_insert_unique_items(param_name.c_str(), reqdAttrs);

	// <SUBSYS>_EXPRS is deprecated, but still supported for now, we merge merge it into the existing list.
	param_name = subsys; param_name += "_EXPRS";
	param_and_insert_unique_items(param_name.c_str(), reqdAttrs);

	// SYSTEM_<SUBSYS>_ATTRS is the set of attrs that are required by HTCondor, this is a non-public config knob.
	formatstr(param_name, "SYSTEM_%s_ATTRS", subsys);
	param_and_insert_unique_items(param_name.c_str(), reqdAttrs);

	if (prefix) {
		// <PREFIX>_<SUBSYS>_ATTRS is additional attributes needed
		formatstr(param_name, "%s_%s_ATTRS", prefix, subsys);
		param_and_insert_unique_items(param_name.c_str(), reqdAttrs);

		// <PREFIX>_<SUBSYS>_EXPRS is deprecated, but still supported for now.
		formatstr(param_name, "%s_%s_EXPRS", prefix, subsys);
		param_and_insert_unique_items(param_name.c_str(), reqdAttrs);
	}

	for (const auto& attr: reqdAttrs) {
		auto_free_ptr expr(NULL);
		if (prefix) {
			formatstr(param_name, "%s_%s", prefix, attr.c_str());
			expr.set(param(param_name.c_str()));
		}
		if ( ! expr) {
			expr.set(param(attr.c_str()));
		}
		if ( ! expr) continue;

		if ( ! ad->AssignExpr(attr, expr.ptr())) {
			dprintf(D_ALWAYS,
					"CONFIGURATION PROBLEM: Failed to insert ClassAd attribute %s = %s.  The most common reason for this is that you forgot to quote a string value in the list of attributes being added to the %s ad.\n",
					attr.c_str(), expr.ptr(), subsys);
		}
	}
	
	/* Insert the version into the ClassAd */
	ad->Assign( ATTR_VERSION, CondorVersion() );

	ad->Assign( ATTR_PLATFORM, CondorPlatform() );
}

/*
static bool macro_set_default_sort(MACRO_ITEM const & a, MACRO_ITEM const & b)
{
	return strcasecmp(a.key, b.key) < 0;
}*/

//#define OPTIMIZE_COMPACT_STRING_POOL

// this is a helper class for sorting the param table and 
// the associated metadata table
//
class MACRO_SORTER {
public:
	MACRO_SET & set;
	MACRO_SORTER(MACRO_SET & setIn) : set(setIn) {}
	bool operator()(MACRO_META const & a, MACRO_META const & b) {
		int ixa = a.index;
		int ixb = b.index;
		if  ( !  ((ixa >= 0 && ixa < set.size) && (ixb >= 0 && ixb < set.size)) ) {
			return false;
		}
		return strcasecmp(set.table[ixa].key, set.table[ixb].key) < 0;
	};
	bool operator()(MACRO_ITEM const & a, MACRO_ITEM const & b) {
		return strcasecmp(a.key, b.key) < 0;
	};
};

void optimize_macros(MACRO_SET & set)
{
	if (set.size <= 1)
		return;

	// the metadata table has entries that give the index of the corresponding
	// entry in the param table so that we can sort it. So we have to
	// sort the metadata first, then the param table itself
	// and finally then fixup the indexes in the metadata table.
	//
	MACRO_SORTER sorter(set);
	if (set.metat) { std::sort(&set.metat[0], &set.metat[set.size], sorter); }
	std::sort(&set.table[0], &set.table[set.size], sorter);
	if (set.metat) { for (int ix = 0; ix < set.size; ++ix) { set.metat[ix].index = ix; } }
	set.sorted = set.size;

#ifdef OPTIMIZE_COMPACT_STRING_POOL
	// build a compact string pool.
	int cbFree, cHunks, cb = set.apool.usage(cHunks, cbFree);
	if (cbFree > 1024) {
		ALLOCATION_POOL tmp;
		tmp.reserve(cb);
		set.apool.swap(tmp);

		for (int ii = 0; ii < set.size; ++ii) {
			MACRO_ITEM * pi = &set.table[ii];
			if (tmp.contains(pi->key)) pi->key = set.apool.insert(pi->key);
			if (tmp.contains(pi->raw_value)) pi->raw_value = set.apool.insert(pi->raw_value);
		}

		for (int ii = 0; ii < (int)set.sources.size(); ++ii) {
			if (tmp.contains(set.sources[ii])) set.sources[ii] = set.apool.insert(set.sources[ii]);
		}

		tmp.clear();
		cb = set.apool.usage(cHunks, cbFree);
	}
#endif // OPTIMIZE_COMPACT_STRING_POOL
}

/*
Walks all found configuration entries looking for the
"forbidden string".  If said string is found, EXCEPT.

Output is via a giant EXCEPT string because the dprintf
system probably isn't working yet.
*/
const char * FORBIDDEN_CONFIG_VAL = "YOU_MUST_CHANGE_THIS_INVALID_CONDOR_CONFIGURATION_VALUE";
bool validate_config(bool abort_if_invalid, int opt)
{
	bool deprecation_check = (opt & CONFIG_OPT_DEPRECATION_WARNINGS);
	unsigned int invalid_entries = 0;
	unsigned int deprecated_entries = 0;
	std::string invalid_out = "The following configuration macros appear to contain default values that must be changed before Condor will run.  These macros are:\n";
	std::string deprecated_out;
	Regex re;
	if (deprecation_check) {
		int errcode, erroffset;
		// check for knobs of the form SUBSYS.LOCALNAME.*
		if (!re.compile("^[A-Za-z_]*\\.[A-Za-z_0-9]*\\.", &errcode, &erroffset, PCRE2_CASELESS)) {
			EXCEPT("Programmer error in condor_config: invalid regexp");
		}
	}

	HASHITER it = hash_iter_begin(ConfigMacroSet, HASHITER_NO_DEFAULTS);
	while( ! hash_iter_done(it) ) {
		const char * name = hash_iter_key(it);
		const char * val = hash_iter_value(it);
		if (val && strstr(val, FORBIDDEN_CONFIG_VAL)) {
			invalid_out += "   ";
			invalid_out += name;
			MACRO_META * pmet = hash_iter_meta(it);
			if (pmet) {
				invalid_out += " at ";
				param_append_location(pmet, invalid_out);
			}
			invalid_out += "\n";
			invalid_entries++;
		}
		if (deprecation_check && re.match(name)) {
			deprecated_out += "   ";
			deprecated_out += name;
			MACRO_META * pmet = hash_iter_meta(it);
			if (pmet) {
				deprecated_out += " at ";
				param_append_location(pmet, deprecated_out);
			}
			deprecated_out += "\n";
			deprecated_entries++;
		}
		hash_iter_next(it);
	}
	hash_iter_delete(&it);
	if(invalid_entries > 0) {
		if (abort_if_invalid) {
			EXCEPT("%s", invalid_out.c_str());
		} else {
			dprintf(D_ALWAYS, "%s", invalid_out.c_str());
			return false;
		}
	}
	if (deprecated_entries > 0) {
		dprintf(D_ALWAYS,
			"WARNING: Some configuration variables appear to be an unsupported form of SUBSYS.LOCALNAME.* override\n"
			"       The supported form is just LOCALNAME.* Variables are:\n%s",
			deprecated_out.c_str());
	}
	return true;
}

void foreach_param(int options, bool (*fn)(void* user, HASHITER& it), void* user)
{
	HASHITER it = hash_iter_begin(ConfigMacroSet, options);
	while ( ! hash_iter_done(it)) {
		if ( ! fn(user, it))
			break;
		hash_iter_next(it);
	}
	hash_iter_delete(&it);
}

void foreach_param_matching(Regex & re, int options, bool (*fn)(void* user, HASHITER& it), void* user)
{
	HASHITER it = hash_iter_begin(ConfigMacroSet, options);
	while ( ! hash_iter_done(it)) {
		const char *name = hash_iter_key(it);
		if (re.match(name)) {
			if ( ! fn(user, it))
				break;
		}
		hash_iter_next(it);
	}
	hash_iter_delete(&it);
}

// return a list of param names that match the given regex, this list is in hashtable order (i.e. no order)	

int param_names_matching(Regex& re, std::vector<std::string>& names) {
    const int s0 = (int)names.size();
    HASHITER it = hash_iter_begin(ConfigMacroSet);
    for (;  !hash_iter_done(it);  hash_iter_next(it)) {
		const char *name = hash_iter_key(it);
		if (re.match(name)) names.push_back(name);
	}
    hash_iter_delete(&it);
    return (int)names.size() - s0;
}

// return the param names that condor_config_val -summary would show (sort of)
// A difference is, the returned names will include the obsolete param names, unlike -summary
// this is intended for use by remote config val, where the caller can choose to ignore the obsolete names
int param_names_for_summary(std::map<int64_t, std::string>& names)
{
	_param_names_sumy_key key; key.all = 0;
	bool got_meta = false;
	HASHITER it = hash_iter_begin(ConfigMacroSet, HASHITER_SHOW_DUPS);
	while ( ! hash_iter_done(it)) {
		MACRO_META * pmeta = hash_iter_meta(it);
		if ( ! pmeta) break;
		got_meta = true;
		if ( ! pmeta->matches_default && ! pmeta->param_table /* && ! pmeta->inside */) {
			// build a key that will order the names by file and line since that is what summary does
			key.iter += 1;
			key.off = pmeta->source_meta_off;
			key.line = pmeta->source_line;
			key.sid = pmeta->source_id;
			// force Env and Wire to sort last since they win over others
			if (key.sid == EnvMacro.id) { key.sid = summary_env_source_id; }
			else if (key.sid == WireMacro.id) { key.sid = summary_wire_source_id; }

			names[key.all] = hash_iter_key(it);
		}
		hash_iter_next(it);
	}
	hash_iter_delete(&it);
	return got_meta;
}


// the generic config entry point for most call sites
bool config()
{
	return config_ex(CONFIG_OPT_WANT_META);
}

// the full-figured config entry point
bool config_ex(int config_options)
{
#ifdef WIN32
	char *locale = setlocale( LC_ALL, "English" );
	//dprintf ( D_LOAD | D_VERBOSE, "Locale: %s\n", locale );
	_set_fmode(_O_BINARY);
#endif
	bool wantsQuiet = config_options & CONFIG_OPT_WANT_QUIET;
	bool result = real_config(NULL, wantsQuiet, config_options, NULL);
	if (!result) { return result; }
	int validate_opt = config_options & (CONFIG_OPT_DEPRECATION_WARNINGS | CONFIG_OPT_WANT_QUIET);
	return validate_config(!(config_options & CONFIG_OPT_NO_EXIT), validate_opt);
}


bool
config_host(const char* host, int config_options, const char * root_config)
{
	bool wantsQuiet = config_options & CONFIG_OPT_WANT_QUIET;
	return real_config(host, wantsQuiet, config_options, root_config);
}

bool
real_config(const char* host, int wantsQuiet, int config_options, const char * root_config)
{
	const char* config_source = root_config;
	std::string config_file_tmp; // used as a temp buffer by find_global
	char* tmp = NULL;

	#ifdef WARN_COLON_FOR_PARAM_ASSIGN
	config_options |= CONFIG_OPT_COLON_IS_META_ONLY;
	#endif

	static bool first_time = true;
	if( first_time ) {
		first_time = false;
		init_global_config_table(config_options);
	} else {
			// Clear out everything in our config hash table so we can
			// rebuild it from scratch.
		clear_global_config_table();
	}

	dprintf( D_CONFIG, "config: using subsystem '%s', local '%s'\n",
			 get_mySubSystem()->getName(), get_mySubSystem()->getLocalName("") );

	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);

		// Try to find user "condor" in the passwd file.
	init_tilde();

		// Insert an entry for "tilde", (~condor)
	if( tilde ) {
		insert_macro("TILDE", tilde, ConfigMacroSet, DetectedMacro, ctx);

	} else {
			// What about tilde if there's no ~condor?
	}

	sysapi_clear_network_device_info_cache();

		// Insert some default values for attributes we want even if
		// they're not defined in the config sources: ARCH and OPSYS.
		// We also want to insert the special "SUBSYSTEM" macro here.
		// We do this now since if they are defined in the config
		// files, these values will get overridden.  However, we want
		// them defined to begin with so that people can use them in
		// the global config source to specify the location of
		// platform-specific config sources, etc.  -Derek Wright 6/8/98
		// Moved all the domain-specific stuff to a separate function
		// since we might not know our full hostname yet. -Derek 10/20/98
	fill_attributes();

		// Try to find the global config source
	if (config_options & CONFIG_OPT_USE_THIS_ROOT_CONFIG) {
		if (config_source && strcasecmp(config_source, "ONLY_ENV") == MATCH) {
			// special case, no config source desired
			have_config_source = false;
		}
	} else {
		char* env = getenv(ENV_CONDOR_CONFIG);
		if( env && strcasecmp(env, "ONLY_ENV") == MATCH ) {
				// special case, no config source desired
			have_config_source = false;
		} else {
			config_source = NULL; // scan the usual places for the root config
		}
	}

	if( have_config_source && ! config_source &&
		! (config_source = find_global(config_options, config_file_tmp)) &&
		! continue_if_no_config)
	{
		if( wantsQuiet ) {
			fprintf( stderr, "Condor error: can't find config source.\n");
			if (config_options & CONFIG_OPT_NO_EXIT) { return false; }
			else { exit(1); }
		}
		fprintf(stderr,"\nNeither the environment variable CONDOR_CONFIG,\n");
#	  if defined UNIX
		fprintf(stderr,"/etc/condor/, /usr/local/etc/, nor ~condor/ contain a condor_config source.\n");
#	  elif defined WIN32
		fprintf(stderr,"nor the registry contains a condor_config source.\n" );
#	  else
#		error "Unknown O/S"
#	  endif
		fprintf( stderr,"Either set CONDOR_CONFIG to point to a valid config "
				"source,\n");
#	  if defined UNIX
		fprintf( stderr,"or put a \"condor_config\" file in /etc/condor/ /usr/local/etc/ or ~condor/\n");
#	  elif defined WIN32
		fprintf( stderr,"or put a \"condor_config\" source in the registry at:\n"
				 " HKEY_LOCAL_MACHINE\\Software\\Condor\\CONDOR_CONFIG");
#	  else
#		error "Unknown O/S"
#	  endif
		if (config_options & CONFIG_OPT_NO_EXIT) { return false; }
		else
		{
			fprintf( stderr, "Exiting.\n\n" );
			exit(1);
		}
	}

	bool only_env = YourStringNoCase("ONLY_ENV") == config_source;
	bool null_config = YourString("/dev/null") == config_source || !config_source || !config_source[0];

	// even if we have no config files, we stil want the special sources like <detected> in the sources table.
	insert_special_sources(ConfigMacroSet);

	if ( ! only_env && ! null_config) {
		// inject the directory of the root config file into the config
		// if no directory was supplied, "." will be used
		// if no root config, "" will be used
		std::string config_root = condor_dirname(config_source);
		if (!config_root.empty() && ! null_config) {
			insert_macro("CONFIG_ROOT", config_root.c_str(), ConfigMacroSet, DetectedMacro, ctx);
		}

			// Read in the global file
		if( config_source ) {
			process_config_source( config_source, 0, "global config source", NULL, !continue_if_no_config );
			global_config_source = config_source;
			config_source = NULL;
		}
	}

		// Insert entries for "hostname" and "full_hostname".  We do
		// this here b/c we need these macros defined so that we can
		// find the local config source if that's defined in terms of
		// hostname or something.  However, we do this after reading
		// the global config source so people can put the
		// DEFAULT_DOMAIN_NAME parameter somewhere if they need it.
		// -Derek Wright <wright@cs.wisc.edu> 5/11/98
	if( host ) {
		insert_macro("HOSTNAME", host, ConfigMacroSet, DetectedMacro, ctx);
	} else {
		insert_macro("HOSTNAME", get_local_hostname().c_str(), ConfigMacroSet, DetectedMacro, ctx);
	}
	insert_macro("FULL_HOSTNAME", get_local_fqdn().c_str(), ConfigMacroSet, DetectedMacro, ctx);

		// Also insert tilde since we don't want that over-written.
	if( tilde ) {
		insert_macro("TILDE", tilde, ConfigMacroSet, DetectedMacro, ctx);
	}

		// Read in the LOCAL_CONFIG_FILE as a string list and process
		// all the files in the order they are listed.
	char *dirlist = param("LOCAL_CONFIG_DIR");
	if(dirlist && ! only_env) {
		process_directory(dirlist, host);
	}
	process_locals( "LOCAL_CONFIG_FILE", host );

	char* newdirlist = param("LOCAL_CONFIG_DIR");
	if(newdirlist && ! only_env) {
		if (dirlist) {
			if(strcmp(dirlist, newdirlist) ) {
				process_directory(newdirlist, host);
			}
		}
		else {
			process_directory(newdirlist, host);
		}
	}

	if(dirlist) { free(dirlist); dirlist = NULL; }
	if(newdirlist) { free(newdirlist); newdirlist = NULL; }

		// Now, insert overrides from the user config file (if any)
	user_config_source.clear();
	std::string user_config_name;
	param(user_config_name, "USER_CONFIG_FILE");
	if (!user_config_name.empty() && ! only_env) {
		if (find_user_file(user_config_source, user_config_name.c_str(), true, false)) {
			dprintf(D_FULLDEBUG|D_CONFIG, "Reading condor user-specific configuration from '%s'\n", user_config_source.c_str());
			process_config_source(user_config_source.c_str(), 1, "user_config source", host, false);
			local_config_sources.emplace_back(user_config_source);
		}
	}

		// Now, insert any macros defined in the environment.
	char **my_environ = GetEnviron();

		// Build "magic_prefix" with _condor_, or w/e distro we are

	const size_t prefix_len = sizeof("_condor_")-1;

	for( int i = 0; my_environ[i]; i++ ) {
		// proceed only if we see the magic prefix
		if( strncasecmp( my_environ[i], "_condor_",  prefix_len ) != 0 ) {
			continue;
		}

		char *varname = strdup( my_environ[i] );
		if( !varname ) {
			EXCEPT( "Out of memory in %s:%d", __FILE__, __LINE__ );
		}

		// isolate variable name by finding & nulling the '=', and trimming spaces before and after 
		int equals_offset = strchr( varname, '=' ) - varname;
		varname[equals_offset] = '\0';
		for (int ii = equals_offset-1; ii > 1; --ii) { if (isspace(varname[ii])) { varname[ii] = 0; } }
		// isolate value by pointing to everything after the '='
		char *varvalue = varname + equals_offset + 1;
		while (isspace(*varvalue)) ++varvalue;
//		assert( !strcmp( varvalue, getenv( varname ) ) );
		// isolate Condor macro_name by skipping magic prefix
		char *macro_name = varname + prefix_len;

		if( macro_name[0] != '\0' ) {
			insert_macro(macro_name, varvalue, ConfigMacroSet, EnvMacro, ctx);
		}

		free( varname ); varname = NULL;
	}

		// Insert the special macros.  We don't want the user to
		// override them, since it's not going to work.
		// We also do this last because some things (like USERNAME)
		// may depend on earlier configuration (USERID_MAP).
	reinsert_specials( host );

	process_dynamic_configs();

	CondorError errorStack;
	if(! init_network_interfaces( & errorStack )) {
		const char * subsysName = get_mySubSystem()->getName();
		if( 0 == strcmp( subsysName, "TOOL" ) ) {
			fprintf( stderr, "%s\n", errorStack.getFullText().c_str() );
		} else {
			EXCEPT( "%s", errorStack.getFullText().c_str() );
		}
	}

		// Now that we're done reading files, if DEFAULT_DOMAIN_NAME
		// is set, we need to re-initialize out hostname information.
	if( (tmp = param("DEFAULT_DOMAIN_NAME")) ) {
		free( tmp );
		reset_local_hostname();
	}

		// The IPv6 code currently caches some results that depend
		// on configuration settings such as NETWORK_INTERFACE.
		// Therefore, force the cache to be reset, now that the
		// configuration has been loaded.
	reset_local_hostname();

		// Re-insert the special macros.  We don't want the user to
		// override them, since it's not going to work.
	reinsert_specials( host );

		// Make sure our FILESYSTEM_DOMAIN and UID_DOMAIN settings are
		// correct.
	check_domain_attributes();

		// once the config table is fully populated, we can optimize it.
		// WARNING!! if you insert new params after this, the table *might*
		// be de-optimized.
	optimize_macros(ConfigMacroSet);


		// now process knobs of the pattern AUTO_USE_<catgory>_<metaknob>
	if ( ! (config_options & CONFIG_OPT_NO_SMART_AUTO_USE)) {
		do_smart_auto_use(config_options);
		// re-sort the macros if we added any
		if (ConfigMacroSet.sorted < ConfigMacroSet.size) {
			optimize_macros(ConfigMacroSet);
		}
	}

	condor_except_should_dump_core( param_boolean("ABORT_ON_EXCEPTION", false) );

	//Configure condor_fsync
	condor_fsync_on = param_boolean("CONDOR_FSYNC", true);
	if(!condor_fsync_on)
		dprintf(D_FULLDEBUG, "FSYNC while writing user logs turned off.\n");

		// Re-initialize the ClassAd compat data (in case if CLASSAD_USER_LIBS is set).
	ClassAdReconfig();

	return true;
}


void
process_config_source( const char* file, int depth, const char* name,
					   const char* host, int required )
{
	int rval;
	if( access( file, R_OK ) != 0 && !is_piped_command(file)) {
		if( !required) { return; }

		if( !host ) {
			fprintf( stderr, "ERROR: Can't read %s %s\n",
					 name, file );
			exit( 1 );
		}
	} else {
		std::string errmsg;
		MACRO_SOURCE source;
		FILE * fp = Open_macro_source(source, file, false, ConfigMacroSet, errmsg);
		if ( ! fp) { rval = -1; }
		else {
			MACRO_EVAL_CONTEXT ctx; init_macro_eval_context(ctx);
			MacroStreamYourFile ms(fp, source);
			rval = Parse_macros(ms, depth, ConfigMacroSet, 0, &ctx, errmsg, NULL, NULL);
			rval = Close_macro_source(fp, source, ConfigMacroSet, rval); fp = NULL;
		}
		if( rval < 0 ) {
			fprintf( stderr,
					 "Configuration Error Line %d while reading %s %s\n",
					 source.line, name, file );
			if (!errmsg.empty()) { fprintf(stderr, "%s\n", errmsg.c_str()); }
			exit( 1 );
		}
	}
}

const char * simulated_local_config = NULL;

// Param for given name, read it in as a string list, and process each
// config source listed there.  If the value is actually a cmd whose
// output should be piped, then do *not* treat it as a file list.
void
process_locals( const char* param_name, const char* host )
{
	std::vector<std::string> sources_to_process;
	std::vector<std::string> sources_done;
	const char* source;
	char *sources_value;
	int local_required;

	local_required = param_boolean_crufty("REQUIRE_LOCAL_CONFIG_FILE", true);

	sources_value = param( param_name );
	if( sources_value ) {
		if ( is_piped_command( sources_value ) ) {
			sources_to_process.emplace_back( sources_value );
		} else {
			sources_to_process = split( sources_value );
		}
		if (simulated_local_config) sources_to_process.emplace_back(simulated_local_config);
		auto src_it = sources_to_process.begin();
		while (src_it != sources_to_process.end()) {
			source = src_it->c_str();
			local_config_sources.emplace_back( source );
			process_config_source( source, 1, "config source", host,
								   local_required );

			sources_done.emplace_back(source);

			char* new_sources_value = param(param_name);
			if(new_sources_value) {
				if(strcmp(sources_value, new_sources_value) ) {
				// the file we just processed altered the list of sources to
				// process
					sources_to_process.clear();
					if ( is_piped_command( new_sources_value ) ) {
						sources_to_process.emplace_back( new_sources_value );
					} else {
						sources_to_process = split(new_sources_value);
					}

					// remove all the ones we've finished from the old list
					for (const auto& src: sources_done) {
						std::erase(sources_to_process, src);
					}
					src_it = sources_to_process.begin();
					free(sources_value);
					sources_value = new_sources_value;
					continue;
				} else {
					free(new_sources_value);
				}
			}
			src_it++;
		}
		free(sources_value);
	}
}


template <class T> bool re_match(const char * str, pcre2_code * re, PCRE2_SIZE options, T& tags)
{
	if ( ! re) return false;

	PCRE2_SPTR str_pcre2 = reinterpret_cast<const unsigned char *>(str);
	pcre2_match_data * matchdata = pcre2_match_data_create_from_pattern(re, NULL);

	int rc = pcre2_match(re, str_pcre2, strlen(str), 0, options, matchdata, NULL);
	PCRE2_SIZE * ovec = pcre2_get_ovector_pointer(matchdata);

	for (int ii = 1; ii < rc; ++ii) {
		tags[ii-1].assign(str + ovec[ii * 2], 
			static_cast<size_t>(ovec[ii * 2 + 1] - ovec[ii * 2]));
	}

	pcre2_match_data_free(matchdata);

	return rc > 0;
}

void do_smart_auto_use(int /*options*/)
{
	PCRE2_SIZE erroffset = 0;
	int errcode;
	pcre2_code * re = pcre2_compile(reinterpret_cast<const unsigned char *>("AUTO_USE_([A-Za-z]+)_(.+)"),
		PCRE2_ZERO_TERMINATED,
		PCRE2_CASELESS | PCRE2_ANCHORED,
		&errcode, &erroffset, NULL);
	ASSERT(re);

	std::string tags[2];
	MACRO_EVAL_CONTEXT ctx; init_macro_eval_context(ctx);
	MACRO_SOURCE src = {true, false, -1, -2, -1, -2};
	std::string errstring;
	std::string args;

	HASHITER it = hash_iter_begin(ConfigMacroSet);
	for (; !hash_iter_done(it); hash_iter_next(it)) {
		const char *name = hash_iter_key(it);
		if (re_match(name, re, PCRE2_NOTEMPTY, tags)) {
			// check trigger
			auto_free_ptr trigger(param(name));
			bool trigger_value = false;
			if ( ! trigger) // an empty trigger does not fire
				continue;
			if ( ! Test_config_if_expression(trigger, trigger_value, errstring, ConfigMacroSet, ctx)) {
				fprintf(stderr, "Configuration error while interpreting %s : %s\n", name, errstring.c_str());
				continue;
			}
			if ( ! trigger_value)
				continue;

			int meta_id = 0;
			const char * raw_template = param_meta_value(tags[0].c_str(), tags[1].c_str(), &meta_id);
			if ( ! raw_template) {
				fprintf(stderr, "Configuration error while interpreting %s : no template named %s:%s\n",
					name, tags[0].c_str(), tags[1].c_str());
				continue;
			}
			// register the pseudo filename "AUTO_USE_<cat>_<tag>"
			insert_source(name, ConfigMacroSet, src);
			src.meta_id = (short int)meta_id;

			auto_free_ptr expanded(expand_meta_args(raw_template, args));
			Parse_config_string(src, 1, expanded, ConfigMacroSet, ctx);
		}
	}
	hash_iter_delete(&it);
	pcre2_code_free(re);
}


int compareFiles(const void *a, const void *b) {
	 return strcmp(*(char *const*)a, *(char *const*)b);
}

static void
get_exclude_regex(Regex &excludeFilesRegex)
{
	int _errcode;
	int _erroffset;
	char* excludeRegex = param("LOCAL_CONFIG_DIR_EXCLUDE_REGEXP");
	if(excludeRegex) {
		if (!excludeFilesRegex.compile(excludeRegex,
									&_errcode, &_erroffset)) {
			EXCEPT("LOCAL_CONFIG_DIR_EXCLUDE_REGEXP "
				   "config parameter is not a valid "
				   "regular expression.  Value: %s,  Error Code: %d",
				   excludeRegex, _errcode);
		}
		if(!excludeFilesRegex.isInitialized() ) {
			EXCEPT("Could not init regex "
				   "to exclude files in %s", __FILE__);
		}
	}
	free(excludeRegex);
}

/* Call this after loading the config files as root to see if we can still access the config once we drop priv
	Used when running as root to verfiy that the config files will still be accessible after we switch
	to the condor user. returns true on success, false if access check fails
	when false is returned, the errmsg will indicate the names of files that cannot be accessed.
*/
bool check_config_file_access(
	const char * username,
	std::vector<std::string> &errfiles)
{
	if ( ! can_switch_ids())
		return true;

	priv_state priv_to_check = PRIV_UNKNOWN;
	if (MATCH == strcasecmp(username, "root") || MATCH == strcasecmp(username, "SYSTEM")) {
		// no need to check access again for root. 
		return true;
	} else if (MATCH == strcasecmp(username, MY_condor_NAME)) {
		priv_to_check = PRIV_CONDOR;
	} else {
		priv_to_check = PRIV_USER;
	}

	// set desired priv state for access check.
	priv_state current_priv = set_priv(priv_to_check);

	bool any_failed = false;
	if (0 != access(global_config_source.c_str(), R_OK)) {
		any_failed = true; 
		errfiles.emplace_back(global_config_source);
	}

	for (const auto& file: local_config_sources) {
		// If we switch users, then we wont even see the current user config file, so dont' check it's access.
		if ( ! user_config_source.empty() && (MATCH == strcmp(file.c_str(), user_config_source.c_str())))
			continue;
		if (is_piped_command(file.c_str())) continue;
		// check for access, other failures we ignore here since if the file or directory doesn't exist
		// that will most likely not be an error once we reconfig
		if (0 != access(file.c_str(), R_OK) && errno == EACCES) {
			any_failed = true;
			errfiles.emplace_back(file);
		}
	}

	// restore priv state
	set_priv(current_priv);

	return ! any_failed;
}


bool
get_config_dir_file_list( char const *dirpath, std::vector<std::string> &files )
{
	Regex excludeFilesRegex;
	get_exclude_regex(excludeFilesRegex);

	Directory dir(dirpath);
	if(!dir.Rewind()) {
		// Don't bother dprintf'ing here, it just causes pre-banner log spam
		return false;
	}

	const char *file;
	while( (file = dir.Next()) ) {
		// don't consider directories
		// maybe we should squash symlinks here...
		if(! dir.IsDirectory() ) {
			if(!excludeFilesRegex.isInitialized() ||
			   !excludeFilesRegex.match(file)) {
				files.emplace_back(dir.GetFullPath());
			} else {
				dprintf(D_FULLDEBUG|D_CONFIG,
						"Ignoring config file "
						"based on "
						"LOCAL_CONFIG_DIR_EXCLUDE_REGEXP, "
						"'%s'\n", dir.GetFullPath());
			}
		}
	}

	std::sort(files.begin(), files.end());
	return true;
}

// examine each file in a directory and treat it as a config file
void
process_directory( const char* dirlist, const char* host )
{
	int local_required;
	
	local_required = param_boolean_crufty("REQUIRE_LOCAL_CONFIG_FILE", true);

	if(!dirlist) { return; }
	for (const auto& dirpath: StringTokenIterator(dirlist)) {
		std::vector<std::string> file_list;
		get_config_dir_file_list(dirpath.c_str(), file_list);

		for (auto& file: file_list) {
			process_config_source( file.c_str(), 1, "config source", host, local_required );

			local_config_sources.emplace_back(file);
		}
	}
}

// Try to find the "condor" user's home directory
void
init_tilde()
{
	if( tilde ) {
		free( tilde );
		tilde = NULL;
	}
# if defined UNIX
	struct passwd *pw;
	if( (pw=getpwnam( MY_condor_NAME )) ) {
		tilde = strdup( pw->pw_dir );
	}
# else
	// On Windows, we'll just look in the registry for TILDE.
	HKEY	handle;
	if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Condor",
		0, KEY_READ, &handle) == ERROR_SUCCESS ) {

		// got the reg key open; now we just need to see if
		// we can open the TILDE string value.

		char the_path[MAX_PATH];
		DWORD valType;
		DWORD valSize = MAX_PATH - 2;

		the_path[0] = '\0';

		if ( RegQueryValueEx(handle, "TILDE", 0,
			&valType, (unsigned char *)the_path, &valSize) == ERROR_SUCCESS ) {

			if ( valType == REG_SZ && the_path[0] ) {
				// got it!
				tilde = strdup(the_path);
			}
		}
		RegCloseKey(handle);
	}
	
# endif
}


char*
get_tilde()
{
	init_tilde();
	return tilde;
}


const char*
find_global(int config_options, std::string & config_file)
{
	return find_file( ENV_CONDOR_CONFIG, "condor_config", config_options, config_file);
}

// Find user-specific location of a file
// Returns true if found, and puts the location in the file_location argument.
// If not found, returns false.  The contents of file_location are undefined.
// if basename is a fully qualified path, then it is used as-is. otherwise
// it is prefixed with ~/.condor/ to create the effective file location
bool
find_user_file(std::string &file_location, const char * basename, bool check_access, bool daemon_ok)
{
	file_location.clear();
	if ( ! basename || ! basename[0])
		return false;

	if (!daemon_ok && can_switch_ids())
		return false;
	if ( fullpath(basename)) {
		file_location = basename;
	} else {
#ifdef UNIX
		// $HOME/.condor/user_config
		struct passwd *pw = getpwuid( geteuid() );
		if ( !pw || !pw->pw_dir) {
			return false;
		}
		formatstr(file_location, "%s/.condor/%s", pw->pw_dir, basename);
#elif defined WIN32
		// %USERPROFILE%\.condor\user_config
		const char * pw_dir = getenv("USERPROFILE");
		if ( !pw_dir)
			return false;
		formatstr(file_location, "%s\\.condor\\%s", pw_dir, basename);
#else
		const char * pw_dir = getenv("HOME");
		if ( !pw_dir)
			return false;
		formatstr(file_location, "%s/.condor/%s", pw_dir, basename);
#endif
	}
	if (check_access) {
		int fd = safe_open_wrapper_follow(file_location.c_str(), O_RDONLY);
		if (fd < 0) {
			return false;
		} else {
			close(fd);
		}
	}

	return true;
}

// Find location of specified file, filename is written into buffer config_file
// and also returned as the return value of this function. if file not found
// then NULL is returned or the process is exited depending on the config_options
const char*
find_file(const char *env_name, const char *file_name, int config_options, std::string & config_file)
{
	const char * config_source = NULL;
	char* env = NULL;
	int fd = 0;

		// If we were given an environment variable name, try that first.
	if( env_name && (env = getenv( env_name )) ) {
		config_file = env;
		config_source = config_file.c_str();
		StatInfo si( config_source );
		switch( si.Error() ) {
		case SIGood:
			if( si.IsDirectory() ) {
				fprintf( stderr, "File specified in %s environment "
						 "variable:\n\"%s\" is a directory.  "
						 "Please specify a file.\n", env_name, env );
				config_file.clear(); config_source = NULL;
				if (config_options & CONFIG_OPT_NO_EXIT) { return NULL; }
				exit( 1 );
			}
				// Otherwise, we're happy
			return config_source;
			break;
		case SINoFile:
			// Check to see if it is a pipe command, in which case we're fine.
			if (!is_piped_command(config_source) ||
				!is_valid_command(config_source)) {

				fprintf( stderr, "File specified in %s environment "
						 "variable:\n\"%s\" does not exist.\n",
						 env_name, config_source );
				config_file.clear(); config_source = NULL;
				if (config_options & CONFIG_OPT_NO_EXIT) { return NULL; }
				exit( 1 );
				break;
			}
			// Otherwise, we're happy
			return config_file.c_str();

		case SIFailure:
			fprintf( stderr, "Cannot stat file specified in %s "
					 "environment variable:\n\"%s\", errno: %d\n",
					 env_name, config_file.c_str(), si.Errno() );
			config_file.clear(); config_source = NULL;
			if (config_options & CONFIG_OPT_NO_EXIT) { return NULL; }
			exit( 1 );
			break;
		}
	}

# ifdef UNIX

	if (!config_source) {
			// List of condor_config file locations we'll try to open.
			// As soon as we find one, we'll stop looking.
		const int locations_length = 4;
		std::string locations[locations_length];
			// 1) $HOME/.condor/condor_config
		// $HOME/.condor/condor_config was added for BOSCO and never used, We are removing it in 8.3.1, but may put it back if users complain.
		//find_user_file(locations[0], file_name, false);
			// 2) /etc/condor/condor_config
		formatstr( locations[1], "/etc/condor/%s", file_name );
			// 3) /usr/local/etc/condor_config (FreeBSD)
		formatstr( locations[2], "/usr/local/etc/%s", file_name );
		if (tilde) {
				// 4) ~condor/condor_config
			formatstr( locations[3], "%s/%s", tilde, file_name );
		}

		int ctr;	
		for (ctr = 0 ; ctr < locations_length; ctr++) {
				// Only use this file if the path isn't empty and
				// if we can read it properly.
			if (!locations[ctr].empty()) {
				config_file = locations[ctr];
				config_source = config_file.c_str();
				if ((fd = safe_open_wrapper_follow(config_source, O_RDONLY)) < 0) {
					config_file.clear(); config_source = NULL;
				} else {
					close(fd);
					dprintf(D_FULLDEBUG, "Reading condor configuration "
							"from '%s'\n", config_source);
					break;
				}
			}
		} // FOR
	} // IF

# elif defined WIN32	// ifdef UNIX
	// Only look in the registry on WinNT.
	HKEY	handle;
	if ( !config_source && RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Condor",
		0, KEY_READ, &handle) == ERROR_SUCCESS ) {
		// We have found a registry key for Condor, which
		// means this user has a pulse and has actually run the
		// installation program before trying to run Condor.
		// This user deserves a tax credit.

		// So now that we found the key, read it.
		char the_path[MAX_PATH];
		DWORD valType;
		DWORD valSize = MAX_PATH - 2;

		the_path[0] = '\0';
		if ( RegQueryValueEx(handle, env_name, 0,
			&valType, (unsigned char *)the_path, &valSize) == ERROR_SUCCESS ) {

			// confirm it is a string value with something there
			if ( valType == REG_SZ && the_path[0] ) {
				// got it!  whoohooo!
				config_file = the_path;
				config_source = config_file.c_str();

				if ( strncmp(config_source, "\\\\", 2 ) == 0 ) {
					// UNC Path, so run a 'net use' on it first.
					NETRESOURCE nr;
					std::string dirName = condor_dirname(config_source);
					nr.dwType = RESOURCETYPE_DISK;
					nr.lpLocalName = NULL;
					nr.lpRemoteName = const_cast<char *>(dirName.c_str());
					nr.lpProvider = NULL;
					
					if ( NO_ERROR != WNetAddConnection2(
										&nr,   /* NetResource */
										NULL,  /* password (default) */
										NULL,  /* username (default) */
										0      /* flags (none) */
						) ) {

						if ( GetLastError() == ERROR_INVALID_PASSWORD ) {
							// try again with an empty password
							#pragma warning(suppress: 6031) // yeah. we aren't checking the return value...
							WNetAddConnection2(
										&nr,   /* NetResource */
										"",    /* password (none) */
										NULL,  /* username (default) */
										0      /* flags (none) */
							);
						}

						// whether it worked or not, we're gonna
						// continue.  The goal of running the
						// WNetAddConnection2() is to make a mapping
						// to the UNC path. For reasons I don't fully
						// understand, some sites need the mapping,
						// and some don't. If it works, great; if not,
						// try the safe_open_wrapper() anyways, and at
						// worst we'll fail fast and the user can fix
						// their file server.
					}
				}

				if( !(is_piped_command(config_source) &&
					  is_valid_command(config_source)) &&
					(fd = safe_open_wrapper_follow( config_source, O_RDONLY)) < 0 ) {
					config_file.clear();
					config_source = NULL;
				} else {
					if (fd != 0) {
						close( fd );
					}
				}
			}
		}

		RegCloseKey(handle);
	}
# else
#	error "Unknown O/S"
# endif		/* ifdef UNIX / Win32 */

	return config_source;
}

#ifdef WIN32
char * get_winreg_string_value(const char * key, const char * valuename)
{
	char * pval = nullptr;
	DWORD valType;
	DWORD valSize = 0;
	if (RegGetValue(HKEY_LOCAL_MACHINE, key, valuename, RRF_RT_REG_SZ, &valType, NULL, &valSize) == ERROR_SUCCESS)
	{
		valSize += 2;
		pval = (char*)malloc(valSize);
		if (RegGetValue(HKEY_LOCAL_MACHINE, key, valuename, RRF_RT_REG_SZ, &valType, pval, &valSize) != ERROR_SUCCESS)
		{
			free(pval); pval = nullptr;
		}
	}
	return pval;
}
#endif

char * find_python3_dot(int minor_ver) {
#ifdef WIN32
	std::string regKey;
	formatstr(regKey, "Software\\Python\\PythonCore\\3.%d\\InstallPath", minor_ver);
	return get_winreg_string_value(regKey.c_str(), "ExecutablePath");
#else
	// TODO: add non-windows implementation
	(void)minor_ver; // Shut the compiler up
	return nullptr;
#endif
}

void apply_thread_limit(int detected_cpus, MACRO_EVAL_CONTEXT & ctx)
{
	char val[32];
	int thread_limit = detected_cpus;
	const char * effective_env = nullptr;
	static const char * const envlimits[] = { "OMP_THREAD_LIMIT", "SLURM_CPUS_ON_NODE" };
	for (size_t ii = 0; ii < COUNTOF(envlimits); ++ii) {
		const char* env = getenv(envlimits[ii]);
		if (!env) continue;

		int lim = atoi(env);
		if (lim > 0 && lim < thread_limit) {
			thread_limit = lim;
			effective_env = envlimits[ii];
		}
	}
	if (thread_limit < detected_cpus) {
		snprintf(val,32, "%d", thread_limit);
		insert_macro("DETECTED_CPUS_LIMIT", val, ConfigMacroSet, DetectedMacro, ctx);
		dprintf(D_CONFIG, "setting DETECTED_CPUS_LIMIT=%s due to environment %s\n", val, effective_env);
	}
}

void
fill_attributes()
{
		/* There are a few attributes that specify what platform we're
		   on that we want to insert values for even if they're not
		   defined in the config sources.  These are ARCH and OPSYS,
		   which we compute with the sysapi_condor_arch() and sysapi_opsys()
		   functions.  We also insert the subsystem here.  Moved all
		   the domain stuff to check_domain_attributes() on
		   10/20.  Also, since this is called before we read in any
		   config sources, there's no reason to check to see if any of
		   these are already defined.  -Derek Wright
		   Amended -Pete Keller 06/01/99 */

	const char *tmp;
	std::string val;
	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);

	if( (tmp = sysapi_condor_arch()) != NULL ) {
		insert_macro("ARCH", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}

	if( (tmp = sysapi_uname_arch()) != NULL ) {
		insert_macro("UNAME_ARCH", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}

	if( (tmp = sysapi_opsys()) != NULL ) {
		insert_macro("OPSYS", tmp, ConfigMacroSet, DetectedMacro, ctx);

		int ver = sysapi_opsys_version();
		if (ver > 0) {
			formatstr(val,"%d", ver);
			insert_macro("OPSYSVER", val.c_str(), ConfigMacroSet, DetectedMacro, ctx);
		}
	}

	if( (tmp = sysapi_opsys_versioned()) != NULL ) {
		insert_macro("OPSYSANDVER", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}

	if( (tmp = sysapi_uname_opsys()) != NULL ) {
		insert_macro("UNAME_OPSYS", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}

	int major_ver = sysapi_opsys_major_version();
	if (major_ver > 0) {
		formatstr(val,"%d", major_ver);
		insert_macro("OPSYSMAJORVER", val.c_str(), ConfigMacroSet, DetectedMacro, ctx);
	}

	if( (tmp = sysapi_opsys_name()) != NULL ) {
		insert_macro("OPSYSNAME", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}
	
	if( (tmp = sysapi_opsys_long_name()) != NULL ) {
		insert_macro("OPSYSLONGNAME", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}

	if( (tmp = sysapi_opsys_short_name()) != NULL ) {
		insert_macro("OPSYSSHORTNAME", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}

	if( (tmp = sysapi_opsys_legacy()) != NULL ) {
		insert_macro("OPSYSLEGACY", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}

#if ! defined WIN32
        // temporary attributes for raw utsname info
	if( (tmp = sysapi_utsname_sysname()) != NULL ) {
		insert_macro("UTSNAME_SYSNAME", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}

	if( (tmp = sysapi_utsname_nodename()) != NULL ) {
		insert_macro("UTSNAME_NODENAME", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}

	if( (tmp = sysapi_utsname_release()) != NULL ) {
		insert_macro("UTSNAME_RELEASE", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}

	if( (tmp = sysapi_utsname_version()) != NULL ) {
		insert_macro("UTSNAME_VERSION", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}

	if( (tmp = sysapi_utsname_machine()) != NULL ) {
		insert_macro("UTSNAME_MACHINE", tmp, ConfigMacroSet, DetectedMacro, ctx);
	}
#endif

	// if the defaults table has a non-zero default python3 minor version
	// locate the appropriate python3 executable for that minor version
	// This is presumed to be the required python minor version needed by the bindings
	int py3minor = param_default_integer("PYTHON3_VERSION_MINOR",nullptr,nullptr,nullptr,nullptr);
	if (py3minor > 0) {
		auto_free_ptr py3val(find_python3_dot(py3minor));
		if (py3val) {
			insert_macro("PYTHON3", py3val, ConfigMacroSet, DetectedMacro, ctx);
		}
	}

#ifdef WIN32
	// on windows it is also useful to know the location of the perl binary
	auto_free_ptr perlval(get_winreg_string_value("Software\\Perl", "BinDir"));
	if (perlval) {
		insert_macro("PERL", perlval, ConfigMacroSet, DetectedMacro, ctx);
	}
#endif

	insert_macro("CondorIsAdmin", can_switch_ids() ? "true" : "false", ConfigMacroSet, DetectedMacro, ctx);

	insert_macro("SUBSYSTEM", get_mySubSystem()->getName(), ConfigMacroSet, DetectedMacro, ctx);
	// insert $(LOCALNAME) macro as the value of LocalName OR the value of SubSystem if there is no local name.
	const char * localname = get_mySubSystem()->getLocalName();
	if ( ! localname || !localname[0]) { localname = get_mySubSystem()->getName(); }
	insert_macro("LOCALNAME", localname, ConfigMacroSet, DetectedMacro, ctx);

	formatstr(val, "%d",sysapi_phys_memory_raw_no_param());
	insert_macro("DETECTED_MEMORY", val.c_str(), ConfigMacroSet, DetectedMacro, ctx);

		// Currently, num_hyperthread_cores is defined as everything
		// in num_cores plus other junk, which on some systems may
		// include non-hyperthreaded cores and on other systems may include
		// hyperthreaded cores.  Since num_hyperthread_cpus is a super-set
		// of num_cpus, we use it for NUM_CORES in the config.  Some day,
		// we may want to break things out into NUM_HYPERTHREAD_CORES,
		// NUM_PHYSICAL_CORES, and what-have-you.
	int num_cpus=0;
	int detected_cpus=0;
	int num_hyperthread_cpus=0;
	sysapi_ncpus_raw(&num_cpus,&num_hyperthread_cpus);

	// DETECTED_PHYSICAL_CPUS will always be the number of real CPUs not counting hyperthreads.
	formatstr(val,"%d",num_cpus);
	insert_macro("DETECTED_PHYSICAL_CPUS", val.c_str(), ConfigMacroSet, DetectedMacro, ctx);

	int def_valid = 0;
	bool count_hyper = param_default_boolean("COUNT_HYPERTHREAD_CPUS", get_mySubSystem()->getName(), &def_valid);
	if ( ! def_valid) count_hyper = true;
	// DETECTED_CPUS will be the value that NUM_CPUS will be set to by default.
	detected_cpus = count_hyper ? num_hyperthread_cpus : num_cpus;
	formatstr(val,"%d", detected_cpus);
	insert_macro("DETECTED_CPUS", val.c_str(), ConfigMacroSet, DetectedMacro, ctx);

	// DETECTED_CORES is not a good name, but we're stuck with it now...
	// it will ALWAYS be the number of hyperthreaded cores.
	formatstr(val,"%d",num_hyperthread_cpus);
	insert_macro("DETECTED_CORES", val.c_str(), ConfigMacroSet, DetectedMacro, ctx);

	// new for version 10.0. DETECTED_CPUS_LIMIT is the minimum of DETECTED_CPUS and several environment variables
	// This is meant to limit the default slot count of personal condors and glide-ins
	// for this pass (early detection) we set a DETECTED_CPUS_LIMIT only if the environment is less than non-hyper CPUs
	// in reinsert_specials when we apply the final value of COUNT_HYPERTHREAD_CPUS we will limit again if necessary
	apply_thread_limit(num_cpus, ctx);
}


void
check_domain_attributes()
{
		/* Make sure the FILESYSTEM_DOMAIN and UID_DOMAIN attributes
		   are set to something reasonable.  If they're not already
		   defined, we default to our own full hostname.  Moved this
		   to its own function so we're sure we have our full hostname
		   by the time we call this. -Derek Wright 10/20/98 */

	char *uid_domain, *filesys_domain;
	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);

	filesys_domain = param("FILESYSTEM_DOMAIN");
	if( !filesys_domain ) {
		insert_macro("FILESYSTEM_DOMAIN", get_local_fqdn().c_str(), ConfigMacroSet, DetectedMacro, ctx);
	} else {
		free( filesys_domain );
	}

	uid_domain = param("UID_DOMAIN");
	if( !uid_domain ) {
		insert_macro("UID_DOMAIN", get_local_fqdn().c_str(), ConfigMacroSet, DetectedMacro, ctx);
	} else {
		free( uid_domain );
	}
}

// Sometimes tests want to be able to pretend that params were set
// to a certain value by the user.  This function lets them do that.
// but it will copy the value (unless it equals the current value)
// and never free the copy until a re-config. If you need to frequently
// change the value of a single param, use set_live_param_value instead.
//
void 
param_insert(const char * name, const char * value)
{
	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);
	insert_macro(name, value, ConfigMacroSet, WireMacro, ctx);
}

// set the value of a param equal to the given pointer. if the param is
// not currently in the hash table, it is inserted. The old param value
// is returned. It is your responsibility to put the old param value back
// before the value pointed to by live_value goes out of scope.
const char * set_live_param_value(const char * name, const char * live_value)
{
	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);

	MACRO_ITEM * pitem = find_macro_item(name, NULL, ConfigMacroSet);
	if ( ! pitem) {
		if ( ! live_value) return NULL;
		insert_macro(name, "", ConfigMacroSet, WireMacro, ctx);
		pitem = find_macro_item(name, NULL, ConfigMacroSet);
	}
	ASSERT(pitem);
	const char * old_value = pitem->raw_value;
	if ( ! live_value) {
		//PRAGMA_REMIND("need a param_remove function to implement this properly!")
		// remove(name, ConfigMacroSet);
		pitem->raw_value = "";
	} else {
		pitem->raw_value = live_value;
	}
	return old_value;
}


void
init_global_config_table(int config_options)
{
	bool want_meta = (config_options & CONFIG_OPT_WANT_META) != 0;
	ConfigMacroSet.size = 0;
	ConfigMacroSet.sorted = 0;
	ConfigMacroSet.options = (config_options & ~CONFIG_OPT_WANT_META);
#ifdef PARSE_CONFIG_TO_DECIDE_COMMENT_RULES
	ConfigMacroSet.options |= CONFIG_OPT_SMART_COM_IN_CONT;
#endif
#ifdef DISCARD_CONFIG_MATCHING_DEFAULT
#else
	ConfigMacroSet.options |= CONFIG_OPT_KEEP_DEFAULTS;
#endif
	if (ConfigMacroSet.table) delete [] ConfigMacroSet.table;
	ConfigMacroSet.table = new MACRO_ITEM[512];
	if (ConfigMacroSet.table) {
		ConfigMacroSet.allocation_size = 512;
		clear_global_config_table(); // to zero-init the table.
	}
	if (ConfigMacroSet.defaults) {
		// Initialize the default table.
		if (ConfigMacroSet.defaults->metat) delete [] ConfigMacroSet.defaults->metat;
		ConfigMacroSet.defaults->metat = NULL;
		ConfigMacroSet.defaults->size = param_info_init((const void**)&ConfigMacroSet.defaults->table);
		ConfigMacroSet.options |= CONFIG_OPT_DEFAULTS_ARE_PARAM_INFO;
	}
	if (want_meta) {
		if (ConfigMacroSet.metat) delete [] ConfigMacroSet.metat;
		ConfigMacroSet.metat = new MACRO_META[ConfigMacroSet.allocation_size];
		ConfigMacroSet.options |= CONFIG_OPT_WANT_META;
		if (ConfigMacroSet.defaults && ConfigMacroSet.defaults->size) {
			ConfigMacroSet.defaults->metat = new MACRO_DEFAULTS::META[ConfigMacroSet.defaults->size];
			memset(ConfigMacroSet.defaults->metat, 0, sizeof(ConfigMacroSet.defaults->metat[0]) * ConfigMacroSet.defaults->size);
		}
	}

	return;
}

void
clear_global_config_table()
{
	if (ConfigMacroSet.table) {
		memset(ConfigMacroSet.table, 0, sizeof(ConfigMacroSet.table[0]) * ConfigMacroSet.allocation_size);
	}
	if (ConfigMacroSet.metat) {
		memset(ConfigMacroSet.metat, 0, sizeof(ConfigMacroSet.metat[0]) * ConfigMacroSet.allocation_size);
	}
	ConfigMacroSet.size = 0;
	ConfigMacroSet.sorted = 0;
	ConfigMacroSet.apool.clear();
	ConfigMacroSet.sources.clear();
	if (ConfigMacroSet.defaults && ConfigMacroSet.defaults->metat) {
		memset(ConfigMacroSet.defaults->metat, 0, sizeof(ConfigMacroSet.defaults->metat[0]) * ConfigMacroSet.defaults->size);
	}

	/* don't want to do this here because of reconfig.
	ConfigMacroSet.allocation_size = 0;
	delete[] ConfigMacroSet.table; ConfigMacroSet.table = NULL;
	delete[] ConfigMacroSet.metat; ConfigMacroSet.metat = NULL;
	*/
	global_config_source       = "";
	local_config_sources.clear();
	return;
}

MACRO_SET * param_get_macro_set()
{
	return &ConfigMacroSet;
}

bool param_defined_by_config(const char *name)
{
	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);
	ctx.without_default = true;

	const char * pval = lookup_macro(name, ConfigMacroSet, ctx);
	return pval != NULL;
}

const char * param_unexpanded(const char *name)
{
	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);
	const char * pval = lookup_macro(name, ConfigMacroSet, ctx);
	if (pval && ! pval[0]) return NULL;
	return pval;
}

bool param_defined(const char* name) {
	const char * pval = param_unexpanded(name);
	if (pval) {
		auto_free_ptr val(expand_param(pval));
		return val;
	}
	return false;
}

unsigned int expand_defined_config_macros (std::string &value)
{
	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);
	return expand_defined_macros(value, ConfigMacroSet, ctx);
}


char *param(const char * name) {
	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);
	ctx.use_mask = 3;
	return param_ctx(name, ctx);
}

char *param_with_context(const char *name, const char *subsys, const char *localname, const char * cwd ) {
	MACRO_EVAL_CONTEXT ctx;
	ctx.init(subsys, 3);
	ctx.localname = localname;
	ctx.cwd = cwd;
	return param_ctx(name, ctx);
}

char*
param_ctx(const char* name, MACRO_EVAL_CONTEXT & ctx)
{

	const char * pval = lookup_macro(name, ConfigMacroSet, ctx);
	if ( ! pval || ! pval[0]) {
		// If we don't find any value at all, return NULL
		return NULL;
	}

	// if we get here, it means that we found a val of note, so expand it and
	// return the canonical value of it. expand_macro returns allocated memory.
	// note that expand_macro will first try and expand
	char * expanded_val = expand_macro(pval, ConfigMacroSet, ctx);
	if ( ! expanded_val) {
		return NULL;
	}
	
	// If expand_macro returned an empty string, free it before returning NULL
	if ( ! expanded_val[0]) {
		free(expanded_val);
		return NULL;
	}

	// return the fully expanded value
	return expanded_val;
}

#define PARAM_PARSE_ERR_REASON_ASSIGN 1
#define PARAM_PARSE_ERR_REASON_EVAL   2

#if defined WIN32 && ! defined strtoll
#define strtoll _strtoi64
#endif

bool
string_is_long_param(
	const char * string,
	long long& result,
	ClassAd *me /* = NULL*/,
	ClassAd *target /*= NULL*/,
	const char * name /*=NULL*/,
	int* err_reason /*=NULL*/) // return 0 or PARAM_PARSE_ERR_REASON_*
{
	char *endptr = NULL;
	result = strtoll(string,&endptr,10);

	ASSERT(endptr);
	if( endptr != string ) {
		while( isspace(*endptr) ) {
			endptr++;
		}
	}
	bool valid = (endptr != string && *endptr == '\0');

	if( !valid ) {
		// For efficiency, we first tried to read the value as a
		// simple literal.  Since that didn't work, now try parsing it
		// as an expression.
		ClassAd rhs;
		if( me ) {
			rhs = *me;
		}
		if ( ! name) { name = "CondorLong"; }

		if( !rhs.AssignExpr( name, string ) ) {
			if (err_reason) *err_reason = PARAM_PARSE_ERR_REASON_ASSIGN;
		} else if( !EvalInteger(name,&rhs,target,result) ) {
			if (err_reason) *err_reason = PARAM_PARSE_ERR_REASON_EVAL;
		} else {
			valid = true;
		}
	}

	return valid;
}

/*
** Return the integer value associated with the named paramter.
** This version returns true if a the parameter was found, or false
** otherwise.
** If the value is not defined or not a valid integer, then
** return the default_value argument .  The min_value and max_value
** arguments are optional and default to MININT and MAXINT.
** These range checks are disabled if check_ranges is false.
*/

bool
param_integer( const char *name, int &value,
			   bool use_default, int default_value,
			   bool check_ranges, int min_value, int max_value,
			   ClassAd *me, ClassAd *target,
			   bool use_param_table )
{
	if(use_param_table) {
		const char* subsys = get_mySubSystem()->getName();
		if (subsys && ! subsys[0]) subsys = NULL;

		int def_valid = 0;
		int is_long = false;
		int was_truncated = false;
		int tbl_default_value = param_default_integer(name, subsys, &def_valid, &is_long, &was_truncated);
		bool tbl_check_ranges = 
			(param_range_integer(name, &min_value, &max_value)==-1) 
				? false : true;

		if (is_long) {
			if (was_truncated)
				dprintf (D_ERROR, "Error - long param %s was fetched as integer and truncated\n", name);
			else
				dprintf (D_CONFIG, "Warning - long param %s fetched as integer\n", name);
		}

		// if found in the default table, then we overwrite the arguments
		// to this function with the defaults from the table. This effectively
		// nullifies the hard coded defaults in the higher level layers.
		if (def_valid) {
			use_default = true;
			default_value = tbl_default_value;
		}
		if (tbl_check_ranges) {
			check_ranges = true;
		}
	}
	
	int result;
	long long long_result;
	char *string = NULL;

	ASSERT( name );
	string = param( name );
	if( ! string ) {
		dprintf( D_CONFIG | D_VERBOSE, "%s is undefined, using default value of %d\n",
				 name, default_value );
		if ( use_default ) {
			value = default_value;
		}
		return false;
	}

	int err_reason = 0;
	bool valid = string_is_long_param(string, long_result, me, target, name, &err_reason);
	if ( ! valid) {
		if (err_reason == PARAM_PARSE_ERR_REASON_ASSIGN) {
			EXCEPT("Invalid expression for %s (%s) "
				   "in condor configuration.  Please set it to "
				   "an integer expression in the range %d to %d "
				   "(default %d).",
				   name,string,min_value,max_value,default_value);
		}

		if (err_reason == PARAM_PARSE_ERR_REASON_EVAL) {
			EXCEPT("Invalid result (not an integer) for %s (%s) "
				   "in condor configuration.  Please set it to "
				   "an integer expression in the range %d to %d "
				   "(default %d).",
				   name,string,min_value,max_value,default_value);
		}
		long_result = default_value;
	}
	result = long_result;

	if( (int)result != long_result ) {
		EXCEPT( "%s in the condor configuration is out of bounds for"
				" an integer (%s)."
				"  Please set it to an integer in the range %d to %d"
				" (default %d).",
				name, string, min_value, max_value, default_value );
	}
	else if ( check_ranges  &&  ( result < min_value )  ) {
		EXCEPT( "%s in the condor configuration is too low (%s)."
				"  Please set it to an integer in the range %d to %d"
				" (default %d).",
				name, string, min_value, max_value, default_value );
	}
	else if ( check_ranges  && ( result > max_value )  ) {
		EXCEPT( "%s in the condor configuration is too high (%s)."
				"  Please set it to an integer in the range %d to %d"
				" (default %d).",
				name, string, min_value, max_value, default_value );
	}
	free( string );

	value = result;
	return true;
}


bool
param_longlong( const char *name, long long int &value,
			   bool use_default, long long default_value,
			   bool check_ranges, long long min_value, long long max_value,
			   ClassAd *me, ClassAd *target,
			   bool use_param_table )
{
	if(use_param_table) {
		const char* subsys = get_mySubSystem()->getName();
		if (subsys && ! subsys[0]) subsys = NULL;

		int def_valid = 0;
		long long tbl_default_value = param_default_long(name, subsys, &def_valid);
		bool tbl_check_ranges = param_range_long(name, &min_value, &max_value) != -1;

		// if found in the default table, then we overwrite the arguments
		// to this function with the defaults from the table. This effectively
		// nullifies the hard coded defaults in the higher level layers.
		if (def_valid) {
			use_default = true;
			default_value = tbl_default_value;
		}
		if (tbl_check_ranges) {
			check_ranges = true;
		}
	}
	
	long long long_result;
	char *string = NULL;

	ASSERT( name );
	string = param( name );
	if( ! string ) {
		dprintf( D_CONFIG | D_VERBOSE, "%s is undefined, using default value of %lld\n",
				 name, default_value );
		if ( use_default ) {
			value = default_value;
		}
		return false;
	}

	int err_reason = 0;
	bool valid = string_is_long_param(string, long_result, me, target, name, &err_reason);
	if ( ! valid) {
		if (err_reason == PARAM_PARSE_ERR_REASON_ASSIGN) {
			EXCEPT("Invalid expression for %s (%s) "
				   "in condor configuration.  Please set it to "
				   "an integer expression in the range %lld to %lld "
				   "(default %lld).",
				   name,string,min_value,max_value,default_value);
		}

		if (err_reason == PARAM_PARSE_ERR_REASON_EVAL) {
			EXCEPT("Invalid result (not an integer) for %s (%s) "
				   "in condor configuration.  Please set it to "
				   "an integer expression in the range %lld to %lld "
				   "(default %lld).",
				   name,string,min_value,max_value,default_value);
		}
		long_result = default_value;
	}

	if ( check_ranges  &&  ( long_result < min_value )  ) {
		EXCEPT( "%s in the condor configuration is too low (%s)."
				"  Please set it to an integer in the range %lld to %lld"
				" (default %lld).",
				name, string, min_value, max_value, default_value );
	}
	else if ( check_ranges  && ( long_result > max_value )  ) {
		EXCEPT( "%s in the condor configuration is too high (%s)."
				"  Please set it to an integer in the range %lld to %lld"
				" (default %lld).",
				name, string, min_value, max_value, default_value );
	}
	free( string );

	value = long_result;
	return true;
}


/*
** Return the integer value associated with the named paramter.
** If the value is not defined or not a valid integer, then
** return the default_value argument.  The min_value and max_value
** arguments are optional and default to MININT and MAXINT.
*/

int
param_integer( const char *name, int default_value,
			   int min_value, int max_value, bool use_param_table )
{
	int result;

	param_integer( name, result, true, default_value,
				   true, min_value, max_value, NULL, NULL, use_param_table );
	return result;
}

// require that the attribute I'm looking for is defined in the config file.
char* param_or_except(const char *attr)
{
	char *tmp = NULL;

	tmp = param(attr);
	if (tmp == NULL || strlen(tmp) <= 0) {
		EXCEPT("Please define config file entry to non-null value: %s", attr);
	}

	return tmp;
}

/*
 * Parse and/or evaluate the string and return a [double precision] floating
 * point value parameter.If the value is not a valid float, then return
 * the default_value argument. the return value indicates whether the string
 * contained a valid float or expression that evaluated to a float.
 */
bool string_is_double_param(
	const char * string,
	double& result,
	ClassAd *me /*= NULL*/,
	ClassAd *target /* = NULL*/,
	const char * name /*=NULL*/,
	int* err_reason /*=NULL*/)
{
	char *endptr = NULL;
	result = strtod(string,&endptr);

	ASSERT(endptr);
	if( endptr != string ) {
		while( isspace(*endptr) ) {
			endptr++;
		}
	}
	bool valid = (endptr != string && *endptr == '\0');
	if( !valid ) {
		// For efficiency, we first tried to read the value as a
		// simple literal.  Since that didn't work, now try parsing it
		// as an expression.
		ClassAd rhs;
		if( me ) {
			rhs = *me;
		}
		if ( ! name) { name = "CondorDouble"; }
		if ( ! rhs.AssignExpr( name, string )) {
			if (err_reason) *err_reason = PARAM_PARSE_ERR_REASON_ASSIGN;
		}
		else if ( ! EvalFloat(name,&rhs,target,result) ) {
			if (err_reason) *err_reason = PARAM_PARSE_ERR_REASON_EVAL;
		} else {
			valid = true;
		}
	}
	return valid;
}

/*
 * Return the [double precision] floating point value associated with the named
 * parameter.  If the value is not defined or not a valid float, then return
 * the default_value argument.  The min_value and max_value arguments are
 * optional and default to DBL_MIN and DBL_MAX.
 */

double
param_double( const char *name, double default_value,
			  double min_value, double max_value,
			  ClassAd *me, ClassAd *target,
			  bool use_param_table )
{
	if(use_param_table) {
		const char* subsys = get_mySubSystem()->getName();
		if (subsys && ! subsys[0]) subsys = NULL;

		int def_valid = 0;
		double tbl_default_value = param_default_double(name, subsys, &def_valid);

		// if the min_value & max_value are changed, we use it.


		// if found in the default table, then we overwrite the arguments
		// to this function with the defaults from the table. This effectively
		// nullifies the hard coded defaults in the higher level layers.
		if (def_valid) {
			default_value = tbl_default_value;
		}
	}
	
	double result;
	char *string;

	ASSERT( name );
	string = param( name );
	
	if( ! string ) {
		dprintf( D_CONFIG | D_VERBOSE, "%s is undefined, using default value of %f\n",
				 name, default_value );
		return default_value;
	}

	int err_reason = 0;
	bool valid = string_is_double_param(string, result, me, target, name, &err_reason);
	if( !valid ) {
		if (err_reason == PARAM_PARSE_ERR_REASON_ASSIGN) {
			EXCEPT("Invalid expression for %s (%s) "
				   "in condor configuration.  Please set it to "
				   "a numeric expression in the range %lg to %lg "
				   "(default %lg).",
				   name,string,min_value,max_value,default_value);
		}

		if (err_reason == PARAM_PARSE_ERR_REASON_EVAL) {
			EXCEPT("Invalid result (not a number) for %s (%s) "
				   "in condor configuration.  Please set it to "
				   "a numeric expression in the range %lg to %lg "
				   "(default %lg).",
				   name,string,min_value,max_value,default_value);
		}
		result = default_value;
	}

	if( result < min_value ) {
		EXCEPT( "%s in the condor configuration is too low (%s)."
				"  Please set it to a number in the range %lg to %lg"
				" (default %lg).",
				name, string, min_value, max_value, default_value );
	}
	else if( result > max_value ) {
		EXCEPT( "%s in the condor configuration is too high (%s)."
				"  Please set it to a number in the range %lg to %lg"
				" (default %lg).",
				name, string, min_value, max_value, default_value );
	}
	free( string );
	return result;
}

/*
 * Like param_boolean, but allow for 'T' or 'F' (no quotes, case
 * insensitive) to mean True/False, respectively.
 */
bool
param_boolean_crufty( const char *name, bool default_value )
{
	char *tmp = param(name);
	if (tmp) {
		char c = *tmp;
		free(tmp);

		if ('t' == c || 'T' == c) {
			return true;
		} else if ('f' == c || 'F' == c) {
			return false;
		} else {
			return param_boolean(name, default_value);
		}
	} else {
		return param_boolean(name, default_value);
	}
}


// Parse a string and return true if it is a valid boolean
bool string_is_boolean_param(const char * string, bool& result, ClassAd *me /*= NULL*/, ClassAd *target /*= NULL*/, const char *name /*= NULL*/)
{
	bool valid = true;

	const char *endptr = string;
	if( strncasecmp(endptr,"true",4) == 0 ) {
		endptr+=4;
		result = true;
	}
	else if( strncasecmp(endptr,"1",1) == 0 ) {
		endptr+=1;
		result = true;
	}
	else if( strncasecmp(endptr,"false",5) == 0 ) {
		endptr+=5;
		result = false;
	}
	else if( strncasecmp(endptr,"0",1) == 0 ) {
		endptr+=1;
		result = false;
	}
	else {
		valid = false;
	}

	while( isspace(*endptr) ) {
		endptr++;
	}
	if( *endptr != '\0' ) {
		valid = false;
	}

	if( !valid ) {
		// For efficiency, we first tried to read the value as a
		// simple literal.  Since that didn't work, now try parsing it
		// as an expression.
		ClassAd rhs;
		if( me ) {
			rhs = *me;
		}
		if ( ! name) { name = "CondorBool"; }

		if( rhs.AssignExpr( name, string ) &&
			EvalBool(name,&rhs,target,result) )
		{
			valid = true;
		}
	}

	return valid;
}


/*
** Return the boolean value associated with the named paramter.
** The parameter value is expected to be set to the string
** "TRUE" or "FALSE" (no quotes, case insensitive).
** If the value is not defined or not a valid, then
** return the default_value argument.
*/

bool
param_boolean( const char *name, bool default_value, bool do_log,
			   ClassAd *me, ClassAd *target,
			   bool use_param_table )
{
	if(use_param_table) {
		const char* subsys = get_mySubSystem()->getName();
		if (subsys && ! subsys[0]) subsys = NULL;

		int def_valid = 0;
		bool tbl_default_value = param_default_boolean(name, subsys, &def_valid);

		// if found in the default table, then we overwrite the arguments
		// to this function with the defaults from the table. This effectively
		// nullifies the hard coded defaults in the higher level layers.
		if (def_valid) {
			default_value = tbl_default_value;
		}
	}

	bool result = default_value;
	char *string;
	bool valid = true;

	ASSERT( name );
	string = param( name );
	
	if (!string) {
		if (do_log) {
			dprintf( D_CONFIG | D_VERBOSE, "%s is undefined, using default value of %s\n",
					 name, default_value ? "True" : "False" );
		}
		return default_value;
	}

	valid = string_is_boolean_param(string, result, me, target, name);

	if( !valid ) {
		EXCEPT( "%s in the condor configuration  is not a valid boolean (\"%s\")."
				"  Please set it to True or False (default is %s)",
				name, string, default_value ? "True" : "False" );
	}

	free( string );
	
	return result;
}

bool
param_true( const char * name ) {
	bool value;
	char * string = param( name );
	if ( ! string) return false;
	bool valid = string_is_boolean_param( string, value );
	free( string );
	return valid && value;
}

bool
param_false( const char * name ) {
	bool value;
	char * string = param( name );
	if ( ! string) return false;
	bool valid = string_is_boolean_param( string, value );
	free( string );
	return valid && (!value);
}

const char * param_raw_default(const char *name)
{
	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);
	ctx.use_mask = 3;
	return lookup_macro_default(name, ConfigMacroSet, ctx);
}

char *
expand_param( const char *str )
{
	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);
	return expand_macro(str, ConfigMacroSet, ctx);
}

char *
expand_param(const char *str, const char * localname, const char *subsys, int use)
{
	MACRO_EVAL_CONTEXT ctx = { localname, subsys, NULL, false, (char)use, 0, 0 };
	if (ctx.localname && ! ctx.localname[0]) ctx.localname = NULL;
	if (ctx.subsys && ! ctx.subsys[0]) ctx.subsys = NULL;
	return expand_macro(str, ConfigMacroSet, ctx);
}

const char * param_append_location(const MACRO_META * pmet, std::string & value)
{
	value += config_source_by_id(pmet->source_id);
	if (pmet->source_line >= 0) {
		formatstr_cat(value, ", line %d", pmet->source_line);
		MACRO_TABLE_PAIR * ptable = nullptr;
		MACRO_DEF_ITEM * pmsi = param_meta_source_by_id(pmet->source_meta_id, &ptable);
		if (pmsi) {
			formatstr_cat(value, ", use %s:%s+%d", ptable->key, pmsi->key, pmet->source_meta_off);
		}
	}
	return value.c_str();
}

const char * param_get_location(const MACRO_META * pmet, std::string & value)
{
	value.clear();
	return param_append_location(pmet, value);
}


// find an item and return a hash iterator that points to it.
bool param_find_item (
	const char * name,
	const char * subsys,
	const char * local,
	std::string & name_found, // out
	HASHITER& it)          //
{
	it = HASHITER(ConfigMacroSet, 0);
	if (subsys && ! subsys[0]) subsys = NULL;
	if (local && ! local[0]) local = NULL;
	it.id = it.set.defaults ? it.set.defaults->size : 0;
	it.ix = it.set.size;
	it.is_def = false;

	MACRO_ITEM * pi = NULL;
#if 0
	//PRAGMA_REMIND("tj: remove subsys.local.knob support in the 8.5 devel series")
	if (subsys && local) {
		formatstr(name_found, "%s.%s", subsys, local);
		pi = find_macro_item(name, name_found.c_str(), ConfigMacroSet);
		if (pi) {
			name_found = pi->key;
			it.ix = (int)(pi - it.set.table);
			return true;
		}
	}
#endif
	if (local) {
		pi = find_macro_item(name, local, ConfigMacroSet);
		if (pi) {
			name_found = pi->key;
			it.ix = (int)(pi - it.set.table);
			return true;
		}
	}
	if (subsys) {
		pi = find_macro_item(name, subsys, ConfigMacroSet);
		if (pi) {
			name_found = pi->key;
			it.ix = (int)(pi - it.set.table);
			return true;
		}
		const MACRO_DEF_ITEM* pdf = (const MACRO_DEF_ITEM*)param_subsys_default_lookup(subsys, name);
		if (pdf) {
			name_found = subsys;
			upper_case(name_found);
			name_found += ".";
			name_found += pdf->key;
			it.is_def = true;
			it.pdef = pdf;
			it.id = param_default_get_id(name, NULL);
			return true;
		}
	}

	pi = find_macro_item(name, NULL, ConfigMacroSet);
	if (pi) {
		name_found = pi->key;
		it.ix = (int)(pi - it.set.table);
		return true;
	}

	const char * pdot = strchr(name, '.');
	if (pdot) {
		const MACRO_DEF_ITEM* pdf = (const MACRO_DEF_ITEM*)param_subsys_default_lookup(name, pdot+1);
		if (pdf) {
			name_found = name;
			upper_case(name_found);
			name_found.erase((size_t)(pdot - name)+1);
			name_found += pdf->key;
			it.is_def = true;
			it.pdef = pdf;
			it.id = param_default_get_id(name, NULL);
			return true;
		}
	}

	MACRO_DEF_ITEM * pdf = param_default_lookup(name);
	if (pdf) {
		name_found = pdf->key;
		it.is_def = true;
		it.pdef = pdf;
		it.id = param_default_get_id(name, NULL);
		return true;
	}

	name_found.clear();
	it.id = it.set.defaults ? it.set.defaults->size : 0;
	it.ix = it.set.size;
	it.is_def = false;
	return false;
}

const char * hash_iter_info(
	HASHITER& it,
	int& use_count,
	int& ref_count,
	std::string &source_name,
	int &line_number)
{
	MACRO_META * pmet = hash_iter_meta(it);
	if ( ! pmet) {
		use_count = ref_count = -1;
		line_number = -2;
		source_name.clear();
	} else {
		source_name = config_source_by_id(pmet->source_id);
		line_number = pmet->source_line;
		use_count = pmet->use_count;
		ref_count = pmet->ref_count;
	}
	return hash_iter_value(it);
}

const char * hash_iter_def_value(HASHITER& it)
{
	if (it.is_def)
		return hash_iter_value(it);
	const char * name =  hash_iter_key(it);
	if ( ! name)
		return NULL;
	return param_exact_default_string(name);
}


const char * param_get_info(
	const char * name,
	const char * subsys,
	const char * local,
	std::string & name_used,
	const char ** pdef_val,
	const MACRO_META **ppmet)
{
	const char * val = NULL;
	if (pdef_val) { *pdef_val = NULL; }
	if (ppmet)    { *ppmet = NULL; }
	name_used.clear();

	std::string ms;
	HASHITER it(ConfigMacroSet, 0);
	if (param_find_item(name, subsys, local, ms, it)) {
		name_used = ms;
		val = hash_iter_value(it);
		if (pdef_val) { *pdef_val = hash_iter_def_value(it); }
		if (ppmet) { *ppmet = hash_iter_meta(it); }
	}
	return val;
}

void
reinsert_specials( const char* host )
{
	static unsigned int reinsert_pid = 0;
	static unsigned int reinsert_ppid = 0;
	static bool warned_no_user = false;
	char buf[40];

	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);

	if( tilde ) {
		insert_macro("TILDE", tilde, ConfigMacroSet, DetectedMacro, ctx);
	}
	if( host ) {
		insert_macro("HOSTNAME", host, ConfigMacroSet, DetectedMacro, ctx);
	} else {
		insert_macro("HOSTNAME", get_local_hostname().c_str(), ConfigMacroSet, DetectedMacro, ctx);
	}
	insert_macro("FULL_HOSTNAME", get_local_fqdn().c_str(), ConfigMacroSet, DetectedMacro, ctx);
	insert_macro("SUBSYSTEM", get_mySubSystem()->getName(), ConfigMacroSet, DetectedMacro, ctx);
	// insert $(LOCALNAME) macro as the value of LocalName OR the value of SubSystem if there is no local name.
	const char * localname = get_mySubSystem()->getLocalName();
	if ( ! localname || !localname[0]) { localname = get_mySubSystem()->getName(); }
	insert_macro("LOCALNAME", localname, ConfigMacroSet, DetectedMacro, ctx);

	// Insert login-name for our real uid as "username".  At the time
	// we're reading in the config source, the priv state code is not
	// initialized, so our euid will always be the same as our ruid.
	char *myusernm = my_username();
	if( myusernm ) {
		insert_macro( "USERNAME", myusernm, ConfigMacroSet, DetectedMacro, ctx);
		free(myusernm);
		myusernm = NULL;
	} else {
		if( ! warned_no_user ) {
			dprintf( D_ALWAYS, "ERROR: can't find username of current user! "
					 "BEWARE: $(USERNAME) will be undefined\n" );
			warned_no_user = true;
		}
	}

	// Insert real-uid and real-gid as "real_uid" and "real_gid".
	// Now these values are meaningless on Win32, but leaving
	// them undefined can be undesireable, and setting them
	// to "0" could be dangerous (that is root uid on unix),
	// so we set them to something....
	{
		uid_t myruid;
		gid_t myrgid;
#ifdef WIN32
			// Hmmm...
		myruid = 666;
		myrgid = 666;
#else
		myruid = getuid();
		myrgid = getgid();
#endif
		snprintf(buf,40,"%u",myruid);
		insert_macro("REAL_UID", buf, ConfigMacroSet, DetectedMacro, ctx);
		snprintf(buf,40,"%u",myrgid);
		insert_macro("REAL_GID", buf, ConfigMacroSet, DetectedMacro, ctx);
	}
		
	// Insert values for "pid" and "ppid".  Use static values since
	// this is expensive to re-compute on Windows.
	// Note: we have to resort to ifdef WIN32 junk even though
	// DaemonCore can nicely give us this information.  We do this
	// because the config code is used by the tools as well as daemons.
	if (!reinsert_pid) {
#ifdef WIN32
		reinsert_pid = ::GetCurrentProcessId();
#else
		reinsert_pid = getpid();
#endif
	}
	snprintf(buf,40,"%u",reinsert_pid);
	insert_macro("PID", buf, ConfigMacroSet, DetectedMacro, ctx);
	if ( !reinsert_ppid ) {
#ifdef WIN32
		CSysinfo system_hackery;
		reinsert_ppid = system_hackery.GetParentPID(reinsert_pid);
#else
		reinsert_ppid = getppid();
#endif
	}
	snprintf(buf,40,"%u",reinsert_ppid);
	insert_macro("PPID", buf, ConfigMacroSet, DetectedMacro, ctx);

	//
	// get_local_ipaddr() may return the 'default' IP if the protocol-
	// specific address for the given protocol isn't set.  The 'default'
	// IP address could be IPv6 if:
	//
	//	* NETWORK_INTERACE is IPv6,
	//  * the IPv6 address is the only public one, or
	//	* the public IPv6 address is listed before the IPv4 address.
	//
	// (We should probably prefer public Ipv4 to public IPv6, but don't.)  See
	// init_local_hostname_impl(), which calls network_interface_to_ip().
	//
	condor_sockaddr ip = get_local_ipaddr( CP_IPV4 );
	insert_macro("IP_ADDRESS", ip.to_ip_string().c_str(), ConfigMacroSet, DetectedMacro, ctx);
	if( ip.is_ipv6() ) {
		insert_macro("IP_ADDRESS_IS_IPV6", "true", ConfigMacroSet, DetectedMacro, ctx);
	} else {
		insert_macro("IP_ADDRESS_IS_IPV6", "false", ConfigMacroSet, DetectedMacro, ctx);
	}

	condor_sockaddr v4 = get_local_ipaddr( CP_IPV4 );
	if( v4.is_ipv4() ) {
		insert_macro("IPV4_ADDRESS", v4.to_ip_string().c_str(), ConfigMacroSet, DetectedMacro, ctx);
	}

	condor_sockaddr v6 = get_local_ipaddr( CP_IPV6 );
	if( v6.is_ipv6() ) {
		insert_macro("IPV6_ADDRESS", v6.to_ip_string().c_str(), ConfigMacroSet, DetectedMacro, ctx);
	}


	{ // set DETECTED_CPUS to the correct value, either hyperthreaded or not.
		int num_cpus=0;
		int num_hyperthread_cpus=0;
		sysapi_ncpus_raw(&num_cpus,&num_hyperthread_cpus);
		bool count_hyper = param_boolean("COUNT_HYPERTHREAD_CPUS", true);
		// DETECTED_CPUS will be the value that NUM_CPUS will be set to by default.
		snprintf(buf,40,"%d", count_hyper ? num_hyperthread_cpus : num_cpus);
		insert_macro("DETECTED_CPUS", buf, ConfigMacroSet, DetectedMacro, ctx);
		if (count_hyper) {
			// if hyperthreads are enabled, we have to check again to see if environmental limits apply
			apply_thread_limit(num_hyperthread_cpus, ctx);
		}
	}
}


void
config_insert( const char* attrName, const char* attrValue )
{
	if( ! (attrName && attrValue) ) {
		return;
	}
	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);
	insert_macro(attrName, attrValue, ConfigMacroSet, WireMacro, ctx);
}

int macro_stats(MACRO_SET& set, struct _macro_stats &stats)
{
	int cHunks, cQueries = 0;
	memset((void*)&stats, 0, sizeof(stats));
	stats.cSorted = set.sorted;
	stats.cFiles = (int)set.sources.size();
	stats.cEntries = set.size;
	stats.cbStrings = set.apool.usage(cHunks, stats.cbFree);
	int cbPer = sizeof(MACRO_ITEM) + (set.metat ? sizeof(MACRO_META) : 0);
	stats.cbTables = (set.size * cbPer) + (int)set.sources.size() * sizeof(char*);
	stats.cbFree += (set.allocation_size - set.size) * cbPer;
	if ( ! set.metat) {
		cQueries = stats.cReferenced = stats.cUsed = -1;
	} else {
		for (int ii = 0; ii < set.size; ++ii) {
			if (set.metat[ii].use_count) ++stats.cUsed;
			if (set.metat[ii].ref_count) ++stats.cReferenced;
			if (set.metat[ii].use_count > 0) cQueries += set.metat[ii].use_count;
		}
		if (set.defaults && set.defaults->metat) {
			MACRO_DEFAULTS * defs = set.defaults;
			for (int ii = 0; ii < defs->size; ++ii) {
				if (defs->metat[ii].use_count) ++stats.cUsed;
				if (defs->metat[ii].ref_count) ++stats.cReferenced;
				if (defs->metat[ii].use_count > 0) cQueries += defs->metat[ii].use_count;
			}
		}
	}
	return cQueries;
}

int  get_config_stats(struct _macro_stats *pstats)
{
	return macro_stats(ConfigMacroSet, *pstats);
}


/* Begin code for runtime support for modifying a daemon's config source.
   See condor_daemon_core.V6/README.config for more details. */

static std::set<std::string> PersistAdminList;

class RuntimeConfigItem {
public:
	RuntimeConfigItem() : admin(nullptr), config(nullptr) { }
	RuntimeConfigItem(char *admin, char *config): admin(admin), config(config) {}
	RuntimeConfigItem(const RuntimeConfigItem &rhs): admin(strdup(rhs.admin)), config(strdup(rhs.config))  {}

	~RuntimeConfigItem() { if (admin) free(admin); if (config) free(config); }
	void initialize() { admin = config = nullptr; }
	char *admin;
	char *config;
};

static std::vector<RuntimeConfigItem> rArray;

static std::string toplevel_persistent_config;

/*
  we want these two bools to be global, and only initialized on
  startup, so that folks can't play tricks and change these
  dynamically.  for example, if a site enables runtime but not
  persistent configs, we can't allow someone to set
  "ENABLE_PERSISTENT_CONFIG" with a condor_config_val -rset.
  therefore, we only read these once, before we look at any of the
  dynamic config source, to make sure we're happy.  this means it
  requires a restart to change any of these, but i think that's a
  reasonable burden on admins, considering the potential security
  implications.  -derek 2006-03-17
*/
static bool enable_runtime;
static bool enable_persistent;

static void
init_dynamic_config()
{
	static bool initialized = false;

	if( initialized ) {
			// already have a value, we're done
		return;
	}

	enable_runtime = param_boolean( "ENABLE_RUNTIME_CONFIG", false );
	enable_persistent = param_boolean( "ENABLE_PERSISTENT_CONFIG", false );
	initialized = true;

	if( !enable_persistent ) {
			// we don't want persistent configs, leave the toplevel blank
		return;
	}

	char* tmp;

		// if we're using runtime config, try a subsys-specific config
		// knob for the root location
	std::string filename_parameter;
	formatstr( filename_parameter, "%s_CONFIG", get_mySubSystem()->getName() );
	tmp = param( filename_parameter.c_str() );
	if( tmp ) {
		toplevel_persistent_config = tmp;
		free( tmp );
		return;
	}

	tmp = param( "PERSISTENT_CONFIG_DIR" );

	if( !tmp ) {
		if ( get_mySubSystem()->isClient( ) || !have_config_source ) {
				/*
				   we are just a tool, not a daemon.
				   or, we were explicitly told we don't have
				   the usual config sources.
				   thus it is not imperative that we find what we
				   were looking for...
				*/
			return;
		} else {
				// we are a daemon.  if we fail, we must exit.
			fprintf( stderr, "Condor error: ENABLE_PERSISTENT_CONFIG is TRUE, "
					 "but neither %s nor PERSISTENT_CONFIG_DIR is "
					 "specified in the configuration file\n",
					 filename_parameter.c_str() );
			exit( 1 );
		}
	}
	formatstr( toplevel_persistent_config, "%s%c.config.%s", tmp,
										DIR_DELIM_CHAR,
										get_mySubSystem()->getName() );
	free(tmp);
}


/*
** Caller is responsible for allocating admin and config with malloc.
** Caller should not free admin and config after the call.
*/

#define ABORT \
	if(admin) { free(admin); } \
	if(config) { free(config); } \
	set_priv(priv); \
	return -1

int
set_persistent_config(char *admin, char *config)
{
	int fd, rval;
	std::string filename;
	std::string tmp_filename;
	priv_state priv;

	if (!admin || !admin[0] || !enable_persistent) {
		if (!enable_persistent) {
			dprintf( D_ALWAYS, "set_persistent_config(): "
				"ENABLE_PERSISTENT_CONFIG is false. "
				"Not setting persistent config file param: "
				"Name = %s, Value = %s\n",
				admin?admin:"(null pointer)",
				config?config:"(null pointer)");
		}
		if (admin)  { free(admin);  }
		if (config) { free(config); }
		return -1;
	}

	// make sure top level config source is set
	init_dynamic_config();
	if( ! toplevel_persistent_config.length() ) {
		EXCEPT( "Impossible: programmer error: toplevel_persistent_config "
				"is 0-length, but we already initialized, enable_persistent "
				"is TRUE, and set_persistent_config() has been called" );
	}

	priv = set_root_priv();
	if (config && config[0]) {	// (re-)set config
			// write new config to temporary file
		formatstr( filename, "%s.%s", toplevel_persistent_config.c_str(), admin );
		formatstr( tmp_filename, "%s.tmp", filename.c_str() );
		do {
			MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
			unlink( tmp_filename.c_str() );
			fd = safe_open_wrapper_follow( tmp_filename.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0644 );
		} while (fd == -1 && errno == EEXIST);
		if( fd < 0 ) {
			dprintf( D_ALWAYS, "safe_open_wrapper(%s) returned %d '%s' (errno %d) in "
					 "set_persistent_config()\n", tmp_filename.c_str(),
					 fd, strerror(errno), errno );
			ABORT;
		}
		if (write(fd, config, (unsigned int)strlen(config)) != (ssize_t)strlen(config)) {
			dprintf( D_ALWAYS, "write() failed with '%s' (errno %d) in "
					 "set_persistent_config()\n", strerror(errno), errno );
			close(fd);
			ABORT;
		}
		if (close(fd) < 0) {
			dprintf( D_ALWAYS, "close() failed with '%s' (errno %d) in "
					 "set_persistent_config()\n", strerror(errno), errno );
			ABORT;
		}
		
			// commit config changes
		if (rotate_file(tmp_filename.c_str(), filename.c_str()) < 0) {
			dprintf( D_ALWAYS, "rotate_file(%s,%s) failed with '%s' "
					 "(errno %d) in set_persistent_config()\n",
					 tmp_filename.c_str(), filename.c_str(),
					 strerror(errno), errno );
			ABORT;
		}
	
		// update admin list in memory
		if (!PersistAdminList.count(admin)) {
			PersistAdminList.insert(admin);
		} else {
			free(admin);
			free(config);
			set_priv(priv);
			return 0;		// if no update is required, then we are done
		}

	} else {					// clear config

		// update admin list in memory
		PersistAdminList.erase(admin);
		if (config) {
			free(config);
			config = NULL;
		}
	}		

	// update admin list on disk
	formatstr( tmp_filename, "%s.tmp", toplevel_persistent_config.c_str() );
	do {
		MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
		unlink( tmp_filename.c_str() );
		fd = safe_open_wrapper_follow( tmp_filename.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0644 );
	} while (fd == -1 && errno == EEXIST);
	if( fd < 0 ) {
		dprintf( D_ALWAYS, "safe_open_wrapper(%s) returned %d '%s' (errno %d) in "
				 "set_persistent_config()\n", tmp_filename.c_str(),
				 fd, strerror(errno), errno );
		ABORT;
	}
	const char param[] = "RUNTIME_CONFIG_ADMIN = ";
	if (write(fd, param, (unsigned int)strlen(param)) != (ssize_t)strlen(param)) {
		dprintf( D_ALWAYS, "write() failed with '%s' (errno %d) in "
				 "set_persistent_config()\n", strerror(errno), errno );
		close(fd);
		ABORT;
	}
	bool first_time = true;
	for (const auto& tmp: PersistAdminList) {
		if (!first_time) {
			if (write(fd, ", ", 2) != 2) {
				dprintf( D_ALWAYS, "write() failed with '%s' (errno %d) in "
						 "set_persistent_config()\n", strerror(errno), errno );
				close(fd);
				ABORT;
			}
		} else {
			first_time = false;
		}
		if (write(fd, tmp.c_str(), tmp.size()) != (ssize_t)tmp.size()) {
			dprintf( D_ALWAYS, "write() failed with '%s' (errno %d) in "
					 "set_persistent_config()\n", strerror(errno), errno );
			close(fd);
			ABORT;
		}
	}
	if (write(fd, "\n", 1) != 1) {
		dprintf( D_ALWAYS, "write() failed with '%s' (errno %d) in "
				 "set_persistent_config()\n", strerror(errno), errno );
		close(fd);
		ABORT;
	}
	if (close(fd) < 0) {
		dprintf( D_ALWAYS, "close() failed with '%s' (errno %d) in "
				 "set_persistent_config()\n", strerror(errno), errno );
		ABORT;
	}
	
	rval = rotate_file( tmp_filename.c_str(),
						toplevel_persistent_config.c_str() );
	if (rval < 0) {
		dprintf( D_ALWAYS, "rotate_file(%s,%s) failed with '%s' (errno %d) "
				 "in set_persistent_config()\n", tmp_filename.c_str(),
				 filename.c_str(), strerror(errno), errno );
		ABORT;
	}

	// if we removed a config, then we should clean up by removing the file(s)
	if (!config || !config[0]) {
		formatstr( filename, "%s.%s", toplevel_persistent_config.c_str(), admin );
		MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
		unlink( filename.c_str() );
		if (PersistAdminList.size() == 0) {
			MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
			unlink( toplevel_persistent_config.c_str() );
		}
	}

	set_priv( priv );
	free( admin );
	if (config) { free( config ); }
	return 0;
}


int
set_runtime_config(char *admin, char *config)
{
	size_t i;

	if (!admin || !admin[0] || !enable_runtime) {
		if (admin)  { free(admin);  }
		if (config) { free(config); }
		return -1;
	}

	if (config && config[0]) {
		for (i=0; i < rArray.size(); i++) {
			if (strcmp(rArray[i].admin, admin) == MATCH) {
				free(admin);
				free(rArray[i].config);
				rArray[i].config = config;
				return 0;
			}
		}

		rArray.emplace_back(admin,config );
	} else {
		// config is nullptr or zero-length, meaning "remove"
		auto it = std::remove_if(rArray.begin(), rArray.end(),
				[&admin](const RuntimeConfigItem &rci) { 
					return strcmp(rci.admin,admin) == MATCH;
				}
		);
		rArray.erase(it, rArray.end());
		free(admin);
		if (config) free(config);
		return 0;
	}
	return 0;
}


static bool Check_config_source_security(FILE* conf_fp, const char * config_source)
{
#ifndef WIN32
		// unfortunately, none of this works on windoze... (yet)
	if ( is_piped_command(config_source) ) {
		fprintf( stderr, "Configuration Error File <%s>: runtime config "
					"not allowed to come from a pipe command\n",
					config_source );
		return false;
	}
	int fd = fileno(conf_fp);
	struct stat statbuf;
	uid_t f_uid;
	int rval = fstat( fd, &statbuf );
	if( rval < 0 ) {
		fprintf( stderr, "Configuration Error File <%s>, fstat() failed: %s (errno: %d)\n",
					config_source, strerror(errno), errno );
		return false;
	}
	f_uid = statbuf.st_uid;
	if( can_switch_ids() ) {
			// if we can switch, the file *must* be owned by root
		if( f_uid != 0 ) {
			fprintf( stderr, "Configuration Error File <%s>, "
						"running as root yet runtime config file owned "
						"by uid %d, not 0!\n", config_source, (int)f_uid );
			return false;
		}
	} else {
			// if we can't switch, at least ensure we own the file
		if( f_uid != get_my_uid() ) {
			fprintf( stderr, "Configuration Error File <%s>, "
						"running as uid %d yet runtime config file owned "
						"by uid %d!\n", config_source, (int)get_my_uid(),
						(int)f_uid );
			return false;
		}
	}
#endif /* ! WIN32 */
	return true;
}

static void process_persistent_config_or_die (const char * source_file, bool top_level)
{
	int rval = 0;

	std::string errmsg;

	MACRO_SOURCE source;
	insert_source(source_file, ConfigMacroSet, source);
	FILE* fp = safe_fopen_wrapper_follow(source_file, "r");
	if ( ! fp) { rval = -1; errmsg = "can't open file"; }
	else {
		if ( ! Check_config_source_security(fp, source_file)) {
			rval = -1;
		} else {
			MACRO_EVAL_CONTEXT ctx; init_macro_eval_context(ctx);
			MacroStreamYourFile ms(fp, source);
			rval = Parse_macros(ms, 0, ConfigMacroSet, 0, &ctx, errmsg, NULL, NULL);
		}
		fclose(fp); fp = NULL;
	}

	if (rval < 0) {
		dprintf( D_ERROR, "Configuration Error Line %d %s while reading"
					"%s persistent config source: %s\n",
					source.line, errmsg.c_str(), top_level ? " top-level" : " ", source_file );
		exit(1);
	}
}

static int
process_persistent_configs()
{
	bool processed = false;

	if( access( toplevel_persistent_config.c_str(), R_OK ) == 0 &&
		PersistAdminList.size() == 0 )
	{
		processed = true;

		process_persistent_config_or_die(toplevel_persistent_config.c_str(), true);

		char* tmp = param ("RUNTIME_CONFIG_ADMIN");
		if (tmp) {
			for (const auto& item: StringTokenIterator(tmp)) {
				PersistAdminList.insert(item);
			}
			free(tmp);
		}
	}

	for (const auto& tmp: PersistAdminList) {
		processed = true;
		std::string config_source;
		formatstr( config_source, "%s.%s", toplevel_persistent_config.c_str(),
							   tmp.c_str() );
		process_persistent_config_or_die(config_source.c_str(), false);
	}
	return (int)processed;
}


static int
process_runtime_configs()
{
	int rval;
	bool processed = false;

	MACRO_SOURCE source;
	insert_source("<runtime>", ConfigMacroSet, source);

	MACRO_EVAL_CONTEXT ctx;
	init_macro_eval_context(ctx);

	for (size_t i=0; i < rArray.size(); i++) {
		processed = true;
		source.line = (int)i;
		rval = Parse_config_string(source, 0, rArray[i].config, ConfigMacroSet, ctx);
		if (rval < 0) {
			dprintf( D_ALWAYS | D_ERROR, "Configuration Error parsing runtime[%zu] name '%s', at line %d in config: %s\n",
					 i, rArray[i].admin, source.meta_off+1, rArray[i].config);
			exit(1);
		}
	}

	return (int)processed;
}


/*
** returns 1 if dynamic (runtime or persistent) configs were
** processed; 0 if no dynamic configs were defined, and -1 on error.
*/
static int
process_dynamic_configs()
{
	int per_rval = 0;
	int run_rval = 0;

	init_dynamic_config();

	if( enable_persistent ) {
		per_rval = process_persistent_configs();
	}

	if( enable_runtime ) {
		run_rval = process_runtime_configs();
	}

	if( per_rval < 0 || run_rval < 0 ) {
		return -1;
	}
	if( per_rval || run_rval ) {
		return 1;
	}
	return 0;
}

struct _write_macros_args {
	FILE * fh;
	int    options;
	const char * pszLast;
};

bool write_macro_variable(void* user, HASHITER & it) {
	struct _write_macros_args * pargs = (struct _write_macros_args *)user;
	FILE * fh = pargs->fh;
	const int options = pargs->options;

	MACRO_META * pmeta = hash_iter_meta(it);
	// if is default, detected, or matches default, skip it
	if (pmeta->matches_default || pmeta->param_table || pmeta->inside) {
		if ( ! (options & WRITE_MACRO_OPT_DEFAULT_VALUES))
			return true; // keep scanning
	} else {
	}
	const char * name = hash_iter_key(it);
	if (pargs->pszLast && (MATCH == strcasecmp(name, pargs->pszLast))) {
		// don't write entries more than once.
		return true;
	}

	const char * rawval = hash_iter_value(it);
	fprintf(fh, "%s = %s\n", name, rawval ? rawval : "");

	if (options & WRITE_MACRO_OPT_SOURCE_COMMENT) {
		const char * filename = config_source_by_id(pmeta->source_id);
		if (pmeta->source_line < 0) {
			if (pmeta->source_id == 1) {
				fprintf(fh, " # at: %s, item %d\n", filename, pmeta->param_id);
			} else {
				fprintf(fh, " # at: %s\n", filename);
			}
		} else {
			fprintf(fh, " # at: %s, line %d\n", filename, pmeta->source_line);
		}
	}

	pargs->pszLast = name;
	return true;
}

int write_macros_to_file(const char* pathname, MACRO_SET& macro_set, int options)
{
	FILE * fh = safe_fopen_wrapper_follow(pathname, "w");
	if ( ! fh) {
		dprintf(D_ALWAYS, "Failed to create configuration file %s.\n", pathname);
		return -1;
	}

	struct _write_macros_args args;
	memset(&args, 0, sizeof(args));
	args.fh = fh;
	args.options = options;

	int iter_opts = HASHITER_SHOW_DUPS;
	HASHITER it = hash_iter_begin(macro_set, iter_opts);
	while ( ! hash_iter_done(it)) {
		if ( ! write_macro_variable(&args, it))
			break;
		hash_iter_next(it);
	}
	hash_iter_delete(&it);

	if (fclose(fh) == -1) {
		dprintf(D_ALWAYS, "Error closing new configuration file %s.\n", pathname);
		return -1;
	}
	return 0;
}

int write_config_file(const char* pathname, int options) {
	return write_macros_to_file(pathname, ConfigMacroSet, options);
}

// so that condor_config_val can test config if expressions.
bool config_test_if_expression(const char * expr, bool & result, const char * localname, const char * subsys, std::string & err_reason)
{
	//PRAGMA_REMIND("add ad to config_test_if_expression")
	MACRO_EVAL_CONTEXT ctx = { localname, subsys, NULL, false, 0, 0, 0 };
	if (ctx.localname && !ctx.localname[0]) ctx.localname = NULL;
	if (ctx.subsys && !ctx.subsys[0]) ctx.subsys = NULL;
	return Test_config_if_expression(expr, result, err_reason, ConfigMacroSet, ctx);
}

bool param(std::string &buf,char const *param_name, char const *default_value)
{
	bool found = false;
	char *param_value = param(param_name);
	if( param_value ) {
		buf = param_value;
		found = true;
	}
	else if( default_value ) {
		buf = default_value;
	}
	else {
		buf = "";
	}
	free( param_value );

	return found;
}


bool param_eval_string(std::string &buf, const char *param_name, const char *default_value,
                       classad::ClassAd *me, classad::ClassAd *target)
{
	if (!param(buf, param_name, default_value)) {return false;}

	ClassAd rhs;
	if (me) {
		rhs = *me;
	}
	classad::ClassAdParser parser;
	classad::ExprTree *expr_raw = parser.ParseExpression(buf);

	std::string result;
	if( rhs.Insert( "_condor_bool", expr_raw) && EvalString("_condor_bool", &rhs, target, result) ) {
		buf = result;
		return true;
	}
	return false;
}


/* Take a param which is the name of an executable, and safely expand it
 * if required to a full pathname by searching the PATH environment.
 * Useful for seting an entry in the param table like "MAIL=mailx", and
 * then this function will expand it to "/bin/mailx" or "/usr/bin/mailx".
 * If the path cannot be expanded safely (to something that could be execed
 * by root), then NULL is returned.  For instance using the MAIL example,
 * this function would not expand the path to "/tmp/mailx" even if /tmp
 * is somehow in the path. Like param(), if the return value is not NULL,
 * it must be freed() by the caller.
 */
char *
param_with_full_path(const char *name)
{
	char * real_path = NULL;

	if (!name || (name && !name[0])) {
		return NULL;
	}

	// lookup name in param table first, so admins can configure what they want
	char * command = param(name);
	if (command && !command[0]) {
		// treat empty string as a NULL
		free(command);
		command = NULL;
	}

	// if not found in the param table, just use the value of name as the command.
	if ( command == NULL ) {
		command = strdup(name);
	}

	if (command && !fullpath(command)) {
		// Fullpath unknown, so we will try and find it ourselves.
		// Search the PATH for it, and if found, confirm that the path is "safe"
		// for a privledged user to run.  "safe" means ok to exec as root,
		// so either path starts with /bin, /usr, etc, ie
		// the path is not writeable by any user other than root.
		// Store the result back into the param table so we don't repeat
		// operation every time we are invoked, and so result can be
		// inspected with condor_config_val.
		std::string p = which(command
#ifndef WIN32
			// on UNIX, always include system path entries
			, "/bin:/usr/bin:/sbin:/usr/sbin"
#endif
			);
		free(command);
		command = NULL;
		if ((real_path = realpath(p.c_str(),NULL))) {
			p = real_path;
			free(real_path);
			if (
#ifndef WIN32
				p.find("/usr/")==0 ||
				p.find("/bin/")==0 ||
				p.find("/sbin/")==0
#else
				p.find("\\Windows\\")==0 ||
				(p[1]==':' && p.find("\\Windows\\")==2)  // ie C:\Windows
#endif
			)
			{
				// we have a full path, and it looks safe.
				// restash command as the full path into config table.
				command = strdup( p.c_str() );
				config_insert(name,command);
			}
		}
	}

	return command;
}
