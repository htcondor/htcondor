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
#include "string_funcs.h"

/* Convert a string to it's "upper case first" match */
char *
getUcFirst( const char *orig )
{
	int		len = strlen(orig);
	char	*new = calloc( 1, len+1 );	/* calloc() zeros memory */
	char	*tmp = new;

	*tmp++ = toupper( *orig++ );
	while( *orig ) {
		*tmp++ = tolower( *orig++ );
	}
	return new;
}

/*
### Local Variables: ***
### mode:c++ ***
### style:cc-mode ***
### tab-width:4 ***
### End: ***
*/
