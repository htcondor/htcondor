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
#include "condor_debug.h"
#include "string_set.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

/*
  Create a new and empty string set.
*/
StringSet::StringSet()
{
	data = new char * [ INITIAL_SIZE ];
	cur_size = 0;
	max_size = INITIAL_SIZE;
}

StringSet::~StringSet()
{
	int		i;

	for( i=0; i<cur_size; i++ ) {
		delete [] data[i];
	}
	delete [] data;
}

/*
  Append a copy of the given string to the set.
*/
void
StringSet::append( const char *str )
{
	int		i;
	char	*str_copy;
	char	**new_data;

	if( contains(str) ) {
		return;
	}

	if( cur_size == max_size ) {
		new_data = new char * [ max_size * 2 ];
		for( i=0; i<max_size; i++ ) {
			new_data[i] = data[i];
		}
		delete [] data;
		data = new_data;
		max_size *= 2;
	}
	str_copy = new char [ strlen(str) + 1 ];
	strcpy( str_copy, str );
	data[ cur_size++ ] = str_copy;
}

int
StringSet::contains( const char *str )
{
	int		i;

	for( i=0; i<cur_size; i++ ) {
		if( strcmp(str,data[i]) == 0 ) {
			return TRUE;
		}
	}
	return FALSE;
}

/*
  Display the string set for debugging purposes.
*/
void
StringSet::display()
{
	int		i;

	dprintf( D_ALWAYS, "StringSet[ %d ] {\n", cur_size );
	for( i=0; i<cur_size; i++ ) {
		dprintf( D_ALWAYS, "\t%s\n", data[i] );
	}
	dprintf( D_ALWAYS, "}\n" );
}
	
