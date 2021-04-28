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

#ifndef __DOMAIN_TOOLS_H
#define __DOMAIN_TOOLS_H

#include <string>

// returns true if the two specified accounts and domains match
bool
domainAndNameMatch( const char *account1, 
		                 const char *account2,
						 const char *domain1, 
						 const char *domain2 );

// takes string of the form <DOMAIN_NAME>\<ACCOUNT_NAME> 
// and chops it into two strings
void
getDomainAndName( char* namestr, char* &domain, char* &name );

void
joinDomainAndName( char const *domain, char const *name, std::string &result );

#endif  /* __DOMAIN_TOOLS_H */
