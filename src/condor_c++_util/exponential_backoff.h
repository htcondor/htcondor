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
