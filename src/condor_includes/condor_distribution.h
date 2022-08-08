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

#define MY_DISTRO_NAME     "condor"
#define MY_DISTRO_NAME_UC  "CONDOR"
#define MY_DISTRO_NAME_Cap "Condor"
#define MY_DISTRO_NAME_Len 6

class Distribution
{
  public:
	// Get my distribution name..
	const char *Get(void) const { return MY_DISTRO_NAME;     };
	const char *GetUc()   const { return MY_DISTRO_NAME_UC;  };
	const char *GetCap()  const { return MY_DISTRO_NAME_Cap; };
	        int GetLen()  const { return MY_DISTRO_NAME_Len; };
};

constexpr const Distribution myDistribution;
constexpr const Distribution *myDistro = &myDistribution;

#endif	/* _CONDOR_DISTRIBUTION_H */



