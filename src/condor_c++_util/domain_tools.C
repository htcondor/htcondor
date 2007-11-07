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
#include "domain_tools.h"
#include "condor_debug.h"
#include "MyString.h"

bool
domainAndNameMatch( const char *account1,
                    const char *account2,
				   	const char *domain1,
				   	const char *domain2 ) 
{
	
	return ( ( stricmp ( account1, account2 ) == 0 ) && 
		( domain1 == NULL || domain1 == "" || 
		stricmp ( domain1, domain2 ) == 0 ) );

}

void
getDomainAndName( char* namestr, char* &domain, char* &name ) {
	char* nameptr = strrchr ( namestr, '\\' ); 
	if ( nameptr != NULL ) {
		domain = namestr;
		*nameptr = '\0';
		name = nameptr+1;
	} else {
		name = namestr;
		domain = NULL;
	}
}

void
joinDomainAndName( char const *domain, char const *name, class MyString &result )
{
	ASSERT( name );
	if( !domain ) {
		result = name;
	}
	else {
		result.sprintf("%s\\%s",domain,name);
	}
}
