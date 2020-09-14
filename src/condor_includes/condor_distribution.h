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

#ifndef _CONDOR_DISTRIBUTION_H_
#define _CONDOR_DISTRIBUTION_H_

// Max length of the distribution name
static const int MAX_DISTRIBUTION_NAME = 20;

class Distribution
{
  public:
	int Init( int argc, char **argv );
	int Init( int argc, const char **argv );

	// Get my distribution name..
	const char *Get(void) { return distribution; };
	const char *GetUc() { return distribution_uc; };
	const char *GetCap() { return distribution_cap; };
	int GetLen() const { return distribution_length; };

	Distribution( );
	~Distribution( );

  private:
	const char * distribution;
	const char * distribution_uc;
	const char * distribution_cap;
	int		distribution_length;

	int Init( const char *argv0 );
	void	SetDistribution( const char *names );
};

extern Distribution	myDistribution, *myDistro;

#endif	/* _CONDOR_DISTRIBUTION_H */



