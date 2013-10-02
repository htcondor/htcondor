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

#endif

#include "param_info.h"

typedef struct bucket {
	char	*name;
	char	*value;
	int		used;	
	struct bucket	*next;
} BUCKET;

#if defined(__cplusplus)
// used by param_all();
class ParamValue {
	public:

	// name of macro
	MyString name;
	// the expression that is the value of the macro
	MyString value;
	// Which file the macro is found in
	MyString filename;
	// on what line is the macro found in.
	int lnum;
	// where this configuration information came from (localhost, another
	// machine, etc, etc, etc)
	MyString source;

	ParamValue() { 
		name = "";
		value = "";
		filename = "";
		lnum = -1;
		source = "";
	}

	ParamValue(const ParamValue &old) {
		name = old.name;
		value = old.value;
		filename = old.filename;
		lnum = old.lnum;
		source = old.source;
	}

	ParamValue& operator=(const ParamValue &rhs) {
		if (this == &rhs) {
			return *this;
		}

		name = rhs.name;
		value = rhs.value;
		filename = rhs.filename;
		lnum = rhs.lnum;

		return *this;
	}
};
#endif


/*
**  Types of macro expansion
*/
#define EXPAND_LAZY         1
#define EXPAND_IMMEDIATE    2
#define TABLESIZE 113

#if defined(__cplusplus)
	extern MyString global_config_source;
	extern MyString global_root_config_source;
	extern StringList local_config_sources;
	class Regex;

	ExtArray<ParamValue>* param_all(void);
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

	bool param_get_location(const char *parameter, MyString &filename,
							int &line_number);
	
	/** Look up a value by the name 'name' from the table 'table' which is table_size big.
	
	Values should have been inserted with insert() (above).
	Treats name case insensitively.  Returns NULL if the name isn't in the
	table.  On success returns a pointer to the associated value.  The
	value is owned by the table; do not free it.
	*/
	char * lookup_macro ( const char *name, const char *prefix, BUCKET *table[], int table_size );

	/*This is a faster version of lookup_macro that assumes the param name
	  has already been converted to the canonical lowercase form.
	  and prefixed with "prefix." if needed.*/
	char * lookup_macro_lower( const char *name, BUCKET **table, int table_size );

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
	void config( int wantsQuiet=0 , bool ignore_invalid_entry = false, bool wantsExtra = true );
	void config_host( char* host=NULL );
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
	char * expand_macro ( const char *value, BUCKET *table[], int table_size,
						  const char *self=NULL, bool use_default_param_table=false,
						  const char *subsys=NULL);

	// Iterator for the hash array managed by insert() and expand_macro().  See
	// hash_iter_begin(), hash_iter_next(), hash_iter_key(), hash_iter_value(),
	// and hash_iter_delete() for details.
	struct hash_iter {
		BUCKET ** table;
		int table_size;
		int index;
		bucket * current;
	};
	typedef hash_iter * HASHITER;

	/** Begin iteration over the table table with size table_size.

	Returns an opaque object for iterating over all entries in a hash (of the
	sort managed by insert() and expand_macro().  Use hash_iter_key() and
	hash_iter_value() to retrieve keys (names) and values for the current item.
	Use hash_iter_next() to move through the entries.  After hash_iter_begin()
	the "current" entry is set to the first entry; it is not necessary to call
	hash_iter_next() immediately.  When done be sure to call hash_iter_delete
	to free any storage that has been allocated.

	The iterator remains valid if entries are inserted into the hash, but the
	iterator may overlook them.  The iterator becomes undefined if entries are
	deleted from the hash.  If an entry is deleted, the only safe thing to
	do is call hash_iter_delete.
	*/
	HASHITER hash_iter_begin(BUCKET ** table, int table_size);


	/** Return true if we're out of entries in the hash table */
	int hash_iter_done(HASHITER iter);

	/** Move to the next entry in the hash table

	Returns false when trying to advance from the last item in the
	hash table.  Once this has returned false, the only safe things
	you can do is call hash_iter_done or hash_iter_delete.
	*/
	int hash_iter_next(HASHITER iter);

	/** Return key(name) for the current entry in the hash table

	Do no modify or free the returned value.
	*/
	char * hash_iter_key(HASHITER iter);

	/** Return value for the current entry in the hash table

	Do no modify or free the returned value.
	*/
	char * hash_iter_value(HASHITER iter);

	/** Returns 1 if the current entry in the hash table is used in 
		an expression; otherwise, returns 0.
	*/
	int hash_iter_used_value(HASHITER iter);

	/** Destroy iterator and reclaim memory.
	You must pass in a pointer to the hash iterator.
	After calling the HASHITER is no longer valid.
	*/
	void hash_iter_delete(HASHITER * iter);




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

	void init_config ( bool );
}

#endif

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
	void insert ( const char *name, const char *value, BUCKET *table[], int table_size );
	
	/** Sets the whether or not a macro has actually been used
	*/
	void set_macro_used ( const char *name, int used, BUCKET *table[], int table_size );

	/*
	As expand_macro() (above), but assumes the table 'ConfigTab' which is
	of size TABLESIZE.
	*/
	char * macro_expand ( const char *name );
	void clear_config ( void );
	void set_debug_flags( const char * strFlags, int flags );
	void config_insert( const char* attrName, const char* attrValue);
	int  param_boolean_int( const char *name, int default_value );
	int  param_boolean_int_with_default( const char* name );
	int  param_boolean_int_without_default( const char* name, int default_value );
	
	// Process an additional chunk of file
	void process_config_source(const char*, const char*, const char*, int);

	// Write out a config file of non-default values.
	// Returns 0 on success and -1 on failure.
	int write_config_file( const char* pathname );
	// Helper function, of form to iterate over the hash table of parameter
	// information.  Returns 0 to continue, -1 to stop (i.e. on an error).
	int write_config_variable(const param_info_t* value, void* file_desc);

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

