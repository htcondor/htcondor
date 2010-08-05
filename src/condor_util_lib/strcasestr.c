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
#include "strcasestr.h"

#if ( !HAVE_STRCASESTR )
// Like strstr(), but case-insensitive
char *
strcasestr( const char *string, const char *pattern )
{
	char	*str;
	int			n;

	n = strlen( pattern );
	for( str=(char *)string; *str; str++ ) {
		if( strncasecmp(str,pattern,n) == 0 ) {
			return str;
		}
	}
	return NULL;
}
#endif
