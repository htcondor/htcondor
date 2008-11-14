/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include "translation_utils.h"


const char *
getNameFromNum( int num, const struct Translation *table )
{
	if( num < 0 ) {
		return NULL;
	}

	int i;

	for( i=0 ; (table[i].name != NULL) ; i++ ) {
		if ( table[i].number == num ) {
			return table[i].name;
		}
	}
	return NULL;
}


int
getNumFromName( const char* str, const struct Translation *table )
{

	if( !str ) {
		return -1;
	}

	int i;
	
	for( i=0 ; (table[i].name != NULL) ; i++ ) {
		if ( !stricmp ( table[i].name, str ) ) {
			return table[i].number;
		}
	}
	return -1;
}

