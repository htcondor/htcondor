/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#ifndef _CONDOR_CONFIG_H
#define _CONDOR_CONFIG_H

#include "condor_classad.h"

#include <vector>
#include <string>
#include <limits>

typedef std::vector<const char *> MACRO_SOURCES;
class CondorError;
namespace condor_params { typedef struct key_value_pair key_value_pair; }
typedef const struct condor_params::key_value_pair MACRO_DEF_ITEM;

#include "pool_allocator.h"
//#include "param_info.h"

#define PARAM_USE_COUNTING
#define CALL_VIA_MACRO_SET
#define USE_MACRO_STREAMS 1
#define MACRO_SET_KNOWS_DEFAULT
#define COLON_DEFAULT_FOR_MACRO_EXPAND  // enable $(FOO:default-value) and $(FOO:$(OTHER)) for config
#define WARN_COLON_FOR_PARAM_ASSIGN   0 // parameter assigment with : (instead of =) is disallowed, a value of 0 is warn, 1 is fail
#define CONFIG_MAX_NESTING_DEPTH 20     // the maximum depth of nesting/recursion for include & meta

// structures for param/submit macro storage
// These structures are carefully tuned to allow for minimal private memory
// use when param metadata is disabled.
//
typedef struct macro_item {
	const char * key;
	const char * raw_value;
} MACRO_ITEM;
typedef struct macro_meta {
	short int    param_id;
	short int    index;
	union {
	  int        flags; // all of the below flags
	  struct {
	    unsigned matches_default :1;
	    unsigned inside          :1;
	    unsigned param_table     :1;
	    unsigned multi_line      :1;
	    unsigned live            :1;
	    unsigned checkpointed    :1;
	  };
	};
	short int    source_id;    // index into MACRO_SOURCES table
	short int    source_line;  // line number for files, param.in entry for internal
	short int    source_meta_id;   // metaknob id
	short int    source_meta_off;  // statement offset within metaknob   (i.e. 0 based line number)
	short int    use_count;
	short int    ref_count;
} MACRO_META;
typedef struct macro_defaults {
	int size;
	MACRO_DEF_ITEM * table; // points to const table[size] key/default-value pairs
	struct META {
		short int use_count;
		short int ref_count;
	} * metat; // optional, points to metat[size] of use counts parallel to table[]
} MACRO_DEFAULTS;
// this holds table and tablesize for use passing to the config functions.
typedef struct macro_set {
	int       size;
	int       allocation_size;
	int       options; // use CONFIG_OPT_xxx flags OR'd together
	int       sorted;  // number of items in table which are sorted
	MACRO_ITEM *table;
	MACRO_META *metat; // optional array of metadata, is parallel to table
	ALLOCATION_POOL apool;
	MACRO_SOURCES sources;
	MACRO_DEFAULTS * defaults; // optional reference to const defaults table
	CondorError * errors; // optional error stack, if non NULL, use instead of fprintf to stderr

	// fprintf an error if the above errors field is NULL, otherwise format an error and add it to the above errorstack
	// the preface is printed with fprintf but not with the errors stack.
	void push_error(FILE * fh, int code, const char* preface, const char* format, ... ) CHECK_PRINTF_FORMAT(5,6);
	void initialize(int opts);
} MACRO_SET;

// Used as the header for a MACRO_SET checkpoint, the actual allocation is larger than this.
typedef struct macro_set_checkpoint_hdr {
	int cSources;
	int cTable;
	int cMetaTable;
	int spare;
} MACRO_SET_CHECKPOINT_HDR;

// this holds context during macro lookup/expansion
typedef struct macro_eval_context {
	const char *localname;
	const char *subsys;  // default subsys prefix
	const char *cwd;     // current working directory, used for $F macro expansion
	char without_default;
	char use_mask;
	char also_in_config; // also do lookups in the config hashtable (used by submit)
	char is_context_ex;

	void init(const char * sub, char mask=2) {
		memset(this, 0, sizeof(*this));
		this->subsys = sub;
		this->use_mask = mask;
	}
} MACRO_EVAL_CONTEXT;

typedef struct macro_eval_context_ex : macro_eval_context {
	// to do lookups of last resort into the given Ad, set these. they are useually null.
	const char *adname; // name prefix for lookups into the ad
	const ClassAd * ad; // classad for lookups
	void init(const char * sub, char mask=2) {
		memset(this, 0, sizeof(*this));
		this->subsys = sub;
		this->use_mask = mask;
		this->is_context_ex = true;
	}
} MACRO_EVAL_CONTEXT_EX;


	extern std::string global_config_source;
	extern std::vector<std::string> local_config_sources;
	class Regex;

	// class that can be used to hold a malloc'd pointer such as the one returned by param
	// it will free the pointer when this object is destroyed.
	// The intended use is:
	//   auto_free_ptr value(param("param_name"));
	//   if (value) { dprintf(D_ALWAYS, "param_name has value %s\n", value.ptr()); }
	//
	// NOTE: it is NOT SAFE to use this class as a member of a class or struct that you intend to copy.
	//   This class has minimal support for copy construction/assigment using the swap() idiom, which necessary
	//   for populating STL containers. but it does NOT support deep copying or reference counting
	//   which would be needed to support its use as a member in a class that you intend to copy and keep
	//   both copies around. 
	class auto_free_ptr {
	public:
		auto_free_ptr(char* str=NULL) : p(str) {}
		friend void swap(auto_free_ptr& first, auto_free_ptr& second) { char*t = first.p; first.p = second.p; second.p = t; }
		auto_free_ptr(const auto_free_ptr& that) { if (that.p) p = strdup(that.p); else p = nullptr; }
		auto_free_ptr & operator=(auto_free_ptr that) { swap(*this, that); return *this; } // swap on assigment.
		~auto_free_ptr() { clear(); }
		void set(char*str) { clear(); p = str; }   // set a new pointer, freeing the old pointer (if any)
		void clear() { if (p) free(p); p = NULL; } // free the pointer if any
		bool empty() { return ! (p && p[0]); }     // return true if there is some data, NULL and "" are both empty
		char * detach() { char * t = p; p = NULL; return t; } // get the pointer, and remove it from this class without freeing it
		char * ptr() { return p; }                 // get the pointer, may return NULL if no pointer
		operator const char *() const { return const_cast<const char*>(p); } // get this pointer as type const char*
		operator bool() const { return p!=NULL; }  // eval to true if there is a pointer, false if not.
	private:
		char * p;
	};

	int param_names_matching(Regex& re, std::vector<std::string>& names);
	union _param_names_sumy_key { int64_t all; struct { short int iter; short int off; short int line; short int sid; }; };
	int param_names_for_summary(std::map<int64_t, std::string>& names);
	const short int summary_env_source_id = 0x7FFE;
	const short int summary_wire_source_id = 0x7FFF;

	bool param_defined(const char* name);
	// Does not check if the expanded parameter is nonempty, only that
	// the parameter was assigned a value by something other than the
	// param table.
	bool param_defined_by_config(const char* name);
	const char * param_unexpanded(const char *name);
	char* param_or_except( const char *name );
	int param_integer( const char *name, int default_value = 0,
					   int min_value = INT_MIN, int max_value = INT_MAX, bool use_param_table = true );
	// Alternate param_integer():
	bool param_integer( const char *name, int &value,
						bool use_default, int default_value,
						bool check_ranges = true,
						int min_value = INT_MIN, int max_value = INT_MAX,
						ClassAd *me=NULL, ClassAd *target=NULL,
						bool use_param_table = true );

	bool param_longlong( const char *name, long long int &value,
						bool use_default, long long default_value,
						bool check_ranges = true,
						long long min_value = (std::numeric_limits<long long>::min)(),
						long long max_value = (std::numeric_limits<long long>::max)(),
						ClassAd *me=NULL, ClassAd *target=NULL,
						bool use_param_table = true );


	double param_double(const char *name, double default_value = 0,
						double min_value = -DBL_MAX, double max_value = DBL_MAX,
						ClassAd *me=NULL, ClassAd *target=NULL,
						bool use_param_table = true );

	bool param_boolean_crufty( const char *name, bool default_value );

	bool param_boolean( const char *name, bool default_value,
						bool do_log = true,
						ClassAd *me=NULL, ClassAd *target=NULL,
						bool use_param_table = true );

	// get the raw default value from the param table.
	// if the param table has a subsys or localname override that is fetched instead.
	// note that this does *not* return a macro expanded value. it is the raw param table value
	const char * param_raw_default(const char *name);

	char* param_with_full_path(const char *name);

	// helper function, parse and/or evaluate string and return true if it is a valid boolean
	// if it is a valid boolean, the value is returned in 'result', otherwise result is unchanged.
	bool string_is_boolean_param(const char * string, bool& result, ClassAd *me = NULL, ClassAd *target = NULL, const char * name=NULL);
	bool string_is_double_param(const char * string, double& result, ClassAd *me = NULL, ClassAd *target = NULL, const char * name=NULL, int* err_reason=NULL);
	bool string_is_long_param(const char * string, long long& result, ClassAd *me = NULL, ClassAd *target = NULL, const char * name=NULL, int* err_reason=NULL);

	// A convenience function for use with trinary parameters.
	bool param_true( const char * name );

	// A convenience function for use with trinary parameters.
	bool param_false( const char * name );

	const char * param_append_location(const MACRO_META * pmet, std::string & value);
	const char * param_get_location(const MACRO_META * pmet, std::string & value);

	const char * param_get_info(const char * name,
								const char * subsys,
								const char * local,
								std::string & name_used,
								const char ** pdef_value,
								const MACRO_META **ppmet);

	MACRO_SET * param_get_macro_set();

	// lookup "name" in ALL of relevant tables as indicated by the MACRO_EVAL_CONTEXT.
	// as soon as an item is found (even an empty item) lookup stops.  the lookup order is
	//    localname.name in MACRO_SET
	//    localname.name in MACRO_SET.defaults (if defaults is not null and context permits)
	//    subsys.name in MACRO_SET
	//    subsys.name in MACRO_SET.defaults  (if defaults is not null and context permits)
	//    name in MACRO_SET
	//    name in MACRO_SET.defaults  (if defaults is not null and context permits)
	// returns:
	//   NULL         if the macro does not exist in any of the tables
	//   ""           if the macro exists but was not given a value.
	//   otherwise the return value is a pointer to const memory owned by the MACRO_SET
	//   it will be valid until the MACRO_SET is cleared (usually a reconfig)
	//
	const char * lookup_macro(const char * name, MACRO_SET& set, MACRO_EVAL_CONTEXT &ctx);
	const char * lookup_macro_exact_no_default_impl(const char *name, MACRO_SET & set, int use = 3);

	// lookup "name" in the defaults table of the MACRO_SET
	// using the localname and subsys overrides of MACRO_EVAL_CONTEXT take precedence
	// returns:
	//   NULL         if the macro does not exist in the defaults table
	//   ""           if the macro exists but was not given a value.
	//   otherwise the return value is a pointer to static const memory in the TEXT segment
	//   it will be always be valid
	const char * lookup_macro_default(const char * name, MACRO_SET & macro_set, MACRO_EVAL_CONTEXT & ctx);

	// find an item in the macro_set, but do not look in the defaults table.
	// if prefix is not NULL, then "prefix.name" is looked up and "name" is NOT
	// this function does not look in the defaults (param) table.

	// If you don't care about the difference between unset and empty.
	std::string lookup_macro_exact_no_default( const std::string & name,
		MACRO_SET & set, int use = 3 );

	// Returns false if the macro is unset.
	bool exists_macro_exact_no_default( const std::string & name,
		MACRO_SET & set, int use = 3 );

	// Expand parameter references of the form "left$(middle)right".
	// handle multiple and or nested references.
	// Also expand references of the form "left$ENV(middle)right",
	// replacing $ENV(middle) with getenv(middle).
	// Also expands other $<func>() references such as $INT and $RANDOM_CHOICE
	//
	// value is the string to be expanded
	// macro_set is a hash table (managed with insert() and lookup_macro)
	// ctx   holds arguments that will be passed on to lookup_macro as needed.
	//
	// Returns malloc()ed memory; caller is responsible for calling free().
	char * expand_macro(const char * value, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx);
	// expand only $(self) and $<function>(self), if ctx.subsys and/or ctx.localname is set
	// then $(subsys.self) and/or $(localname.self) is also expanded.
	char * expand_self_macro(const char *value, const char *self, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx);

	// do $() macro expansion in-place in a std:string. This function is more efficient than the the above
	// it takes option flags that influence the expansion
	#define EXPAND_MACRO_OPT_KEEP_DOLLARDOLLAR 0x0001 // don't expand $(DOLLAR)
	#define EXPAND_MACRO_OPT_IS_PATH           0x0002 // fixup path separators (remove extras, fix for windows)
	// Returns: a bit mask of macros that expanded to non-empty values
	//          where bit 0 is the first macro in the input, etc.
	unsigned int expand_macro(std::string &value, unsigned int options, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx);

	// do macro expansion in-place in a std::string, expanding only macros not in the skip list
	// returns the number of macros that were skipped.
	// used by submit_utils to selectively expand submit hash keys when creating the submit digest
	unsigned int selective_expand_macro (std::string &value, classad::References & skip_knobs, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx);

	// do macro expansion in-place in a std::string, expanding only macros that are defined in the given macro table
	// returns the number of $() and $func() patterns that were skipped.
	unsigned int expand_defined_macros (std::string &value, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx);

	// do macro expansion in-place in a std::string, expanding only macros that are defined in the config
	// returns the number of $() and $func() patterns that were skipped
	// used by submit_utils to selectively submit templates against the config at load time
	unsigned int expand_defined_config_macros (std::string &value);

	// this is the lowest level primative to doing a lookup in the macro set.
	// it looks ONLY for an exact match of "name" in the given macro set and does
	// not look in the defaults (param) table.
	MACRO_ITEM* find_macro_item (const char *name, const char * prefix, MACRO_SET& set);

	// lookup the macro name ONLY in the given subsys defaults table. subsys may not be null
	const MACRO_DEF_ITEM * find_macro_subsys_def_item(const char * name, const char * subsys, MACRO_SET & set, int use);
	// do an exact match lookup for the given name looking only in the defaults (param) table.
	// if the name is "prefix.name" then it will look for "name" in the "prefix" defaults table
	// OR "prefix.name" in the main defaults table.
	const MACRO_DEF_ITEM * find_macro_def_item(const char * name, MACRO_SET & set, int use);

	void optimize_macros(MACRO_SET& macro_set);

	/* A convenience function that calls param() with a std::string buffer. */
	bool param(std::string &buf, char const *param_name, char const *default_value=NULL);

	/* A convenience function that evaluates a config parameter to a string. */
	bool param_eval_string(std::string &buf, const char *param_name, const char *default_value=NULL,
		classad::ClassAd *me=NULL, classad::ClassAd *target=NULL);

	/* A convenience function that calls param() then inserts items from the value into the given string list if they are not already there */
	bool param_and_insert_unique_items(const char * param_name, std::vector<std::string> & items, bool case_sensitive=false);

	/*  A convenience function that calls param() then inserts items from the value
		into the given classad:References set.  Useful whenever a param knob contains
		a string list of ClassAd attribute names, e.g. IMMUTABLE_JOB_ATTRS.
		Return true if given param name was found, false if not. */
	bool param_and_insert_attrs(const char * param_name, classad::References & attrs);

	/* Call this after loading the config files to see if the given user has access to all of the files
	   Used when running as root to verfiy that the config files will still be accessible after we switch
	   to the condor user. returns true on success, false if access check fails
	   when false is returned, the errmsg will indicate the names of files that cannot be accessed.
	*/
	bool check_config_file_access(const char * username, std::vector<std::string> &errfiles);

	bool get_config_dir_file_list( char const *dirpath, std::vector<std::string> &files );

	#define CONFIG_OPT_WANT_META      0x01   // also keep metdata about config
	#define CONFIG_OPT_KEEP_DEFAULTS  0x02   // keep items that match defaults
	#define CONFIG_OPT_OLD_COM_IN_CONT 0x04  // ignore # after \ (i.e. pre 8.1.3 comment/continue behavior)
	#define CONFIG_OPT_SMART_COM_IN_CONT 0x08 // parse #opt:oldcomment/newcomment to decide comment behavior
	#define CONFIG_OPT_COLON_IS_META_ONLY 0x10 // colon isn't valid for use in param assigments (only = is allowed)
	#define CONFIG_OPT_NO_SMART_AUTO_USE  0x20 // ignore SMART_AUTO_USE_* knobs, default is to process them for CONFIG but not SUBMIT
	#define CONFIG_OPT_DEFAULTS_ARE_PARAM_INFO 0x80 // the defaults table is the table defined in param_info.in.
	#define CONFIG_OPT_NO_EXIT 0x100 // If a config file is missing or the config is invalid, do not abort/exit the process.
	#define CONFIG_OPT_WANT_QUIET 0x200 // Keep printing to stdout/err to a minimum
	#define CONFIG_OPT_DEPRECATION_WARNINGS 0x400 // warn about obsolete syntax/elements
	#define CONFIG_OPT_USE_THIS_ROOT_CONFIG 0x800 // use the root config file specified in the last argument of real_config
	#define CONFIG_OPT_SUBMIT_SYNTAX 0x1000 // allow +Attr and -Attr syntax like submit files do.
	#define CONFIG_OPT_NO_INCLUDE_FILE 0x2000 // don't allow includes from files (late materialization)
	bool config();
	int set_priv_initialize(void); // duplicated here for 8.8.0 to minimize code churn. actual function is in uids.cpp
	bool config_ex(int opt);
	bool config_host(const char* host, int config_options, const char * root_config); // used by condor_config_val
	bool validate_config(bool abort_if_invalid, int opt);
	void config_dump_string_pool(FILE * fh, const char * sep);
	void config_dump_sources(FILE * fh, const char * sep);
	const char * config_source_by_id(int source_id);
	bool config_continue_if_no_config(bool contin);
	void config_fill_ad( ClassAd*, const char *prefix = NULL );
	void condor_net_remap_config( bool force_param=false );
	int  set_persistent_config(char *admin, char *config);
	int  set_runtime_config(char *admin, char *config);
	int is_valid_param_name(const char *name);
	char * is_valid_config_assignment(const char *config);
	// this function allows tests to pretend that a param was set to a given value. but it leaks memory if used frequently
    void  param_insert(const char * name, const char * value);
	// this function allows tests to set the actual backend data for a param value and returns the old value.
	// make sure that live_value stays in scope until you put the old value back
	const char * set_live_param_value(const char * name, const char * live_value);
		// Find a file associated with a user; by default, this fails if called in a context
		// where can_switch_ids() is true; set daemon_ok = false if calling this from a root-level
		// condor.
	bool find_user_file(std::string & filename, const char * basename, bool check_access, bool daemon_ok);


// the HASHITER can only be defined with c++ linkage
class HASHITER {
public:
	int opts;
	int ix; int id; int is_def;
	MACRO_DEF_ITEM * pdef; // for use when default comes from per-daemon override table.
	MACRO_SET & set;
	HASHITER(MACRO_SET & setIn, int options=0) : opts(options), ix(0), id(0), is_def(0), pdef(NULL), set(setIn) {}
	HASHITER( const HASHITER & rhs) :
	opts(rhs.opts), ix(rhs.ix), is_def(rhs.is_def), pdef(rhs.pdef), set(rhs.set)
	{ }
	HASHITER & operator =( const HASHITER & rhs ) {
		if( this != & rhs ) {
			this->opts = rhs.opts;
			this->ix = rhs.ix;
			this->id = rhs.id;
			this->is_def = rhs.is_def;
			this->pdef = rhs.pdef;
			this->set = rhs.set;
		}
		return * this;
	}
};
enum { HASHITER_NO_DEFAULTS=1, HASHITER_USED_DEFAULTS=2, HASHITER_USED=4, HASHITER_SHOW_DUPS=8 };
inline HASHITER hash_iter_begin(MACRO_SET & set, int options=0) { return HASHITER(set,options); }
inline void hash_iter_delete(HASHITER*) {}
bool hash_iter_done(HASHITER& it);
bool hash_iter_next(HASHITER& it);
const char * hash_iter_key(HASHITER& it);
const char * hash_iter_value(HASHITER& it);
int hash_iter_used_value(HASHITER& it);
MACRO_META * hash_iter_meta(HASHITER& it);
const char * hash_iter_info(HASHITER& it, int& use_count, int& ref_count, std::string& source_name, int& line_number);
const char * hash_iter_def_value(HASHITER& it);
bool param_find_item (const char * name, const char * subsys, const char * local, std::string& name_found, HASHITER& it);
void foreach_param(int options, bool (*fn)(void* user, HASHITER& it), void* user);
void foreach_param_matching(Regex & re, int options, bool (*fn)(void* user, HASHITER& it), void* user);

/*
expand_param(), expand config variables $() against the current config table and return an strdup'd string with the result
the char* return value should be freed using free()
*/
char * expand_param (const char *str); // same as below but defaults subsys and use flags
char * expand_param (const char *str, const char * localname, const char *subsys, int use);
inline bool expand_param (const char *str, std::string & expanded) {
	char * p = expand_param(str);
	if (!p) return false;
	expanded = p;
	free(p);
	return true;
}

// Write out a config file of values from the param table.
// Returns 0 on success and -1 on failure.
#define WRITE_MACRO_OPT_DEFAULT_VALUES  0x01 // include default values
#define WRITE_MACRO_OPT_SELDOM_VALUES   0x02 // include default values marked 'seldom'
#define WRITE_MACRO_OPT_FUTURE_VALUES   0x04 // include default values marked 'future'
#define WRITE_MACRO_OPT_DEVLOPER_VALUES 0x08 // include default values marked 'developer'
#define WRITE_MACRO_OPT_EXPAND          0x10 // write expanded values
#define WRITE_MACRO_OPT_SOURCE_COMMENT  0x20 // include comments showing source of values
int write_macros_to_file(const char* pathname, MACRO_SET& macro_set, int options);
int write_config_file(const char* pathname, int options);

	/** Find next $$(MACRO) or $$([expression]) in value
		search begins at pos and continues to terminating null

	- value - The null-terminated string to scan. WILL BE MODIFIED!

	- pos - 0-indexed position in value to start scanning at.

	- left - OUTPUT. *leftp will be set to value+search_pos.  It
	  will be null terminated at the first $ for the next $$(MACRO) found.

	- name - OUTPUT. The name of the MACRO (the bit between the
	  parenthesis).  Pointer into value.  Null terminated at the
	  closing parenthesis.

	- right - OUTPUT. Everything to the right of the close paren for the $$(MACRO).
	  Pointer into value.

	returns non-zero if a $$() is found, zero if not found.
	*/
	int next_dollardollar_macro(char * value, int pos, char** left, char** name, char** right);

	void init_global_config_table(int options);
	void clear_global_config_table(void);

	char * get_tilde(void);
	char * param ( const char *name );
	// do param lookup with explicit subsys and localname
	char * param_with_context ( const char *name, const char *subsys, const char *localname, const char * cwd );

	// do param lookup with explicit subsys and localname
	char * param_ctx (const char *name, MACRO_EVAL_CONTEXT & ctx);

	// config source file info
	typedef struct macro_source { bool is_inside; bool is_command; short int id; int line; short int meta_id; short int meta_off; } MACRO_SOURCE;
	void insert_source(const char * filename, MACRO_SET& macro_set, MACRO_SOURCE & source);
	void insert_special_sources(MACRO_SET& macro_set);
	extern const MACRO_SOURCE EnvMacro;
	extern const MACRO_SOURCE WireMacro;
	extern const MACRO_SOURCE DetectedMacro;

	void insert_macro (const char *name, const char *value, MACRO_SET& macro_set, const MACRO_SOURCE & source, MACRO_EVAL_CONTEXT & ctx, bool is_herefile=false);
	inline const char * macro_source_filename(MACRO_SOURCE& source, MACRO_SET& set) { return set.sources[source.id]; }
	
	/** Sets the whether or not a macro has actually been used
	*/
	int increment_macro_use_count (const char *name, MACRO_SET& macro_set);
	void clear_macro_use_count (const char *name, MACRO_SET& macro_set);
	int get_macro_use_count (const char *name, MACRO_SET& macro_set);
	int get_macro_ref_count (const char *name, MACRO_SET& macro_set);
	bool config_test_if_expression(const char * expr, bool & result, const char * localname, const char * subsys, std::string & err_reason);

	// simple class to split metaknob name from arguments and store the result in public member variables
	class MetaKnobAndArgs {
	public:
		std::string knob;
		std::string args;
		std::string extra;
		MetaKnobAndArgs(const char * p=NULL) { if (p) init_from_string(p); }
		// set member variables by parsing p, returns pointer to the next metaknob name or NULL
		// if there are no characters after. leading & trailing whitespace and , are skipped.
		const char* init_from_string(const char * p);
	};

	// expand $(N) from value against arguments in argstr
	// returns a malloc()'d ptr to expanded value, caller is responsible from freeing returned pointer.
	char * expand_meta_args(const char *value, std::string & argstr);

	// macro stream class wraps up an fp or string so that the macro parser can read from either.
	//
	class MacroStream {
	public:
		virtual ~MacroStream() {};
		virtual char * getline(int gl_opt) = 0;
		virtual MACRO_SOURCE& source() = 0;
		virtual const char * source_name(MACRO_SET &set) = 0;
	};

	// A MacroStream that uses, but does not own a FILE* and MACRO_SOURCE
	class MacroStreamYourFile : public MacroStream {
	public:
		MacroStreamYourFile(FILE * _fp, MACRO_SOURCE& _src) : fp(_fp), src(&_src) {}
		virtual ~MacroStreamYourFile() { fp = NULL; src = NULL; }
		virtual char * getline(int gl_opt);
		virtual MACRO_SOURCE& source() { return *src; }
		virtual const char * source_name(MACRO_SET&set) {
			if (src && src->id >= 0 && src->id < (int)set.sources.size())
				return set.sources[src->id];
			return "file";
		}
		void set(FILE* _fp, MACRO_SOURCE& _src) { fp =  _fp; src = &_src; }
		void reset() { fp = NULL; src = NULL; }
	protected:
		FILE * fp;
		MACRO_SOURCE * src;
	};

	// A MacroStream that owns the FILE* and MACRO_SOURCE
	class MacroStreamFile : public MacroStream {
	public:
		MacroStreamFile() : fp(NULL) { memset(&src, 0, sizeof(src)); }
		virtual ~MacroStreamFile() { if (fp) fclose(fp); fp = NULL; memset(&src, 0, sizeof(src)); }
		virtual char * getline(int gl_opt);
		virtual MACRO_SOURCE& source() { return src; }
		virtual const char * source_name(MACRO_SET&set) {
			if (src.id >= 0 && src.id < (int)set.sources.size())
				return set.sources[src.id];
			return "file";
		}
		bool open(const char * filename, bool is_command, MACRO_SET& set, std::string &errmsg);
		int  close(MACRO_SET& set, int parsing_return_val);
	protected:
		FILE * fp;
		MACRO_SOURCE src;
	};

	// A MacroStream that parses memory in exactly the same way it would parse a FILE*
	class MacroStreamMemoryFile : public MacroStream {
	public:
		MacroStreamMemoryFile(const char * _fp, ssize_t _cb, MACRO_SOURCE& _src) : ls(_fp, _cb, 0), src(&_src) {}
		virtual ~MacroStreamMemoryFile() { ls.clear(); }
		virtual char * getline(int gl_opt);
		virtual MACRO_SOURCE& source() { return *src; }
		virtual const char * source_name(MACRO_SET&set) {
			if (src && src->id >= 0 && src->id < (int)set.sources.size())
				return set.sources[src->id];
			return "memory";
		}
		void set(const char* _fp, ssize_t _cb, size_t _ix, MACRO_SOURCE& _src) { ls.init(_fp, _cb, _ix); src = &_src; }
		void reset() { ls.clear(); src = NULL; }
		// return a pointer to the part of the memory buffer that has not yet been read
		// the buffer must be null terminated, so this can be used as a string.
		const char * remainder(size_t & cb) {
			if ( ! ls.at_eof()) {
				cb = ls.cb - ls.ix;
				return ls.str+ls.ix;
			} else {
				cb = 0;
				return NULL;
			}
		}
		// save and restore the 'have read up to' position
		void save_pos(size_t & saved_ix, int & saved_line) { saved_ix = ls.ix; saved_line = src ? src->line : 0; }
		void rewind_to(size_t saved_ix, int saved_line) { ls.ix = saved_ix; if (src) src->line = saved_line; }

		// this class allows for template expansion of getline_implementation
		class LineSource {
		public:
			const char* str;
			ssize_t cb;
			size_t ix;

			LineSource(const char*p, ssize_t _cb, size_t _ix) : str(p), cb(_cb), ix(_ix) {}
			~LineSource() {}

			void init(const char*p, ssize_t _cb, size_t _ix) { str = p; cb = _cb; ix = _ix; }
			void clear() { str= NULL; cb = 0; ix = 0; }
			int at_eof();
			char *readline(char *s, int cb); // returns up to \n, for use only by getline_implementation
		};

	protected:
		LineSource ls;
		MACRO_SOURCE * src;
	};

	// A MacroStream that parses memory but does not support line continuation like MacroStreamMemoryFile does 
	// used by transforms
	class MacroStreamCharSource : public MacroStream {
	public:
		MacroStreamCharSource() 
			: input(NULL)
			, cbBufAlloc(0)
			, file_string(NULL)
		{
			memset(&src, 0, sizeof(src));
		}
		virtual ~MacroStreamCharSource() { if (input) delete input; input = NULL; }
		virtual char * getline(int gl_opt);
		virtual MACRO_SOURCE& source() { return src; }
		virtual const char * source_name(MACRO_SET&set) {
			if (src.id >= 0 && src.id < (int)set.sources.size())
				return set.sources[src.id];
			return "param";
		}
		bool open(const char * src_string, const MACRO_SOURCE & _src);
		int  close(MACRO_SET& set, int parsing_return_val);
		int  load(FILE* fp, MACRO_SOURCE & _src, bool preserve_linenumbers = false);
		void rewind();
	protected:
		StringTokenIterator * input;
		MACRO_SOURCE src;
		size_t cbBufAlloc;
		auto_free_ptr line_buf;
		auto_free_ptr file_string; // holds file content when load is called.
	private:
		// copy construction and assignment are not permitted.
		MacroStreamCharSource(const MacroStreamCharSource&);
		MacroStreamCharSource & operator=(MacroStreamCharSource that); 
	};

	// this must be C++ linkage because condor_string already has a c linkage function by this name.
	char * getline_trim(MacroStream & ms, int mode=0); // see condor_string.h for mode values

	// populate a MACRO_SET from either a config file or a submit file.
	#define READ_MACROS_SUBMIT_SYNTAX           0x01
	#define READ_MACROS_EXPAND_IMMEDIATE        0x02
	//#define READ_MACROS_CHECK_RUNTIME_SECURITY  0x04

	// read a file and populate the macro_set
	// This function is used both for reading config file and for parsing the submit file.
	//
	// when parsing the submit file, supply a fnParse function, this function will handle
	// lines that begin with + or - or any line that doesn't look like a valid key=value
	// the parse function should return 0 to continue parsing, and non-zero to abort
		/*
	int Read_macros(
		const char* source,
		int depth, // a simple recursion detector
		MACRO_SET& macro_set,
		int options, // zero or more of READ_MACROS_* flags
		const char * subsys,
		std::string & errmsg,
		int (*fnParse)(void* pv, MACRO_SOURCE& source, MACRO_SET& set, const char * line, std::string & errmsg),
		void * pvParseData);
		*/

	int Parse_config_string(MACRO_SOURCE& source, int depth, const char * config, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx);

	int
	Parse_macros(
		MacroStream& ms,
		int depth, // a simple recursion detector
		MACRO_SET& macro_set,
		int options,
		MACRO_EVAL_CONTEXT * pctx,
		std::string& config_errmsg,
		int (*fnSubmit)(void* pv, MACRO_SOURCE& source, MACRO_SET& set, char * line, std::string & errmsg),
		void * pvSubmitData);

	FILE* Open_macro_source (
		MACRO_SOURCE& macro_source,
		const char* source,
		bool        source_is_command,
		MACRO_SET& macro_set,
		std::string & errmsg);

	int Close_macro_source(FILE* conf_fp, MACRO_SOURCE& source, MACRO_SET& macro_set, int parsing_return_val);


	struct _macro_stats {
		int cbStrings;
		int cbTables;
		int cbFree;
		int cEntries;
		int cSorted;
		int cFiles;
		int cUsed;
		int cReferenced;
	};
	int  get_config_stats(struct _macro_stats *pstats);
	void set_debug_flags( const char * strFlags, int flags );
	void config_insert( const char* attrName, const char* attrValue);
	
	bool is_piped_command(const char* filename);
	// Process an additional chunk of file
	void process_config_source(const char* filename, int depth, const char* sourcename, const char* host, int required);

#endif /* _CONDOR_CONFIG_H */

