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
typedef struct key_value_pair { const char * key; const void * def} key_value_pair;
typedef const struct key_value_pair MACRO_DEF_ITEM;
#endif

#include "pool_allocator.h"
//#include "param_info.h"

#define PARAM_USE_COUNTING
#define CALL_VIA_MACRO_SET
#define MACRO_SET_KNOWS_DEFAULT
#define COLON_DEFAULT_FOR_MACRO_EXPAND // enable $(FOO:default-value) and $(FOO:$(OTHER)) for config

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

/*
**  Types of macro expansion
*/
#define EXPAND_LAZY         1
#define EXPAND_IMMEDIATE    2

#if defined(__cplusplus)
	extern MyString global_config_source;
	extern MyString global_root_config_source;
	extern StringList local_config_sources;
	class Regex;

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

#if 1
	const char * param_get_location(const MACRO_META * pmet, MyString & value);
#else
	bool param_get_location(const char *parameter, MyString &filename,
							int &line_number);
#endif

	const char * param_get_info(const char * name,
								const char * subsys,
								const char * local,
#if 1
								MyString &name_used,
								const char ** pdef_value,
								const MACRO_META **ppmet);
#else
								const char ** pdef_value,
								MyString &name_used,
								int &use_count,	int & ref_count,
								MyString &filename,
								int &line_number);
#endif

	
	/** Look up a value by the name 'name' from the table 'table' which is table_size big.
	
	Values should have been inserted with insert() (above).
	Treats name case insensitively.  Returns NULL if the name isn't in the
	table.  On success returns a pointer to the associated value.  The
	value is owned by the table; do not free it.
	*/
	MACRO_ITEM* find_macro_item (const char *name, MACRO_SET& set);
	const char * lookup_macro (const char *name, const char *prefix, MACRO_SET& set, int use=3);
	//const char * lookup_and_use_macro (const char *name, const char *prefix, MACRO_SET& set, int use);

	/*This is a faster version of lookup_macro that assumes the param name
	  has already been prefixed with "prefix." if needed.*/
	const char * lookup_macro_exact(const char *name, MACRO_SET& set, int use);

	void optimize_macros(MACRO_SET& macro_set);

	/* A convenience function that calls param() with a MyString buffer. */
	bool param(MyString &buf,char const *param_name,char const *default_value=NULL);

	/* A convenience function that calls param() with a std::string buffer. */
	bool param(std::string &buf,char const *param_name,char const *default_value=NULL);

	/* A function to fill in a wrapper class for the param functions. */
	param_functions * get_param_functions();

	bool get_config_dir_file_list( char const *dirpath, class StringList &files );

/* here we provide C linkage to C++ defined functions. This seems a bit
	odd since if a .c file includes this, these prototypes technically don't
	exist.... */
extern "C" {
	#define CONFIG_OPT_WANT_META      0x01   // also keep metdata about config
	#define CONFIG_OPT_KEEP_DEFAULTS  0x02   // keep items that match defaults
	#define CONFIG_OPT_OLD_COM_IN_CONT 0x04  // ignore # after \ (i.e. pre 8.1.3 comment/continue behavior)
	#define CONFIG_OPT_SMART_COM_IN_CONT 0x08 // parse #opt:oldcomment/newcomment to decide comment behavior
	void config();
	void config_ex(int wantsQuiet, bool abort_if_invalid, int opt = CONFIG_OPT_WANT_META);
	void config_host(const char* host, int wantsQuiet, int config_options);
	void validate_config(bool abort_if_invalid);
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
	char * parse_param_name_from_config(const char *config);
	// this function allows tests to pretend that a param was set to a given value.	
    void  param_insert(const char * name, const char * value);
	/** Expand parameter references of the form "left$(middle)right".  
	
	This is deceptively simple, but does handle multiple and or nested
	references.  If self is not NULL, then we only expand references to to the
	parameter specified by self.  This is used when we want to expand
	self-references only.

	table is a hash table (managed with insert() and macro_lookup)
	that contains the various $(middle)s.

	Also expand references of the form "left$ENV(middle)right",
	replacing $ENV(middle) with getenv(middle).

	Also expand references of the form "left$RANDOM_CHOICE(middle)right".

	NOTE: Returns malloc()ed memory; caller is responsible for calling free().
	*/
	char * expand_macro (const char *value, MACRO_SET& macro_set,
						 const char *self=NULL, bool use_default_param_table=false,
						 const char *subsys=NULL, int use=2);
	// Iterator for the hash array managed by insert() and expand_macro().  See
	// hash_iter_begin(), hash_iter_next(), hash_iter_key(), hash_iter_value(),
	// and hash_iter_delete() for details.
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
	int find_special_config_macro( const char *prefix, bool only_id_chars,
		register char *value, register char **leftp,
		register char **namep, register char **rightp);

	void init_config (int options);
}

#endif // __cplusplus

BEGIN_C_DECLS

	char * get_tilde(void);
	char * param ( const char *name );
	char * param_without_default(const char *name);
	char * param_with_default_abort(const char *name, int abort);
	/** Insert a value into a hash table.

	The value of 'value' is associated with the name 'name'.  This is
	inserted into the table 'table' which is 'table_size' big.
	insert keeps copies of the name and value.
	*/
#ifdef __cplusplus
	typedef struct macro_source { int inside; short int id; short int line; short int meta_id; short int meta_off; } MACRO_SOURCE;
	void insert_source(const char * filename, MACRO_SET& macro_set, MACRO_SOURCE & source);
	extern const MACRO_SOURCE EnvMacro;
	extern const MACRO_SOURCE WireMacro;
	extern const MACRO_SOURCE DetectedMacro;

	void insert (const char *name, const char *value, MACRO_SET& macro_set, const MACRO_SOURCE & source);
	
	/** Sets the whether or not a macro has actually been used
	*/
	int increment_macro_use_count (const char *name, MACRO_SET& macro_set);
	void clear_macro_use_count (const char *name, MACRO_SET& macro_set);
	int get_macro_use_count (const char *name, MACRO_SET& macro_set);
	int get_macro_ref_count (const char *name, MACRO_SET& macro_set);

#endif // __cplusplus

	/*
	As expand_macro() (above), but assumes the table 'ConfigTab' which is
	of size TABLESIZE.
	*/
	char * macro_expand ( const char *name );
	char * expand_param (const char *str, const char *subsys, int use);

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
	void process_config_source(const char* filename, const char* sourcename, const char* host, int required);

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

