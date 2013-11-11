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

 

#include "condor_common.h"
#include "condor_debug.h"
#include "pool_allocator.h"
#include "condor_config.h"
#include "param_info.h"
#include "param_info_tables.h"
#include "condor_string.h"
#include "extra_param_info.h"
#include "condor_random_num.h"
#include "condor_uid.h"
#include "my_popen.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define CONFIG_GETLINE_OPT_COMMENT_DOESNT_CONTINUE        1
#define CONFIG_GETLINE_OPT_CONTINUE_MAY_BE_COMMENTED_OUT  2
static char *getline_implementation(FILE * fp, int buffer_size, int options);
extern "C++" void param_default_set_use(const char * name, int use, MACRO_SET & set);

int		ConfigLineNo;

/* WARNING: When we mean alphanumeric in this snippet of code, we really mean 
	characters that are legal in a C indentifier plus period and forward slash.
	It looks like what character set is allowable to be in the default value
	of the $$ expansion hasn't been thought about very well....

	XXX: If you've come here looking to add \ so windows paths may be
	substituted as the default value in a $$ expansion with a default
	value, be very careful. The $$() expansion algorithm is deep in
	the parsing of the RHS of the attr/value pair, and at the writing
	of this comment, it is unknown if \ substitution would happen
	before/during/after the $$ expansion would happen, in which case
	you'd be screwed and have to understand/alter much more code.
	If you perform these code alterings to support \ in this manner,
	then remove this XXX comment.
*/
int
condor_isidchar(int c)
{
	if( ('a' <= c && c <= 'z') ||
		('A' <= c && c <= 'Z') ||
		('0' <= c && c <= '9') ||
		/* See the above comment for this function and about the next line. */
		(strchr("_./", c) != NULL) )
	{
		return 1;
	} 

	return 0;
}

#define ISIDCHAR(c)		( condor_isidchar(c) )

// $$ expressions may also contain a colon
#define ISDDCHAR(c) ( ISIDCHAR(c) || ((c)==':') )

#define ISOP(c)		(((c) == '=') || ((c) == ':'))

// Magic macro to represent a dollar sign, i.e. $(DOLLAR)="$"
#define DOLLAR_ID "DOLLAR"
// The length of the DOLLAR_ID string
// Should probably use constexpr here when we use C++11 in earnest
#define DOLLAR_ID_LEN (6)

int is_valid_param_name(const char *name)
{
		/* Check that "name" is a legal identifier : only
		   alphanumeric characters and _ allowed*/
	while( *name ) {
		if( !ISIDCHAR(*name++) ) {
			return 0;
		}
	}

	return 1;
}

char * parse_param_name_from_config(const char *config)
{
	char *name, *tmp;

	if (!(name = strdup(config))) {
		EXCEPT("Out of memory!");
	}

	tmp = strchr(name, '=');
	if (!tmp) {
		tmp = strchr(name, ':');
	}
	if (!tmp) {
			// Line is invalid, should be "name = value" or "name : value"
		return NULL;
	}

		// Trim whitespace...
	*tmp = ' ';
	while (isspace(*tmp)) {
		*tmp = '\0';
		tmp--;
	}

	return name;
}

bool 
is_piped_command(const char* filename)
{
	bool retVal = false;

	char const *pdest = strchr( filename, '|' );
	if ( pdest != NULL ) {
		// This is not a filename (still not sure it's a valid command though)
		retVal = true;
	}

	return retVal;
}

bool is_meta_knob(const char * name)
{
	if ( ! name || name[0] != '$')
		return false;
	return param_meta_table(name+1) != NULL;
}

int Parse_config(MACRO_SOURCE & source, const char * self, const char * config, MACRO_SET& macro_set, const char * subsys);

int read_meta_config(MACRO_SOURCE & source, const char *name, const char * rhs, MACRO_SET& macro_set, const char * subsys)
{
	if ( ! name || name[0] != '$')
		return -1;

	MACRO_TABLE_PAIR* ptable = param_meta_table(name+1);
	if ( ! ptable)
		return -1;

	StringList items(rhs);
	items.rewind();
	char * item;
	while ((item = items.next()) != NULL) {
		const char * value = param_meta_table_string(ptable, item);
		if ( ! value) {
			fprintf(stderr,
					"Configuration Error: Meta %s does not have a value for %s\n",
					name, item);
			return -1;
		}
		source.meta_id = param_default_get_source_meta_id(name+1, item);
		int ret = Parse_config(source, name, value, macro_set, subsys);
		if (ret < 0) {
			fprintf(stderr,
					"Internal Configuration Error: Meta %s has a bad value for %s\n",
					name, item);
			return ret;
		}
	}

	source.meta_id = -1;
	return 0;
}

// Make sure the last character is the '|' char.  For now, that's our only criteria.
bool 
is_valid_command(const char* cmdToExecute)
{
	bool retVal = false;

	int cmdStrLength = strlen(cmdToExecute);
	if ( cmdToExecute[cmdStrLength - 1] == '|' ) {
		retVal = true;
	}

	return retVal;
}


int Parse_config(MACRO_SOURCE & source, const char * self, const char * config, MACRO_SET& macro_set, const char * subsys)
{
	source.meta_off = -1;

	StringList lines(config, "\n");
	lines.rewind();
	char * line;
	while ((line = lines.next()) != NULL) {
		++source.meta_off;
		if( line[0] == '#' || blankline(line) )
			continue;

		const char * name = line;
		char * ptr = line;
		int op = 0;

		// parse to the end of the name and null terminate it
		while (*ptr) {
			if (isspace(*ptr) || ISOP(*ptr)) {
				op = *ptr;  // capture the operator
				*ptr++ = 0; // null terminate the name
				break;
			}
			++ptr;
		}
		// parse to the start of the value, it's ok to not have any value
		while (*ptr) {
			if (ISOP(*ptr)) {
				if (ISOP(op)) {
					op = 0; // more than one op is not allowed, so trigger a failure
					break;
				}
				op = *ptr;
			} else if ( ! isspace(*ptr)) {
				break;
			}
			++ptr;
		}

		if ( ! *ptr && ! ISOP(op)) {
			// Here we have determined this line has no operator, or too many
			PRAGMA_REMIND("tj: should report parse error in meta knobs here.")
			return -1;
		}

		// ptr now points to the first non-space character of the right hand side
		const char * rhs = ptr;

		// Expand the knob name - do we allow this??
		/*
		name = expand_macro(name, macro_set);
		if (name == NULL) {
			return -1;
		}
		*/

		// if name begins with $ it might be a metaknob.
		if (is_meta_knob(name)) {
			if (MATCH == strcasecmp(self, name)) {
				// self ref is invalid
				return -1;
			}
			// for recursive metaknobs, we need to use a temp copy of the source info
			// to avoid loosing the source id/offset info.
			MACRO_SOURCE source2 = source;
			int retval = read_meta_config(source2, name, rhs, macro_set, subsys);
			if (retval < 0) {
				return retval;
			}
		} else {

			/* Check that "name" is a legal identifier : only
			   alphanumeric characters and _ allowed*/
			if ( ! is_valid_param_name(name) ) {
				return -1;
			}

			/* expand self references only */
			PRAGMA_REMIND("TJ: this handles only trivial self-refs, needs rethink.")
			char * value = expand_macro(rhs, macro_set, name, false, subsys);
			if (value == NULL) {
				return -1;
			}

			insert(name, value, macro_set, source);
			FREE(value);
		}
	}
	source.meta_off = -2;
	return 0;
}

int
Read_config(const char* config_source, MACRO_SET& macro_set,
			int expand_flag,
			bool check_runtime_security,
			const char * subsys)
{
	FILE*	conf_fp = NULL;
	char*	name = NULL;
	char*	value = NULL;
	char*	rhs = NULL;
	char*	ptr = NULL;
	char	op;
	int		retval = 0;
	bool	is_pipe_cmd = false;
	bool	firstRead = true;
	const int gl_opt_old = 0;
	const int gl_opt_new = CONFIG_GETLINE_OPT_COMMENT_DOESNT_CONTINUE | CONFIG_GETLINE_OPT_CONTINUE_MAY_BE_COMMENTED_OUT;
	int gl_opt = (macro_set.options & CONFIG_OPT_OLD_COM_IN_CONT) ? gl_opt_old : gl_opt_new;
	bool gl_opt_smart = (macro_set.options & CONFIG_OPT_SMART_COM_IN_CONT) ? true : false;


	if (subsys && ! *subsys) subsys = NULL;

	ConfigLineNo = 0;
	// initialize a MACRO_SOURCE for this file that we will use
	// in subsequent macro insert calls.
	MACRO_SOURCE FileMacro;
	insert_source(config_source, macro_set, FileMacro);

	// Determine if the config file name specifies a file to open, or a
	// pipe to suck on. Process each accordingly
	if ( is_piped_command(config_source) ) {
		is_pipe_cmd = true;
		if ( is_valid_command(config_source) ) {
			// try to run the cmd specified before the '|' symbol, and
			// get the configuration from it's output.
			char *cmdToExecute = strdup( config_source );
			cmdToExecute[strlen(cmdToExecute)-1] = '\0';

			ArgList argList;
			MyString args_errors;
			if(!argList.AppendArgsV1RawOrV2Quoted(cmdToExecute, &args_errors)) {
				printf("Can't append cmd %s(%s)\n", cmdToExecute, args_errors.Value());
				free( cmdToExecute );
				return -1;
			}
			conf_fp = my_popen(argList, "r", FALSE);
			if( conf_fp == NULL ) {
				printf("Can't open cmd %s\n", cmdToExecute);
				free( cmdToExecute );
				return -1;
			}
			free( cmdToExecute );
		} else {
			printf("Specified cmd, %s, not a valid command to execute.  It must have a '|' "
				   "character at the end.\n", config_source);
			return( -1 );
		}
	} else {
		is_pipe_cmd = false;
		conf_fp = safe_fopen_wrapper_follow(config_source, "r");
		if( conf_fp == NULL ) {
			printf("Can't open file %s\n", config_source);
			return( -1 );
		}
	}

	if( check_runtime_security ) {
#ifndef WIN32
			// unfortunately, none of this works on windoze... (yet)
		if ( is_pipe_cmd ) {
			fprintf( stderr, "Configuration Error File <%s>: runtime config "
					 "not allowed to come from a pipe command\n",
					 config_source );
			retval = -1;
			goto cleanup;
		}
		int fd = fileno(conf_fp);
		struct stat statbuf;
		uid_t f_uid;
		int rval = fstat( fd, &statbuf );
		if( rval < 0 ) {
			fprintf( stderr, "Configuration Error File <%s>, fstat() failed: %s (errno: %d)\n",
					 config_source, strerror(errno), errno );
			retval = -1;
			goto cleanup;
		}
		f_uid = statbuf.st_uid;
		if( can_switch_ids() ) {
				// if we can switch, the file *must* be owned by root
			if( f_uid != 0 ) {
				fprintf( stderr, "Configuration Error File <%s>, "
						 "running as root yet runtime config file owned "
						 "by uid %d, not 0!\n", config_source, (int)f_uid );
				retval = -1;
				goto cleanup;
			}
		} else {
				// if we can't switch, at least ensure we own the file
			if( f_uid != get_my_uid() ) {
				fprintf( stderr, "Configuration Error File <%s>, "
						 "running as uid %d yet runtime config file owned "
						 "by uid %d!\n", config_source, (int)get_my_uid(),
						 (int)f_uid );
				retval = -1;
				goto cleanup;
			}
		}
#endif /* ! WIN32 */
	} // if( check_runtime_security )

	while(true) {
		name = getline_implementation(conf_fp, 128, gl_opt);
		// If the file is empty the first time through, warn the user.
		if (name == NULL) {
			if (firstRead) {
				dprintf(D_FULLDEBUG, "WARNING: Config source is empty: %s\n",
						config_source);
			}
			break;
		}
		firstRead = false;
		
		/* Skip over comments */
		if( *name == '#' || blankline(name) ) {
			if (gl_opt_smart) {
				if (MATCH == strcasecmp(name, "#opt:oldcomment")) {
					gl_opt = gl_opt_old;
				} else if (MATCH == strcasecmp(name, "#opt:newcomment")) {
					gl_opt = gl_opt_new;
				}
			}
			continue;
		}

		ptr = name;

		// Assumption is that the line starts with a non whitespace
		// character
		// Example :
		// OP_SYS=SunOS
		while( *ptr ) {
			if( isspace(*ptr) || ISOP(*ptr) ) {
			  /* *ptr is now whitespace or '=' or ':' */
			  break;
			} else {
			  ptr++;
			}
		}

		if( !*ptr ) {
				// Here we have determined this line has no operator
			if ( name && name[0] && name[0] == '[' ) {
				// Treat a line w/o an operator that begins w/ a square bracket
				// as a comment so a config file can look like
				// a Win32 .ini file for MS Installer purposes.		
				continue;
			} else {
				// No operator and no square bracket... bail.
				retval = -1;
				goto cleanup;
			}
		}

		if( ISOP(*ptr) ) {
			op = *ptr;
			//op is now '=' in the above eg
			*ptr++ = '\0';
			// name is now 'OpSys' in the above eg
		} else {
			*ptr++ = '\0';
			while( *ptr && !ISOP(*ptr) ) {
				ptr++;
			}

			if( !*ptr ) {
				retval = -1;
				goto cleanup;
			}

			op = *ptr++;
		}

		/* Skip to next non-space character */
		while( *ptr && isspace(*ptr) ) {
			ptr++;
		}

		rhs = ptr;
		// rhs is now 'SunOS' in the above eg

		/* Expand references to other parameters */
		name = expand_macro(name, macro_set);
		if( name == NULL ) {
			retval = -1;
			goto cleanup;
		}

		// if name begins with $ it might be a metaknob.
		if (is_meta_knob(name)) {
			FileMacro.line = ConfigLineNo;
			retval = read_meta_config(FileMacro, name, rhs, macro_set, subsys);
			if (retval < 0) {
				fprintf( stderr,
						 "Configuration Error File <%s>, Line %d: Meta <%s>\n",
						 config_source, ConfigLineNo, name );
				goto cleanup;
			}
		} else {

			/* Check that "name" is a legal identifier : only
			   alphanumeric characters and _ allowed*/
			if( !is_valid_param_name(name) ) {
				fprintf( stderr,
						 "Configuration Error File <%s>, Line %d: Illegal Identifier: <%s>\n",
						 config_source, ConfigLineNo, name );
				retval = -1;
				goto cleanup;
			}

			if( expand_flag == EXPAND_IMMEDIATE ) {
				value = expand_macro(rhs, macro_set);
				if( value == NULL ) {
					retval = -1;
					goto cleanup;
				}
			} else  {
				/* expand self references only */
				PRAGMA_REMIND("TJ: this handles only trivial self-refs, needs rethink.")
				value = expand_macro(rhs, macro_set, name, false, subsys);
				if( value == NULL ) {
					retval = -1;
					goto cleanup;
				}
			}

			if( op == ':' || op == '=' ) {
					/*
					  Yee haw!!! We can treat "expressions" and "macros"
					  the same way: just insert them into the config hash
					  table.  Everything now behaves like macros used to
					  Derek Wright <wright@cs.wisc.edu> 4/11/00
					*/
				FileMacro.line = ConfigLineNo;
				insert(name, value, macro_set, FileMacro);
			} else {
				fprintf( stderr,
					"Configuration Error File <%s>, Line %d: Syntax Error\n",
					config_source, ConfigLineNo );
				retval = -1;
				goto cleanup;
			}
		}

		FREE( name );
		name = NULL;
		FREE( value );
		value = NULL;
	}

 cleanup:
	if ( conf_fp ) {
		if ( is_pipe_cmd ) {
			int exit_code = my_pclose( conf_fp );
			if ( retval == 0 && exit_code != 0 ) {
				fprintf( stderr, "Configuration Error File <%s>: "
						 "command terminated with exit code %d\n",
						 config_source, exit_code );
				retval = -1;
			}
		} else {
			fclose( conf_fp );
		}
	}
	if(name) {
		FREE( name );
	}
	if(value) {
		FREE( value );
	}
	return retval;
}


/*
** Just compute a hash value for the given string such that
** 0 <= value < size 
*/
#if 0 // this is not currently used.
int
condor_hash( register const char *string, register int size )
{
	register unsigned int		answer;

	answer = 1;

	for( ; *string; string++ ) {
		answer <<= 1;
		answer += (int)*string;
	}
	answer >>= 1;	/* Make sure not negative */
	answer %= size;
	return answer;
}
#endif

// case-insensitive hash, usable when the character set of name
// is restricted to A-Za-Z0-9 and symbols except []{}\|^~
#if 0 // this is not currently used
int
condor_hash_symbol_name(const char *name, int size)
{
	register const char * psz = name;
	unsigned int answer = 1;

	// in order to make this hash interoperate with strlwr/condor_hash
	// _ is the only legal character for symbol name for which |= 0x20
	// is not benign.  the test for _ is needed only to make this hash
	// behave identically to strlwr/condor_hash. 
	for( ; *psz; ++psz) {
		answer <<= 1;
		int ch = (int)*psz;
		if (ch != '_') ch |= 0x20;
		answer += ch;
	}
	answer >>= 1;	/* Make sure not negative */
	answer %= size;
	return answer;
}
#endif

/*
** Insert the parameter name and value into the given hash table.
*/
PRAGMA_REMIND("TJ: insert bug - self refs to default refs never expanded")

extern "C++" MACRO_ITEM* find_macro_item (const char *name, MACRO_SET& set)
{
	int cElms = set.size;
	MACRO_ITEM* aTable = set.table;

	if (set.sorted < set.size) {
		// use a brute force search on the unsorted parts of the table.
		for (int ii = set.sorted; ii < cElms; ++ii) {
			if (MATCH == strcasecmp(aTable[ii].key, name))
				return &aTable[ii];
		}
		cElms = set.sorted;
	}

	// table is sorted, so we can binary search it.
	if (cElms <= 0)
		return NULL;

	int ixLower = 0;
	int ixUpper = cElms-1;
	for (;;) {
		if (ixLower > ixUpper)
			return NULL; // return null for "not found"

		int ix = (ixLower + ixUpper) / 2;
		int iMatch = strcasecmp(aTable[ix].key, name);
		if (iMatch < 0)
			ixLower = ix+1;
		else if (iMatch > 0)
			ixUpper = ix-1;
		else
			return &aTable[ix];
	}
}

void insert_source(const char * filename, MACRO_SET & set, MACRO_SOURCE & source)
{
	if ( ! set.sources.size()) {
		set.sources.push_back("<Detected>");
		set.sources.push_back("<Default>");
		set.sources.push_back("<Environment>");
		set.sources.push_back("<Over>");
	}
	source.line = 0;
	source.inside = false;
	source.id = (int)set.sources.size();
	set.sources.push_back(set.apool.insert(filename));
}

static bool same_param_value(
	const char * a,
	const char * b,
#ifdef WIN32
	bool is_path)
#else
	bool /*is_path*/) // to get rid of stupid g++ warning
#endif
{
	if ( ! a || ! b)
		return (a == b);

	// exact matches are always matches.
	if (MATCH == strcmp(a, b))
		return true;

	// some special cases tolerate case insensitive matches.
	if (MATCH == strcasecmp(a, b)) {
		if (MATCH == strcasecmp(a, "true") || MATCH == strcasecmp(a, "false"))
			return true;
		#ifdef WIN32
		// paths are case-insensitive on windows.
		if (is_path) { return true; }
		#endif
	}
#ifdef WIN32
	if (is_path) {
		while (*a && *b) {
			if (toupper(*a) != toupper(*b) && ((*a != '/' && *a != '\\') || (*b != '/' && *b != '\\')))
				return false;
			++a, ++b;
		}
		return true;
	}
#endif

	return false;
}

void insert(const char *name, const char *value, MACRO_SET & set, const MACRO_SOURCE & source)
{
	// if already in the macro-set, we need to expand self-references and replace
	MACRO_ITEM * pitem = find_macro_item(name, set);
	if (pitem) {
		char * tvalue = expand_macro(value,set,name,true);
		if (MATCH != strcmp(tvalue, pitem->raw_value)) {
			pitem->raw_value = set.apool.insert(tvalue);
		}
		if (set.metat) {
			MACRO_META * pmeta = &set.metat[pitem - set.table];
			pmeta->source_id = source.id;
			pmeta->source_line = source.line;
			pmeta->source_meta_id = source.meta_id;
			pmeta->source_meta_off = source.meta_off;
			pmeta->inside = (source.inside != false);
			pmeta->param_table = false;
			// use the name here in case we have a compound name, i.e "master.value"
			int param_id = param_default_get_id(name);
			const char * def_value = param_default_rawval_by_id(param_id);
			pmeta->matches_default = (def_value == pitem->raw_value);
			if ( ! pmeta->matches_default) {
				bool is_path = param_default_ispath_by_id(pmeta->param_id);
				pmeta->matches_default = same_param_value(def_value, pitem->raw_value, is_path);
			}
		}
		if (tvalue) free(tvalue);
		return;
	}

	if (set.size+1 >= set.allocation_size) {
		int cAlloc = set.allocation_size*2;
		if ( ! cAlloc) cAlloc = 32;
		MACRO_ITEM * ptab = new MACRO_ITEM[cAlloc];
		if (set.table) {
			// transfer existing key/value pairs old allocation to new one.
			if (set.size > 0) {
				memcpy(ptab, set.table, sizeof(set.table[0]) * set.size);
				memset(set.table, 0, sizeof(set.table[0]) * set.size);
			}
			delete [] set.table;
		}
		set.table = ptab;
		if (set.metat != NULL || (set.options & CONFIG_OPT_WANT_META) != 0) {
			MACRO_META * pmet = new MACRO_META[cAlloc];
			if (set.metat) {
				// transfer existing metadata from old allocation to new one.
				if (set.size > 0) {
					memcpy(pmet, set.metat, sizeof(set.metat[0]) * set.size);
					memset(set.metat, 0, sizeof(set.metat[0]) * set.size);
				}
				delete [] set.metat;
			}
			set.metat = pmet;
		}
	}

	int matches_default = false;
	int param_id = param_default_get_id(name);
	const char * def_value = param_default_rawval_by_id(param_id);
	bool is_path = param_default_ispath_by_id(param_id);
	if (same_param_value(def_value, value, is_path)) {
		matches_default = true; // flag value as matching the default.
		if ( ! (set.options & CONFIG_OPT_KEEP_DEFAULTS))
			return; // don't put default-matching values into the macro set
	}

	// for now just append the item.
	// the set after this will no longer be sorted.
	int ixItem = set.size++;

	pitem = &set.table[ixItem];
	const char * def_name = param_default_name_by_id(param_id);
	if (def_name && MATCH == strcmp(name, def_name)) {
		pitem->key = def_name;
	} else {
		pitem->key = set.apool.insert(name);
	}
	if (matches_default) {
		pitem->raw_value = def_value;
	} else {
		pitem->raw_value = set.apool.insert(value);
	}
	if (set.metat) {
		MACRO_META * pmeta = &set.metat[ixItem];
		pmeta->flags = 0; // clear all flags.
		pmeta->matches_default = matches_default;
		pmeta->inside = source.inside;
		pmeta->source_id = source.id;
		pmeta->source_line = source.line;
		pmeta->source_meta_id = source.meta_id;
		pmeta->source_meta_off = source.meta_off;
		pmeta->use_count = 0;
		pmeta->ref_count = 0;
		pmeta->index = ixItem;
		pmeta->param_id = param_id;
	}
}

/*
** Sets the given macro's state to used
*/

int increment_macro_use_count (const char *name, MACRO_SET & set)
{
	MACRO_ITEM* pitem = find_macro_item(name, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		return ++(pmeta->use_count);
	}
	return -1;
}

void clear_macro_use_count (const char *name, MACRO_SET & set)
{
	MACRO_ITEM* pitem = find_macro_item(name, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		pmeta->ref_count = pmeta->use_count = 0;
	}
}

int get_macro_use_count (const char *name, MACRO_SET & set)
{
	MACRO_ITEM* pitem = find_macro_item(name, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		return pmeta->use_count;
	}
	return -1;
}

int get_macro_ref_count (const char *name, MACRO_SET & set)
{
	MACRO_ITEM* pitem = find_macro_item(name, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		return pmeta->ref_count;
	}
	return -1;
}

/*
** Read one line and any continuation lines that go with it.  Lines ending
** with <white space><backslash> are continued onto the next line.
** Lines can be of any lengh.  We pass back a pointer to a buffer; do _not_
** free this memory.  It will get freed the next time getline() is called (this
** function used to contain a fixed-size static buffer).
*/
char *
getline( FILE *fp )
{
	return getline_implementation(fp,_POSIX_ARG_MAX, 0);
}

static char *
getline_implementation( FILE *fp, int requested_bufsize, int options )
{
	static char	*buf = NULL;
	static unsigned int buflen = 0;
	char	*end_ptr;	// Pointer to read into next read
	char    *line_ptr;	// Pointer to beginning of current line from input
	int      in_comment = FALSE;
	//int      in_continuation = FALSE;

	if( feof(fp) ) {
			// We're at the end of the file, clean up our buffers and
			// return NULL.  
		if ( buf ) {
			free(buf);
			buf = NULL;
			buflen = 0;
		}
		return NULL;
	}

	if ( buflen < (unsigned int)requested_bufsize ) {
		if ( buf ) free(buf);
		buf = (char *)malloc(requested_bufsize);
		buflen = requested_bufsize;
	}
	ASSERT( buf != NULL );
	buf[0] = '\0';
	end_ptr = buf;
	line_ptr = buf;

	// Loop 'til we're done reading a whole line, including continutations
	for(;;) {
		int		len = buflen - (end_ptr - buf);
		if( len <= 5 ) {
			// we need a larger buffer -- grow buffer by 4kbytes
			char *newbuf = (char *)realloc(buf, 4096 + buflen);
			if ( newbuf ) {
				end_ptr = (end_ptr - buf) + newbuf;
				line_ptr = (line_ptr - buf) + newbuf;
				buf = newbuf;	// note: realloc() freed our old buf if needed
				buflen += 4096;
				len += 4096;
			} else {
				// malloc returned NULL, we're out of memory
				EXCEPT( "Out of memory - config file line too long" );
			}
		}

		if( fgets(end_ptr,len,fp) == NULL ) {
			if( buf[0] == '\0' ) {
				return NULL;
			} else {
				return buf;
			}
		}

		// See if fgets read an entire line, or simply ran out of buffer space
		if ( *end_ptr == '\0' ) {
			continue;
		}

		int cch = strlen(end_ptr);
		if (end_ptr[cch-1] != '\n') {
			// if we made it here, fgets() ran out of buffer space.
			// move our read_ptr pointer forward so we concatenate the
			// rest on after we realloc our buffer above.
			end_ptr += cch;
			continue;	// since we are not finished reading this line
		}

		ConfigLineNo++;
		end_ptr += cch;

			// Instead of calling ltrim() below, we do it inline,
			// taking advantage of end_ptr to avoid overhead.

			// trim whitespace from the end
		while( end_ptr>line_ptr && isspace( end_ptr[-1] ) ) {
			*(--end_ptr) = '\0';
		}	

			// trim whitespace from the beginning of the line
		char	*ptr = line_ptr;
		while( isspace(*ptr) ) {
			ptr++;
		}
		// special interactions between \ and #.
		// if we have a # AFTER a continuation then we may want to treat everthing between the # and \n
		// as if it were whitespace. conversely, if the entire line begins with # we may want to ignore
		// \ at the end of that line.
		in_comment = (*ptr == '#');
		if (in_comment) {
			if (line_ptr == buf) {
				// we are the the start of the whole line.
			} else if (options & CONFIG_GETLINE_OPT_CONTINUE_MAY_BE_COMMENTED_OUT) {
				// pretend this is whitespace to the end of the line
				ptr = end_ptr-1;
				in_comment = false;
			}
		}
		if( ptr != line_ptr ) {
			(void)memmove( line_ptr, ptr, end_ptr-ptr+1 );
			end_ptr = (end_ptr - ptr) + line_ptr;
		}

		if( end_ptr > buf && end_ptr[-1] == '\\' ) {
			/* Ok read the continuation and concatenate it on */
			*(--end_ptr) = '\0';

			// special interactions between \ and #.
			// if we have a \ at the end of a line that begins with #
			// we want to pretend that the \ isn't there and NOT continue
			// we do this on the theory that a comment that has continuation
			// is likely to be an error.
			if ( ! in_comment || ! (options & CONFIG_GETLINE_OPT_COMMENT_DOESNT_CONTINUE)) {
				line_ptr = end_ptr;
				continue;
			}
		}
		return buf;
	}
}

} // end of extern "C"

/* 
** Utility function to get an integer from a string.
** Returns: -1 if input is NULL, -2 if input is invalid, 0 for success
*/
static int
string_to_long( const char *s, long *valuep )
{
	// Verify that we have a valid pointer
	if ( !s ) {
		return -1;
	}

	// Call strtol(), verify that it liked the input
	char	*endp;
	long	value = strtol( s, &endp, 10 );
	if ( (const char *)endp == s ) {
		return -2;
	}

	// Done, get out of here
	*valuep = value;
	return 0;
}

/*
** Expand parameter references of the form "left$(middle)right".  This
** is deceptively simple, but does handle multiple and or nested references.
** If self is not NULL, then we only expand references to to the parameter
** specified by self.  This is used when we want to expand self-references
** only.
** Also expand references of the form "left$ENV(middle)right",
** replacing $ENV(middle) with getenv(middle).
** Also expand references of the form "left$RANDOM_CHOICE(middle)right".
*/
extern "C" char *
expand_macro(const char *value,
			 MACRO_SET& macro_set,
			 const char *self,
			 bool use_default_param_table,
			 const char *subsys,
			 int use)
{
	char *tmp = strdup( value );
	char *left, *name, *right;
	const char *tvalue;
	char *rval;
	const char *selfless = NULL; // if self=="master.foo" and subsys=="master", then this contains "foo"

	// to avoid infinite recursive expansion, we have to look for both "subsys.self" and "self"
	if (self && subsys) {
		const char * a = subsys;
		const char * b = self;
		while (*a && (tolower(*a) == tolower(*b))) {
			++a; ++b;
		}
		// if a now points to a 0, and b now points to ".", then self contains subsys as a prefix.
		if (0 == a[0] && '.' == b[0] && b[1] != 0) {
			selfless = b+1;
		}
	}

	bool all_done = false;
	while( !all_done ) {		// loop until all done expanding
		all_done = true;

		if( !self && find_special_config_macro("$ENV",true,tmp, &left, &name, &right) ) 
		{
			all_done = false;
			tvalue = getenv(name);
			if( tvalue == NULL ) {
				//EXCEPT("Can't find %s in environment!",name);
				tvalue = "UNDEFINED";		
			}

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) + strlen(right) + 1));
			ASSERT(rval);

			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}

		if( !self && find_special_config_macro("$RANDOM_CHOICE",false,tmp, &left, &name, 
			&right) ) 
		{
			all_done = false;
			StringList entries(name,",");
			int num_entries = entries.number();
			tvalue = NULL;
			if ( num_entries > 0 ) {
				int rand_entry = (get_random_int() % num_entries) + 1;
				int i = 0;
				entries.rewind();
				while ( (i < rand_entry) && (tvalue=entries.next()) ) {
					i++;
				}
			}
			if( tvalue == NULL ) {
				EXCEPT("$RANDOM_CHOICE() macro in config file empty!" );
			}

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) +
											  strlen(right) + 1));
			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}

		if( !self && find_special_config_macro("$RANDOM_INTEGER",false,tmp, &left, &name, 
			&right) ) 
		{
			all_done = false;
			StringList entries(name, ",");

			entries.rewind();
			const char *tmp2;

			tmp2 = entries.next();
			long	min_value=0;
			if ( string_to_long( tmp2, &min_value ) < 0 ) {
				EXCEPT( "$RANDOM_INTEGER() config macro: invalid min!" );
			}

			tmp2 = entries.next();
			long	max_value=0;
			if ( string_to_long( tmp2, &max_value ) < 0 ) {
				EXCEPT( "$RANDOM_INTEGER() config macro: invalid max!" );
			}

			tmp2 = entries.next();
			long	step = 1;
			if ( string_to_long( tmp2, &step ) < -1 ) {
				EXCEPT( "$RANDOM_INTEGER() config macro: invalid step!" );
			}

			if ( step < 1 ) {
				EXCEPT( "$RANDOM_INTEGER() config macro: invalid step!" );
			}
			if ( min_value > max_value ) {
				EXCEPT( "$RANDOM_INTEGER() config macro: min > max!" );
			}

			// Generate the random value
			long	range = step + max_value - min_value;
			long 	num = range / step;
			long	random_value =
				min_value + (get_random_int() % num) * step;

			// And, convert it to a string
			char	buf[128];
			snprintf( buf, sizeof(buf)-1, "%ld", random_value );
			buf[sizeof(buf)-1] = '\0';
			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(buf) +
											  strlen(right) + 1));
			ASSERT( rval != NULL );
			(void)sprintf( rval, "%s%s%s", left, buf, right );
			FREE( tmp );
			tmp = rval;
		}

		if (find_config_macro(tmp, &left, &name, &right, self) ||
			(selfless && find_config_macro(tmp, &left, &name, &right, selfless)) ) {
			all_done = false;
		   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
			char * pcolon = strchr(name, ':');
			if (pcolon) { *pcolon++ = 0; }
		   #endif
			tvalue = lookup_macro(name, subsys, macro_set, use);
			if (subsys && ! tvalue)
				tvalue = lookup_macro(name, NULL, macro_set, use);

				// Note that if 'name' has been explicitly set to nothing,
				// tvalue will _not_ be NULL so we will not call
				// param_default_string().  See gittrack #1302
			if( !self && use_default_param_table && tvalue == NULL ) {
				tvalue = param_default_string(name, subsys);
				if (use) { param_default_set_use(name, use, macro_set); }
			}
		   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
			if (pcolon && ( ! tvalue || ! tvalue[0])) {
				tvalue = pcolon;
			}
		   #endif
			if( tvalue == NULL ) {
				tvalue = "";
			}

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) +
											  strlen(right) + 1));
			ASSERT( rval != NULL );
			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}
	}

	// Now, deal with the special $(DOLLAR) macro.
	if (!self)
	while( find_config_macro(tmp, &left, &name, &right, DOLLAR_ID) ) {
		rval = (char *)MALLOC( (unsigned)(strlen(left) + 1 +
										  strlen(right) + 1));
		ASSERT( rval != NULL );
		(void)sprintf( rval, "%s$%s", left, right );
		FREE( tmp );
		tmp = rval;
	}

	return( tmp );
}


bool hash_iter_done(HASHITER& it) {
	// the first time this is called, so some setup
	if (it.ix == 0 && it.id == 0) {
		if ( ! it.set.defaults || ! it.set.defaults->table || ! it.set.defaults->size) {
			it.opts |= HASHITER_NO_DEFAULTS;
		} else if ( ! (it.opts & HASHITER_NO_DEFAULTS)) {
			// decide whether the first item is in the defaults table or not.
			const char * pix_key = it.set.table[it.ix].key;
			const char * pid_key = it.set.defaults->table[it.id].key;
			int cmp = strcasecmp(pix_key, pid_key);
			it.is_def = (cmp > 0);
			if ( ! cmp && ! (it.opts & HASHITER_SHOW_DUPS)) {
				++it.id;
			}
		}
	}
	if (it.ix >= it.set.size && ((it.opts & HASHITER_NO_DEFAULTS) != 0 || (it.id >= it.set.defaults->size)))
		return true;
	return false;
}
bool hash_iter_next(HASHITER& it) {
	if (hash_iter_done(it)) return false;
	if (it.is_def) {
		++it.id;
	} else {
		++it.ix;
	}

	if (it.opts & HASHITER_NO_DEFAULTS) {
		it.is_def = false;
		return (it.ix < it.set.size);
	}

	if (it.ix < it.set.size) {
		if (it.id < it.set.defaults->size) {
			const char * pix_key = it.set.table[it.ix].key;
			const char * pid_key = it.set.defaults->table[it.id].key;
			int cmp = strcasecmp(pix_key, pid_key);
			it.is_def = (cmp > 0);
			if ( ! cmp && ! (it.opts & HASHITER_SHOW_DUPS)) {
				++it.id;
			}
		} else {
			it.is_def = false;
		}
		return true;
	}
	it.is_def = (it.id < it.set.defaults->size);
	return it.is_def;
}
const char * hash_iter_key(HASHITER& it) {
	if (hash_iter_done(it)) return NULL;
	if (it.is_def) {
		return it.pdef ? it.pdef->key : it.set.defaults->table[it.id].key;
	}
	return it.set.table[it.ix].key;
}
const char * hash_iter_value(HASHITER& it) {
	if (hash_iter_done(it)) return NULL;
	if (it.is_def) {
		const condor_params::nodef_value * pdef = it.pdef ? it.pdef->def : it.set.defaults->table[it.id].def;
		if ( ! pdef)
			return NULL;
		return pdef->psz;
	}
	return it.set.table[it.ix].raw_value;
}
MACRO_META * hash_iter_meta(HASHITER& it) {
	if (hash_iter_done(it)) return NULL;
	if (it.is_def) {
		static MACRO_META meta;
		memset(&meta, 0, sizeof(meta));
		meta.inside = true;
		meta.param_table = true;
		meta.param_id = it.id;
		meta.index = it.ix;
		meta.source_id = 1;
		meta.source_line = -2;
		if (it.set.defaults && it.set.defaults->metat) {
			meta.ref_count = it.set.defaults->metat[it.id].ref_count;
			meta.use_count = it.set.defaults->metat[it.id].use_count;
		} else {
			meta.ref_count = -1;
			meta.use_count = -1;
		}
		return &meta;
	}
	return it.set.metat ? &it.set.metat[it.ix] : NULL;
}
int hash_iter_used_value(HASHITER& it) {
	if (hash_iter_done(it)) return -1;
	if (it.is_def) {
		if (it.set.defaults && it.set.defaults->metat) {
			return it.set.defaults->metat[it.id].use_count + it.set.defaults->metat[it.id].ref_count;
		}
		return -1;
	}
	if (it.set.metat && (it.ix >= 0 && it.ix < it.set.size))
		return it.set.metat[it.ix].use_count + it.set.metat[it.ix].ref_count;
	return -1;
};

/*
** Same as find_config_macro() below, but finds special references like $ENV().
*/
extern "C" int
find_special_config_macro( const char *prefix, bool only_id_chars, register char *value, 
		register char **leftp, register char **namep, register char **rightp )
{
	char *left, *left_end, *name, *right;
	char *tvalue;
	int prefix_len;

	if ( prefix == NULL ) {
		return( 0 );
	}

	prefix_len = strlen(prefix);
	tvalue = value;
	left = value;

		// Loop until we're done, helped with the magic of goto's
	while (1) {
tryagain:
		if (tvalue) {
			value = const_cast<char *>(strstr(tvalue, prefix) );
		}
		
		if( value == NULL ) {
			return( 0 );
		}

		value += prefix_len;
		if( *value == '(' ) {
			left_end = value - prefix_len;
			name = ++value;
			while( *value && *value != ')' ) {
				char c = *value++;
				if( !ISIDCHAR(c) && only_id_chars ) {
					tvalue = name;
					goto tryagain;
				}
			}

			if( *value == ')' ) {
				right = value;
				break;
			} else {
				tvalue = name;
				goto tryagain;
			}
		} else {
			tvalue = value;
			goto tryagain;
		}
	}

	*left_end = '\0';
	*right++ = '\0';

	*leftp = left;
	*namep = name;
	*rightp = right;

	return( 1 );
}

/* Docs are in /src/condor_includes/condor_config.h */
extern "C" int
find_config_macro( register char *value, register char **leftp, 
		 register char **namep, register char **rightp,
		 const char *self,
		 bool getdollardollar, int search_pos)
{
	char *left, *left_end, *name, *right;
	char *tvalue;
   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
	bool after_colon = false;
   #endif

	tvalue = value + search_pos;
	left = value;

	for(;;) {
tryagain:
		if (tvalue) {
			value = const_cast<char *>( strchr(tvalue, '$') );
		}
		
		if( value == NULL ) {
			return( 0 );
		}

			// Do not treat $$(foo) as a macro unless
			// getdollardollar = true.  This is for
			// condor_submit, so it does not try to expand
			// macros when we do "executable = foo.$$(arch)"
			// If getdollardollar is true, than only get
			// macros with two dollar signs, i.e. $$(foo).
		if ( getdollardollar ) {
			if ( *++value != '$' ) {
				// this is not a $$ macro
				tvalue = value;
				goto tryagain;
			}
		} else {
			// here getdollardollar is false, so ignore
			// any $$(foo) macro.
			if ( (*(value + sizeof(char))) == '$' ) {
				value++; // skip current $
				value++; // skip following $
				tvalue = value;
				goto tryagain;
			}
		}

		if( *++value == '(' ) {
			if( getdollardollar && *value != '\0' && value[1] == '[' ) {

				// This is a classad expression to be evaled.  This layer
				// doesn't need any intelligence, just scan for "])" which
				// terminates it.  If we get to end of string without finding
				// the terminating pattern, this $$ match fails, try again.

				char * end_marker = strstr(value, "])");
				if( end_marker == NULL ) {
					tvalue = value;
					goto tryagain;
				}

				left_end = value - 2;
				name = ++value;
				right = end_marker + 1;
				break;

			} else { 

				// This might be a "normal" values $(FOO), $$(FOO) or
				// $$(FOO:Bar) Skim until ")".  We only allow valid characters
				// inside (basically alpha-numeric. $$ adds ":"); a non-valid
				// character means this $$ match fails.  End of string before
				// ")" means this match fails, try again.

				if ( getdollardollar ) {
					left_end = value - 2;
				} else {
					left_end = value - 1;
				}
				name = ++value;
				while( *value && *value != ')' ) {
					char c = *value++;
				   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
					if ( ! after_colon && c == ':') {
						after_colon = true;
					} else if (after_colon) {
						if (c == '(') {
							// skip ahead past the close )
							char * ptr = strchr(value, ')');
							if (ptr) value = ptr+1;
							continue;
						} else if (strchr("$ ,\\", c)) {
							// allow some characters after the : that we don't allow in param names
							continue;
						}
					}
					if( !ISDDCHAR(c) ) {
						tvalue = name;
						goto tryagain;
					}
				   #else
					if( getdollardollar ) {
						if( !ISDDCHAR(c) ) {
							tvalue = name;
							goto tryagain;
						}
					} else {
						if( !ISIDCHAR(c) ) {
							tvalue = name;
							goto tryagain;
						}
					}
				   #endif
				}

				if( *value == ')' ) {
					// We pass (value-name) to strncmp since name contains
					// the entire line starting from the identifier and value
					// points to the end of the identifier.  Thus, the difference
					// between these two pointers gives us the length of the
					// identifier.
					int namelen = value-name;
					if( !self || ( strncasecmp( name, self, namelen ) == MATCH && self[namelen] == '\0' ) ) {
							// $(DOLLAR) has special meaning; it is
							// set to "$" and is _not_ recursively
							// expanded.  To implement this, we have
							// find_config_macro() ignore $(DOLLAR) and we then
							// handle it in expand_macro().
							// Note that $$(DOLLARDOLLAR) is handled a little
							// differently.  Instead of skipping over it,
							// we treat it like anything else, and it is up
							// to the caller to advance search_pos, so we
							// don't run into the literal $$ again.
						if ( !self && namelen == DOLLAR_ID_LEN &&
								strncasecmp(name,DOLLAR_ID,namelen) == MATCH ) {
							tvalue = name;
							goto tryagain;
						}
						right = value;
						break;
					} else {
						tvalue = name;
						goto tryagain;
					}
				} else {
					tvalue = name;
					goto tryagain;
				}
			}
		} else {
			tvalue = value;
			goto tryagain;
		}
	}

	*left_end = '\0';
	*right++ = '\0';

	*leftp = left;
	*namep = name;
	*rightp = right;

	return( 1 );
}

/*
** Return the value associated with the named parameter.  Return NULL
** if the given parameter is not defined.
*/
const char * lookup_macro_exact(const char *name, MACRO_SET & set, int use)
{
	MACRO_ITEM* pitem = find_macro_item(name, set);
	if (pitem) {
		if (set.metat) {
			MACRO_META* pmeta = &set.metat[pitem - set.table];
			pmeta->use_count += (use&1);
			pmeta->ref_count += (use>>1)&1;
		}
		return pitem->raw_value;
	}
	return NULL;
}

const char * lookup_macro(const char *name, const char *prefix, MACRO_SET & set, int use)
{
	MyString prefixed_name;
	if (prefix) {
		// snprintf(tmp_name, MAX_PARAM_LEN, "%s.%s", prefix, name);
		prefixed_name.formatstr("%s.%s", prefix, name);
		name = prefixed_name.Value();
	}
	return lookup_macro_exact(name, set, use);
}

