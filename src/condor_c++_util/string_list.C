/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h" 

#include "string_list.h"
#include "condor_debug.h"

// initialize the List<char> from the VALID_*_FILES variable in the
// config file; the files are separated by commas
	//changed isSeparator to allow constructor to redefine
//#define isSeparator(x) (isspace(x) || x == ',' )

char *strnewp( const char * );

int
StringList::isSeparator( char x )
{
	for ( char *sep = delimiters; *sep; sep++ ) {
		if ( x == ( *sep ) ) {
			return 1;
		}
	}
	return 0;
}

StringList::StringList(const char *s, const char *delim ) 
{
	delimiters = strnewp( delim );
	if ( s ) {
		initializeFromString(s);
	}
}

void
StringList::initializeFromString (const char *s)
{
	char *walk_ptr = (char*)s;

	while (*walk_ptr != '\0')
	{
		// skip leading separators & whitespace
		while ((isSeparator (*walk_ptr) || isspace(*walk_ptr)) 
					&& *walk_ptr != '\0') 
			walk_ptr++;

		// mark the beginning of this String in the list.
		char *begin_ptr = walk_ptr;

		// walk to the end of this string
		while (!isSeparator (*walk_ptr) && *walk_ptr != '\0')
			walk_ptr++;

		// malloc new space for just this item
		int len = (walk_ptr - begin_ptr);
		char *tmp_string = (char*)malloc( 1 + len );
		strncpy (tmp_string, begin_ptr, len);
		tmp_string[len] = '\0';
		
		// put the string into the StringList
		strings.Append (tmp_string);
	}
}

void
StringList::print (void)
{
	char *x;
	strings.Rewind ();
	while ((x = strings.Next ()))
		printf ("[%s]\n", x);
}

StringList::~StringList ()
{
	char *x;
	strings.Rewind ();
	while ((x = strings.Next ()))
	{
		deleteCurrent();
	}
	if ( delimiters )
		delete [] delimiters;
}

BOOLEAN
StringList::contains( const char *st )
{
	char	*x;

	strings.Rewind ();
	while ((x = strings.Next ())) {
		if( strcmp(st, x) == MATCH ) {
			return TRUE;
		}
	}
	return FALSE;
}


BOOLEAN
StringList::contains_anycase( const char *st )
{
	char	*x;

	strings.Rewind ();
	while ((x = strings.Next ())) {
		if( stricmp(st, x) == MATCH ) {
			return TRUE;
		}
	}
	return FALSE;
}


void
StringList::remove(const char *str)
{
	char *x;

	strings.Rewind();
	while ((x = strings.Next())) {
		if (strcmp(str, x) == MATCH) {
			deleteCurrent();
		}
	}
}

void
StringList::remove_anycase(const char *str)
{
	char *x;

	strings.Rewind();
	while ((x = strings.Next())) {
		if (stricmp(str, x) == MATCH) {
			deleteCurrent();
		}
	}
}

BOOLEAN
StringList::substring( const char *st )
{
	char    *x;
	int len;
	
	strings.Rewind ();
	while( (x = strings.Next()) ) {
		len = strlen(x);
		if( strncmp(st, x, len) == MATCH ) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOLEAN
StringList::contains_withwildcard(const char *string)
{
	return (contains_withwildcard(string, false) != NULL);
}

BOOLEAN
StringList::contains_anycase_withwildcard(const char *string)
{
	return (contains_withwildcard(string, true) != NULL);
}

const char * StringList :: string_anycase_withwildcard( const char * string)
{
    return contains_withwildcard(string, true);
}

// contains_withwildcard() is just like the contains() method except that
// list members can have an asterisk wildcard in them.  So, if
// the string passed in is "pppmiron.cs.wisc.edu", and an entry in the
// the list is "ppp*", then it will return TRUE.
const char *
StringList::contains_withwildcard(const char *string, bool anycase)
{
	char *x;
	char *matchstart;
	char *matchend;
	char *asterisk;
	int matchendlen, len;
    BOOLEAN result; 
	int temp;
	
	if ( !string )
		return NULL;

	strings.Rewind();

	while ( (x=strings.Next()) ) {

		if ( (asterisk = strchr(x,'*')) == NULL ) {
			// There is no wildcard in this entry; just compare
			if (anycase) {
				temp = strcasecmp(x,string);
			} else {
				temp = strcmp(x,string);
			}
			if ( temp == MATCH )
				return x;
			else
				continue;
		}

		if ( asterisk == x ) {
			// asterisk at the start behavior
			matchstart = NULL;
			matchend = &(x[1]);
		} else {
			if ( asterisk[1] == '\0' ) {
				// asterisk at the end behavior.
				*asterisk = '\0';	// remove asterisk
				if (anycase) {
					temp = strncasecmp(x,string,strlen(x));
				} else {
					temp = strncmp(x,string,strlen(x));
				}
				*asterisk = '*';	// replace asterisk
				if ( temp == MATCH ) {
					return x;
				} else {
					continue;
				}				
			} else {
				// asterisk must be in the middle somewhere				
				matchstart = x;
				matchend = &(asterisk[1]);
			}
		}

		// at this point, we know that there is an  asterisk either at the start
		// or in the middle somewhere, and both matchstart and matchend are set 
		// appropiately with what we want

		result = TRUE;
		*asterisk = '\0';	// replace asterisk with a NULL
		if ( matchstart ) {
			if ( anycase ) {
				temp = strncasecmp(matchstart,string,strlen(matchstart));
			} else {
				temp = strncmp(matchstart,string,strlen(matchstart));
			}
			if ( temp != MATCH ) 
				result = FALSE;
		}
		if ( matchend && result == TRUE) {
			len = strlen(string);
			matchendlen = strlen(matchend);
			if ( matchendlen > len )	// make certain we do not SEGV below
				result = FALSE;
			if ( result == TRUE ) {
				if (anycase) {
					temp = strcasecmp(&(string[len-matchendlen]),matchend);
				} else {
					temp = strcmp(&(string[len-matchendlen]),matchend);
				}
				if ( temp != MATCH )
					result = FALSE;
			}
		}
		*asterisk = '*';	// set asterisk back no matter what the result
		if ( result == TRUE ) {
			return x;
		}
	
	}	// end of while loop

	return NULL;
}

/* returns a malloc'ed string that contains a comma delimited list of
the internals of the string list. */
char*
StringList::print_to_string(void)
{
	char *tmp;
	int num, i;
	int sum = 0;

	strings.Rewind();
	num = strings.Number();

	/* no string at all if there isn't anything in it */
	if(num == 0)
	{
		return NULL;
	}

	for (i = 0; i < num; i++)
	{
		sum += strlen(strings.Next());
	}

	/* get memory for all of the strings, plus the ',' character between them
		and one more for the \0 */
	tmp = (char*)calloc(sum + num + 1, 1);
	if (tmp == NULL)
	{
		EXCEPT("Out of memory in StringList::print_to_string");
	}
	tmp[0] = '\0';

	strings.Rewind();
	for (i = 0; i < num; i++)
	{
		strcat(tmp, strings.Next());
		
		/* add commas until the last attr entry in the list */
		if (i < (num - 1))
		{
			strcat(tmp, ",");
		}
	}

	return tmp;
}


void
StringList::deleteCurrent() {
	if( strings.Current() ) {
		FREE( strings.Current() );
	}
	strings.DeleteCurrent();
}
