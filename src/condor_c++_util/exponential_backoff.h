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

#ifndef EXPONENTIAL_BACKOFF_H
#define EXPONENTIAL_BACKOFF_H

#include "condor_common.h"

/*
  An implementation of exponential backoff.
  - Joe Meehean 12/19/05
*/

class ExponentialBackoff{

		// Variables
 private:
	static int NEXT_SEED;
	
	int min;
	int max;
	double base;
	int seed;
	unsigned int tries;
	int prevBackoff;

	
		// Functions
 public:
	ExponentialBackoff(int min, int max, double base);	
	ExponentialBackoff(int min, int max, double base, int seed);
	ExponentialBackoff(const ExponentialBackoff& orig);
	ExponentialBackoff& operator =(const ExponentialBackoff& rhs);
	virtual ~ExponentialBackoff();

		/*
		  Resets the backoff to assume 0 tries.
		*/
	void freshStart();
		/*
		  Returns the next deterministic backoff (base^tries).
		 */
	int nextBackoff();
		/*
		  Returns a random back off between min and base^tries.
		 */
	int nextRandomBackoff();
	
		/*
		  Returns the last backoff returned by this class.
		*/
	int previousBackoff();

		/*
		  Returns the number of tries since the last freshStart()
		 */
	int numberOfTries();

 protected:
	void init(int min, int max, double base, int seed);
	void deepCopy(const ExponentialBackoff& orig);
	void noLeak();

 private:
	ExponentialBackoff();

};

#endif
