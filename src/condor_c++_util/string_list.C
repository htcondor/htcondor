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

 

#include "string_list.h"
#include "condor_debug.h"

// initialize the List<char> from the VALID_*_FILES variable in the
// config file; the files are separated by commas
#define isSeparator(x) (isspace(x) || x == ',')

void
StringList::initializeFromString (char *s)
{
	char buffer[1024];
	char *ptr = s;
	int  index = 0;

	while (*ptr != '\0')
	{
		// skip leading separators
		while (isSeparator (*ptr) && *ptr != '\0') 
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
		FREE (x);
		strings.DeleteCurrent ();
	}
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
