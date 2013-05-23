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
#include "condor_distribution.h"

// Constructor
Distribution::Distribution()
{
	// Are we 'Condor' or 'Hawkeye'?
	SetDistribution( "condor\0CONDOR\0Condor\0" );
}

int
Distribution::Init( int  /*argc*/, const char **argv )
{
	return Init( argv[0] );
}

// Non-const version for backward compatibility
int
Distribution::Init( int  /*argc*/, char **argv )
{
	return Init( argv[0] );
}

int
Distribution::Init( const char *argv0 )
{
	// Are we 'Condor' or 'Hawkeye'?
	if (  ( strstr ( argv0, "hawkeye" ) ) ||
		  ( strstr ( argv0, "Hawkeye" ) ) ||
		  ( strstr ( argv0, "HAWKEYE" ) )  ) {
		SetDistribution( "hawkeye\0HAWKEYE\0Hawkeye\0" );
	} else {
		SetDistribution( "condor\0CONDOR\0Condor\0" );
	}

	return 1;
}

// Destructor (does nothing for now)
Distribution::~Distribution( )
{
}

// Set my actual distro name, in lowercase, all UPPERCASE, and Capitalized
void Distribution :: SetDistribution( const char *names )
{
	// names is expected to be of the form "name\0NAME\0Name\0";
	distribution_cap = distribution_uc = distribution = names;
	distribution_length = strlen(distribution);
	//ASSERT(distribution_length <= MAX_DISTRIBUTION_NAME);
	if (distribution_length > 0) {
		distribution_uc = distribution + distribution_length +1;
		distribution_cap = distribution_uc + strlen(distribution_uc) +1;
	}
}
