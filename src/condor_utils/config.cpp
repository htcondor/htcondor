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
#include "condor_config.h"
#include "condor_string.h"
#include "extra_param_info.h"
#include "condor_random_num.h"
#include "condor_uid.h"
#include "my_popen.h"

#if defined(__cplusplus)
extern "C" {
#endif

static char *getline_implementation( FILE *, int );

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


int
Read_config( const char* config_source, BUCKET** table, 
			 int table_size, int expand_flag,
			 bool check_runtime_security,
			 ExtraParamTable *extra_info,
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

	if (subsys && ! *subsys) subsys = NULL;

	ConfigLineNo = 0;
   
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
		name = getline_implementation(conf_fp, 128);
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
		if( *name == '#' || blankline(name) )
			continue;

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
		name = expand_macro( name, table, table_size );
		if( name == NULL ) {
			retval = -1;
			goto cleanup;
		}

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
			value = expand_macro( rhs, table, table_size );
			if( value == NULL ) {
				retval = -1;
				goto cleanup;
			}
		} else  {
			/* expand self references only */
			PRAGMA_REMIND("tj: this handles only trivial self-refs, needs rethink.")
			value = expand_macro( rhs, table, table_size, name, false, subsys );
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

			/* Put the value in the Configuration Table */
			insert( name, value, table, table_size );
			if (extra_info != NULL) {
				extra_info->AddFileParam(name, config_source, ConfigLineNo);
			}
		} else {
			fprintf( stderr,
				"Configuration Error File <%s>, Line %d: Syntax Error\n",
				config_source, ConfigLineNo );
			retval = -1;
			goto cleanup;
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

// case-insensitive hash, usable when the character set of name
// is restricted to A-Za-Z0-9 and symbols except []{}\|^~
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

/*
** Insert the parameter name and value into the given hash table.
*/
void
insert( const char *name, const char *value, BUCKET **table, int table_size )
{
	register BUCKET	*ptr;
	int		loc;
	BUCKET	*bucket;
	char	tmp_name[ MAX_PARAM_LEN ],*tvalue;

		/* Make sure not already in hash table */
	snprintf( tmp_name, MAX_PARAM_LEN, "%s", name);
	tmp_name[MAX_PARAM_LEN-1] = '\0';
	strlwr( tmp_name );
	loc = condor_hash( tmp_name, table_size );
	for( ptr=table[loc]; ptr; ptr=ptr->next ) {
		if( strcmp(tmp_name,ptr->name) == 0 ) {
			tvalue = expand_macro(value,table,table_size,name,true);
			FREE( ptr->value );
			ptr->value = tvalue;
			return;
		}
	}

		/* Insert it */
	bucket = (BUCKET *)MALLOC( sizeof(BUCKET) );
	ASSERT( bucket != NULL );
	bucket->name = strdup( tmp_name );
	bucket->value = strdup( value );
#ifdef PARAM_USE_COUNTING
	bucket->use_count = 0;
	bucket->ref_count = 0;
#else
	bucket->used = 0;
#endif
	bucket->next = table[ loc ];
	table[loc] = bucket;
}

/*
** Sets the given macro's state to used
*/
#ifdef PARAM_USE_COUNTING
static BUCKET* find_bucket (const char *name, BUCKET *table[], int table_size)
{
	/* find the macro in the hash table */
	int loc = condor_hash_symbol_name( name, table_size );
	for (BUCKET * ptr = table[loc]; ptr; ptr = ptr->next ) {
		if (MATCH == strcasecmp(name, ptr->name)) {
			return ptr;
		}
	}
	return 0;
}

int
increment_macro_use_count ( const char *name, BUCKET *table[], int table_size )
{
	BUCKET	*ptr = find_bucket(name, table, table_size);
	if (ptr) {
		return ++(ptr->use_count);
	}
	return -1;
}

void
clear_macro_use_count ( const char *name, BUCKET *table[], int table_size )
{
	BUCKET	*ptr = find_bucket(name, table, table_size);
	if (ptr) {
		ptr->use_count = 0;
	}
}

int
get_macro_use_count ( const char *name, BUCKET *table[], int table_size )
{
	BUCKET	*ptr = find_bucket(name, table, table_size);
	if (ptr) {
		return ptr->use_count;
	}
	return -1;
}

int
get_macro_ref_count ( const char *name, BUCKET *table[], int table_size )
{
	BUCKET	*ptr = find_bucket(name, table, table_size);
	if (ptr) {
		return ptr->ref_count;
	}
	return -1;
}
#else
void 
set_macro_used ( const char *name, int used, BUCKET *table[], int table_size )
{
	register BUCKET	*ptr;
	int		loc;
	char	tmp_name[ MAX_PARAM_LEN ];
	
	/* find the macro in the hash table */
	snprintf( tmp_name, MAX_PARAM_LEN, "%s", name);
	tmp_name[MAX_PARAM_LEN-1] = '\0';
	strlwr( tmp_name );
	loc = condor_hash( tmp_name, table_size );
	for( ptr=table[loc]; ptr; ptr=ptr->next ) {
		if( strcmp(tmp_name,ptr->name) == 0 ) {
			ptr->used = used;
			return;
		}
	}
}
#endif

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
	return getline_implementation(fp,_POSIX_ARG_MAX);
}
static char *
getline_implementation( FILE *fp, int requested_bufsize )
{
	static char	*buf = NULL;
	static unsigned int buflen = 0;
	char	*end_ptr;		// Pointer to read into next read
	char    *line_ptr;

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

		end_ptr += strlen(end_ptr);
		if( end_ptr[-1] != '\n' ) {
			// if we made it here, fgets() ran out of buffer space.
			// move our read_ptr pointer forward so we concatenate the
			// rest on after we realloc our buffer above.
			continue;	// since we are not finished reading this line
		}

		ConfigLineNo++;

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
		if( ptr != line_ptr ) {
			(void)memmove( line_ptr, ptr, end_ptr-ptr+1 );
			end_ptr = (end_ptr - ptr) + line_ptr;
		}

		if( end_ptr > buf && end_ptr[-1] == '\\' ) {
			/* Ok read the continuation and concatenate it on */
			*(--end_ptr) = '\0';
			line_ptr = end_ptr;
			continue;
		}
		return buf;
	}
}

/* 
** Utility function to get an integer from a string.
** Returns: -1 if input is NULL, -2 if input is invalid, 0 for success
*/
int
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
char *
expand_macro( const char *value,
			  BUCKET **table,
			  int table_size,
			  const char *self,
			  bool use_default_param_table,
			  const char *subsys,
			  int use)
{
	char *tmp = strdup( value );
	char *left, *name, *right;
	const char *tvalue;
	char *rval;
	const char *selfless = NULL;

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
			tvalue = lookup_and_use_macro( name, subsys, table, table_size, use );
			if (subsys && ! tvalue)
				tvalue = lookup_and_use_macro( name, NULL, table, table_size, use );

				// Note that if 'name' has been explicitly set to nothing,
				// tvalue will _not_ be NULL so we will not call
				// param_default_string().  See gittrack #1302
			if( !self && use_default_param_table && tvalue == NULL ) {
				tvalue = param_default_string(name, subsys);
			}
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

// Local helper only.  If iter's current pointer is
// null, move along to the beginning of the next non-empty 
// bucket.  If there are no more non-empty buckets, leave
// it set to null and advance iter's index past the end (to
// make it clear we're done)
static void
hash_iter_scoot_to_next_bucket(HASHITER p)
{
	while(p->current == NULL) {
		(p->index)++;
		if(p->index >= p->table_size) {
			// The hash table is empty
			return;
		}
		p->current = p->table[p->index];
	}
}

HASHITER 
hash_iter_begin(BUCKET ** table, int table_size)
{
	ASSERT(table != NULL);
	ASSERT(table_size > 0);
	hash_iter * p = (hash_iter *)MALLOC(sizeof(hash_iter));
	ASSERT( p != NULL );
	p->table = table;
	p->table_size = table_size;
	p->index = 0;
	p->current = p->table[p->index];
	hash_iter_scoot_to_next_bucket(p);
	return p;
}

int
hash_iter_done(HASHITER iter)
{
	ASSERT(iter);
	ASSERT(iter->table); // Probably trying to use a iter already hash_iter_delete()d
	return iter->current == NULL;
}

int 
hash_iter_next(HASHITER iter)
{
	ASSERT(iter);
	ASSERT(iter->table); // Probably trying to use a iter already hash_iter_delete()d
	if(hash_iter_done(iter)) {
		return 0;
	}
	iter->current = iter->current->next;
	hash_iter_scoot_to_next_bucket(iter);
	return (iter->current) ? 1 : 0;
}

char * 
hash_iter_key(HASHITER iter)
{
	ASSERT(iter);
	ASSERT(iter->table); // Probably trying to use a iter already hash_iter_delete()d
	ASSERT( ! hash_iter_done(iter) );
	return iter->current->name;
}

char * 
hash_iter_value(HASHITER iter)
{
	ASSERT(iter);
	ASSERT(iter->table); // Probably trying to use a iter already hash_iter_delete()d
	ASSERT( ! hash_iter_done(iter) );
	return iter->current->value;
}

int
hash_iter_used_value(HASHITER iter)
{
	ASSERT(iter);
	ASSERT(iter->table); // Probably trying to use a iter already hash_iter_delete()d
	ASSERT( ! hash_iter_done(iter) );
#ifdef PARAM_USE_COUNTING
	return iter->current->use_count;
#else
	return iter->current->used;
#endif
}

void 
hash_iter_delete(HASHITER * iter)
{
	ASSERT(iter);
	ASSERT(iter[0]);
	ASSERT(iter[0]->table); // Probably trying to use a iter already hash_iter_delete()d
	iter[0]->table = NULL;
	free(*iter);
	*iter = 0;
}


/*
** Same as find_config_macro() below, but finds special references like $ENV().
*/
int
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
int
find_config_macro( register char *value, register char **leftp, 
		 register char **namep, register char **rightp,
		 const char *self,
		 bool getdollardollar, int search_pos)
{
	char *left, *left_end, *name, *right;
	char *tvalue;

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

#if defined(__cplusplus)
}

/*
** Return the value associated with the named parameter.  Return NULL
** if the given parameter is not defined.
*/
BUCKET *
lookup_macro_bucket(const char *name, BUCKET **table, int table_size)
{
	int loc = condor_hash(name, table_size);
	for (BUCKET* ptr = table[loc]; ptr; ptr = ptr->next) {
		if ( ! strcmp(name,ptr->name)) {
			return ptr;
		}
	}
	return NULL;
}

char *
lookup_macro_lower( const char *name, BUCKET **table, int table_size, int use )
{
	int				loc;
	register BUCKET	*ptr;

	loc = condor_hash( name, table_size );
	for( ptr=table[loc]; ptr; ptr=ptr->next ) {
		if( !strcmp(name,ptr->name) ) {
#ifdef PARAM_USE_COUNTING
			ptr->use_count += (use&1);
			ptr->ref_count += (use>>1)&1;
#else
			ptr->used |= use;
#endif
			return ptr->value;
		}
	}
	return NULL;
}

char *
lookup_macro( const char *name, const char *prefix, BUCKET **table, int table_size )
{
	char			tmp_name[ MAX_PARAM_LEN ];

	if (prefix) {
		snprintf(tmp_name, MAX_PARAM_LEN, "%s.%s", prefix, name);
	} else {
		// snprintf() is faster than strncpy() for large target buffers,
		// because strncpy() nulls out rest of buffer
		snprintf(tmp_name, MAX_PARAM_LEN, "%s", name);
	}
	tmp_name[MAX_PARAM_LEN-1] = '\0';
	strlwr( tmp_name );
	return lookup_macro_lower(tmp_name, table, table_size, 3);
}

char *
lookup_and_use_macro( const char *name, const char *prefix, BUCKET **table, int table_size, int use )
{
	char	tmp_name[ MAX_PARAM_LEN ];

	if (prefix) {
		snprintf(tmp_name, MAX_PARAM_LEN, "%s.%s", prefix, name);
	} else {
		// snprintf() is faster than strncpy() for large target buffers,
		// because strncpy() nulls out rest of buffer
		snprintf(tmp_name, MAX_PARAM_LEN, "%s", name);
	}
	tmp_name[MAX_PARAM_LEN-1] = '\0';
	strlwr( tmp_name );
	return lookup_macro_lower(tmp_name, table, table_size, use);
}

#endif
