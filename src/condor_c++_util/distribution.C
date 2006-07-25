/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_distribution.h"

// Constructor
Distribution::Distribution()
{
	// Are we 'Condor' or 'Hawkeye'?
		SetDistribution( "condor" );
}

int Distribution::Init( int argc, char **argv )
{
	char	*argv0 = argv[0];

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
