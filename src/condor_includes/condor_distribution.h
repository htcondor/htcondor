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
#ifndef _CONDOR_DISTRIBUTION_H_
#define _CONDOR_DISTRIBUTION_H_

// Max length of the distribution name
static const int MAX_DISTRIBUTION_NAME = 20;

class Distribution
{
  public:
	int Init( int argc, char **argv );

	// Get my distribution name..
	const char *Get(void) { return distribution; };
	const char *GetUc() { return distribution_uc; };
	const char *GetCap() { return distribution_cap; };
	int GetLen() { return distribution_length; };

	Distribution( );
	~Distribution( );
	void	SetDistribution( const char *name = "condor" );

  private:
	char	distribution[ MAX_DISTRIBUTION_NAME + 1 ];
	char	distribution_uc[ MAX_DISTRIBUTION_NAME + 1];
	char	distribution_cap[ MAX_DISTRIBUTION_NAME + 1];
	int		distribution_length;
};

extern Distribution	myDistribution, *myDistro;

#endif	/* _CONDOR_DISTRIBUTION_H */



