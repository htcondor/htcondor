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

#ifndef CONFIG_H
#define CONFIG_H

#if defined(__cplusplus)

#include "condor_classad.h"
#include "MyString.h"
#include "string_list.h"
#include "simplelist.h"
#include "extArray.h"

#include <vector>
#include <string>

typedef std::vector<const char *> MACRO_SOURCES;
namespace condor_params { typedef struct key_value_pair key_value_pair; }
typedef const struct condor_params::key_value_pair MACRO_DEF_ITEM;
#else // ! __cplusplus
typedef void* MACRO_SOURCES; // placeholder for use in C
typedef struct key_value_pair { const char * key; const void * def; } key_value_pair;
typedef const struct key_value_pair MACRO_DEF_ITEM;
#endif

#include "pool_allocator.h"
//#include "param_info.h"

#define PARAM_USE_COUNTING
#define CALL_VIA_MACRO_SET
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
} MACRO_SET;

// this holds context during macro lookup/expansion
typedef struct macro_eval_context {
	const char *localname;
	const char *subsys;
	int without_default;
	int use_mask;
} MACRO_EVAL_CONTEXT;


#if defined(__cplusplus)
	extern MyString global_config_source;
	extern MyString global_root_config_source;
	extern StringList local_config_sources;
	class Regex;

	// class that can be used to hold a malloc'd pointer such as the one returned by param
	// it will free the pointer when this object is destroyed.
	class auto_free_ptr {
	public:
		auto_free_ptr(char* str=NULL) : p(str) {}
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

	int param_names_matching(Regex & re, ExtArray<const char *>& names);
	int param_names_matching(Regex& re, std::vector<std::string>& names);

    bool param_defined(const char* name);
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


	double param_double(const char *name, double default_value = 0,
                        double min_value = -DBL_MAX, double max_value = DBL_MAX,
                        ClassAd *me=NULL, ClassAd *target=NULL,
						bool use_param_table = true );

	bool param_boolean_crufty( const char *name, bool default_value );

	bool param_boolean( const char *name, bool default_value,
                        bool do_log = true,
						ClassAd *me=NULL, ClassAd *target=NULL,
						bool use_param_table = true );

	char* param_with_full_path(const char *name);

	// helper function, parse and/or evaluate string and return true if it is a valid boolean
	// if it is a valid boolean, the value is returned in 'result', otherwise result is unchanged.
	bool string_is_boolean_param(const char * string, bool& result, ClassAd *me = NULL, ClassAd *target = NULL, const char * name=NULL);
	bool string_is_double_param(const char * string, double& result, ClassAd *me = NULL, ClassAd *target = NULL, const char * name=NULL, int* err_reason=NULL);
	bool string_is_long_param(const char * string, long long& result, ClassAd *me = NULL, ClassAd *target = NULL, const char * name=NULL, int* err_reason=NULL);

	const char * param_get_location(const MACRO_META * pmet, MyString & value);

	const char * param_get_info(const char * name,
								const char * subsys,
								const char * local,
								MyString &name_used,
								const char ** pdef_value,
								const MACRO_META **ppmet);

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

	// find an item in the macro_set, but do not look in the defaults table.
	// if prefix is not NULL, then "prefix.name" is looked up and "name" is NOT
	// this function does not look in the defaults (param) table.
	const char * lookup_macro_exact_no_default (const char *name, const char *prefix, MACRO_SET& set, int use=3);
	const char * lookup_macro_exact_no_default (const char *name, MACRO_SET& set, int use=3);

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

	/* A convenience function that calls param() with a MyString buffer. */
	bool param(MyString &buf,char const *param_name,char const *default_value=NULL);

	/* A convenience function that calls param() with a std::string buffer. */
	bool param(std::string &buf,char const *param_name,char const *default_value=NULL);

	/* A convenience function that calls param() then inserts items from the value into the given StringList if they are not already there */
	bool param_and_insert_unique_items(const char * param_name, StringList & items, bool case_sensitive=false);

	/* Call this after loading the config files to see if the given user has access to all of the files
	   Used when running as root to verfiy that the config files will still be accessible after we switch
	   to the condor user. returns true on success, false if access check fails
	   when false is returned, the errmsg will indicate the names of files that cannot be accessed.
	*/
	bool check_config_file_access(const char * username, class StringList &errfiles);

	bool get_config_dir_file_list( char const *dirpath, class StringList &files );

/* here we provide C linkage to C++ defined functions. This seems a bit
	odd since if a .c file includes this, these prototypes technically don't
	exist.... */
extern "C" {
	#define CONFIG_OPT_WANT_META      0x01   // also keep metdata about config
	#define CONFIG_OPT_KEEP_DEFAULTS  0x02   // keep items that match defaults
	#define CONFIG_OPT_OLD_COM_IN_CONT 0x04  // ignore # after \ (i.e. pre 8.1.3 comment/continue behavior)
	#define CONFIG_OPT_SMART_COM_IN_CONT 0x08 // parse #opt:oldcomment/newcomment to decide comment behavior
	#define CONFIG_OPT_COLON_IS_META_ONLY 0x10 // colon isn't valid for use in param assigments (only = is allowed)
	#define CONFIG_OPT_DEFAULTS_ARE_PARAM_INFO 0x80 // the defaults table is the table defined in param_info.in.
	#define CONFIG_OPT_NO_EXIT 0x100 // If a config file is missing or the config is invalid, do not abort/exit the process.
	#define CONFIG_OPT_WANT_QUIET 0x200 // Keep printing to stdout/err to a minimum
	#define CONFIG_OPT_SUBMIT_SYNTAX 0x1000 // allow +Attr and -Attr syntax like submit files do.
	bool config();
	bool config_ex(int opt);
	bool config_host(const char* host, int config_options);
	bool validate_config(bool abort_if_invalid);
	void config_dump_string_pool(FILE * fh, const char * sep);
	void config_dump_sources(FILE * fh, const char * sep);
	const char * config_source_by_id(int source_id);
	bool config_continue_if_no_config(bool contin);
	void config_fill_ad( ClassAd*, const char *prefix = NULL );
	void condor_net_remap_config( bool force_param=false );
	int param_integer_c( const char *name, int default_value,
					   int min_value, int max_value, bool use_param_table = true );
    //int  param_boolean_int( const char *name, int default_value, bool use_param_table = true );
    int  param_boolean_int_with_default( const char *name );
	int  set_persistent_config(char *admin, char *config);
	int  set_runtime_config(char *admin, char *config);
	int is_valid_param_name(const char *name);
	char * is_valid_config_assignment(const char *config);
	// this function allows tests to pretend that a param was set to a given value.	
    void  param_insert(const char * name, const char * value);
} // end extern "C"


// the HASHITER can only be defined with c++ linkage
class HASHITER {
public:
	int opts;
	int ix; int id; int is_def;
	MACRO_DEF_ITEM * pdef; // for use when default comes from per-daemon override table.
	MACRO_SET & set;
	HASHITER(MACRO_SET & setIn, int options=0) : opts(options), ix(0), id(0), is_def(0), pdef(NULL), set(setIn) {}
	HASHITER& operator=(const HASHITER& rhs) { if (this != &rhs) { memcpy(this, &rhs, sizeof(HASHITER)); } return *this; }
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
const char * hash_iter_info(HASHITER& it, int& use_count, int& ref_count, MyString& source_name, int& line_number);
const char * hash_iter_def_value(HASHITER& it);
bool param_find_item (const char * name, const char * subsys, const char * local, MyString& name_found, HASHITER& it);
void foreach_param(int options, bool (*fn)(void* user, HASHITER& it), void* user);
void foreach_param_matching(Regex & re, int options, bool (*fn)(void* user, HASHITER& it), void* user);

/*
expand_param(), expand config variables $() against the current config table and return an strdup'd string with the result
the char* return value should be freed using free()
*/
BEGIN_C_DECLS
char * expand_param (const char *str); // same as below but defaults subsys and use flags
END_C_DECLS
char * expand_param (const char *str, const char * localname, const char *subsys, int use);
inline bool expand_param (const char *str, std::string & expanded) {
	char * p = expand_param(str);
	if (!p) return false;
	expanded = p;
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

extern "C" {

#if 1
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
#else

	/** Find next $(MACRO) or $$(MACRO) in a string

	The caller is expected to concatenate *leftp, the evaluated
	*namep, then *rightp to get the expanded setting.

	- value - The null-terminated string to scan. WILL BE MODIFIED!

	- leftp - OUTPUT. *leftp will be set to value+search_pos.  It
	  will be null terminated at the $ for the first $(MACRO) found.

	- namep - OUTPUT. The name of the MACRO (the bit between the
	  parenthesis).  Pointer into value.  Null terminated at the
	  closing parenthesis.

	- rightp - OUTPUT. Everything to the right of the $(MACRO).
	  Pointer into value.

	- self - Default to null. If non-null, only macros whose name is
	  identical to self will be expanded. (Used for the special
	  $(DOLLAR) case?)

	- getdollardollar - Defaults false. If true, scans for $$(MACRO)
	  and $$([expression]) instead of $(MACRO)

	- search_pos - 0-indexed position in value to start scanning at.
	  Defaults to 0.
	*/
	int find_config_macro( register char *value, register char **leftp,
		register char **namep, register char **rightp,
		const char *self=NULL, bool getdollardollar=false, int search_pos=0);
#endif

	void init_config (int options);
}

#endif // __cplusplus

BEGIN_C_DECLS

	char * get_tilde(void);
	char * param ( const char *name );

	/** Insert a value into a hash table.

	The value of 'value' is associated with the name 'name'.  This is
	inserted into the table 'table' which is 'table_size' big.
	insert keeps copies of the name and value.
	*/
#ifdef __cplusplus
	typedef struct macro_source { bool is_inside; bool is_command; short int id; int line; short int meta_id; short int meta_off; } MACRO_SOURCE;
	void insert_source(const char * filename, MACRO_SET& macro_set, MACRO_SOURCE & source);
	extern const MACRO_SOURCE EnvMacro;
	extern const MACRO_SOURCE WireMacro;
	extern const MACRO_SOURCE DetectedMacro;

	void insert_macro (const char *name, const char *value, MACRO_SET& macro_set, const MACRO_SOURCE & source, MACRO_EVAL_CONTEXT & ctx);
	inline const char * macro_source_filename(MACRO_SOURCE& source, MACRO_SET& set) { return set.sources[source.id]; }
	
	/** Sets the whether or not a macro has actually been used
	*/
	int increment_macro_use_count (const char *name, MACRO_SET& macro_set);
	void clear_macro_use_count (const char *name, MACRO_SET& macro_set);
	int get_macro_use_count (const char *name, MACRO_SET& macro_set);
	int get_macro_ref_count (const char *name, MACRO_SET& macro_set);
	bool config_test_if_expression(const char * expr, bool & result, const char * localname, const char * subsys, std::string & err_reason);

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
		FILE* conf_fp,
		MACRO_SOURCE& FileMacro,
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

#endif // __cplusplus


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
	void clear_config ( void );
	void set_debug_flags( const char * strFlags, int flags );
	void config_insert( const char* attrName, const char* attrValue);
	int  param_boolean_int( const char *name, int default_value );
	int  param_boolean_int_with_default( const char* name );
	int  param_boolean_int_without_default( const char* name, int default_value );
	
	// Process an additional chunk of file
	void process_config_source(const char* filename, int depth, const char* sourcename, const char* host, int required);


/* This function initialize GSI (maybe other) authentication related
   stuff Daemons that should use the condor daemon credentials should
   set the argument is_daemon=true.  This function is automatically
   called at config init time with is_daemon=false, so that all
   processes get the basic auth config.  The order of calls to this
   function do not matter, as the results are only additive.
   Therefore, calling with is_daemon=false and then with
   is_daemon=true or vice versa are equivalent.
*/
void condor_auth_config(int is_daemon);

END_C_DECLS


#endif /* CONFIG_H */

