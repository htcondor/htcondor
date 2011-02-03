/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#ifndef __DEBUG_TIMER_H__
#define __DEBUG_TIMER_H__

class DebugTimerBase
{
  public:
	DebugTimerBase( bool start = true );
	virtual ~DebugTimerBase( void );
	void Start( void );
	double Stop( void );		// stop + return diff
	double Elapsed( void );		// Seconds since started
	double Diff( void );		// stop time - start time
	void Log( const char *s, int count = -1, bool stop = true );
	virtual void Output( const char *) { };

  private:
	bool	m_on;
	double	m_t1;
	double	m_t2;
	double	dtime( void );
};

#endif//__DEBUG_TIMER_H__
