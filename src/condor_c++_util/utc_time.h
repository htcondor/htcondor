/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department, 
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

