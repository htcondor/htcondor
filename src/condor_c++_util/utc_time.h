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

		/// Default constructor, does not compute current time by default
	UtcTime( bool get_time = false );

		/// Compute the current time
	void getTime( void );

		/// Return the last computed epoch time in seconds
	long seconds( void ) const { return sec; };

		/// Return mircosecond field of the last computed epoch time
	long microseconds( void ) const { return usec; };

		// Return the last computed time as a floating point combination
		// of seconds and microseconds
	double combined( void ) const { return( sec + usec/1000000.0 ); };

		/** How much time elapsed between the two times.  This method
			subtracts the time of the other UtcTime object we're
			passed from the value in this current object.
			@param other_time Another UtcTime class to compare against
			@return The elapsed time between the two, represented as a
			double precision float, with both seconds and micro
			seconds in the same number.
		 */
	double difference( const UtcTime* other_time ) const;
	double difference( const UtcTime &other_time ) const;

private:

	long sec;
	long usec;
};

bool operator==(const UtcTime &lhs, const UtcTime &rhs);

#endif /* _CONDOR_UTC_TIME_H */

