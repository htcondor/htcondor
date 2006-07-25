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



