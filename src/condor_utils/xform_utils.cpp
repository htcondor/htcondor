/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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
#include "condor_string.h"
#include "basename.h"
#include "condor_getcwd.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "daemon.h"

#include "string_list.h"
#include "my_username.h" // for my_domainname
#include "directory.h"
#include "filename_tools.h"
#include "fs_util.h"
#include "ad_printmask.h"
#include "condor_version.h"
#include "submit_utils.h" // for queue iteration stuff
#include "Regex.h"
#include "xform_utils.h"

#include "list.h"
#include "my_popen.h"

#include <string>
#include <set>

// values for hashtable defaults, these are declared as char rather than as const char to make g++ on fedora shut up.
static char OneString[] = "1", ZeroString[] = "0";
//static char ParallelNodeString[] = "#pArAlLeLnOdE#";
static char UnsetString[] = "";


static condor_params::string_value ArchMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysVerMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysMajorVerMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysAndVerMacroDef = { UnsetString, 0 };
#ifdef WIN32
static condor_params::string_value IsLinuxMacroDef = { ZeroString, 0 };
static condor_params::string_value IsWinMacroDef = { OneString, 0 };
#elif defined LINUX
static condor_params::string_value IsLinuxMacroDef = { OneString, 0 };
static condor_params::string_value IsWinMacroDef = { ZeroString, 0 };
#else
static condor_params::string_value IsLinuxMacroDef = { ZeroString, 0 };
static condor_params::string_value IsWinMacroDef = { ZeroString, 0 };
#endif
static condor_params::string_value UnliveRulesFileMacroDef = { UnsetString, 0 };
//static condor_params::string_value UnliveClusterMacroDef = { OneString, 0 };
static condor_params::string_value UnliveProcessMacroDef = { ZeroString, 0 };
static condor_params::string_value UnliveStepMacroDef = { ZeroString, 0 };
static condor_params::string_value UnliveRowMacroDef = { ZeroString, 0 };
static condor_params::string_value UnliveIteratingMacroDef = { ZeroString, 0 };

// Table of macro 'defaults'. This provides a convenient way to inject things into the
// hashtable without actually running a bunch of code to stuff the table.
// Because they are defaults they will be ignored when scanning for unreferenced macros.
// NOTE: TABLE MUST BE SORTED BY THE FIRST FIELD!!
static MACRO_DEF_ITEM XFormMacroDefaults[] = {
	{ "ARCH",      &ArchMacroDef },
	{ "IsLinux",   &IsLinuxMacroDef },
	{ "IsWindows", &IsWinMacroDef },
	{ "ItemIndex", &UnliveRowMacroDef },
	{ "Iterating", &UnliveIteratingMacroDef },
	{ "OPSYS",           &OpsysMacroDef },
	{ "OPSYSANDVER",     &OpsysAndVerMacroDef },
	{ "OPSYSMAJORVER",   &OpsysMajorVerMacroDef },
	{ "OPSYSVER",        &OpsysVerMacroDef },
	{ "Row",       &UnliveRowMacroDef },
	{ "RulesFile", &UnliveRulesFileMacroDef },
	{ "Step",      &UnliveStepMacroDef },
	{ "XFormId",    &UnliveProcessMacroDef },
};


// setup XFormParamInfoDefaults using the global param table
extern "C" { int param_info_init(const void ** pvdefaults); }
static MACRO_DEFAULTS XFormParamInfoDefaults = { 0, NULL, NULL };

// these are used to keep track of the source of various macros in the table.
const MACRO_SOURCE LocalMacro = { true, false, 0, -2, -1, -2 }; // for macros set by default
const MACRO_SOURCE ArgumentMacro = { true, false, 1, -2, -1, -2 }; // for macros set by command line
const MACRO_SOURCE LiveMacro = { true, false, 2, -2, -1, -2 };    // for macros use as queue loop variables

// from submit_utils.cpp
condor_params::string_value * allocate_live_default_string(MACRO_SET& set, const condor_params::string_value & Def, int cch);

// create a checkpoint of the current params
// and store it in the param pool
MACRO_SET_CHECKPOINT_HDR* checkpoint_macro_set(MACRO_SET& set)
{
	optimize_macros(set);

	// calcuate a size for the checkpoint
	// we want to save macro table, meta table and sources table in it.
	// we don't have to save the defaults metadata, because we know that defaults
	// cannot be added.
	size_t cbCheckpoint = sizeof(MACRO_SET_CHECKPOINT_HDR);
	cbCheckpoint += set.size * (sizeof(set.metat[0]) + sizeof(set.table[0]));
	cbCheckpoint += set.sources.size() * sizeof(const char *);

	// Now we build a compact string pool that has only a single hunk.
	// and has room for the checkpoint plus extra items.
	int cbFree, cHunks, cb = set.apool.usage(cHunks, cbFree);
	if (cHunks > 1 || cbFree < (int)(1024 + cbCheckpoint)) {
		ALLOCATION_POOL tmp;
		int cbAlloc = (int)MAX(cb*2, cb+4096+(int)cbCheckpoint);
		tmp.reserve(cbAlloc);
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

	// if there is metadata, mark all current items as checkpointed.
	if (set.metat) {
		for (int ii = 0; ii < set.size; ++ii) {
			set.metat[ii].checkpointed = true;
		}
	}

	// now claim space in the pool for the checkpoint
	char * pchka = set.apool.consume((int)cbCheckpoint + sizeof(void*), sizeof(void*));
	// make sure that the pointer is aligned
	pchka += sizeof(void*) - (((size_t)pchka) & (sizeof(void*)-1));

	// write the checkpoint into it.
	MACRO_SET_CHECKPOINT_HDR * phdr = reinterpret_cast<MACRO_SET_CHECKPOINT_HDR *>(pchka);
	pchka = (char*)(phdr+1);
	phdr->cTable = phdr->cMetaTable = 0;
	phdr->cSources = (int)set.sources.size();
	if (phdr->cSources) {
		const char ** psrc = reinterpret_cast<const char**>(pchka);
		for (int ii = 0; ii < phdr->cSources; ++ii) {
			*psrc++ = set.sources[ii];
		}
		pchka = (char*)psrc;
	}
	if (set.table) {
		phdr->cTable = set.size;
		int cbTable = sizeof(set.table[0]) * phdr->cTable;
		memcpy(pchka, set.table, cbTable);
		pchka += cbTable;
	}
	if (set.metat) {
		phdr->cMetaTable = set.size;
		int cbMeta = sizeof(set.metat[0]) * phdr->cMetaTable;
		memcpy(pchka, set.metat, cbMeta);
		pchka += cbMeta;
	}

	// return the checkpoint
	return phdr;
}


// rewind local params to the given checkpoint.
//
void rewind_macro_set(MACRO_SET& set, MACRO_SET_CHECKPOINT_HDR* phdr, bool and_delete_checkpoint)
{
	char * pchka = (char*)(phdr+1);
	ASSERT(set.apool.contains(pchka));

	set.sources.clear();
	if (phdr->cSources > 0) {
		const char ** psrc = reinterpret_cast<const char **>(pchka);
		for (int ii = 0; ii < phdr->cSources; ++ii) {
			set.sources.push_back(*psrc++);
		}
		pchka = (char*)psrc;
	}
	if (phdr->cTable >= 0) {
		ASSERT(set.allocation_size >= phdr->cTable);
		ASSERT(set.table || ! phdr->cTable);
		set.sorted = set.size = phdr->cTable;
		int cbTable = sizeof(set.table[0]) * phdr->cTable;
		if (cbTable > 0) memcpy(set.table, pchka, cbTable);
		pchka += cbTable;
	}
	if (phdr->cMetaTable >= 0) {
		ASSERT(set.allocation_size >= phdr->cMetaTable);
		ASSERT(set.metat || ! phdr->cMetaTable);
		int cbMeta = sizeof(set.metat[0]) * phdr->cMetaTable;
		if (cbMeta > 0) memcpy(set.metat, pchka, cbMeta);
		pchka += cbMeta;
	}

	if (and_delete_checkpoint) {
		set.apool.free_everything_after((char*)phdr);
	} else {
		set.apool.free_everything_after(pchka);
	}
}


// setup a MACRO_DEFAULTS table for the macro set, we have to re-do this each time we clear
// the macro set, because we allocate the defaults from the ALLOCATION_POOL.
void XFormHash::setup_macro_defaults()
{
	if (LocalMacroSet.sources.empty()) {
		LocalMacroSet.sources.reserve(4);
		LocalMacroSet.sources.push_back("<Local>");
		LocalMacroSet.sources.push_back("<Argument>");
		LocalMacroSet.sources.push_back("<Live>");
	}

	// use param table as defaults table.  this uses less memory, but disables all of the live looping variables
	if (LocalMacroSet.options & CONFIG_OPT_DEFAULTS_ARE_PARAM_INFO) {
		XFormParamInfoDefaults.size = param_info_init((const void**)&XFormParamInfoDefaults.table);
		LocalMacroSet.defaults = &XFormParamInfoDefaults;
		return;
	}

	// init the global xform default macros table (ARCH, OPSYS, etc) in case this hasn't happened already.
	init_xform_default_macros();

	// make an copy of the global xform default macros table that is private to this function.
	// we do this because of the 'live' keys in the defaults table
	struct condor_params::key_value_pair* pdi = reinterpret_cast<struct condor_params::key_value_pair*> (LocalMacroSet.apool.consume(sizeof(XFormMacroDefaults), sizeof(void*)));
	memcpy((void*)pdi, XFormMacroDefaults, sizeof(XFormMacroDefaults));
	LocalMacroSet.defaults = reinterpret_cast<MACRO_DEFAULTS*>(LocalMacroSet.apool.consume(sizeof(MACRO_DEFAULTS), sizeof(void*)));
	LocalMacroSet.defaults->size = COUNTOF(XFormMacroDefaults);
	LocalMacroSet.defaults->table = pdi;
	LocalMacroSet.defaults->metat = NULL;

	// allocate space for the 'live' macro default string_values and for the strings themselves.
	LiveProcessString = allocate_live_default_string(LocalMacroSet, UnliveProcessMacroDef, 24)->psz;
	LiveRowString = allocate_live_default_string(LocalMacroSet, UnliveRowMacroDef, 24)->psz;
	LiveStepString = allocate_live_default_string(LocalMacroSet, UnliveStepMacroDef, 24)->psz;
	LiveRulesFileMacroDef = allocate_live_default_string(LocalMacroSet, UnliveRulesFileMacroDef, 2);
	LiveIteratingMacroDef = allocate_live_default_string(LocalMacroSet, UnliveIteratingMacroDef, 2);
}



XFormHash::XFormHash(int options /* =0 */)
	: LiveProcessString(NULL), LiveRowString(NULL), LiveStepString(NULL)
	, LiveRulesFileMacroDef(NULL), LiveIteratingMacroDef(NULL)
{
	int opts = options | CONFIG_OPT_SUBMIT_SYNTAX | CONFIG_OPT_NO_INCLUDE_FILE | CONFIG_OPT_NO_SMART_AUTO_USE;
	LocalMacroSet.initialize(opts);
	setup_macro_defaults();
}

MACRO_SET_CHECKPOINT_HDR * XFormHash::save_state()
{
	return checkpoint_macro_set(LocalMacroSet);
}
bool XFormHash::rewind_to_state(MACRO_SET_CHECKPOINT_HDR * chkhdr, bool and_delete)
{
	if (chkhdr) {
		rewind_macro_set(LocalMacroSet, chkhdr, and_delete);
		return true;
	}
	return false;
}


XFormHash::~XFormHash()
{
	delete LocalMacroSet.errors;
	LocalMacroSet.errors = NULL;
	delete [] LocalMacroSet.table;
	LocalMacroSet.table = NULL;
	delete LocalMacroSet.metat;
	LocalMacroSet.metat = NULL;
	LocalMacroSet.sources.clear();
	LocalMacroSet.apool.clear();
}

void XFormHash::push_error(FILE * fh, const char* format, ... ) const //CHECK_PRINTF_FORMAT(3,4);
{
	va_list ap;
	va_start(ap, format);
	int cch = vprintf_length(format, ap);
	char * message = (char*)malloc(cch + 1);

	vsprintf ( message, format, ap );
	
	va_end(ap);

	if (LocalMacroSet.errors) {
		LocalMacroSet.errors->push("XForm", -1, message);
	} else {
		fprintf(fh, "\nERROR: %s", message);
	}
	free(message);
}

void XFormHash::push_warning(FILE * fh, const char* format, ... ) const //CHECK_PRINTF_FORMAT(3,4);
{
	va_list ap;
	va_start(ap, format);
	int cch = vprintf_length(format, ap);
	char * message = (char*)malloc(cch + 1);
	if (message) {
		vsprintf ( message, format, ap );
	}
	va_end(ap);

	if (LocalMacroSet.errors) {
		LocalMacroSet.errors->push("XForm", 0, message ? message : "");
	} else {
		fprintf(fh, "\nWARNING: %s", message ? message : "");
	}
	if (message) {
		free(message);
	}
}

// lookup and expand an item from the hashtable
//
char * XFormHash::local_param(const char* name, const char* alt_name, MACRO_EVAL_CONTEXT & ctx)
{
	bool used_alt = false;
	const char *pval = lookup_macro(name, LocalMacroSet, ctx);

	if ( ! pval && alt_name) {
		pval = lookup_macro(alt_name, LocalMacroSet, ctx);
		used_alt = true;
	}

	if( ! pval) {
		return NULL;
	}

	char *pval_expanded = expand_macro(pval, ctx);
	if ( ! pval_expanded) {
		push_error(stderr, "Failed to expand macros in: %s\n", used_alt ? alt_name : name );
		return NULL;
	}

	return  pval_expanded;
}


void XFormHash::set_local_param(const char *name, const char *value, MACRO_EVAL_CONTEXT & ctx)
{
	insert_macro(name, value, LocalMacroSet, LocalMacro, ctx);
}

void XFormHash::set_arg_variable(const char* name, const char * value, MACRO_EVAL_CONTEXT & ctx)
{
	insert_macro(name, value, LocalMacroSet, ArgumentMacro, ctx);
}

// stuff a live value into the hashtable.
// IT IS UP TO THE CALLER TO INSURE THAT live_value IS VALID FOR THE LIFE OF THE HASHTABLE
// this function is intended for use during queue iteration to stuff
// changing values like $(Step) and $(ItemIndex) into the hashtable.
// The pointer passed in as live_value may be changed at any time to
// affect subsequent macro expansion of name.
// live values are automatically marked as 'used'.
//
void XFormHash::set_live_variable(const char *name, const char *live_value, MACRO_EVAL_CONTEXT & ctx)
{
	const bool force_used = true;
	MACRO_ITEM* pitem = find_macro_item(name, NULL, LocalMacroSet);
	if ( ! pitem) {
		insert_macro(name, "", LocalMacroSet, LiveMacro, ctx);
		pitem = find_macro_item(name, NULL, LocalMacroSet);
	}
	ASSERT(pitem);
	pitem->raw_value = live_value;
	if (LocalMacroSet.metat && force_used) {
		MACRO_META* pmeta = &LocalMacroSet.metat[pitem - LocalMacroSet.table];
		pmeta->use_count += 1;
		pmeta->live = true;
	}
}

void XFormHash::clear_live_variables() const
{
	if (LocalMacroSet.metat) {
		for (int ii = 0; ii < LocalMacroSet.size; ++ii) {
			if (LocalMacroSet.metat[ii].live) {
				LocalMacroSet.table[ii].raw_value = "";
			}
		}
	}
}


void XFormHash::set_iterate_step(int step, int proc)
{
	sprintf(LiveProcessString, "%d", proc);
	sprintf(LiveStepString, "%d", step);
}

void XFormHash::set_iterate_row(int row, bool iterating)
{
	sprintf(LiveRowString, "%d", row);
	LiveIteratingMacroDef->psz = const_cast<char*>(iterating ? "1" : "0");
}

void XFormHash::set_local_param_used(const char *name) 
{
	increment_macro_use_count(name, LocalMacroSet);
}

const char * init_xform_default_macros()
{
	static bool initialized = false;
	if (initialized)
		return NULL;
	initialized = true;

	const char * ret = NULL; // null return is success.

	//PRAGMA_REMIND("tj: fix to reconfig properly")

	ArchMacroDef.psz = param( "ARCH" );
	if ( ! ArchMacroDef.psz) {
		ArchMacroDef.psz = UnsetString;
		ret = "ARCH not specified in config file";
	}

	OpsysMacroDef.psz = param( "OPSYS" );
	if( OpsysMacroDef.psz == NULL ) {
		OpsysMacroDef.psz = UnsetString;
		ret = "OPSYS not specified in config file";
	}
	// also pick up the variations on opsys if they are defined.
	OpsysAndVerMacroDef.psz = param( "OPSYSANDVER" );
	if ( ! OpsysAndVerMacroDef.psz) OpsysAndVerMacroDef.psz = UnsetString;
	OpsysMajorVerMacroDef.psz = param( "OPSYSMAJORVER" );
	if ( ! OpsysMajorVerMacroDef.psz) OpsysMajorVerMacroDef.psz = UnsetString;
	OpsysVerMacroDef.psz = param( "OPSYSVER" );
	if ( ! OpsysVerMacroDef.psz) OpsysVerMacroDef.psz = UnsetString;

	return ret;
}

void XFormHash::init()
{
	clear();
}

void XFormHash::clear()
{
	if (LocalMacroSet.table) {
		memset(LocalMacroSet.table, 0, sizeof(LocalMacroSet.table[0]) * LocalMacroSet.allocation_size);
	}
	if (LocalMacroSet.metat) {
		memset(LocalMacroSet.metat, 0, sizeof(LocalMacroSet.metat[0]) * LocalMacroSet.allocation_size);
	}
	if (LocalMacroSet.defaults && LocalMacroSet.defaults->metat) {
		memset(LocalMacroSet.defaults->metat, 0, sizeof(LocalMacroSet.defaults->metat[0]) * LocalMacroSet.defaults->size);
	}
	LocalMacroSet.size = 0;
	LocalMacroSet.sorted = 0;
	LocalMacroSet.apool.clear();
	if (LocalMacroSet.sources.size() > 3) {
		LocalMacroSet.sources.resize(3);
	}
	setup_macro_defaults(); // setup a defaults table for the macro_set. have to re-do this because we cleared the apool
}


/** Given a universe in string form, return the number

Passing a universe in as a null terminated string in univ.  This can be
a case-insensitive word ("standard", "java"), or the associated number (1, 7).
Returns the integer of the universe.  In the event a given universe is not
understood, returns 0.

(The "Ex"tra functionality over CondorUniverseNumber is that it will
handle a string of "1".  This is primarily included for backward compatility
with the old icky way of specifying a Remote_Universe.
*/
static int CondorUniverseNumberEx(const char * univ)
{
	if( univ == 0 ) {
		return 0;
	}

	if( atoi(univ) != 0) {
		return atoi(univ);
	}

	return CondorUniverseNumber(univ);
}

bool XFormHash::local_param_bool(const char * name, bool def_value, MACRO_EVAL_CONTEXT & ctx, bool * pfExist)
{
	auto_free_ptr val(local_param(name, ctx));
	bool exist = false;
	bool value = def_value;
	if ( val) {
		exist = string_is_boolean_param(val, value);
	}
	if (pfExist) *pfExist = exist;
	return value;
}

int XFormHash::local_param_int(const char * name, int def_value, MACRO_EVAL_CONTEXT & ctx, bool * pfExist)
{
	auto_free_ptr val(local_param(name, ctx));
	bool exist = false;
	int value = def_value;
	if ( val) {
		long long lvalue;
		exist = string_is_long_param(val, lvalue);
		if (exist) {
			if (lvalue < INT_MIN) value = INT_MIN;
			else if (lvalue > INT_MAX) value = INT_MAX;
			else value = (int)lvalue;
		}
	}
	if (pfExist) *pfExist = exist;
	return value;
}

double XFormHash::local_param_double(const char * name, double def_value, MACRO_EVAL_CONTEXT & ctx, bool * pfExist)
{
	auto_free_ptr val(local_param(name, ctx));
	bool exist = false;
	double value = def_value;
	if ( val) {
		exist = string_is_double_param(val, value);
	}
	if (pfExist) *pfExist = exist;
	return value;
}

bool XFormHash::local_param_string(const char * name, std::string & value, MACRO_EVAL_CONTEXT & ctx)
{
	auto_free_ptr val(local_param(name, ctx));
	if (val) {
		value = val.ptr();
		return true;
	}
	return false;
}

static char * trim_and_strip_quotes_in_place(char * str)
{
	char * p = str;
	while (isspace(*p)) ++p;
	char * pe = p + strlen(p);
	while (pe > p && isspace(pe[-1])) --pe;
	*pe = 0;

	if (*p == '"' && pe > p && pe[-1] == '"') {
		// if we also had both leading and trailing quotes, strip them.
		if (pe > p && pe[-1] == '"') {
			*--pe = 0;
			++p;
		}
	}
	return p;
}

bool XFormHash::local_param_unquoted_string(const char * name, std::string & value, MACRO_EVAL_CONTEXT & ctx)
{
	auto_free_ptr val(local_param(name, ctx));
	if (val) {
		value = trim_and_strip_quotes_in_place(val.ptr());
		return true;
	}
	return false;
}


void XFormHash::insert_source(const char * filename, MACRO_SOURCE & source)
{
	source.line = 0;
	source.is_inside = false;
	source.is_command = false;
	source.id = (int)LocalMacroSet.sources.size();
	source.meta_id = -1;
	source.meta_off = -2;
	LocalMacroSet.sources.push_back(filename);
}

void XFormHash::set_RulesFile(const char * filename, MACRO_SOURCE & source)
{
	this->insert_source(filename, source);
	LiveRulesFileMacroDef->psz = const_cast<char*>(filename);
}

const char* XFormHash::get_RulesFilename()
{
	return LiveRulesFileMacroDef->psz;
}

void XFormHash::warn_unused(FILE* out, const char *app)
{
	if ( ! app) app = "condor_transform_ads";

	HASHITER it = hash_iter_begin(LocalMacroSet);
	for ( ; !hash_iter_done(it); hash_iter_next(it) ) {
		MACRO_META * pmeta = hash_iter_meta(it);
		if (pmeta && !pmeta->use_count && !pmeta->ref_count) {
			const char *key = hash_iter_key(it);
			if (*key && *key=='+') {continue;}
			if (pmeta->source_id == LiveMacro.id) {
				push_warning(out, "the TRANSFORM variable '%s' was unused by %s. Is it a typo?\n", key, app);
			} else {
				const char *val = hash_iter_value(it);
				push_warning(out, "the line '%s = %s' was unused by %s. Is it a typo?\n", key, val, app);
			}
		}
	}
	hash_iter_delete(&it);
}

void XFormHash::dump(FILE* out, int flags)
{
	HASHITER it = hash_iter_begin(LocalMacroSet, flags);
	for ( ; ! hash_iter_done(it); hash_iter_next(it)) {
		const char * key = hash_iter_key(it);
		if (key && key[0] == '$') continue; // dont dump meta params.
		const char * val = hash_iter_value(it);
		//fprintf(out, "%s%s = %s\n", it.is_def ? "d " : "  ", key, val ? val : "NULL");
		fprintf(out, "  %s = %s\n", key, val ? val : "NULL");
	}
	hash_iter_delete(&it);
}

// returns a pointer to the iteration args if this line is an TRANSFORM statement
// returns NULL if it is not.
static const char * is_xform_statement(const char * line, const char * keyword)
{
	const size_t cchKey = strlen(keyword);
	while (*line && isspace(*line)) ++line;
	if (starts_with_ignore_case(line, keyword) && isspace(line[cchKey])) {
		const char * pargs = line+cchKey;
		while (*pargs && isspace(*pargs)) ++pargs;
		if (*pargs == '=' || *pargs == ':') return NULL;
		return pargs;
	}
	return NULL;
}

static const char * is_non_trivial_iterate(const char * is_transform)
{
	const char * iter_args = NULL;
	if (*is_transform) {
		// figure out if this is a trival TRANSFORM argument list where trivial is either no args, or the arg is 1
		// if we have a non-trival list, set iter_args to point to the arguments
		char * endp = NULL;
		long num = strtol(is_transform, &endp, 10);
		if (num != 0 && num != 1) {
			iter_args = is_transform;
		} else if (endp) {
			// anything after the number?
				while (isspace(*endp)) ++endp;
				if (*endp) iter_args = is_transform;
		}
	}
	return iter_args;
}

MacroStreamXFormSource::MacroStreamXFormSource(const char *nam)
	: MacroStreamCharSource()
	, requirements(), universe(0), checkpoint(NULL)
	, fp_iter(NULL), fp_lineno(0)
	, step(0), row(0), proc(0)
	, close_fp_when_done(false)
	, iterate_init_state(0)
	, iterate_args(NULL)
	, curr_item(NULL)
{
	if (nam) name = nam;
	ctx.init("XFORM");
}

MacroStreamXFormSource::~MacroStreamXFormSource()
{
	// we don't free the checkpoint, since it points into the LocalMacroSet allocation pool...
	checkpoint = NULL;
}

int MacroStreamXFormSource::setUniverse(const char * uni) {
	universe = CondorUniverseNumberEx(uni);
	return universe;
}

int MacroStreamXFormSource::open(StringList & lines, const MACRO_SOURCE & FileSource, std::string & errmsg)
{
	for (const char *line = lines.first(); line; line = lines.next()) {
		const char * p;
		if (NULL != (p = is_xform_statement(line, "name"))) {
			std::string tmp(p); trim(tmp);
			if ( ! tmp.empty()) name = tmp;
			lines.deleteCurrent();
		} else if (NULL != (p = is_xform_statement(line, "requirements"))) {
			int err = 0;
			setRequirements(p, err);
			if (err < 0) {
				formatstr(errmsg, "invalid REQUIREMENTS : %s", p);
				return err;
			}
			lines.deleteCurrent();
		} else if (NULL != (p = is_xform_statement(line, "universe"))) {
			setUniverse(p);
			lines.deleteCurrent();
		} else if (NULL != (p = is_xform_statement(line, "transform"))) {
			if ( ! iterate_args) {
				p = is_non_trivial_iterate(p);
				if (p) { iterate_args.set(strdup(p)); iterate_init_state = 2; }
			}
			lines.deleteCurrent();
		} else {
			// strip blank lines and comments
			//while (*p && isspace(*line)) ++p;
			//if ( ! *p || *p == '#') { lines.deleteCurrent(); }
		}
	}

	file_string.set(lines.print_to_delimed_string("\n"));
	MacroStreamCharSource::open(file_string, FileSource);
	rewind();
	return lines.number();
}

// like the above open, but more efficient when loading from a config value
// that has no iteration/transform statements
int MacroStreamXFormSource::open(const char * statements_in, int & offset, std::string & errmsg)
{
	const char * statements = statements_in + offset;
	size_t cb = strlen(statements);
	char * buf = (char*)malloc(cb + 2);
	file_string.set(buf);
	StringTokenIterator lines(statements, 0, "\r\n");
	int start, length, linecount = 0;
	while ((start = lines.next_token(length)) >= 0) {
		// tentatively copy the line, then append a null terminator
		// at the bottom of the loop, we will advance buf if keepit is still true
		memcpy(buf, statements+start, length);
		buf[length] = 0;
		bool keepit = true;
		bool at_end = false;

		// look for specific keywords that we want to parse and remove from the final transform
		const char * p = buf + strspn(buf, " \t");
		switch (tolower(*p)) {
		case 'n':
			p = is_xform_statement(buf, "name");
			if (p) {
				std::string tmp(p); trim(tmp);
				// set the name, but don't overwrite a user-supplied name
				if (! tmp.empty() && name.empty()) name = tmp;
				keepit = false;
			}
			break;
		case 'u':
			p = is_xform_statement(buf, "universe");
			if (p) {
				setUniverse(p);
				keepit = false;
			}
			break;
		case 'r': 
			p = is_xform_statement(buf, "requirements");
			if (p) {
				int err = 0;
				setRequirements(p, err);
				if (err < 0) {
					formatstr(errmsg, "invalid REQUIREMENTS : %s", p);
					return err;
				}
				keepit = false;
			}
			break;
		case 't':
			p = is_xform_statement(buf, "transform");
			if (p) {
				if ( ! iterate_args) {
					p = is_non_trivial_iterate(p);
					if (p) { iterate_args.set(strdup(p)); iterate_init_state = 2; }
				}
				keepit = false;
				at_end = true;
			}
			break;
		default:
			break;
		}

		// if this was not a special keyword, append a newline and null terminator
		// and move the buffer pointer forward so we won't overwrite it when we fetch the next line
		// otherwise truncate the buffer in case there are no more lines to fetch
		if (keepit) {
			buf[length++] = '\n';
			buf[length] = 0;
			buf += length;
			++linecount;
		} else {
			buf[0] = 0;
		}
		if (at_end) {
			break;
		}
	}

	MacroStreamCharSource::open(file_string, LocalMacro);
	rewind();

	// tell the caller where we stopped parsing
	offset += start + length;
	return linecount;
}

// this parser is used with condor_transform_ads to parse rules files
// it supports TRANSFORM statements with complex arguments and itemdata (like submit queue statements)
// but does NOT currently support NAME, UNIVERSE or REQUIREMENTS
//
int MacroStreamXFormSource::load(FILE* fp, MACRO_SOURCE & FileSource, std::string & errmsg)
{
	StringList lines;

	while (true) {
		int lineno = FileSource.line;
		char * line = getline_trim(fp, FileSource.line);
		if ( ! line) {
			if (ferror(fp)) return -1;
			break;
		}

		/*
		// discard comment lines
		if (testing.enabled) {
			if (*line == '#') continue;
			lineno = FileSource.line-1; // disable linenumber corrections
		}
		*/

		if (FileSource.line != lineno+1) {
			// if we read more than a single line, comment the new linenumber
			std::string buf = "#opt:lineno:";
			buf += std::to_string(FileSource.line);
			lines.append(buf.c_str());
		}
		lines.append(line);

		const char * is_transform = is_xform_statement(line, "transform");
		if (is_transform) {
			// if this is an TRANSFORM statement, then we don't read any more statements.
			// if it is has a non-trivial argument set, then we can't expand the TRANSFORM yet
			// and we may need to treat the rest of the file as iteration arguments
			const char * iter_args = is_non_trivial_iterate(is_transform);
			// if we get to here and iter_args is NULL, the the line is eisther TRANSFORM or TRANSFORM 1,
			// there is no need to save the transform args, ust quit parsing the input file.
			if (iter_args) {
				iterate_args.set(strdup(iter_args));
				iterate_init_state = 2;
				// also store the fp until we can later parse the expanded TRANSFORM args
				fp_iter = fp;
				fp_lineno = FileSource.line;
			}
			break;
		}
	}

	return open(lines, FileSource, errmsg);
}

const char * MacroStreamXFormSource::getFormattedText(std::string & buf, const char *prefix /*=""*/, bool include_comments /*=false*/)
{
	buf = "";
	if ( ! name.empty()) {
		buf += prefix;
		buf += "NAME ";
		buf += name;
	}
	if (universe) {
		if ( ! buf.empty()) buf += "\n";
		buf += prefix;
		buf += "UNIVERSE ";
		buf += CondorUniverseName(universe);
	}
	if ( ! requirements.empty()) {
		if ( ! buf.empty()) buf += "\n";
		buf += prefix;
		buf += "REQUIREMENTS ";
		buf += requirements.c_str();
	}
	if (file_string) {
		StringTokenIterator lines(file_string.ptr(), 128, "\n");
		for (const char * line = lines.first(); line; line = lines.next()) {
			if ( ! include_comments) {
				while (*line && isspace(*line)) ++line;
				if (*line == 0 || *line == '#') continue;
			}
			if ( ! buf.empty()) buf += "\n";
			buf += prefix;
			buf += line;
		}
	}
	return buf.c_str();
}

classad::ExprTree* MacroStreamXFormSource::setRequirements(const char * require, int & err)
{
	requirements.set(require ? strdup(require) : NULL);
	return requirements.Expr(&err);
}

bool MacroStreamXFormSource::matches(ClassAd * candidate_ad)
{
	classad::ExprTree* expr = requirements.Expr();
	if ( ! expr) return true;

	classad::Value val;
	if (candidate_ad->EvaluateExpr(expr, val)) {
		bool matches = true;
		if (val.IsBooleanValueEquiv(matches)) return matches;
		return false;
	}
	return true;
}


int MacroStreamXFormSource::parse_iterate_args(char * pargs, int expand_options, XFormHash &set, std::string & errmsg)
{
	int citems = 0;
	int begin_lineno = fp_lineno;

	FILE * fp = fp_iter;
	fp_iter = NULL; // so we cant end up here again.

	// parse it using the same parser that condor submit uses for queue statements
	int rval = oa.parse_queue_args(pargs);
	if (rval < 0) {
		formatstr(errmsg, "invalid TRANSFORM statement");
		if (close_fp_when_done && fp) { fclose(fp); }
		return rval;
	}

	// if no loop variable specified, but a foreach mode is used. use "Item" for the loop variable.
	if (oa.vars.isEmpty() && (oa.foreach_mode != foreach_not)) { oa.vars.append("Item"); }

	// fill in the items array from a file
	if ( ! oa.items_filename.empty()) {
		if (oa.items_filename == "<") {
			// if reading iterate items from the input file, we have to read them now
			if ( ! fp) {
				errmsg = "unexpected error while attempting to read TRANSFORM items from xform file.";
				return -1;
			}

			// read items from submit file until we see the closing brace on a line by itself.
			bool saw_close_brace = false;
			for (;;) {
				char *line = getline_trim(fp, fp_lineno);
				if ( ! line) break; // null indicates end of file
				if (line[0] == '#') continue; // skip comments.
				if (line[0] == ')') { saw_close_brace = true; break; }
				if (oa.foreach_mode == foreach_from) {
					oa.items.append(line);
				} else {
					oa.items.initializeFromString(line);
				}
			}
			if (close_fp_when_done) { fclose(fp); fp = NULL; }
			if ( ! saw_close_brace) {
				formatstr(errmsg, "Reached end of file without finding closing brace ')'"
					" for TRANSFORM command on line %d", begin_lineno);
				return -1;
			}
		} else if (oa.items_filename == "-") {
			int lineno = 0;
			for (char* line=NULL;;) {
				line = getline_trim(stdin, lineno);
				if ( ! line) break;
				if (oa.foreach_mode == foreach_from) {
					oa.items.append(line);
				} else {
					oa.items.initializeFromString(line);
				}
			}
		} else {
			MACRO_SOURCE ItemsSource;
			FILE *fpItems = Open_macro_source(ItemsSource, oa.items_filename.c_str(), false, set.macros(), errmsg);
			if ( ! fpItems) {
				return -1;
			}
			for (char* line=NULL;;) {
				line = getline_trim(fpItems, ItemsSource.line);
				if ( ! line) break;
				oa.items.append(line);
			}
			rval = Close_macro_source(fpItems, ItemsSource, set.macros(), 0);
		}
	}

	if (close_fp_when_done && fp) { fclose(fp); fp = NULL; }

	// fill in the items array from a glob
	//
	switch (oa.foreach_mode) {
	case foreach_in:
	case foreach_from:
		// itemlist is already correct
		// PRAGMA_REMIND("do argument validation here?")
		citems = oa.items.number();
		break;

	case foreach_matching:
	case foreach_matching_files:
	case foreach_matching_dirs:
	case foreach_matching_any:
		if (oa.foreach_mode == foreach_matching_files) {
			expand_options &= ~EXPAND_GLOBS_TO_DIRS;
			expand_options |= EXPAND_GLOBS_TO_FILES;
		} else if (oa.foreach_mode == foreach_matching_dirs) {
			expand_options &= ~EXPAND_GLOBS_TO_FILES;
			expand_options |= EXPAND_GLOBS_TO_DIRS;
		} else if (oa.foreach_mode == foreach_matching_any) {
			expand_options &= ~(EXPAND_GLOBS_TO_FILES|EXPAND_GLOBS_TO_DIRS);
		}
		citems = submit_expand_globs(oa.items, expand_options, errmsg);
		if ( ! errmsg.empty()) {
			fprintf(stderr, "\n%s: %s", citems >= 0 ? "WARNING" : "ERROR", errmsg.c_str());
			errmsg.clear();
		}
		if (citems < 0) return citems;
		break;

	default:
	case foreach_not:
		// to simplify the loop below, set a single empty item into the itemlist.
		citems = 1;
		break;
	}

	return citems;
}

static char * trim_in_place(char * str)
{
	char * p = str;
	while (isspace(*p)) ++p;
	char * pe = p + strlen(p);
	while (pe > p && isspace(pe[-1])) --pe;
	*pe = 0;
	return p;
}


int MacroStreamXFormSource::init_iterator(XFormHash &mset, std::string & errmsg)
{
	if (iterate_init_state <= 1) return iterate_init_state;
	if (iterate_args) {
		auto_free_ptr rhs(mset.expand_macro(iterate_args.ptr(), ctx));
		char * ptr = trim_in_place(rhs.ptr());
		if (*ptr) {
			iterate_init_state = this->parse_iterate_args(ptr, EXPAND_GLOBS_WARN_EMPTY, mset, errmsg);
		} else {
			oa.clear();
		}
		iterate_args.clear();
	}
	if (iterate_init_state >= 0) {
		iterate_init_state = ((oa.foreach_mode == foreach_not) && oa.queue_num == 1) ? 0 : 1;
	}
	return iterate_init_state;
}

static char EmptyItemString[] = "";

int MacroStreamXFormSource::set_iter_item(XFormHash &set, const char* item)
{
	if (oa.vars.isEmpty()) return 0;

	// make a copy of the item so we can destructively edit it.
	char * data;
	if (item) { 
		data = strdup(item);
		curr_item.set(data);
	} else {
		data = EmptyItemString;
		EmptyItemString[0] = 0;
		curr_item.clear();
	}

	// set the first loop variable unconditionally, we set it initially to the whole item
	// we may later truncate that item when we assign fields to other loop variables.
	char * var = oa.vars.first();
	set.set_live_variable(var, data, ctx);

	const char* token_seps = ", \t";
	const char* token_ws = " \t";

	// if there is more than a single loop variable, then assign them as well
	// we do this by destructively null terminating the item for each var
	// the last var gets all of the remaining item text (if any)
	while ((var = oa.vars.next())) {
		// scan for next token separator
		while (*data && ! strchr(token_seps, *data)) ++data;
		// null terminate the previous token and advance to the start of the next token.
		if (*data) {
			*data++ = 0;
			// skip leading separators and whitespace
			while (*data && strchr(token_ws, *data)) ++data;
			set.set_live_variable(var, data, ctx);
		}
	}
	return curr_item.ptr() != NULL;
}


bool  MacroStreamXFormSource::first_iteration(XFormHash &set)
{
	ASSERT(iterate_init_state <= 1);
	step = row = proc = 0;
	set.set_iterate_step(step, proc);

	// if there is no iterator, or it is trivial, there is no need to go again.
	if (oa.foreach_mode == foreach_not && oa.queue_num == 1) {
		set.set_iterate_row(row, false);
		return false;
	}

	set.set_iterate_row(row, true);

	ASSERT( ! checkpoint);
	checkpoint = set.save_state();

	// prime the iteration variables
	oa.items.rewind();
	return set_iter_item(set, oa.items.next()) || (oa.queue_num > 1);
}

bool MacroStreamXFormSource::next_iteration(XFormHash &set)
{
	++proc;

	bool has_next_item = false;
	if (step+1 < oa.queue_num) {
		++step;
		has_next_item = true;
	} else {
		step = 0;
		++row;
		if (checkpoint) { set.rewind_to_state(checkpoint, false); }
		has_next_item = set_iter_item(set, oa.items.next());
		set.set_iterate_row(row, true);
	}
	set.set_iterate_step(step, proc);
	return has_next_item;
}

// at the end of iteration, rewind the iterator and clear live variables.
void MacroStreamXFormSource::clear_iteration(XFormHash &set)
{
	if (checkpoint) { set.rewind_to_state(checkpoint, true);
		checkpoint = NULL;
	}
	set.clear_live_variables();
	curr_item.clear();
	oa.items.rewind();
}

void MacroStreamXFormSource::reset(XFormHash &set)
{
	clear_iteration(set);
	oa.clear();
}

// **************************************************************************************
//
// **************************************************************************************

// delete an attribute of the given classad.
// returns 0  if the delete did not happen
// returns 1  if the delete happened.
static int DoDeleteAttr(ClassAd * ad, const std::string & attr, unsigned int flags)
{
	if (flags&2) fprintf(stdout, "DELETE %s\n", attr.c_str());
	if (ad->Delete(attr)) {
		// Mark the attribute we just removed as dirty, since the schedd relies
		// upon the ClassAd dirty attribute list to see what changed, and it must
		// know about removed attributes.
		// Supposedly classad::ClassAd library is supposed to add removed attrs to the
		// dirty list all on its own, but this is currently not the case.
		ad->MarkAttributeDirty(attr);
		return 1;
	} else {
		return 0;
	}
}

// rename an attribute of the given classad.
// returns -1 if the new attribute is invalid
// returns 0  if the rename did not happen
// returns 1  if the rename happened.
static int DoRenameAttr(ClassAd * ad, const std::string & attr, const char * attrNew, unsigned int flags)
{
	if (flags&2) fprintf(stdout, "RENAME %s to %s\n", attr.c_str(), attrNew);
	if ( ! IsValidAttrName(attrNew)) {
		if (flags&1) fprintf(stderr, "ERROR: RENAME %s new name %s is not valid\n", attr.c_str(), attrNew);
		return -1;
	} else {
		ExprTree * tree = ad->Remove(attr);
		if (tree) {
			if (ad->Insert(attrNew, tree)) {
				return 1;
			} else {
				if (flags&1) fprintf(stderr, "ERROR: could not rename %s to %s\n", attr.c_str(), attrNew);
				if ( ! ad->Insert(attr, tree)) {
					delete tree;
				}
			}
		}
	}
	return 0;
}

// copy the expr tree of an attribute to a new attribute name.
// returns -1 if the new attribute is invalid
// returns 0  if the copy did not happen
// returns 1  if the copy happened.
static int DoCopyAttr(ClassAd * ad, const std::string & attr, const char * attrNew, unsigned int flags)
{
	if (flags&2) fprintf(stdout, "COPY %s to %s\n", attr.c_str(), attrNew);
	if ( ! IsValidAttrName(attrNew)) {
		if (flags&1) fprintf(stderr, "ERROR: COPY %s new name %s is not valid\n", attr.c_str(), attrNew);
		return -1;
	} else {
		ExprTree * tree = ad->Lookup(attr);
		if (tree) {
			tree = tree->Copy();
			if (ad->Insert(attrNew, tree)) {
				return 1;
			} else {
				if (flags&1) fprintf(stderr, "ERROR: could not copy %s to %s\n", attr.c_str(), attrNew);
				delete tree;
			}
		}
	}
	return 0;
}


typedef struct {
	const char * key;
	int          value;
	int          options;
} Keyword;
typedef nocase_sorted_tokener_lookup_table<Keyword> KeywordTable;


#define kw_opt_argcount_mask 0x0F
#define kw_opt_regex         0x10

enum {
	kw_COPY=1,
	kw_DEFAULT,
	kw_DELETE,
	kw_EVALMACRO,
	kw_EVALSET,
	kw_NAME,
	kw_RENAME,
	kw_REQUIREMENTS,
	kw_SET,
	kw_TRANSFORM,
	kw_UNIVERSE,
};

// This must be declared in string sorted order, they do not have to be in enum order.
#define KW(a, f) { #a, kw_##a, f }
static const Keyword ActionKeywordItems[] = {
	KW(COPY, 2 | kw_opt_regex),
	KW(DEFAULT, 2),
	KW(DELETE, 1 | kw_opt_regex),
	KW(EVALMACRO, 2),
	KW(EVALSET, 2),
	KW(NAME, 0),
	KW(RENAME, 2 | kw_opt_regex),
	KW(REQUIREMENTS, 0),
	KW(SET, 2),
	KW(TRANSFORM, 0),
	KW(UNIVERSE, 1),
};
#undef KW
static const KeywordTable ActionKeywords = SORTED_TOKENER_TABLE(ActionKeywordItems);


struct _parse_rules_args {
	MacroStreamXFormSource & xfm;
	XFormHash & mset;
	ClassAd * ad;
	unsigned int flags;
};

// substitute regex groups into a string, appending the output to a std::string
//
// TODO: move this to stl_string_utils
const char * append_substituted_regex (
	std::string &output,      // substituted regex will be appended to this
	const char * input,       // original input passed to pcre_exec (ovector has offsets into this)
	const int ovector[], // output vector from pcre_exec
	int cvec,      // output count from pcre_exec
	const char * replacement, // replacement template string
	char tagChar)  // char that introduces a subtitution in replacement string, usually \ or $
{
	const char * p = replacement;
	const char * lastp = p; // points to the start of the last point we copied from.
	while (*p) {
		if (p[0] == tagChar && p[1] >= '0' && p[1] < '0' + cvec) {
			if (p > lastp) { output.append(lastp, p-lastp); }
			int ix = p[1] - '0';
			int ix1 = ovector[ix*2];
			int ix2 = ovector[ix*2+1];
			output.append(&input[ix1], ix2 - ix1);
			++p;
			lastp = p+1; // skip over digit
		}
		++p;
	}
	if (p > lastp) { output.append(lastp, p-lastp); }
	return output.c_str();
}

// Handle kw_COPY, kw_RENAME and kw_DELETE when the first argument is a regex.
// 
static int DoRegexAttrOp(int kw_value, ClassAd *ad, pcre* re, int re_options, const char * replacement, bool verbose)
{
	const int max_group_count = 11; // only \0 through \9 allowed.
	int ovector[3 * (max_group_count + 1)]; // +1 for the string itself
	std::string newAttr;
	const char tagChar = '\\';
	newAttr.reserve(100);

	// pcre_exec doesn't like to see these flags.
	re_options &= ~(PCRE_CASELESS | PCRE_UNGREEDY | PCRE_MULTILINE);

	// keep track of matching attributes, we will only modify the input ad
	// after we are done iterating through it.
	std::map<std::string, std::string> matched;

	for (ClassAd::iterator it = ad->begin(); it != ad->end(); ++it) {
		const char * input = it->first.c_str();
		int cchin = (int)it->first.length();
		int cvec = pcre_exec(re, NULL, input, cchin, 0, re_options, ovector, (int)COUNTOF(ovector));
		if (cvec <= 0)
			continue; // does not match

		newAttr = "";
		if (kw_value != kw_DELETE) {
			append_substituted_regex(newAttr, input, ovector, cvec, replacement, tagChar);
		}
		matched[it->first] = newAttr;
	}

	// now modify the input ad
	for (std::map<std::string, std::string>::iterator it = matched.begin(); it != matched.end(); ++it) {
		switch (kw_value) {
			case kw_DELETE: DoDeleteAttr(ad, it->first, verbose); break;
			case kw_RENAME: DoRenameAttr(ad, it->first, it->second.c_str(), verbose); break;
			case kw_COPY:   DoCopyAttr(ad, it->first, it->second.c_str(), verbose); break;
		}
	}

	return (int)matched.size();
}


static const char * XFormValueToString(classad::Value & val, std::string & tmp)
{
	if ( ! val.IsStringValue(tmp)) {
		classad::ClassAdUnParser unp;
		unp.SetOldClassAd(true);
		tmp.clear(); // because Unparse appends.
		unp.Unparse(tmp, val);
	}
	return tmp.c_str();
}

static ExprTree * XFormCopyValueToTree(classad::Value & val)
{
	ExprTree *tree = NULL;
	classad::Value::ValueType vtyp = val.GetType();
	if (vtyp == classad::Value::LIST_VALUE) {
		classad::ExprList * list = NULL;
		(void) val.IsListValue(list);
		ASSERT(list);
		tree = list->Copy();
	} else if (vtyp == classad::Value::SLIST_VALUE) {
		classad_shared_ptr<classad::ExprList> list;
		(void) val.IsSListValue(list);
		ASSERT(list.get());
		tree = list.get()->Copy();
	} else if (vtyp == classad::Value::CLASSAD_VALUE || vtyp == classad::Value::SCLASSAD_VALUE) {
		classad::ClassAd* aval = NULL;
		(void) val.IsClassAdValue(aval);
		ASSERT(aval);
		tree = aval->Copy();
	} else {
		tree = classad::Literal::MakeLiteral(val);
	}
	return tree;
}

static int ValidateRulesCallback(void* /*pv*/, MACRO_SOURCE& /*source*/, MACRO_SET& /*mset*/, char * line, std::string & errmsg)
{
	//struct _parse_rules_args * pargs = (struct _parse_rules_args*)pv;
	//XFormHash & mset = pargs->mset;
	//MacroStreamXFormSource & xform = pargs->xfm;

	// give the line to our tokener so we can parse it.
	tokener toke(line);
	if ( ! toke.next()) return 0; // keep scanning
	if (toke.matches("#")) return 0; // not expecting this, but in case...

	// get keyword
	const Keyword * pkw = ActionKeywords.lookup_token(toke);
	if ( ! pkw) {
		std::string tok; toke.copy_token(tok);
		formatstr(errmsg, "%s is not a valid transform keyword\n", tok.c_str());
		return -1;
	}
	// there must be something after the keyword
	if ( ! toke.next()) { return (pkw->value == kw_TRANSFORM) ? 0 : -1; }

	toke.mark_after(); // in case we want to capture the remainder of the line.

	// the first argument will always be an attribute name
	// in some cases, that attribute name is is allowed to be a regex,
	// if it is a regex it will begin with a /
	std::string attr;
	int regex_flags = 0;
	if ((pkw->options & kw_opt_regex) && toke.is_regex()) {
		std::string opts;
		if ( ! toke.copy_regex(attr, regex_flags)) {
			errmsg = "invalid regex";
			return -1;
		}
		regex_flags |= PCRE_CASELESS;
	} else {
		toke.copy_token(attr);
		// if attr ends with , or =, just strip those off. the tokener only splits on whitespace, not other characters
		if (attr.size() > 0 && (attr[attr.size()-1] == ',' || attr[attr.size()-1] == '=')) { attr[attr.size()-1] = 0; }
	}

	return 0; // line is valid, keep scanning.
}

// this gets called while parsing the submit file to process lines
// that don't look like valid key=value pairs.  
// return 0 to keep scanning the file, ! 0 to stop scanning. non-zero return values will
// be passed back out of Parse_macros
//
static int ParseRulesCallback(void* pv, MACRO_SOURCE& source, MACRO_SET& /*mset*/, char * line, std::string & errmsg)
{
	struct _parse_rules_args * pargs = (struct _parse_rules_args*)pv;
	const bool verbose = (pargs->flags & 2) != 0;
	const bool is_tool = (pargs->flags & 1) != 0;
	ClassAd * ad = pargs->ad;
	XFormHash & mset = pargs->mset;
	MacroStreamXFormSource & xform = pargs->xfm;

	classad::ClassAdParser parser;
	parser.SetOldClassAd(true);
	std::string tmp3;

	// give the line to our tokener so we can parse it.
	tokener toke(line);
	if ( ! toke.next()) return 0; // keep scanning
	if (toke.matches("#")) return 0; // not expecting this, but in case...

	// get keyword
	const Keyword * pkw = ActionKeywords.lookup_token(toke);
	if ( ! pkw) {
		std::string tok; toke.copy_token(tok);
		formatstr(errmsg, "%s is not a valid transform keyword\n", tok.c_str());
		return -1;
	}
	// there must be something after the keyword
	if ( ! toke.next()) { return (pkw->value == kw_TRANSFORM) ? 0 : -1; }

	toke.mark_after(); // in case we want to capture the remainder of the line.

	// the first argument will always be an attribute name
	// in some cases, that attribute name is is allowed to be a regex,
	// if it is a regex it will begin with a /
	std::string attr;
	bool attr_is_regex = false;
	int regex_flags = 0;
	if ((pkw->options & kw_opt_regex) && toke.is_regex()) {
		std::string opts;
		attr_is_regex = true;
		if ( ! toke.copy_regex(attr, regex_flags)) {
			errmsg = "invalid regex";
			return -1;
		}
		regex_flags |= PCRE_CASELESS;
	} else {
		toke.copy_token(attr);
		// if attr ends with , or =, just strip those off. the tokener only splits on whitespace, not other characters
		if (attr.size() > 0 && (attr[attr.size()-1] == ',' || attr[attr.size()-1] == '=')) { attr[attr.size()-1] = 0; }
	}

	// for 2 argument keywords, consume the attribute token and move on to the next one
	if ((pkw->options & kw_opt_argcount_mask) == 2) {
		toke.next();
		if (toke.matches("=") || toke.matches(",")) {
			toke.next();  // consume the keyword token
		}
	}

	// if there is a remainder of the line, macro expand it.
	size_t off = toke.offset(); // current pointer
	auto_free_ptr rhs(NULL);
	if (off > 0) {
		if (toke.is_quoted_string()) --off;
		rhs.set(mset.expand_macro(line + off, xform.context()));
	}

	// at this point
	// pkw   identifies the keyword
	// attr  contains the first word after the keyword, thisis usually an attribute name
	// rhs   has the macro-expanded remainder of the line

	//std::string expr_string;
	//if ( ! toke.at_end()) { toke.copy_to_end(expr_string); }

	switch (pkw->value) {
	// these keywords are envelope information
	case kw_NAME:
		if (verbose) fprintf(stdout, "NAME %s\n", rhs.ptr());
		break;
	case kw_UNIVERSE:
		if (verbose) fprintf(stdout, "UNIVERSE %d\n", CondorUniverseNumberEx(attr.c_str()));
		break;
	case kw_REQUIREMENTS:
		if (verbose) fprintf(stdout, "REQUIREMENTS %s\n", rhs.ptr());
		break;

		// evaluate an expression, then set a value into the ad.
	case kw_EVALMACRO:
		if (verbose) fprintf(stdout, "EVALMACRO %s to %s\n", attr.c_str(), rhs.ptr());
		if ( ! rhs) {
			if (is_tool) fprintf(stderr, "ERROR: EVALMACRO %s has no value", attr.c_str());
		} else {
			classad::Value val;
			if ( ! ad->EvaluateExpr(rhs.ptr(), val)) {
				if (is_tool) fprintf(stderr, "ERROR: EVALMACRO %s could not evaluate : %s\n", attr.c_str(), rhs.ptr());
			} else {
				XFormValueToString(val, tmp3);
				insert_macro(attr.c_str(), tmp3.c_str(), mset.macros(), source, xform.context());
				if (verbose) { fprintf(stdout, "          %s = %s\n", attr.c_str(), tmp3.c_str()); }
			}
		}
		break;

	case kw_TRANSFORM:
#if 1
		//PRAGMA_REMIND("tj move this out of the parser callback.")
#else
		if (pargs->xforms->has_pending_fp()) {
			std::string errmsg;
			// EXPAND_GLOBS_TO_FILES | EXPAND_GLOBS_TO_DIRS
			// EXPAND_GLOBS_WARN_EMPTY | EXPAND_GLOBS_FAIL_EMPTY
			// EXPAND_GLOBS_WARN_DUPS | EXPAND_GLOBS_ALLOW_DUPS 
			int expand_options = EXPAND_GLOBS_WARN_EMPTY;
			int rval = pargs->xforms->parse_iterate_args(rhs.ptr(), expand_options, mset, errmsg);
			if (rval < 0) {
				if (is_tool) fprintf(stderr, "ERROR: %s\n", errmsg.c_str()); return -1;
			}
		}
#endif
		break;

	// these keywords manipulate the ad
	case kw_DEFAULT:
		if (verbose) fprintf(stdout, "DEFAULT %s to %s\n", attr.c_str(), rhs.ptr());
		if (ad->Lookup(attr) != NULL) {
			// this attribute already has a value.
			break;
		}
		// fall through
	case kw_SET:
		if (verbose) fprintf(stdout, "SET %s to %s\n", attr.c_str(), rhs.ptr());
		if ( ! rhs) {
			if (is_tool) fprintf(stderr, "ERROR: SET %s has no value", attr.c_str());
		} else {
			ExprTree * expr = NULL;
			if ( ! parser.ParseExpression(rhs.ptr(), expr, true)) {
				if (is_tool) fprintf(stderr, "ERROR: SET %s invalid expression : %s\n", attr.c_str(), rhs.ptr());
			} else {
				if ( ! ad->Insert(attr, expr)) {
					if (is_tool) fprintf(stderr, "ERROR: could not set %s to %s\n", attr.c_str(), rhs.ptr());
					delete expr;
				}
			}
		}
		break;

	case kw_EVALSET:
		if (verbose) fprintf(stdout, "EVALSET %s to %s\n", attr.c_str(), rhs.ptr());
		if ( ! rhs) {
			if (is_tool) fprintf(stderr, "ERROR: EVALSET %s has no value", attr.c_str());
		} else {
			classad::Value val;
			if ( ! ad->EvaluateExpr(rhs.ptr(), val)) {
				if (is_tool) fprintf(stderr, "ERROR: EVALSET %s could not evaluate : %s\n", attr.c_str(), rhs.ptr());
			} else {
				ExprTree * tree = XFormCopyValueToTree(val);
				if ( ! ad->Insert(attr, tree)) {
					if (is_tool) fprintf(stderr, "ERROR: could not set %s to %s\n", attr.c_str(), XFormValueToString(val, tmp3));
					delete tree;
				} else if (verbose) {
					if (is_tool) fprintf(stdout, "    SET %s to %s\n", attr.c_str(), XFormValueToString(val, tmp3));
				}
			}
		}
		break;

	case kw_DELETE:
	case kw_RENAME:
	case kw_COPY:
		if (attr_is_regex) {
			const char *errptr;
			int erroffset;
			pcre * re = pcre_compile(attr.c_str(), regex_flags, &errptr, &erroffset, NULL);
			if (! re) {
				if (is_tool) fprintf(stderr, "ERROR: Error compiling regex '%s'. %s. this entry will be ignored.\n", attr.c_str(), errptr);
			} else {
				DoRegexAttrOp(pkw->value, ad, re, regex_flags, rhs.ptr(), verbose);
				pcre_free(re);
			}
		} else {
			switch (pkw->value) {
				case kw_DELETE: DoDeleteAttr(ad, attr, verbose); break;
				case kw_RENAME: DoRenameAttr(ad, attr, rhs.ptr(), verbose); break;
				case kw_COPY:   DoCopyAttr(ad, attr, rhs.ptr(), verbose); break;
			}
		}
	}

	return 0; // line handled, keep scanning.
}

// in-place transform of a single ad 
// using the transform rules in xfm,
// variables live in the hashtable mset.
// returns < 0 on failure, errmsg will have details
//
int TransformClassAd (
	ClassAd * input_ad,            // the ad to be transformed
	MacroStreamXFormSource & xfm,  // the set of transform rules
	XFormHash & mset,              // the hashtable used as temporary storage
	std::string & errmsg,          // holds parse errors on failure
	unsigned int  flags)  // (flags&1) - errors to stderr, (flags&2) - verbose logging to stdout
{
	MACRO_EVAL_CONTEXT_EX & ctx = xfm.context();
	ctx.ad = input_ad;
	ctx.adname = "MY.";
	ctx.also_in_config = true;

	_parse_rules_args args = { xfm, mset, input_ad, flags };

	xfm.rewind();
	int rval = Parse_macros(xfm, 0, mset.macros(), READ_MACROS_SUBMIT_SYNTAX, &ctx, errmsg, ParseRulesCallback, &args);
	if (rval) {
		if (flags&1) fprintf(stderr, "Transform of ad %s failed!\n", "");
		return rval;
	}
	return rval;
}


#if 0

class apply_transform_args {
public:
	apply_transform_args(MacroStreamXFormSource& xfm, MACRO_SET_CHECKPOINT_HDR* check, FILE* out)
		: xforms(xfm), checkpoint(check), outfile(out), input_helper(NULL) {}
	MacroStreamXFormSource & xforms;
	MACRO_SET_CHECKPOINT_HDR* checkpoint;
	FILE* outfile;
	CondorClassAdFileParseHelper *input_helper;
	std::string errmsg;
};

#define TRANSFORM_IN_PLACE 1

// return true from the transform if no longer care about the job ad
// return false to take ownership of the job ad
int ApplyTransform (
	void* pv,
	ClassAd * input_ad)
{
	if ( ! pv) return 1;
	apply_transform_args & xform_args = *(apply_transform_args*)pv;
	const ClassAd * job = input_ad;

	MacroStreamXFormSource & xforms = xform_args.xforms;
	MACRO_SET_CHECKPOINT_HDR* pvParamCheckpoint = xform_args.checkpoint;
	//FILE* outfile = xform_args.outfile;

	_parse_rules_args args = { NULL, NULL, &xforms, 0 };

	//args.ad = input_ad;
	//if (xforms.will_iterate()) { args.ad = new ClassAd(*job); }
	args.ad = new ClassAd(*job);

	LocalContext.ad = args.ad;
	LocalContext.adname = "MY.";

	// indicate that we are not yet iterating
	(void)sprintf(IteratingString, "%d", 0);

	// the first parse is a keeper only if there is no TRANSFORM statement.
	xforms.rewind();
	int rval = Parse_macros(xforms, 0, LocalMacroSet, READ_MACROS_SUBMIT_SYNTAX, &LocalContext, xform_args.errmsg, ParseRulesCallback, &args);
	if (rval) {
		fprintf(stderr, "Transform of ad %s failed!\n", "");
		return rval;
	}

	// if there is an TRANSFORM, then we want to reparse again with the iterator variables set.
	bool iterating = xforms.first_iteration(LocalMacroSet);
	if ( ! iterating) {
		ReportSuccess(args.ad, xform_args);
	} else {
		while (iterating) {
			delete args.ad;
			LocalContext.ad = args.ad = new ClassAd(*job);

			xforms.rewind();
			rval = Parse_macros(xforms, 0, LocalMacroSet, READ_MACROS_SUBMIT_SYNTAX, &LocalContext, xform_args.errmsg, ParseRulesCallback, &args);
			if (rval < 0)
				break;

			ReportSuccess(args.ad, xform_args);
			iterating = xforms.next_iteration(LocalMacroSet);
		}
	}

	// if (xforms.will_iterate()) { delete args.ad; }
	delete args.ad;
	LocalContext.ad = NULL;
	rewind_macro_set(LocalMacroSet, pvParamCheckpoint, false);

	return rval ? rval : 1; // we return non-zero to allow the job to be deleted
}

#endif

// *******************************************************************************
// Convert old job routes
// *******************************************************************************

// attributes of interest to the code that converts old jobrouter routes
enum {
	atr_NAME=1,
	atr_UNIVERSE,
	atr_TARGETUNIVERSE,
	atr_GRIDRESOURCE,
	atr_REQUIREMENTS,
	atr_ENVIRONMENT,
	atr_OSG_ENVIRONMENT,
	atr_ORIG_ENVIRONMENT,
	atr_REQUESTMEMORY,
	atr_REQUESTCPUS,
	atr_ONEXITHOLD,
	atr_ONEXITHOLDREASON,
	atr_ONEXITHOLDSUBCODE,
	atr_REMOTE_QUEUE,
	atr_REMOTE_CEREQUIREMENTS,
	atr_REMOTE_NODENUMBER,
	atr_REMOTE_SMPGRANULARITY,
	atr_INPUTRSL,
	atr_GLOBUSRSL,
	atr_XCOUNT,
	atr_DEFAULT_XCOUNT,
	atr_QUEUE,
	atr_DEFAULT_QUEUE,
	atr_MAXMEMORY,
	atr_DEFAULT_MAXMEMORY,
	atr_MAXWALLTIME,
	atr_DEFAULT_MAXWALLTIME,
	atr_MINWALLTIME,
	atr_EDITJobInPlace,
	atr_FailureRateThreshold,
	atr_JobFailureTest,
	atr_JobShouldBeSandboxed,
	atr_OVERRIDERoutingEntry,
	atr_USESharedX509UserProxy,
	atr_SHAREDX509UserProxy,
};

#define atr_f_Bool     0x0002
#define atr_f_String   0x0004
#define atr_f_Number   0x0008
#define atr_f_Flatten  0x0010
#define atr_f_UnTarget 0x0020
#define atr_f_MyTarget 0x0040

// This must be declared in string sorted order, they do not have to be in enum order.
#define ATR(a, f) { #a, atr_##a, f }
static const Keyword RouterAttrItems[] = {
	ATR(DEFAULT_MAXMEMORY, 0),
	ATR(DEFAULT_MAXWALLTIME, 0),
	ATR(DEFAULT_QUEUE, 0),
	ATR(DEFAULT_XCOUNT, 0),
	ATR(EDITJobInPlace, atr_f_Flatten | atr_f_MyTarget | atr_f_Bool),
	ATR(ENVIRONMENT, 0),
	ATR(FailureRateThreshold, atr_f_Flatten | atr_f_MyTarget | atr_f_Number),
	ATR(GLOBUSRSL, 0),
	ATR(GRIDRESOURCE, 0),
	ATR(INPUTRSL, 0),
	ATR(JobFailureTest, atr_f_Flatten | atr_f_MyTarget | atr_f_Bool),
	ATR(JobShouldBeSandboxed, atr_f_Flatten | atr_f_MyTarget | atr_f_Bool),
	ATR(MAXMEMORY, 0),
	ATR(MAXWALLTIME, 0),
	ATR(MINWALLTIME, 0),
	ATR(NAME, 0),
	ATR(ONEXITHOLD, 0),
	ATR(ONEXITHOLDREASON, 0),
	ATR(ONEXITHOLDSUBCODE, 0),
	ATR(ORIG_ENVIRONMENT, 0),
	ATR(OSG_ENVIRONMENT, 0),
	ATR(OVERRIDERoutingEntry, atr_f_Flatten | atr_f_MyTarget | atr_f_Bool),
	ATR(QUEUE, 0),
	ATR(REMOTE_CEREQUIREMENTS, 0),
	ATR(REMOTE_NODENUMBER, 0),
	ATR(REMOTE_QUEUE, 0),
	ATR(REMOTE_SMPGRANULARITY, 0),
	ATR(REQUESTCPUS, 0),
	ATR(REQUESTMEMORY, 0),
	ATR(REQUIREMENTS, atr_f_Flatten | atr_f_UnTarget | atr_f_Bool),
	ATR(SHAREDX509UserProxy, atr_f_Flatten | atr_f_MyTarget | atr_f_String),
	ATR(TARGETUNIVERSE, 0),
	ATR(UNIVERSE, 0),
	ATR(USESharedX509UserProxy, atr_f_Flatten | atr_f_MyTarget | atr_f_Bool),
	ATR(XCOUNT, 0),
};
#undef ATR
static const KeywordTable RouterAttrs = SORTED_TOKENER_TABLE(RouterAttrItems);

static int is_interesting_route_attr(const std::string & attr, int * popts=NULL) {
	const Keyword * patr = RouterAttrs.lookup(attr.c_str());
	if (popts) { *popts = patr ? patr->options : 0; }
	if (patr) return patr->value;
	return 0;
}


static int convert_target_to_my(classad::ExprTree * tree)
{
	NOCASE_STRING_MAP mapping;
	mapping["TARGET"] = "MY";
	return RewriteAttrRefs(tree, mapping);
}

static int strip_target_attr_ref(classad::ExprTree * tree)
{
	NOCASE_STRING_MAP mapping;
	mapping["TARGET"] = "";
	return RewriteAttrRefs(tree, mapping);
}

static void unparse_special (
	classad::ClassAdUnParser & unparser,
	std::string & rhs,
	classad::ClassAd & ad,
	ExprTree * tree,
	int       options)
{
	bool untarget = options & atr_f_UnTarget;
	bool mytarget = options & atr_f_MyTarget;
	ExprTree * flat_tree = NULL;
	classad::Value flat_val;
	if (ad.FlattenAndInline(tree, flat_val, flat_tree)) {
		if ( ! flat_tree) {
			unparser.Unparse(rhs, flat_val);
		} else {
			if (untarget) { strip_target_attr_ref(flat_tree); }
			if (mytarget) { convert_target_to_my(flat_tree); }
			unparser.Unparse(rhs, flat_tree);
			delete flat_tree;
		}
	} else {
		if (untarget || mytarget) {
			tree = SkipExprEnvelope(tree)->Copy();
			if (untarget) { strip_target_attr_ref(tree); }
			if (mytarget) { convert_target_to_my(tree); }
			unparser.Unparse(rhs, tree);
			delete tree;
		} else {
			unparser.Unparse(rhs, tree);
		}
	}
}


/*
static bool same_refs(StringList & refs, const char * check_str)
{
	if (refs.isEmpty()) return false;
	StringList check(check_str);
	return check.contains_list(refs, true);
}
*/

typedef std::map<std::string, std::string, classad::CaseIgnLTStr> STRING_MAP;


#define XForm_ConvertJobRouter_Remove_InputRSL 0x00001
#define XForm_ConvertJobRouter_Fix_EvalSet     0x00002

int ConvertClassadJobRouterRouteToXForm (
	StringList & statements,
	const char * config_name, // name from config
	const std::string & routing_string,
	int & offset,
	const classad::ClassAd & base_route_ad,
	int options)
{
	classad::ClassAdParser parser;
	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd(true, true);

	//bool style_2 = (options & 0x0F) == 2;
	int  has_set_xcount = 0, has_set_queue = 0, has_set_maxMemory = 0, has_set_maxWallTime = 0, has_set_minWallTime = 0;
	//bool has_def_RequestMemory = false, has_def_RequestCpus = false, has_def_remote_queue = false;
	bool has_def_onexithold = false;

	ClassAd route_ad(base_route_ad);
	if ((int)routing_string.size() > offset) {
		classad::ClassAd ad;
		if ( ! parser.ParseClassAd(routing_string, ad, offset)) {
			return 0;
		}
		route_ad.Update(ad);
	}

	std::string name, grid_resource, requirements;
	int target_universe = 0;

	STRING_MAP assignments;
	STRING_MAP copy_cmds;
	STRING_MAP delete_cmds;
	STRING_MAP set_cmds;
	STRING_MAP defset_cmds;
	STRING_MAP evalset_cmds;
	classad::References evalset_myrefs;
	classad::References string_assignments;

	// Initialize name with name for this xform; note it may be overwritten below
	// if ClassAd includes a Name attribute.
	if (config_name) { name = config_name; }

	for (ClassAd::iterator it = route_ad.begin(); it != route_ad.end(); ++it) {
		std::string rhs;
		if (starts_with_ignore_case(it->first, "copy_")) {
			std::string attr = it->first.substr(5);
			if (route_ad.EvaluateAttrString(it->first, rhs)) {
				copy_cmds[attr] = rhs;
			}
		} else if (starts_with_ignore_case(it->first, "delete_")) {
			std::string attr = it->first.substr(7);
			delete_cmds[attr] = "";
		} else if (starts_with_ignore_case(it->first, "set_")) {
			std::string attr = it->first.substr(4);
			int atrid = is_interesting_route_attr(attr);
			if ((atrid == atr_INPUTRSL) && (options & XForm_ConvertJobRouter_Remove_InputRSL)) {
				// just eat this.
				continue;
			}
			rhs.clear();
			ExprTree * tree = route_ad.Lookup(it->first);
			if (tree) {
				unparser.Unparse(rhs, tree);
				if ( ! rhs.empty()) {
					set_cmds[attr] = rhs;
				}
			}
		} else if (starts_with_ignore_case(it->first, "eval_set_")) {
			std::string attr = it->first.substr(9);
			int atrid = is_interesting_route_attr(attr);
			ExprTree * tree = route_ad.Lookup(it->first);
			if (tree) {
				rhs.clear();
				unparser.Unparse(rhs, tree);
				if ( ! rhs.empty()) {

					classad::References myrefs;
					GetExprReferences(rhs.c_str(), route_ad, &myrefs, NULL);
					if (atrid == atr_REQUESTMEMORY || 
						atrid == atr_REQUESTCPUS || atrid == atr_REMOTE_NODENUMBER || atrid == atr_REMOTE_SMPGRANULARITY ||
						atrid == atr_ONEXITHOLD || atrid == atr_ONEXITHOLDREASON || atrid == atr_ONEXITHOLDSUBCODE ||
						atrid == atr_REMOTE_QUEUE) {

						ExprTree * def_tree = base_route_ad.Lookup(it->first);
						bool is_def_expr = (def_tree && *tree == *def_tree);

						if (!(options & XForm_ConvertJobRouter_Fix_EvalSet)) {
							// experimental flattening....
							if ( ! myrefs.empty()) {
								rhs.clear();
								unparse_special(unparser, rhs, route_ad, tree, atr_f_MyTarget | atr_f_Flatten);

								myrefs.clear();
								GetExprReferences(rhs.c_str(), route_ad, &myrefs, NULL);
							}
							evalset_cmds[attr] = rhs;
							evalset_myrefs.insert(myrefs.begin(), myrefs.end());
						} else if (atrid == atr_REQUESTMEMORY && is_def_expr) {
							//has_def_RequestMemory = true;
							set_cmds[attr] = "$(maxMemory:2000)";
						} else if ((atrid == atr_REQUESTCPUS || atrid == atr_REMOTE_NODENUMBER || atrid == atr_REMOTE_SMPGRANULARITY) &&
								   is_def_expr) {
							//has_def_RequestCpus = true;
							set_cmds[attr] = "$(xcount:1)";
						} else if ((atrid == atr_ONEXITHOLD || atrid == atr_ONEXITHOLDREASON || atrid == atr_ONEXITHOLDSUBCODE) && is_def_expr) {
							has_def_onexithold = true;
							//evalset_myrefs.insert("minWallTime");
							//evalset_cmds[attr] = rhs;
						} else if (atrid == atr_REMOTE_QUEUE && is_def_expr) {
							//has_def_remote_queue = true;
							set_cmds[attr] = "$Fq(queue)";
						} else {
							// experimental flattening....
							if ( ! myrefs.empty()) {
								rhs.clear();
								unparse_special(unparser, rhs, route_ad, tree, atr_f_MyTarget | atr_f_Flatten);

								myrefs.clear();
								GetExprReferences(rhs.c_str(), route_ad, &myrefs, NULL);
							}
							evalset_cmds[attr] = rhs;
							evalset_myrefs.insert(myrefs.begin(), myrefs.end());
						}
					} else {
						// experimental flattening....
						if ( ! myrefs.empty()) {
							rhs.clear();
							unparse_special(unparser, rhs, route_ad, tree, atr_f_MyTarget | atr_f_Flatten);

							myrefs.clear();
							GetExprReferences(rhs.c_str(), route_ad, &myrefs, NULL);
						}
						evalset_cmds[attr] = rhs;
						evalset_myrefs.insert(myrefs.begin(), myrefs.end());
					}
				}
			}
		} else {
			int atropts = 0;
			int atrid = is_interesting_route_attr(it->first, &atropts);
			switch (atrid) {
				case atr_NAME: route_ad.EvaluateAttrString( it->first, name ); break;
				case atr_TARGETUNIVERSE: route_ad.EvaluateAttrInt( it->first, target_universe ); break;

				case atr_GRIDRESOURCE: {
					route_ad.EvaluateAttrString( it->first, grid_resource );
					ExprTree * tree = route_ad.Lookup(it->first);
					if (tree) {
						rhs.clear();
						unparser.Unparse(rhs, tree);
						if ( ! rhs.empty()) { assignments[it->first] = rhs; string_assignments.insert(it->first); }
					}
				} break;

				case atr_REQUIREMENTS: {
					requirements.clear();
					ExprTree * tree = route_ad.Lookup(it->first);
					if (tree) {
						unparse_special(unparser, requirements, route_ad, tree, atropts);
					}
				} break;

				default: {
					ExprTree * tree = route_ad.Lookup(it->first);
					bool is_string = true;
					if (tree) {
						rhs.clear();
						if (atropts) {
							unparse_special(unparser, rhs, route_ad, tree, atropts);
						} else if ( ! ExprTreeIsLiteralString(tree, rhs)) {
							is_string = false;
							unparser.Unparse(rhs, tree);
						}
						if ( ! rhs.empty()) { assignments[it->first] = rhs; if (is_string) string_assignments.insert(it->first); }
					}
					switch (atrid) {
						case atr_MAXMEMORY: has_set_maxMemory = 1; break;
						case atr_DEFAULT_MAXMEMORY: if (!has_set_maxMemory) has_set_maxMemory = 2; break;

						case atr_XCOUNT: has_set_xcount = 1; break;
						case atr_DEFAULT_XCOUNT: if(!has_set_xcount) has_set_xcount = 2; break;

						case atr_QUEUE: has_set_queue = 1; break;
						case atr_DEFAULT_QUEUE: if (!has_set_queue) has_set_queue = 2; break;

						case atr_MAXWALLTIME: has_set_maxWallTime = 1; break;
						case atr_DEFAULT_MAXWALLTIME: if (!has_set_maxWallTime) has_set_maxWallTime = 2; break;

						case atr_MINWALLTIME: has_set_minWallTime = 1; break;
					}
				} break;
			}
		}
	}

	// no need to manipulate the job's OnExitHold expression if a minWallTime was not set.
	if (has_def_onexithold && (options & XForm_ConvertJobRouter_Fix_EvalSet)) {
		// we won't be needing to copy the OnExitHold reason
		copy_cmds.erase("OnExitHold"); evalset_cmds.erase("OnExitHold");
		copy_cmds.erase("OnExitHoldReason"); evalset_cmds.erase("OnExitHoldReason");
		copy_cmds.erase("OnExitHoldSubCode"); evalset_cmds.erase("OnExitHoldSubCode");
	}

	std::string buf;
	formatstr(buf, "# autoconversion of route '%s' from new-classad syntax", name.empty() ? "" : name.c_str());
	statements.append(buf.c_str());
	if ( ! name.empty()) { 
		formatstr(buf, "NAME %s", name.c_str());
		statements.append(buf.c_str());
	}
	if (target_universe) { formatstr(buf, "UNIVERSE %d", target_universe); statements.append(buf.c_str()); }
	if (!requirements.empty()) {
		formatstr(buf, "REQUIREMENTS %s", requirements.c_str());
		statements.append(buf.c_str()); 
	}

	statements.append("");
	for (STRING_MAP::iterator it = assignments.begin(); it != assignments.end(); ++it) {
		formatstr(buf, "%s = %s", it->first.c_str(), it->second.c_str());
		statements.append(buf.c_str());
	}

// evaluation order of route rules:
//1. copy_* 
//2. delete_* 
//3. set_* 
//4. eval_set_* 
	statements.append("");
	statements.append("# copy_* rules");
	for (STRING_MAP::iterator it = copy_cmds.begin(); it != copy_cmds.end(); ++it) {
		formatstr(buf, "COPY %s %s", it->first.c_str(), it->second.c_str());
		statements.append(buf.c_str());
	}

	statements.append("");
	statements.append("# delete_* rules");
	for (STRING_MAP::iterator it = delete_cmds.begin(); it != delete_cmds.end(); ++it) {
		formatstr(buf, "DELETE %s", it->first.c_str());
		statements.append(buf.c_str());
	}

	statements.append("");
	statements.append("# set_* rules");
	for (STRING_MAP::iterator it = set_cmds.begin(); it != set_cmds.end(); ++it) {
		formatstr(buf, "SET %s %s", it->first.c_str(), it->second.c_str());
		statements.append(buf.c_str());
	}

	if (has_def_onexithold && (options & XForm_ConvertJobRouter_Fix_EvalSet)) {
		// emit new boilerplate on_exit_hold
		if (has_set_minWallTime) {
			statements.append("");
			statements.append("# modify OnExitHold for minWallTime");
			statements.append("if defined MY.OnExitHold");
			statements.append("  COPY OnExitHold orig_OnExitHold");
			statements.append("  COPY OnExitHoldSubCode orig_OnExitHoldSubCode");
			statements.append("  COPY OnExitHoldReason orig_OnExitHoldReason");
			statements.append("  DEFAULT orig_OnExitHoldReason strcat(\"The on_exit_hold expression (\", unparse(orig_OnExitHold), \") evaluated to TRUE.\")");
			statements.append("  SET OnExitHoldMinWallTime ifThenElse(RemoteWallClockTime isnt undefined, RemoteWallClockTime < 60*$(minWallTime), false)");
			statements.append("  SET OnExitHoldReasonMinWallTime strcat(\"The job's wall clock time\", int(RemoteWallClockTime/60), \"min, is is less than the minimum specified by the job ($(minWallTime))\")");
			statements.append("  SET OnExitHold orig_OnExitHold || OnExitHoldMinWallTime");
			statements.append("  SET OnExitHoldSubCode ifThenElse(orig_OnExitHold, $(My.orig_OnExitHoldSubCode:1), 42)");
			statements.append("  SET OnExitHoldReason ifThenElse(orig_OnExitHold, orig_OnExitHoldReason, ifThenElse(OnExitHoldMinWallTime, OnExitHoldReasonMinWallTime, \"Job held for unknown reason.\"))");
			statements.append("else");
			statements.append("  SET OnExitHold ifThenElse(RemoteWallClockTime isnt undefined, RemoteWallClockTime < 60*$(minWallTime), false)");
			statements.append("  SET OnExitHoldSubCode 42");
			statements.append("  SET OnExitHoldReason strcat(\"The job's wall clock time\", int(RemoteWallClockTime/60), \"min, is is less than the minimum specified by the job ($(minWallTime))\")");
			statements.append("endif");
		}
	}

	// we can only do this after we have read the entire input ad.
	int cSpecial = 0;
	for (classad::References::const_iterator it = evalset_myrefs.begin(); it != evalset_myrefs.end(); ++it) {
		if (assignments.find(*it) != assignments.end() && set_cmds.find(*it) == set_cmds.end()) {
			if ( ! cSpecial) {
				statements.append("");
				statements.append("# temporarily SET attrs because eval_set_ rules refer to them");
			}
			++cSpecial;
			if (string_assignments.find(*it) != string_assignments.end()) {
				formatstr(buf, "SET %s \"$(%s)\"", it->c_str(), it->c_str());
			} else {
				formatstr(buf, "SET %s $(%s)", it->c_str(), it->c_str());
			}
			statements.append(buf.c_str());
		}
	}

	statements.append("");
	statements.append("# eval_set_* rules");
	for (STRING_MAP::iterator it = evalset_cmds.begin(); it != evalset_cmds.end(); ++it) {
		formatstr(buf, "EVALSET %s %s", it->first.c_str(), it->second.c_str());
		statements.append(buf.c_str());
	}

	if (cSpecial) {
		statements.append("");
		statements.append("# remove temporary attrs");
		for (classad::References::const_iterator it = evalset_myrefs.begin(); it != evalset_myrefs.end(); ++it) {
			if (assignments.find(*it) != assignments.end() && set_cmds.find(*it) == set_cmds.end()) {
				formatstr(buf, "DELETE %s", it->c_str());
				statements.append(buf.c_str());
			}
		}
	}

	return 1;
}

int XFormLoadFromClassadJobRouterRoute (
	MacroStreamXFormSource & xform,
	const std::string & routing_string,
	int & offset,
	const classad::ClassAd & base_route_ad,
	int options)
{
	StringList statements;
	int rval = ConvertClassadJobRouterRouteToXForm(statements, xform.getName(), routing_string, offset, base_route_ad, options);
	if (rval == 1) {
		std::string errmsg;
		auto_free_ptr xform_text(statements.print_to_delimed_string("\n"));
		int offset = 0;
		rval = xform.open(xform_text, offset, errmsg);
	}
	return rval;
}

bool ValidateXForm (
	MacroStreamXFormSource & xfm,  // the set of transform rules
	XFormHash & mset,              // the hashtable used as temporary storage
	std::string & errmsg)          // holds parse errors on failure
{
	MACRO_EVAL_CONTEXT_EX & ctx = xfm.context();
	ctx.also_in_config = true;

	const char * name = xfm.getName();
	if ( ! name) name = "";

	_parse_rules_args args = { xfm, mset, NULL, 0 };

	xfm.rewind();
	int rval = Parse_macros(xfm, 0, mset.macros(), READ_MACROS_SUBMIT_SYNTAX, &ctx, errmsg, ValidateRulesCallback, &args);
	return rval == 0;
}
