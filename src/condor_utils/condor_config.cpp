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
#include "condor_syscall_mode.h"
#include "pool_allocator.h"
#include "condor_config.h"
#include "condor_string.h"
#include "string_list.h"
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
#include "condor_auth_x509.h"
#include "setenv.h"
#include "HashTable.h"
#include "extra_param_info.h"
#include "condor_uid.h"
#include "condor_mkstemp.h"
#include "basename.h"
#include "condor_random_num.h"
#include "extArray.h"
#include "subsystem_info.h"
#include "param_info.h"
#include "param_info_tables.h"
#include "Regex.h"
#include <algorithm> // for std::sort

// define this to keep param who's values match defaults from going into to runtime param table.
#define DISCARD_CONFIG_MATCHING_DEFAULT
// define this to parse for #opt:newcomment/#opt:oldcomment to decide commenting rules
#define PARSE_CONFIG_TO_DECIDE_COMMENT_RULES

extern "C" {
	
// Function prototypes
void real_config(const char* host, int wantsQuiet, int config_options);
int Read_config(const char*, MACRO_SET& macro_set, int, bool, const char * subsys);
bool is_piped_command(const char* filename);
bool is_valid_command(const char* cmdToExecute);
int SetSyscalls(int);
char* find_global();
char* find_file(const char*, const char*);
void init_tilde();
void fill_attributes();
void check_domain_attributes();
void clear_config();
void reinsert_specials(const char* host);
void process_config_source(const char*, const char*, const char*, int);
void process_locals( const char*, const char*);
void process_directory( const char* dirlist, const char* host);
static int  process_dynamic_configs();
void check_params();

// External variables
extern int	ConfigLineNo;
}  /* End extern "C" */


// Global variables
static MACRO_DEFAULTS ConfigMacroDefaults = { 0, NULL, NULL };
static MACRO_SET ConfigMacroSet = {
	0, 0,
	/* CONFIG_OPT_WANT_META | CONFIG_OPT_KEEP_DEFAULT | */ 0,
	0, NULL, NULL, ALLOCATION_POOL(), std::vector<const char*>(), &ConfigMacroDefaults };
const MACRO_SOURCE DetectedMacro = { true,  0, -2, -1, -2 };
const MACRO_SOURCE DefaultMacro  = { true,  1, -2, -1, -2 };
const MACRO_SOURCE EnvMacro      = { false, 2, -2, -1, -2 };
const MACRO_SOURCE WireMacro     = { false, 3, -2, -1, -2 };

#ifdef _POOL_ALLOCATOR

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

void
_allocation_pool::clear()
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


char * _allocation_pool::consume(int cb, int cbAlign)
{
	if ( ! cb) return NULL;
	cbAlign = MAX(cbAlign, this->alignment());
	int cbConsume = (cb + cbAlign-1) & ~(cbAlign-1);

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
		cbFree = ph->cbAlloc - ph->ixFree;
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
		}

		PRAGMA_REMIND("TJ: fix to account for extra size needed to align start ptr")
		if (ph->ixFree + cbConsume > ph->cbAlloc) {
			int cbAlloc = MAX(ph->cbAlloc * 2, cbConsume);
			ph = &this->phunks[++this->nHunk];
			ph->reserve(cbAlloc);
		}
	}

	char * pb = ph->pb + ph->ixFree;
	if (cbConsume > cb) memset(pb+cb, 0, cbConsume - cb);
	ph->ixFree += cbConsume;
	return pb;
}

const char *
_allocation_pool::insert(const char * pbInsert, int cbInsert)
{
	if ( ! pbInsert || ! cbInsert) return NULL;
	char * pb = this->consume(cbInsert, 1);
	if (pb) memcpy(pb, pbInsert, cbInsert);
	return pb;
}

const char *
_allocation_pool::insert(const char * psz)
{
	if ( ! psz) return NULL;
	int cb = (int)strlen(psz);
	if ( ! cb) return "";
	return this->insert(psz, cb+1);
}

bool
_allocation_pool::contains(const char * pb)
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

void
_allocation_pool::reserve(int cbReserve)
{
	// for now, just consume some memory, and then free it back to the pool
	this->free(this->consume(cbReserve, 1));
}

void
_allocation_pool::compact(int cbLeaveFree)
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

void
_allocation_pool::free(const char * pb)
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

MyString global_config_source;
StringList local_config_sources;

param_functions config_p_funcs;


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
			int cch = strlen(psz);
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

// Function implementations

void
config_fill_ad( ClassAd* ad, const char *prefix )
{
	char 		*tmp;
	char		*expr;
	StringList	reqdExprs;
	MyString 	buffer;

	if( !ad ) return;

	if ( ( NULL == prefix ) && get_mySubSystem()->hasLocalName() ) {
		prefix = get_mySubSystem()->getLocalName();
	}

	buffer.formatstr( "%s_EXPRS", get_mySubSystem()->getName() );
	tmp = param( buffer.Value() );
	if( tmp ) {
		reqdExprs.initializeFromString (tmp);	
		free (tmp);
	}

	buffer.formatstr( "%s_ATTRS", get_mySubSystem()->getName() );
	tmp = param( buffer.Value() );
	if( tmp ) {
		reqdExprs.initializeFromString (tmp);	
		free (tmp);
	}

	if(prefix) {
		buffer.formatstr( "%s_%s_EXPRS", prefix, get_mySubSystem()->getName() );
		tmp = param( buffer.Value() );
		if( tmp ) {
			reqdExprs.initializeFromString (tmp);	
			free (tmp);
		}

		buffer.formatstr( "%s_%s_ATTRS", prefix, get_mySubSystem()->getName() );
		tmp = param( buffer.Value() );
		if( tmp ) {
			reqdExprs.initializeFromString (tmp);	
			free (tmp);
		}

	}

	if( !reqdExprs.isEmpty() ) {
		reqdExprs.rewind();
		while ((tmp = reqdExprs.next())) {
			expr = NULL;
			if(prefix) {
				buffer.formatstr("%s_%s", prefix, tmp);	
				expr = param(buffer.Value());
			}
			if(!expr) {
				expr = param(tmp);
			}
			if(expr == NULL) continue;
			buffer.formatstr( "%s = %s", tmp, expr );

			if( !ad->Insert( buffer.Value() ) ) {
				dprintf(D_ALWAYS,
						"CONFIGURATION PROBLEM: Failed to insert ClassAd attribute %s.  The most common reason for this is that you forgot to quote a string value in the list of attributes being added to the %s ad.\n",
						buffer.Value(), get_mySubSystem()->getName() );
			}

			free( expr );
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
	// entry in the param table so that we can sort it. So we have to sort
	// sort the metadata the metadata first, then the param table itself
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
void validate_config(bool abort_if_invalid)
{
	HASHITER it = hash_iter_begin(ConfigMacroSet, HASHITER_NO_DEFAULTS);
	unsigned int invalid_entries = 0;
	MyString tmp;
	MyString output = "The following configuration macros appear to contain default values that must be changed before Condor will run.  These macros are:\n";
	while( ! hash_iter_done(it) ) {
		const char * val = hash_iter_value(it);
		if (val && strstr(val, FORBIDDEN_CONFIG_VAL)) {
			const char * name = hash_iter_key(it);
#if 1
			MyString filename;
			param_get_location(hash_iter_meta(it), filename);
			tmp.formatstr("   %s (found at %s)\n", name, filename.Value());
#else
			MyString filename;
			int line_number;
			param_get_location(name, filename, line_number);
			tmp.formatstr("   %s (found on line %d of %s)\n", name, line_number, filename.Value());
#endif
			output += tmp;
			invalid_entries++;
		}
		hash_iter_next(it);
	}
	hash_iter_delete(&it);
	if(invalid_entries > 0) {
		if (abort_if_invalid) {
			EXCEPT("%s", output.Value());
		} else {
			dprintf(D_ALWAYS, "%s", output.Value());
		}
	}
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
int param_names_matching(Regex & re, ExtArray<const char *>& names)
{	
	int cAdded = 0;
	HASHITER it = hash_iter_begin(ConfigMacroSet);
	while( ! hash_iter_done(it)) {
		const char *name = hash_iter_key(it);
		if (re.match(name)) {
			names.add(name);
			++cAdded;
		}
		hash_iter_next(it);
	}
	hash_iter_delete(&it);

	return cAdded;
}

int param_names_matching(Regex& re, std::vector<std::string>& names) {
    const int s0 = names.size();
    HASHITER it = hash_iter_begin(ConfigMacroSet);
    for (;  !hash_iter_done(it);  hash_iter_next(it)) {
		const char *name = hash_iter_key(it);
		if (re.match(name)) names.push_back(name);
	}
    hash_iter_delete(&it);
    return names.size() - s0;
}

// the generic config entry point for most call sites
void config() 
{
	const bool abort_if_invalid = true;
	return config_ex(0, abort_if_invalid, CONFIG_OPT_WANT_META);
}

// the full-figured config entry point
void config_ex(int wantsQuiet, bool abort_if_invalid, int config_options)
{
#ifdef WIN32
	char *locale = setlocale( LC_ALL, "English" );
	//dprintf ( D_LOAD | D_VERBOSE, "Locale: %s\n", locale );
#endif
	real_config(NULL, wantsQuiet, config_options);
	validate_config(abort_if_invalid);
}


void
config_host(const char* host, int wantsQuiet, int config_options)
{
	real_config(host, wantsQuiet, config_options);
}

/* This function initialize GSI (maybe other) authentication related
   stuff Daemons that should use the condor daemon credentials should
   set the argument is_daemon=true.  This function is automatically
   called at config init time with is_daemon=false, so that all
   processes get the basic auth config.  The order of calls to this
   function do not matter, as the results are only additive.
   Therefore, calling with is_daemon=false and then with
   is_daemon=true or vice versa are equivalent.
*/
void
condor_auth_config(int is_daemon)
{
#if !defined(SKIP_AUTHENTICATION) && defined(HAVE_EXT_GLOBUS)

		// First, if there is X509_USER_PROXY, we clear it
		// (if we're a daemon).
	if ( is_daemon ) {
		UnsetEnv( "X509_USER_PROXY" );
	}

		// Next, we param the configuration file for GSI related stuff and
		// set the corresponding environment variables for it

	char *pbuf = 0;
	char *proxy_buf = 0;
	char *cert_buf = 0;
	char *key_buf = 0;
	char *trustedca_buf = 0;
	char *mapfile_buf = 0;

	MyString buffer;


		// Here's how it works. If you define any of
		// GSI_DAEMON_CERT, GSI_DAEMON_KEY, GSI_DAEMON_PROXY, or
		// GSI_DAEMON_TRUSTED_CA_DIR, those will get stuffed into the
		// environment.
		//
		// Everything else depends on GSI_DAEMON_DIRECTORY. If
		// GSI_DAEMON_DIRECTORY is not defined, then only settings that are
		// defined above will be placed in the environment, so if you
		// want the cert and host in a non-standard location, but want to use
		// /etc/grid-security/certifcates as the trusted ca dir, only
		// define GSI_DAEMON_CERT and GSI_DAEMON_KEY, and not
		// GSI_DAEMON_DIRECTORY and GSI_DAEMON_TRUSTED_CA_DIR
		//
		// If GSI_DAEMON_DIRECTORY is defined, condor builds a "reasonable"
		// default out of what's already been defined and what it can
		// construct from GSI_DAEMON_DIRECTORY  - ie  the trusted CA dir ends
		// up as in $(GSI_DAEMON_DIRECTORY)/certificates, and so on
		// The proxy is not included in the "reasonable defaults" section

		// First, let's get everything we might want
	pbuf = param( STR_GSI_DAEMON_DIRECTORY );
	trustedca_buf = param( STR_GSI_DAEMON_TRUSTED_CA_DIR );
	mapfile_buf = param( STR_GSI_MAPFILE );
	if( is_daemon ) {
		proxy_buf = param( STR_GSI_DAEMON_PROXY );
		cert_buf = param( STR_GSI_DAEMON_CERT );
		key_buf = param( STR_GSI_DAEMON_KEY );
	}

	if (pbuf) {

		if( !trustedca_buf) {
			buffer.formatstr( "%s%ccertificates", pbuf, DIR_DELIM_CHAR);
			SetEnv( STR_GSI_CERT_DIR, buffer.Value() );
		}

		if (!mapfile_buf ) {
			buffer.formatstr( "%s%cgrid-mapfile", pbuf, DIR_DELIM_CHAR);
			SetEnv( STR_GSI_MAPFILE, buffer.Value() );
		}

		if( is_daemon ) {
			if( !cert_buf ) {
				buffer.formatstr( "%s%chostcert.pem", pbuf, DIR_DELIM_CHAR);
				SetEnv( STR_GSI_USER_CERT, buffer.Value() );
			}
	
			if (!key_buf ) {
				buffer.formatstr( "%s%chostkey.pem", pbuf, DIR_DELIM_CHAR);
				SetEnv( STR_GSI_USER_KEY, buffer.Value() );
			}
		}

		free( pbuf );
	}

	if(trustedca_buf) {
		SetEnv( STR_GSI_CERT_DIR, trustedca_buf );
		free(trustedca_buf);
	}

	if (mapfile_buf) {
		SetEnv( STR_GSI_MAPFILE, mapfile_buf );
		free(mapfile_buf);
	}

	if( is_daemon ) {
		if(proxy_buf) {
			SetEnv( STR_GSI_USER_PROXY, proxy_buf );
			free(proxy_buf);
		}

		if(cert_buf) {
			SetEnv( STR_GSI_USER_CERT, cert_buf );
			free(cert_buf);
		}

		if(key_buf) {
			SetEnv( STR_GSI_USER_KEY, key_buf );
			free(key_buf);
		}
	}

#else
	(void) is_daemon;	// Quiet 'unused parameter' warnings
#endif
}

void
real_config(const char* host, int wantsQuiet, int config_options)
{
	char* config_source = NULL;
	char* tmp = NULL;
	int scm;

	static bool first_time = true;
	if( first_time ) {
		first_time = false;
		init_config(config_options);
	} else {
			// Clear out everything in our config hash table so we can
			// rebuild it from scratch.
		clear_config();
	}

	dprintf( D_CONFIG, "config: using subsystem '%s', local '%s'\n",
			 get_mySubSystem()->getName(), get_mySubSystem()->getLocalName("") );

		/*
		  N.B. if we are using the yellow pages, system calls which are
		  not supported by either remote system calls or file descriptor
 		  mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
		*/
	scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );

		// Try to find user "condor" in the passwd file.
	init_tilde();

		// Insert an entry for "tilde", (~condor)
	if( tilde ) {
		insert("TILDE", tilde, ConfigMacroSet, DetectedMacro);

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

	char* env = getenv( EnvGetName(ENV_CONFIG) );
	if( env && strcasecmp(env, "ONLY_ENV") == MATCH ) {
			// special case, no config source desired
		have_config_source = false;
	}

	if( have_config_source && 
		! (config_source = find_global()) &&
		! continue_if_no_config)
	{
		if( wantsQuiet ) {
			fprintf( stderr, "%s error: can't find config source.\n",
					 myDistro->GetCap() );
			exit( 1 );
		}
		fprintf(stderr,"\nNeither the environment variable %s_CONFIG,\n",
				myDistro->GetUc() );
#	  if defined UNIX
		fprintf(stderr,"/etc/%s/, nor ~%s/ contain a %s_config source.\n",
				myDistro->Get(), myDistro->Get(), myDistro->Get() );
#	  elif defined WIN32
		fprintf(stderr,"nor the registry contains a %s_config source.\n", myDistro->Get() );
#	  else
#		error "Unknown O/S"
#	  endif
		fprintf( stderr,"Either set %s_CONFIG to point to a valid config "
				"source,\n", myDistro->GetUc() );
#	  if defined UNIX
		fprintf( stderr,"or put a \"%s_config\" file in /etc/%s or ~%s/\n",
				 myDistro->Get(), myDistro->Get(), myDistro->Get() );
#	  elif defined WIN32
		fprintf( stderr,"or put a \"%s_config\" source in the registry at:\n"
				 " HKEY_LOCAL_MACHINE\\Software\\%s\\%s_CONFIG",
				 myDistro->Get(), myDistro->Get(), myDistro->GetUc() );
#	  else
#		error "Unknown O/S"
#	  endif
		fprintf( stderr, "Exiting.\n\n" );
		exit( 1 );
	}

		// Read in the global file
	if( config_source ) {
		process_config_source( config_source, "global config source", NULL, true );
		global_config_source = config_source;
		free( config_source );
		config_source = NULL;
	}

		// Insert entries for "hostname" and "full_hostname".  We do
		// this here b/c we need these macros defined so that we can
		// find the local config source if that's defined in terms of
		// hostname or something.  However, we do this after reading
		// the global config source so people can put the
		// DEFAULT_DOMAIN_NAME parameter somewhere if they need it.
		// -Derek Wright <wright@cs.wisc.edu> 5/11/98
	if( host ) {
		insert("HOSTNAME", host, ConfigMacroSet, DetectedMacro);
	} else {
		insert("HOSTNAME", get_local_hostname().Value(), ConfigMacroSet, DetectedMacro);
	}
	insert("FULL_HOSTNAME", get_local_fqdn().Value(), ConfigMacroSet, DetectedMacro);

		// Also insert tilde since we don't want that over-written.
	if( tilde ) {
		insert("TILDE", tilde, ConfigMacroSet, DetectedMacro);
	}

		// Read in the LOCAL_CONFIG_FILE as a string list and process
		// all the files in the order they are listed.
	char *dirlist = param("LOCAL_CONFIG_DIR");
	if(dirlist) {
		process_directory(dirlist, host);
	}
	process_locals( "LOCAL_CONFIG_FILE", host );

	char* newdirlist = param("LOCAL_CONFIG_DIR");
	if(newdirlist) {
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

		// Now, insert any macros defined in the environment.
	char **my_environ = GetEnviron();
	for( int i = 0; my_environ[i]; i++ ) {
		char magic_prefix[MAX_DISTRIBUTION_NAME + 3];	// case-insensitive
		strcpy( magic_prefix, "_" );
		strcat( magic_prefix, myDistro->Get() );
		strcat( magic_prefix, "_" );
		int prefix_len = strlen( magic_prefix );

		// proceed only if we see the magic prefix
		if( strncasecmp( my_environ[i], magic_prefix, prefix_len ) != 0 ) {
			continue;
		}

		char *varname = strdup( my_environ[i] );
		if( !varname ) {
			EXCEPT( "Out of memory in %s:%d\n", __FILE__, __LINE__ );
		}

		// isolate variable name by finding & nulling the '='
		int equals_offset = strchr( varname, '=' ) - varname;
		varname[equals_offset] = '\0';
		// isolate value by pointing to everything after the '='
		char *varvalue = varname + equals_offset + 1;
//		assert( !strcmp( varvalue, getenv( varname ) ) );
		// isolate Condor macro_name by skipping magic prefix
		char *macro_name = varname + prefix_len;

		// special macro START_owner needs to be expanded (for the
		// glide-in code) [which should probably be fixed to use
		// the general mechanism and set START itself --pfc]
		if( !strcmp( macro_name, "START_owner" ) ) {
			MyString ownerstr;
			ownerstr.formatstr( "Owner == \"%s\"", varvalue );
			insert("START", ownerstr.Value(), ConfigMacroSet, EnvMacro);
		}
		// ignore "_CONDOR_" without any macro name attached
		else if( macro_name[0] != '\0' ) {
			insert(macro_name, varvalue, ConfigMacroSet, EnvMacro);
		}

		free( varname );
	}

		// Insert the special macros.  We don't want the user to
		// override them, since it's not going to work.
		// We also do this last because some things (like USERNAME)
		// may depend on earlier configuration (USERID_MAP).
	reinsert_specials( host );

	process_dynamic_configs();

	if (config_source) {
		free( config_source );
	}

	// init_ipaddr and init_full_hostname is now obsolete
	init_network_interfaces(TRUE);

		// Now that we're done reading files, if DEFAULT_DOMAIN_NAME
		// is set, we need to re-initialize my_full_hostname().
	if( (tmp = param("DEFAULT_DOMAIN_NAME")) ) {
		free( tmp );
		//init_full_hostname();
		init_local_hostname();
	}

		// Also, we should be safe to process the NETWORK_INTERFACE
		// parameter at this point, if it's set.
	//init_ipaddr( TRUE );


		// The IPv6 code currently caches some results that depend
		// on configuration settings such as NETWORK_INTERFACE.
		// Therefore, force the cache to be reset, now that the
		// configuration has been loaded.
	init_local_hostname();

		// Re-insert the special macros.  We don't want the user to
		// override them, since it's not going to work.
	reinsert_specials( host );

		// Make sure our FILESYSTEM_DOMAIN and UID_DOMAIN settings are
		// correct.
	check_domain_attributes();

		// once the config table is fully populated, we can optimize it.
		// WARNING!! if you insert new params after this, the able *might*
		// be de-optimized.
	optimize_macros(ConfigMacroSet);

		// We have to do some platform-specific checking to make sure
		// all the parameters we think are defined really are.
	check_params();

	condor_except_should_dump_core( param_boolean("ABORT_ON_EXCEPTION", false) );

		// Daemons should additionally call condor_auth_config()
		// explicitly with the argument is_daemon=true.  Here, we just
		// call with is_daemon=false, since that is fine for both daemons
		// and non-daemons to do.
	condor_auth_config( false );

	ConfigConvertDefaultIPToSocketIP();

	//Configure condor_fsync
	condor_fsync_on = param_boolean("CONDOR_FSYNC", true);
	if(!condor_fsync_on)
		dprintf(D_FULLDEBUG, "FSYNC while writing user logs turned off.\n");

	(void)SetSyscalls( scm );
}


void
process_config_source( const char* file, const char* name,
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
		rval = Read_config( file, ConfigMacroSet, EXPAND_LAZY,
							false, get_mySubSystem()->getName());
		if( rval < 0 ) {
			fprintf( stderr,
					 "Configuration Error Line %d while reading %s %s\n",
					 ConfigLineNo, name, file );
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
	StringList sources_to_process, sources_done;
	char *source, *sources_value;
	int local_required;

	local_required = param_boolean_crufty("REQUIRE_LOCAL_CONFIG_FILE", true);

	sources_value = param( param_name );
	if( sources_value ) {
		if ( is_piped_command( sources_value ) ) {
			sources_to_process.insert( sources_value );
		} else {
			sources_to_process.initializeFromString( sources_value );
		}
		if (simulated_local_config) sources_to_process.append(simulated_local_config);
		sources_to_process.rewind();
		while( (source = sources_to_process.next()) ) {
			process_config_source( source, "config source", host,
								   local_required );
			local_config_sources.append( source );

			sources_done.append(source);

			char* new_sources_value = param(param_name);
			if(new_sources_value) {
				if(strcmp(sources_value, new_sources_value) ) {
				// the file we just processed altered the list of sources to
				// process
					sources_to_process.clearAll();
					if ( is_piped_command( new_sources_value ) ) {
						sources_to_process.insert( new_sources_value );
					} else {
						sources_to_process.initializeFromString(new_sources_value);
					}

					// remove all the ones we've finished from the old list
                	sources_done.rewind();
                	while( (source = sources_done.next()) ) {
						sources_to_process.remove(source);
					}
					sources_to_process.rewind();
					free(sources_value);
					sources_value = new_sources_value;
				} else {
					free(new_sources_value);
				}
			}
		}
		free(sources_value);
	}
}

int compareFiles(const void *a, const void *b) {
	 return strcmp(*(char *const*)a, *(char *const*)b);
}

static void
get_exclude_regex(Regex &excludeFilesRegex)
{
	const char* _errstr;
	int _erroffset;
	char* excludeRegex = param("LOCAL_CONFIG_DIR_EXCLUDE_REGEXP");
	if(excludeRegex) {
		if (!excludeFilesRegex.compile(excludeRegex,
									&_errstr, &_erroffset)) {
			EXCEPT("LOCAL_CONFIG_DIR_EXCLUDE_REGEXP "
				   "config parameter is not a valid "
				   "regular expression.  Value: %s,  Error: %s",
				   excludeRegex, _errstr ? _errstr : "");
		}
		if(!excludeFilesRegex.isInitialized() ) {
			EXCEPT("Could not init regex "
				   "to exclude files in %s\n", __FILE__);
		}
	}
	free(excludeRegex);
}

bool
get_config_dir_file_list( char const *dirpath, StringList &files )
{
	Regex excludeFilesRegex;
	get_exclude_regex(excludeFilesRegex);

	Directory dir(dirpath);
	if(!dir.Rewind()) {
		dprintf(D_ALWAYS, "Cannot open %s: %s\n", dirpath, strerror(errno));
		return false;
	}

	const char *file;
	while( (file = dir.Next()) ) {
		// don't consider directories
		// maybe we should squash symlinks here...
		if(! dir.IsDirectory() ) {
			if(!excludeFilesRegex.isInitialized() ||
			   !excludeFilesRegex.match(file)) {
				files.append(dir.GetFullPath());
			} else {
				dprintf(D_FULLDEBUG|D_CONFIG,
						"Ignoring config file "
						"based on "
						"LOCAL_CONFIG_DIR_EXCLUDE_REGEXP, "
						"'%s'\n", dir.GetFullPath());
			}
		}
	}

	files.qsort();
	return true;
}

// examine each file in a directory and treat it as a config file
void
process_directory( const char* dirlist, const char* host )
{
	StringList locals;
	const char *dirpath;
	int local_required;
	
	local_required = param_boolean_crufty("REQUIRE_LOCAL_CONFIG_FILE", true);

	if(!dirlist) { return; }
	locals.initializeFromString( dirlist );
	locals.rewind();
	while( (dirpath = locals.next()) ) {
		StringList file_list;
		get_config_dir_file_list(dirpath,file_list);
		file_list.rewind();

		char const *file;
		while( (file=file_list.next()) ) {
			process_config_source( file, "config source", host, local_required );

			local_config_sources.append(file);
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
	if( (pw=getpwnam( myDistro->Get() )) ) {
		tilde = strdup( pw->pw_dir );
	}
# else
	// On Windows, we'll just look in the registry for TILDE.
	HKEY	handle;
	char regKey[1024];

	snprintf( regKey, 1024, "Software\\%s", myDistro->GetCap() );

	if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, regKey,
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


char*
find_global()
{
	MyString	file;
	file.formatstr( "%s_config", myDistro->Get() );
	return find_file( EnvGetName( ENV_CONFIG), file.Value() );
}


// Find location of specified file
char*
find_file(const char *env_name, const char *file_name)
{
	char* config_source = NULL;
	char* env = NULL;
	int fd = 0;

		// If we were given an environment variable name, try that first.
	if( env_name && (env = getenv( env_name )) ) {
		config_source = strdup( env );
		StatInfo si( config_source );
		switch( si.Error() ) {
		case SIGood:
			if( si.IsDirectory() ) {
				fprintf( stderr, "File specified in %s environment "
						 "variable:\n\"%s\" is a directory.  "
						 "Please specify a file.\n", env_name,
						 config_source );
				free( config_source );
				config_source = NULL;
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
				free( config_source );
				exit( 1 );
				break;
			}
			// Otherwise, we're happy
			return config_source;

		case SIFailure:
			fprintf( stderr, "Cannot stat file specified in %s "
					 "environment variable:\n\"%s\", errno: %d\n",
					 env_name, config_source, si.Errno() );
			free( config_source );
			exit( 1 );
			break;
		}
	}

# ifdef UNIX

	if (!config_source) {
			// List of condor_config file locations we'll try to open.
			// As soon as we find one, we'll stop looking.
		const int locations_length = 4;
		MyString locations[locations_length];
			// 1) $HOME/.condor/condor_config
		struct passwd *pw = getpwuid( geteuid() );
		if ( !can_switch_ids() && pw && pw->pw_dir ) {
			formatstr( locations[0], "%s/.%s/%s", pw->pw_dir, myDistro->Get(),
					 file_name );
		}
			// 2) /etc/condor/condor_config
		locations[1].formatstr( "/etc/%s/%s", myDistro->Get(), file_name );
			// 3) /usr/local/etc/condor_config (FreeBSD)
		locations[2].formatstr( "/usr/local/etc/%s", file_name );
		if (tilde) {
				// 4) ~condor/condor_config
			locations[3].formatstr( "%s/%s", tilde, file_name );
		}

		int ctr;	
		for (ctr = 0 ; ctr < locations_length; ctr++) {
				// Only use this file if the path isn't empty and
				// if we can read it properly.
			if (!locations[ctr].IsEmpty()) {
				config_source = strdup(locations[ctr].Value());
				if ((fd = safe_open_wrapper_follow(config_source, O_RDONLY)) < 0) {
					free(config_source);
					config_source = NULL;
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
	char	regKey[256];

	snprintf( regKey, 256, "Software\\%s", myDistro->GetCap() );
	if ( !config_source && RegOpenKeyEx(HKEY_LOCAL_MACHINE, regKey,
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
				config_source = strdup(the_path);

				if ( strncmp(config_source, "\\\\", 2 ) == 0 ) {
					// UNC Path, so run a 'net use' on it first.
					NETRESOURCE nr;
					nr.dwType = RESOURCETYPE_DISK;
					nr.lpLocalName = NULL;
					nr.lpRemoteName = condor_dirname(config_source);
					nr.lpProvider = NULL;
					
					if ( NO_ERROR != WNetAddConnection2(
										&nr,   /* NetResource */
										NULL,  /* password (default) */
										NULL,  /* username (default) */
										0      /* flags (none) */
						) ) {

						if ( GetLastError() == ERROR_INVALID_PASSWORD ) {
							// try again with an empty password
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

					if (nr.lpRemoteName) {
						free(nr.lpRemoteName);
					}
				}

				if( !(is_piped_command(config_source) &&
					  is_valid_command(config_source)) &&
					(fd = safe_open_wrapper_follow( config_source, O_RDONLY)) < 0 ) {

					free( config_source );
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
	MyString val;

	if( (tmp = sysapi_condor_arch()) != NULL ) {
		insert("ARCH", tmp, ConfigMacroSet, DetectedMacro);
	}

	if( (tmp = sysapi_uname_arch()) != NULL ) {
		insert("UNAME_ARCH", tmp, ConfigMacroSet, DetectedMacro);
	}

	if( (tmp = sysapi_opsys()) != NULL ) {
		insert("OPSYS", tmp, ConfigMacroSet, DetectedMacro);

		int ver = sysapi_opsys_version();
		if (ver > 0) {
			val.formatstr("%d", ver);
			insert("OPSYSVER", val.Value(), ConfigMacroSet, DetectedMacro);
		}
	}

	if( (tmp = sysapi_opsys_versioned()) != NULL ) {
		insert("OPSYSANDVER", tmp, ConfigMacroSet, DetectedMacro);
	}

	if( (tmp = sysapi_uname_opsys()) != NULL ) {
		insert("UNAME_OPSYS", tmp, ConfigMacroSet, DetectedMacro);
	}

	int major_ver = sysapi_opsys_major_version();
	if (major_ver > 0) {
		val.formatstr("%d", major_ver);
		insert("OPSYSMAJORVER", val.Value(), ConfigMacroSet, DetectedMacro);
	}

	if( (tmp = sysapi_opsys_name()) != NULL ) {
		insert("OPSYSNAME", tmp, ConfigMacroSet, DetectedMacro);
	}
	
	if( (tmp = sysapi_opsys_long_name()) != NULL ) {
		insert("OPSYSLONGNAME", tmp, ConfigMacroSet, DetectedMacro);
	}

	if( (tmp = sysapi_opsys_short_name()) != NULL ) {
		insert("OPSYSSHORTNAME", tmp, ConfigMacroSet, DetectedMacro);
	}

	if( (tmp = sysapi_opsys_legacy()) != NULL ) {
		insert("OPSYSLEGACY", tmp, ConfigMacroSet, DetectedMacro);
	}

#if ! defined WIN32
        // temporary attributes for raw utsname info
	if( (tmp = sysapi_utsname_sysname()) != NULL ) {
		insert("UTSNAME_SYSNAME", tmp, ConfigMacroSet, DetectedMacro);
	}

	if( (tmp = sysapi_utsname_nodename()) != NULL ) {
		insert("UTSNAME_NODENAME", tmp, ConfigMacroSet, DetectedMacro);
	}

	if( (tmp = sysapi_utsname_release()) != NULL ) {
		insert("UTSNAME_RELEASE", tmp, ConfigMacroSet, DetectedMacro);
	}

	if( (tmp = sysapi_utsname_version()) != NULL ) {
		insert("UTSNAME_VERSION", tmp, ConfigMacroSet, DetectedMacro);
	}

	if( (tmp = sysapi_utsname_machine()) != NULL ) {
		insert("UTSNAME_MACHINE", tmp, ConfigMacroSet, DetectedMacro);
	}
#endif

	insert("SUBSYSTEM", get_mySubSystem()->getName(), ConfigMacroSet, DetectedMacro);

	val.formatstr("%d",sysapi_phys_memory_raw_no_param());
	insert("DETECTED_MEMORY", val.Value(), ConfigMacroSet, DetectedMacro);

		// Currently, num_hyperthread_cores is defined as everything
		// in num_cores plus other junk, which on some systems may
		// include non-hyperthreaded cores and on other systems may include
		// hyperthreaded cores.  Since num_hyperthread_cpus is a super-set
		// of num_cpus, we use it for NUM_CORES in the config.  Some day,
		// we may want to break things out into NUM_HYPERTHREAD_CORES,
		// NUM_PHYSICAL_CORES, and what-have-you.
	int num_cpus=0;
	int num_hyperthread_cpus=0;
	sysapi_ncpus_raw_no_param(&num_cpus,&num_hyperthread_cpus);

	val.formatstr("%d",num_hyperthread_cpus);
	insert("DETECTED_CORES", val.Value(), ConfigMacroSet, DetectedMacro);
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

	filesys_domain = param("FILESYSTEM_DOMAIN");
	if( !filesys_domain ) {
		insert("FILESYSTEM_DOMAIN", get_local_fqdn().Value(), ConfigMacroSet, DetectedMacro);
	} else {
		free( filesys_domain );
	}

	uid_domain = param("UID_DOMAIN");
	if( !uid_domain ) {
		insert("UID_DOMAIN", get_local_fqdn().Value(), ConfigMacroSet, DetectedMacro);
	} else {
		free( uid_domain );
	}
}

// Sometimes tests want to be able to pretend that params were set
// to a certain value by the user.  This function lets them do that.
//
void 
param_insert(const char * name, const char * value)
{
	insert(name, value, ConfigMacroSet, WireMacro);
}

void
init_config(int config_options)
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
		clear_config(); // to zero-init the table.
	}
	if (ConfigMacroSet.defaults) {
		// Initialize the default table.
		if (ConfigMacroSet.defaults->metat) delete [] ConfigMacroSet.defaults->metat;
		ConfigMacroSet.defaults->metat = NULL;
		ConfigMacroSet.defaults->size = param_info_init((const void**)&ConfigMacroSet.defaults->table);
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
clear_config()
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
	local_config_sources.clearAll();
	return;
}

template <typename T>
int BinaryLookupIndex (const T aTable[], int cElms, const char * key, int (*fncmp)(const char *, const char *))
{
	if (cElms <= 0)
		return -1;

	int ixLower = 0;
	int ixUpper = cElms-1;
	for (;;) {
		if (ixLower > ixUpper)
			return -1; // -1 for not found

		int ix = (ixLower + ixUpper) / 2;
		int iMatch = fncmp(aTable[ix].key, key);
		if (iMatch < 0)
			ixLower = ix+1;
		else if (iMatch > 0)
			ixUpper = ix-1;
		else
			return ix;
	}
}

static int param_default_get_index(const char * name, MACRO_SET & set)
{
	MACRO_DEFAULTS * defs = set.defaults;
	if ( ! defs || ! defs->table)
		return -1;

	return BinaryLookupIndex<const MACRO_DEF_ITEM>(defs->table, defs->size, name, strcasecmp);
}

void param_default_set_use(const char * name, int use, MACRO_SET & set)
{
	MACRO_DEFAULTS * defs = set.defaults;
	if ( ! defs || ! defs->metat)
		return;
	int ix = param_default_get_index(name, set);
	if (ix >= 0) {
		defs->metat[ix].use_count += (use&1);
		defs->metat[ix].ref_count += (use>>1)&1;
	}
}

/*
** Return the value associated with the named parameter.  Return NULL
** if the given parameter is not defined.
*/
char *
param_without_default( const char *name )
{
	const char *val = NULL;
	char * expanded_val = NULL;

	// Try in order to find the parameter
	// As we walk through, any value (including empty string) will
	// cause a 'match' since presumably it was set to empty
	// specifically to clear this parameter for this specific
	// subsystem / local.

	const char *subsys = get_mySubSystem()->getName();
	if (subsys && ! subsys[0]) subsys = NULL;
	const char *local = get_mySubSystem()->getLocalName();
	bool fLocalMatch = false, fSubsysMatch = false;
	if (local && local[0]) {
		// "subsys.local.name" and "local.name"
		std::string local_name;
		formatstr(local_name, "%s.%s", local, name);
		fLocalMatch = true; fSubsysMatch = subsys != NULL;
		val = lookup_macro(local_name.c_str(), subsys, ConfigMacroSet);
		if (subsys && ! val) {
			val = lookup_macro(local_name.c_str(), NULL, ConfigMacroSet);
			fSubsysMatch = false;
		}
	}
	if ( ! val) {
		// lookup "subsys.name" and "name"
		fLocalMatch = false; fSubsysMatch = subsys != NULL;
		val = lookup_macro(name, subsys, ConfigMacroSet);
		if (subsys && ! val) {
			val = lookup_macro(name, NULL, ConfigMacroSet);
			fSubsysMatch = false;
		}
	}
	// Still nothing (or empty)?  Give up.
	if ( ! val || ! val[0] ) {
		//dprintf(D_STATUS, "param_without_default(%s) returns NULL\n", name);
		return NULL;
	}

	if (IsDebugVerbose(D_CONFIG)) {
		if (fLocalMatch || fSubsysMatch) {
			std::string param_name;
			if (fSubsysMatch) { param_name += subsys; param_name += "."; }
			if (fLocalMatch) { param_name += local; param_name += "."; }
			param_name += name;
			dprintf( D_CONFIG | D_VERBOSE, "Config '%s': using prefix '%s' ==> '%s'\n",
					 name, param_name.c_str(), val );
		}
		else {
			dprintf( D_CONFIG | D_VERBOSE, "Config '%s': no prefix ==> '%s'\n", name, val );
		}
	}

	// Ok, now expand it out...
	expanded_val = expand_macro(val, ConfigMacroSet, NULL, false, subsys);

	// If it returned an empty string, free it before returning NULL
	if( expanded_val == NULL ) {
		//dprintf(D_STATUS, "param_without_default(%s) returns NULL\n", name);
		return NULL;
	} else if ( expanded_val[0] == '\0' ) {
		free( expanded_val );
		//dprintf(D_STATUS, "param_without_default(%s) returns NULL\n", name);
		return( NULL );
	} else {
		//dprintf(D_STATUS, "param_without_default(%s) returns %s\n", name, expanded_val);
		return expanded_val;
	}
}

PRAGMA_REMIND("TJ: this gives incorrect result if the param is defined in defaults table.")
bool param_defined(const char* name) {
    bool retval = false;
    char* v = param_without_default(name);
    if (v) {
        free(v);
        retval = true;
    }
    //dprintf(D_STATUS, "param_defined(%s) returns %s\n", name, retval ? "true" : "false");
    return retval;
}


char*
param(const char* name) 
{
		/* The zero means return NULL on not found instead of EXCEPT */
	char * psz = param_with_default_abort(name, 0);
	//dprintf(D_STATUS, "param(%s) returns %s\n", name, psz ? psz : "NULL");
	return psz;
}

char *
param_with_default_abort(const char *name, int abort) 
{
	const char *val = NULL;
#ifdef DISCARD_CONFIG_MATCHING_DEFAULT // don't copy params from default table to ConfigTab
	const char* subsys = get_mySubSystem()->getName();
	if (subsys && ! subsys[0]) subsys = NULL;

	// params in the local namespace will not exist in the default param table
	// so look them up only in ConfigTab
	const char* local = get_mySubSystem()->getLocalName();
	if (local && local[0]) {
		std::string local_name(local);
		local_name += "."; local_name += name;
		val = lookup_macro(local_name.c_str(), subsys, ConfigMacroSet);
		if (subsys && ! val) {
			val = lookup_macro(local_name.c_str(), NULL, ConfigMacroSet);
		}
	}
	// if not found in the local namespace, search the sybsystem and generic namespaces
	if ( ! val) {
		val = lookup_macro(name, subsys, ConfigMacroSet);
		if (subsys && ! val) {
			val = lookup_macro(name, NULL, ConfigMacroSet);
		}
		if ( ! val) {
			val = param_default_string(name, subsys);
			if (val) {
				param_default_set_use(name, 3, ConfigMacroSet);
				if (val[0] == '\0') {
					// If indeed it was empty, then just bail since it was
					// validly found in the Default Table, but empty.
					return NULL;
				}
			}
		}
	}
#else
	const char * subsys = get_mySubSystem()->getName();
	const char * local = get_mySubSystem()->getLocalName();
	MyString subsys_name;

	// Set up the namespace search for the param name.
	// WARNING: The order of appending matters. We search more specific 
	// to less specific in the namespace.
	StringList sl;
	if (local && local[0]) {
		MyString subsys_local_name;
		subsys_local_name.formatstr("%s.%s.%s", subsys, local, name);
		sl.append(subsys_local_name.Value());

		MyString local_name;
		local_name.formatstr("%s.%s", local, name);
		sl.append(local_name.Value());
	}
	subsys_name.formatstr("%s.%s", subsys, name);
	sl.append(subsys_name.Value());
	sl.append(name);

	// Search in left to right order until we find a meaningful val or
	// can bail out early from the search.
	sl.rewind();
	char *next_param_name = NULL;
	while(val == NULL && (next_param_name = sl.next())) {
		// See if the candidate is in the Config Table
		val = lookup_macro(next_param_name, NULL, ConfigMacroSet);

		if (val != NULL && val[0] == '\0') {
			// The config table specifically wanted the value to be empty, 
			// so we honor it regardless of what is in the Default Table.
			return NULL;
		}

		if (val != NULL) {
			// we found something not empty, don't look in the Default Table
			// and stop the search
			break;
		}

		// At this point in the loop, val == NULL, see if we can find
		// something in the Default Table.

		// The candidate wasn't in the Config Table, so check the Default Table
		const char * def = param_exact_default_string(next_param_name);
		if (def != NULL) {
			// Yay! Found something! Add the entry found in the Default 
			// Table to the Config Table. This could be adding an empty
			// string. If a default found, the loop stops searching.
			insert(next_param_name, def, ConfigMacroSet, DefaultMacro);
			if (def[0] == '\0') {
				// If indeed it was empty, then just bail since it was
				// validly found in the Default Table, but empty.
				return NULL;
			}
			val = def;
		}
	}
#endif

	// If we don't find any value at all, determine if we must abort or 
	// simply return NULL which will allow older code calling param to do
	// the right thing (usually by setting up an ad hoc default at the call
	// site).
	if (val == NULL) {
		if (abort) {
			EXCEPT("Param name '%s' did not have a definition in any of the "
				   "usual namespaces or default table. Aborting since it MUST "
				   "be defined.\n", name);
		}
		return NULL;
	}

	// if we get here, it means that we found a val of note, so expand it and
	// return the canonical value of it. expand_macro returns allocated memory.
	// note that expand_macro will first try and expand
	char * expanded_val = expand_macro(val, ConfigMacroSet, NULL, true, subsys);
	if (expanded_val == NULL) {
		return NULL;
	}
	
	// If expand_macro returned an empty string, free it before returning NULL
	if (expanded_val[0] == '\0') {
		free(expanded_val);
		return NULL;
	}

	// return the fully expanded value
	return expanded_val;
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
		int tbl_default_value = param_default_integer(name, subsys, &def_valid, &is_long);
		bool tbl_check_ranges = 
			(param_range_integer(name, &min_value, &max_value)==-1) 
				? false : true;

		if (is_long) {
			dprintf (D_CONFIG | D_FAILURE, "Warning - long param %s fetched as integer\n", name);
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
	long long_result;
	char *string;
	char *endptr = NULL;

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

	long_result = strtol(string,&endptr,10);
	result = long_result;

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
		if( !rhs.AssignExpr( name, string ) ) {
			EXCEPT("Invalid expression for %s (%s) "
				   "in condor configuration.  Please set it to "
				   "an integer expression in the range %d to %d "
				   "(default %d).",
				   name,string,min_value,max_value,default_value);
		}

		if( !rhs.EvalInteger(name,target,result) ) {
			EXCEPT("Invalid result (not an integer) for %s (%s) "
				   "in condor configuration.  Please set it to "
				   "an integer expression in the range %d to %d "
				   "(default %d).",
				   name,string,min_value,max_value,default_value);
		}
		long_result = result;
	}

	if( (long)result != long_result ) {
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

int param_integer_c( const char *name, int default_value,
					   int min_value, int max_value, bool use_param_table )
{
	return param_integer( name, default_value, min_value, max_value, use_param_table );
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
 * Return the [single precision] floating point value associated with the named
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
		param_range_double(name, &min_value, &max_value);

		// if found in the default table, then we overwrite the arguments
		// to this function with the defaults from the table. This effectively
		// nullifies the hard coded defaults in the higher level layers.
		if (def_valid) {
			default_value = tbl_default_value;
		}
	}
	
	double result;
	char *string;
	char *endptr = NULL;

	ASSERT( name );
	string = param( name );
	
	if( ! string ) {
		dprintf( D_CONFIG | D_VERBOSE, "%s is undefined, using default value of %f\n",
				 name, default_value );
		return default_value;
	}

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
		float float_result = 0.0;
		if( me ) {
			rhs = *me;
		}
		if( !rhs.AssignExpr( name, string ) ) {
			EXCEPT("Invalid expression for %s (%s) "
				   "in condor configuration.  Please set it to "
				   "a numeric expression in the range %lg to %lg "
				   "(default %lg).",
				   name,string,min_value,max_value,default_value);
		}

		if( !rhs.EvalFloat(name,target,float_result) ) {
			EXCEPT("Invalid result (not a number) for %s (%s) "
				   "in condor configuration.  Please set it to "
				   "a numeric expression in the range %lg to %lg "
				   "(default %lg).",
				   name,string,min_value,max_value,default_value);
		}
		result = float_result;
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

	bool result=false;
	char *string;
	char *endptr;
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

	endptr = string;
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
		int int_value = default_value;
		ClassAd rhs;
		if( me ) {
			rhs = *me;
		}

		if( rhs.AssignExpr( name, string ) &&
			rhs.EvalBool(name,target,int_value) )
		{
			result = (int_value != 0);
			valid = true;
		}
	}

	if( !valid ) {
		EXCEPT( "%s in the condor configuration  is not a valid boolean (\"%s\")."
				"  Please set it to True or False (default is %s)",
				name, string, default_value ? "True" : "False" );
	}

	free( string );
	
	return result;
}

char *
macro_expand( const char *str )
{
	return expand_macro(str, ConfigMacroSet);
}

char *
expand_param(const char *str, const char *subsys, int use)
{
	return expand_macro(str, ConfigMacroSet, NULL, true, subsys, use);
}

/*
** Same as param_boolean but for C -- returns 0 or 1
** The parameter value is expected to be set to the string
** "TRUE" or "FALSE" (no quotes, case insensitive).
** If the value is not defined or not a valid, then
** return the default_value argument.
*/
extern "C" int
param_boolean_int( const char *name, int default_value ) {
    bool default_bool;
    default_bool = default_value == 0 ? false : true;
    return param_boolean(name, default_bool) ? 1 : 0;
}

#if 1

const char * param_get_location(const MACRO_META * pmet, MyString & value)
{
	value = config_source_by_id(pmet->source_id);
	if (pmet->source_line >= 0) {
		value.formatstr_cat(", line %d", pmet->source_line);
		MACRO_DEF_ITEM * pmsi = param_meta_source_by_id(pmet->source_meta_id);
		if (pmsi) {
			value.formatstr_cat(", %s+%d", pmsi->key, pmet->source_meta_off);
		}
	}
	return value.c_str();
}

#else

// Note that the line_number can be -1 if the filename isn't a real
// filename, but something like <Internal> or <Environment>
bool param_get_location(
	const char *parameter,
	MyString  &filename,
	int       &line_number)
{
	bool found_it = false;

	MACRO_ITEM * pi = find_macro_item(parameter, ConfigMacroSet);
	if (pi) {
		found_it = true;
		if (ConfigMacroSet.metat) {
			MACRO_META * pmi = &ConfigMacroSet.metat[pi - ConfigMacroSet.table];
			if (pmi->source_id >= 0 && pmi->source_id < (int)ConfigMacroSet.sources.size()) {
				filename = ConfigMacroSet.sources[pmi->source_id];
				line_number = pmi->source_line;
			}
		}
	}
	return found_it;
}
#endif

// find an item and return a hash iterator that points to it.
bool param_find_item (
	const char * name,
	const char * subsys,
	const char * local,
	MyString & name_found, // out
	HASHITER& it)          //
{
	it = HASHITER(ConfigMacroSet, 0);
	if (subsys && ! subsys[0]) subsys = NULL;
	if (local && ! local[0]) local = NULL;
	it.id = it.set.defaults ? it.set.defaults->size : 0;
	it.ix = it.set.size;
	it.is_def = false;

	MACRO_ITEM * pi = NULL;
	if (subsys && local) {
		name_found.formatstr("%s.%s.%s", subsys, local, name);
		pi = find_macro_item(name_found.Value(), ConfigMacroSet);
		if (pi) {
			name_found = pi->key;
			it.ix = (int)(pi - it.set.table);
			return true;
		}
	}
	if (local) {
		name_found.formatstr("%s.%s", local, name);
		pi = find_macro_item(name_found.Value(), ConfigMacroSet);
		if (pi) {
			name_found = pi->key;
			it.ix = (int)(pi - it.set.table);
			return true;
		}
	}
	if (subsys) {
		name_found.formatstr("%s.%s", subsys, name);
		pi = find_macro_item(name_found.Value(), ConfigMacroSet);
		if (pi) {
			name_found = pi->key;
			it.ix = (int)(pi - it.set.table);
			return true;
		}
		const MACRO_DEF_ITEM* pdf = (const MACRO_DEF_ITEM*)param_subsys_default_lookup(subsys, name);
		if (pdf) {
			name_found = subsys;
			name_found.upper_case();
			name_found += ".";
			name_found += pdf->key;
			it.is_def = true;
			it.pdef = pdf;
			it.id = param_default_get_id(name);
			return true;
		}
	}

	pi = find_macro_item(name, ConfigMacroSet);
	if (pi) {
		name_found = pi->key;
		it.ix = (int)(pi - it.set.table);
		return true;
	}

	MACRO_DEF_ITEM * pdf = param_default_lookup(name);
	if (pdf) {
		name_found = pdf->key;
		it.is_def = true;
		it.pdef = pdf;
		it.id = param_default_get_id(name);
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
	MyString &source_name,
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

#if 1

const char * param_get_info(
	const char * name,
	const char * subsys,
	const char * local,
	MyString &name_used,
	const char ** pdef_val,
	const MACRO_META **ppmet)
{
	const char * val = NULL;
	if (pdef_val) { *pdef_val = NULL; }
	if (ppmet)    { *ppmet = NULL; }
	name_used.clear();

	HASHITER it(ConfigMacroSet, 0);
	if (param_find_item(name, subsys, local, name_used, it)) {
		val = hash_iter_value(it);
		if (pdef_val) { *pdef_val = hash_iter_def_value(it); }
		if (ppmet) { *ppmet = hash_iter_meta(it); }
	}
	return val;
}

#else

const char * param_get_info(
	const char * name,
	const char * subsys,
	const char * local,
	const char ** pdef_val,
	MyString &name_used,
	int & use_count,
	int & ref_count,
	MyString &filename,
	int &line_number)
{
	const char * val = NULL;
	if (pdef_val) { *pdef_val = NULL; }

	HASHITER it(ConfigMacroSet, 0);
	if (param_find_item(name, subsys, local, name_used, it)) {
		val = hash_iter_info(it, use_count, ref_count, filename, line_number);
		if (pdef_val) {
			*pdef_val = hash_iter_def_value(it);
		}
	}
	return val;
}
#endif

void
reinsert_specials( const char* host )
{
	static unsigned int reinsert_pid = 0;
	static unsigned int reinsert_ppid = 0;
	static bool warned_no_user = false;
	char buf[40];

	if( tilde ) {
		insert("TILDE", tilde, ConfigMacroSet, DetectedMacro);
	}
	if( host ) {
		insert("HOSTNAME", host, ConfigMacroSet, DetectedMacro);
	} else {
		insert("HOSTNAME", get_local_hostname().Value(), ConfigMacroSet, DetectedMacro);
	}
	insert("FULL_HOSTNAME", get_local_fqdn().Value(), ConfigMacroSet, DetectedMacro);
	insert("SUBSYSTEM", get_mySubSystem()->getName(), ConfigMacroSet, DetectedMacro);

	// Insert login-name for our real uid as "username".  At the time
	// we're reading in the config source, the priv state code is not
	// initialized, so our euid will always be the same as our ruid.
	char *myusernm = my_username();
	if( myusernm ) {
		insert( "USERNAME", myusernm, ConfigMacroSet, DetectedMacro);
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
		insert("REAL_UID", buf, ConfigMacroSet, DetectedMacro);
		snprintf(buf,40,"%u",myrgid);
		insert("REAL_GID", buf, ConfigMacroSet, DetectedMacro);
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
	insert("PID", buf, ConfigMacroSet, DetectedMacro);
	if ( !reinsert_ppid ) {
#ifdef WIN32
		CSysinfo system_hackery;
		reinsert_ppid = system_hackery.GetParentPID(reinsert_pid);
#else
		reinsert_ppid = getppid();
#endif
	}
	snprintf(buf,40,"%u",reinsert_ppid);
	insert("PPID", buf, ConfigMacroSet, DetectedMacro);
	insert("IP_ADDRESS", my_ip_string(), ConfigMacroSet, DetectedMacro);
}


void
config_insert( const char* attrName, const char* attrValue )
{
	if( ! (attrName && attrValue) ) {
		return;
	}
	insert(attrName, attrValue, ConfigMacroSet, WireMacro);
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


void
check_params()
{
#if defined( HPUX )
		// Only on HPUX does this check matter...
	char* tmp;
	if( !(tmp = param("ARCH")) ) {
			// Arch isn't defined.  That means the user didn't define
			// it _and_ the special file we use that maps workstation
			// models to CPU types doesn't exist either.  Print a
			// verbose message and exit.  -Derek Wright 8/14/98
		fprintf( stderr, "ERROR: %s must know if you are running "
				 "on an HPPA1 or an HPPA2 CPU.\n",
				 myDistro->Get() );
		fprintf( stderr, "Normally, we look in %s for your model.\n",
				 "/opt/langtools/lib/sched.models" );
		fprintf( stderr, "This file lists all HP models and the "
				 "corresponding CPU type.  However,\n" );
		fprintf( stderr, "this file does not exist on your machine "
				 "or your model (%s)\n", sysapi_uname_arch() );
		fprintf( stderr, "was not listed.  You should either explicitly "
				 "set the ARCH parameter\n" );
		fprintf( stderr, "in your config source, or install the "
				 "sched.models file.\n" );
		exit( 1 );
	} else {
		free( tmp );
	}
#endif
}

/* Begin code for runtime support for modifying a daemon's config source.
   See condor_daemon_core.V6/README.config for more details. */

static StringList PersistAdminList;

class RuntimeConfigItem {
public:
	RuntimeConfigItem() : admin(NULL), config(NULL) { }
	~RuntimeConfigItem() { if (admin) free(admin); if (config) free(config); }
	void initialize() { admin = config = NULL; }
	char *admin;
	char *config;
};

#include "extArray.h"

static ExtArray<RuntimeConfigItem> rArray;

static MyString toplevel_persistent_config;

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
	MyString filename_parameter;
	filename_parameter.formatstr( "%s_CONFIG", get_mySubSystem()->getName() );
	tmp = param( filename_parameter.Value() );
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
			fprintf( stderr, "%s error: ENABLE_PERSISTENT_CONFIG is TRUE, "
					 "but neither %s nor PERSISTENT_CONFIG_DIR is "
					 "specified in the configuration file\n",
					 myDistro->GetCap(), filename_parameter.Value() );
			exit( 1 );
		}
	}
	toplevel_persistent_config.formatstr( "%s%c.config.%s", tmp,
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
	char *tmp;
	MyString filename;
	MyString tmp_filename;
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
	if( ! toplevel_persistent_config.Length() ) {
		EXCEPT( "Impossible: programmer error: toplevel_persistent_config "
				"is 0-length, but we already initialized, enable_persistent "
				"is TRUE, and set_persistent_config() has been called" );
	}

	priv = set_root_priv();
	if (config && config[0]) {	// (re-)set config
			// write new config to temporary file
		filename.formatstr( "%s.%s", toplevel_persistent_config.Value(), admin );
		tmp_filename.formatstr( "%s.tmp", filename.Value() );
		do {
			MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
			unlink( tmp_filename.Value() );
			fd = safe_open_wrapper_follow( tmp_filename.Value(), O_WRONLY|O_CREAT|O_EXCL, 0644 );
		} while (fd == -1 && errno == EEXIST);
		if( fd < 0 ) {
			dprintf( D_ALWAYS, "safe_open_wrapper(%s) returned %d '%s' (errno %d) in "
					 "set_persistent_config()\n", tmp_filename.Value(),
					 fd, strerror(errno), errno );
			ABORT;
		}
		if (write(fd, config, strlen(config)) != (ssize_t)strlen(config)) {
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
		if (rotate_file(tmp_filename.Value(), filename.Value()) < 0) {
			dprintf( D_ALWAYS, "rotate_file(%s,%s) failed with '%s' "
					 "(errno %d) in set_persistent_config()\n",
					 tmp_filename.Value(), filename.Value(),
					 strerror(errno), errno );
			ABORT;
		}
	
		// update admin list in memory
		if (!PersistAdminList.contains(admin)) {
			PersistAdminList.append(admin);
		} else {
			free(admin);
			free(config);
			set_priv(priv);
			return 0;		// if no update is required, then we are done
		}

	} else {					// clear config

		// update admin list in memory
		PersistAdminList.remove(admin);
		if (config) {
			free(config);
			config = NULL;
		}
	}		

	// update admin list on disk
	tmp_filename.formatstr( "%s.tmp", toplevel_persistent_config.Value() );
	do {
		MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
		unlink( tmp_filename.Value() );
		fd = safe_open_wrapper_follow( tmp_filename.Value(), O_WRONLY|O_CREAT|O_EXCL, 0644 );
	} while (fd == -1 && errno == EEXIST);
	if( fd < 0 ) {
		dprintf( D_ALWAYS, "safe_open_wrapper(%s) returned %d '%s' (errno %d) in "
				 "set_persistent_config()\n", tmp_filename.Value(),
				 fd, strerror(errno), errno );
		ABORT;
	}
	const char param[] = "RUNTIME_CONFIG_ADMIN = ";
	if (write(fd, param, strlen(param)) != (ssize_t)strlen(param)) {
		dprintf( D_ALWAYS, "write() failed with '%s' (errno %d) in "
				 "set_persistent_config()\n", strerror(errno), errno );
		close(fd);
		ABORT;
	}
	PersistAdminList.rewind();
	bool first_time = true;
	while( (tmp = PersistAdminList.next()) ) {
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
		if (write(fd, tmp, strlen(tmp)) != (ssize_t)strlen(tmp)) {
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
	
	rval = rotate_file( tmp_filename.Value(),
						toplevel_persistent_config.Value() );
	if (rval < 0) {
		dprintf( D_ALWAYS, "rotate_file(%s,%s) failed with '%s' (errno %d) "
				 "in set_persistent_config()\n", tmp_filename.Value(),
				 filename.Value(), strerror(errno), errno );
		ABORT;
	}

	// if we removed a config, then we should clean up by removing the file(s)
	if (!config || !config[0]) {
		filename.formatstr( "%s.%s", toplevel_persistent_config.Value(), admin );
		MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
		unlink( filename.Value() );
		if (PersistAdminList.number() == 0) {
			MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
			unlink( toplevel_persistent_config.Value() );
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
	int i;

	if (!admin || !admin[0] || !enable_runtime) {
		if (admin)  { free(admin);  }
		if (config) { free(config); }
		return -1;
	}

	if (config && config[0]) {
		for (i=0; i <= rArray.getlast(); i++) {
			if (strcmp(rArray[i].admin, admin) == MATCH) {
				free(admin);
				free(rArray[i].config);
				rArray[i].config = config;
				return 0;
			}
		}
		rArray[i].admin = admin;
		rArray[i].config = config;
	} else {
		for (i=0; i <= rArray.getlast(); i++) {
			if (strcmp(rArray[i].admin, admin) == MATCH) {
				free(admin);
				if (config) free(config);
				free(rArray[i].admin);
				free(rArray[i].config);
				rArray[i] = rArray[rArray.getlast()];
				rArray[rArray.getlast()].initialize();
				rArray.truncate(rArray.getlast()-1);
				return 0;
			}
		}
	}

	return 0;
}


extern "C" {

static int
process_persistent_configs()
{
	char *tmp = NULL;
	int rval;
	bool processed = false;

	if( access( toplevel_persistent_config.Value(), R_OK ) == 0 &&
		PersistAdminList.number() == 0 )
	{
		processed = true;

		rval = Read_config(toplevel_persistent_config.Value(), ConfigMacroSet,
						EXPAND_LAZY, true, get_mySubSystem()->getName());
		if (rval < 0) {
			dprintf( D_ALWAYS, "Configuration Error Line %d while reading "
					 "top-level persistent config source: %s\n",
					 ConfigLineNo, toplevel_persistent_config.Value() );
			exit(1);
		}

		tmp = param ("RUNTIME_CONFIG_ADMIN");
		if (tmp) {
			PersistAdminList.initializeFromString(tmp);
			free(tmp);
		}
	}

	PersistAdminList.rewind();
	while ((tmp = PersistAdminList.next())) {
		processed = true;
		MyString config_source;
		config_source.formatstr( "%s.%s", toplevel_persistent_config.Value(),
							   tmp );
		rval = Read_config(config_source.Value(), ConfigMacroSet,
						EXPAND_LAZY, true, get_mySubSystem()->getName());
		if (rval < 0) {
			dprintf( D_ALWAYS, "Configuration Error Line %d "
					 "while reading persistent config source: %s\n",
					 ConfigLineNo, config_source.Value() );
			exit(1);
		}
	}
	return (int)processed;
}


static int
process_runtime_configs()
{
	int i, rval, fd;
	bool processed = false;

	for (i=0; i <= rArray.getlast(); i++) {
		processed = true;

		char* tmp_dir = temp_dir_path();
		ASSERT(tmp_dir);
		MyString tmp_file_tmpl = tmp_dir;
		free(tmp_dir);
		tmp_file_tmpl += "/cndrtmpXXXXXX";

		char* tmp_file = strdup(tmp_file_tmpl.Value());
		fd = condor_mkstemp( tmp_file );
		if (fd < 0) {
			dprintf( D_ALWAYS, "condor_mkstemp(%s) returned %d, '%s' (errno %d) in "
				 "process_dynamic_configs()\n", tmp_file, fd,
				 strerror(errno), errno );
			exit(1);
		}

		if (write(fd, rArray[i].config, strlen(rArray[i].config))
			!= (ssize_t)strlen(rArray[i].config)) {
			dprintf( D_ALWAYS, "write failed with errno %d in "
					 "process_dynamic_configs\n", errno );
			exit(1);
		}
		if (close(fd) < 0) {
			dprintf( D_ALWAYS, "close failed with errno %d in "
					 "process_dynamic_configs\n", errno );
			exit(1);
		}
		rval = Read_config(tmp_file, ConfigMacroSet,
						EXPAND_LAZY, false, get_mySubSystem()->getName());
		if (rval < 0) {
			dprintf( D_ALWAYS, "Configuration Error Line %d "
					 "while reading %s, runtime config: %s\n",
					 ConfigLineNo, tmp_file, rArray[i].admin );
			exit(1);
		}
		MSC_SUPPRESS_WARNING_FIXME(6031) // warning: return value of 'unlink' ignored.
		unlink(tmp_file);
		free(tmp_file);
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

} // end of extern "C"

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

/* End code for runtime support for modifying a daemon's config source. */

bool param(MyString &buf,char const *param_name,char const *default_value)
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

bool param(std::string &buf,char const *param_name,char const *default_value)
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

param_functions* get_param_functions()
{
	config_p_funcs.set_param_func(&param);
	config_p_funcs.set_param_bool_int_func(&param_boolean_int);
	config_p_funcs.set_param_wo_default_func(&param_without_default);
	config_p_funcs.set_param_int_func(&param_integer);

	return &config_p_funcs;
}
