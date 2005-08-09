/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
 

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "extra_param_info.h"
#include "condor_random_num.h"

#if defined(__cplusplus)
extern "C" {
#endif

static char *getline_implementation( FILE *, int );

int		ConfigLineNo;

#if defined(LINUX)
int
condor_isalnum(int c)
{
	if( ('a' <= c && c <= 'z') ||
		('A' <= c && c <= 'Z') ||
		('0' <= c && c <= '9') ) {
		return 1;
	} else {
		return 0;
	}
}
#define ISIDCHAR(c)		(condor_isalnum(c) || ((c) == '_'))
#else
#define ISIDCHAR(c)		(isalnum(c) || ((c) == '_'))
#endif

// $$ expressions may also contain a colon
#define ISDDCHAR(c) ( ISIDCHAR(c) || ((c)==':') )

#define ISOP(c)		(((c) == '=') || ((c) == ':'))

// Magic macro to represent a dollar sign, i.e. $(DOLLAR)="$"
#define DOLLAR_ID "DOLLAR"

int Read_config( char* config_file, BUCKET** table, 
				 int table_size, int expand_flag,
				 ExtraParamTable *extra_info)
{
  	FILE	*conf_fp;
	char	*name, *value, *rhs;
	char	*ptr;
	char	op;

	ConfigLineNo = 0;

	conf_fp = fopen(config_file, "r");
	if( conf_fp == NULL ) {
		printf("Can't open file %s\n", config_file);
		return( -1 );
	}

	for(;;) {
		name = getline_implementation(conf_fp,128);
		if( name == NULL ) {
			break;
		}
		
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
				(void)fclose( conf_fp );
				return( -1 );
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
				(void)fclose( conf_fp );
				return( -1 );
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
			(void)fclose( conf_fp );
			return( -1 );
		}

		/* Check that "name" is a legal identifier : only
		   alphanumeric characters and _ allowed*/
		ptr = name;
		while( *ptr ) {
			char c = *ptr++;
			if( !ISIDCHAR(c) ) {
				(void) fclose( conf_fp );
				fprintf( stderr,
		"Configuration Error File <%s>, Line %d: Illegal Identifier: <%s>\n",
					config_file, ConfigLineNo, name );
				return( -1 );
			}
		}

		if( expand_flag == EXPAND_IMMEDIATE ) {
			value = expand_macro( rhs, table, table_size );
			if( value == NULL ) {
				(void)fclose( conf_fp );
				return( -1 );
			}
		} else  {
			/* expand self references only */
			value = expand_macro( rhs, table, table_size, name );
			if( value == NULL ) {
				(void)fclose( conf_fp );
				return( -1 ); 
			}
		}  

		if( op == ':' || op == '=' ) {
				/*
				  Yee haw!!! We can treat "expressions" and "macros"
				  the same way: just insert them into the config hash
				  table.  Everything now behaves like macros used to
				  Derek Wright <wright@cs.wisc.edu> 4/11/00
				*/
			strlwr( name );

			/* Put the value in the Configuration Table */
			insert( name, value, table, table_size );
			if (extra_info != NULL) {
				extra_info->AddFileParam(name, config_file, ConfigLineNo);
			}
		} else {
			fprintf( stderr,
				"Configuration Error File <%s>, Line %d: Syntax Error\n",
				config_file, ConfigLineNo );
			(void)fclose( conf_fp );
			return -1;
		}

		FREE( name );
		FREE( value );
	}

	(void)fclose( conf_fp );
	return 0;
}


/*
** Just compute a hash value for the given string such that
** 0 <= value < size 
*/
int
hash( register char *string, register int size )
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

/*
** Insert the parameter name and value into the given hash table.
*/
void
insert( const char *name, const char *value, BUCKET **table, int table_size )
{
	register BUCKET	*ptr;
	int		loc;
	BUCKET	*bucket;
	char	tmp_name[ 1024 ];

		/* Make sure not already in hash table */
	strcpy( tmp_name, name );
	strlwr( tmp_name );
	loc = hash( tmp_name, table_size );
	for( ptr=table[loc]; ptr; ptr=ptr->next ) {
		if( strcmp(tmp_name,ptr->name) == 0 ) {
			FREE( ptr->value );
			ptr->value = strdup( value );
			return;
		}
	}

		/* Insert it */
	bucket = (BUCKET *)MALLOC( sizeof(BUCKET) );
	bucket->name = strdup( tmp_name );
	bucket->value = strdup( value );
	bucket->next = table[ loc ];
	table[loc] = bucket;
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
	return getline_implementation(fp,_POSIX_ARG_MAX);
}
static char *
getline_implementation( FILE *fp, int requested_bufsize )
{
	static char	*buf = NULL;
	static unsigned int buflen = 0;
	char	*read_ptr;		// Pointer to read into next read
	char	*parse_ptr;		// Pointer to where to start parsing from

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

	if ( buflen != (unsigned int)requested_bufsize ) {
		if ( buf ) free(buf);
		buf = (char *)malloc(requested_bufsize);
		buflen = requested_bufsize;
	}
	buf[0] = '\0';
	read_ptr = buf;
	parse_ptr = buf;

	// Loop 'til we're done reading a whole line, including continutations
	for(;;) {
		int		len = buflen - (read_ptr - buf);
		if( len <= 5 ) {
			// we need a larger buffer -- grow buffer by 4kbytes
			char *newbuf = (char *)realloc(buf, 4096 + buflen);
			if ( newbuf ) {
				read_ptr = (read_ptr - buf) + newbuf;
				parse_ptr = (parse_ptr - buf) + newbuf;
				buf = newbuf;	// note: realloc() freed our old buf if needed
				buflen += 4096;
				len += 4096;
			} else {
				// malloc returned NULL, we're out of memory
				EXCEPT( "Out of memory - config file line too long" );
			}
		}

		if( fgets(read_ptr,len,fp) == NULL ) {
			if( buf[0] == '\0' ) {
				return NULL;
			} else {
				return buf;
			}
		}

		// See if fgets read an entire line, or simply ran out of buffer space
		if ( *read_ptr == '\0' ) {
			continue;
		}
		if( strrchr(read_ptr, '\n') == NULL ) {
			// if we made it here, fgets() ran out of buffer space.
			// move our read_ptr pointer forward so we concatenate the
			// rest on after we realloc our buffer above.
			read_ptr += strlen(read_ptr);
			continue;	// since we are not finished reading this line
		}

		ConfigLineNo++;

			/* See if a continuation is indicated */
		char	*ptr;			// Temp pointer
		ptr = ltrunc( parse_ptr );
		if( ptr != parse_ptr ) {
			(void)memmove( parse_ptr, ptr, strlen(ptr)+1 );
		}

		ptr = (char *) strrchr( parse_ptr, '\\' );
		if( ptr == NULL )
			return buf;
		if( *(ptr+1) != '\0' )
			return buf;

			/* Ok read the continuation and concatenate it on */
		parse_ptr = read_ptr = ptr;
	}
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
expand_macro( const char *value, BUCKET **table, int table_size, char *self )
{
	char *tmp = strdup( value );
	char *left, *name, *tvalue, *right;
	char *rval;

	bool all_done = false;
	while( !all_done ) {		// loop until all done expanding
		all_done = true;

		if( !self && get_special_var("$ENV",true,tmp, &left, &name, &right) ) 
		{
			all_done = false;
			tvalue = getenv(name);
			if( tvalue == NULL ) {
				EXCEPT("Can't find %s in environment!",name);
			}

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) +
											  strlen(right) + 1));
			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}

		if( !self && get_special_var("$RANDOM_CHOICE",false,tmp, &left, &name, 
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
				EXCEPT("$RANDOM_CHOICE() macro in config file empty!",name);
			}

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) +
											  strlen(right) + 1));
			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}

		if( get_var(tmp, &left, &name, &right, self) ) {
			all_done = false;
			tvalue = lookup_macro( name, table, table_size );
			if( tvalue == NULL ) {
				tvalue = "";
			}

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) +
											  strlen(right) + 1));
			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}
	}

	// Now, deal with the special $(DOLLAR) macro.
	if (!self)
	while( get_var(tmp, &left, &name, &right, DOLLAR_ID) ) {
		rval = (char *)MALLOC( (unsigned)(strlen(left) + 1 +
										  strlen(right) + 1));
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
** Same as get_var() below, but finds special references like $ENV().
*/
int
get_special_var( const char *prefix, bool only_id_chars, register char *value, 
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

	for(;;) {
tryagain:
		if (tvalue) {
			value = (char *)strstr( (const char *)tvalue, prefix );
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

/*
** If self is not NULL, then only look for the parameter specified by self.
*/
int
get_var( register char *value, register char **leftp, 
		 register char **namep, register char **rightp, char *self,
		 bool getdollardollar)
{
	char *left, *left_end, *name, *right;
	char *tvalue;
	int selflen = (self) ? strlen(self) : 0;

	tvalue = value;
	left = value;

	for(;;) {
tryagain:
		if (tvalue) {
			value = (char *)strchr( (const char *)tvalue, '$' );
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
				if( !self || ( namelen == selflen &&
							   strincmp( name, self, namelen ) == MATCH ) ) {
						// $(DOLLAR) has special meaning; it is
						// set to "$" and is _not_ recursively
						// expanded.  To implement this, we have
						// get_var() ignore $(DOLLAR) and we then
						// handle it in expand_macro().
					if ( !self && strincmp(name,DOLLAR_ID,namelen) == MATCH ) {
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
char *
lookup_macro( const char *name, BUCKET **table, int table_size )
{
	int				loc;
	register BUCKET	*ptr;
	char			tmp_name[ 1024 ];

	strcpy( tmp_name, name );
	strlwr( tmp_name );
	loc = hash( tmp_name, table_size );
	for( ptr=table[loc]; ptr; ptr=ptr->next ) {
		if( !strcmp(tmp_name,ptr->name) ) {
			return ptr->value;
		}
	}
	return NULL;
}

#if defined(__cplusplus)
}
#endif
