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
	SetDistribution( "condor" );
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
		SetDistribution( "hawkeye" );
	} else {
		SetDistribution( "condor" );
	}

	return 1;
}

// Destructor (does nothing for now)
Distribution::~Distribution( )
{
}

// Set my actual distro name
void Distribution :: SetDistribution( const char *name )
{
	// Make my own private copies of the name
	strncpy( distribution, name, MAX_DISTRIBUTION_NAME );
	distribution[MAX_DISTRIBUTION_NAME] = '\0';
	strcpy( distribution_uc, distribution );
	strcpy( distribution_cap, distribution );

	// Make the 'uc' version upper case
	char	*cp = distribution_uc;
	while ( *cp )
	{
		char	c = *cp;
		*cp = toupper( c );
		cp++;
	}

	// Capitalize the first char of the Cap version
	char	c = distribution_cap[0];
	distribution_cap[0] = toupper( c );

	// Cache away it's length
	distribution_length = strlen( distribution );
}
