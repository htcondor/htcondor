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

StringList::StringList(char *s, char *delim ) 
{
	delimiters = strnewp( delim );
	if ( s ) {
		initializeFromString(s);
	}
}

void
StringList::initializeFromString (char *s)
{
	char buffer[1024];
	char *ptr = s;
	int  index = 0;

	while (*ptr != '\0')
	{
		// skip leading separators & whitespace
		while ((isSeparator (*ptr) || isspace(*ptr)) 
					&& *ptr != '\0') 
			ptr++;

		// copy filename into buffer
		index = 0;
		while (!isSeparator (*ptr) && *ptr != '\0')
			buffer[index++] = *ptr++;

		// put the string into the StringList
		buffer[index] = '\0';
		strings.Append (strdup (buffer));
	}
}

void
StringList::print (void)
{
	char *x;
	strings.Rewind ();
	while (x = strings.Next ())
		printf ("[%s]\n", x);
}

StringList::~StringList ()
{
	char *x;
	strings.Rewind ();
	while (x = strings.Next ())
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
	while (x = strings.Next ()) {
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
	while (x = strings.Next ()) {
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
	while (x = strings.Next()) {
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
	while (x = strings.Next()) {
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
	while( x = strings.Next() ) {
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
	return contains_withwildcard(string, false);
}

BOOLEAN
StringList::contains_anycase_withwildcard(const char *string)
{
	return contains_withwildcard(string, true);
}
	
// contains_withwildcard() is just like the contains() method except that
// list members can have an asterisk wildcard in them.  So, if
// the string passed in is "pppmiron.cs.wisc.edu", and an entry in the
// the list is "ppp*", then it will return TRUE.
BOOLEAN
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
		return FALSE;

	strings.Rewind();

	while ( x=strings.Next() ) {

		if ( (asterisk = strchr(x,'*')) == NULL ) {
			// There is no wildcard in this entry; just compare
			if (anycase) {
				temp = strcasecmp(x,string);
			} else {
				temp = strcmp(x,string);
			}
			if ( temp == MATCH )
				return TRUE;
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
					return TRUE;
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
			return TRUE;
		}
	
	}	// end of while loop

	return FALSE;
}


void
StringList::deleteCurrent() {
	if( strings.Current() ) {
		FREE( strings.Current() );
	}
	strings.DeleteCurrent();
}
