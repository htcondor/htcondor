/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#if !defined(_CONDOR_UTC_TIME_H)
#define _CONDOR_UTC_TIME_H


/** The UtcTime class is a C++ representation of the current UTC time.
	It's a portable way to get finer granularity than seconds (we
	provide microseconds).  It also provides a method to compare
	two UtcTime objects and get the difference between them.
*/

class UtcTime 
{
public:

		/// Default constructor, does not compute current time
	UtcTime();

		/// Compute the current time
	void getTime( void );

		/// Return the last computed epoch time in seconds
	long seconds( void ) { return sec; };

		/// Return mircosecond field of the last computed epoch time
	long microseconds( void ) { return usec; };

		/** How much time elapsed between the two times.  This method
			subtracts the time of the other UtcTime object we're
			passed from the value in this current object.
			@param other_time Another UtcTime class to compare against
			@return The elapsed time between the two, represented as a
			double precision float, with both seconds and micro
			seconds in the same number.
		 */
	double difference( UtcTime* other_time );

private:

	long sec;
	long usec;
};

#endif /* _CONDOR_UTC_TIME_H */

