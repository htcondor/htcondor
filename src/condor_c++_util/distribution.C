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
