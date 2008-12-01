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

#ifndef __LEASE_MANAGER_LEASE_H__
#define __LEASE_MANAGER_LEASE_H__

#include <list>
#include <string>
using namespace std;

class LeaseManagerLease
{
  public:
	LeaseManagerLease( const string &lease_id,
					   int duration = -1,
					   bool rel = true );
	~LeaseManagerLease( void );

	void setLeaseId( const string & );
	void setLeaseDuration( int );
	void setReleaseWhenDone( bool );

	const string &getLeaseId( void ) const { return m_id; };
	int getDuration( void ) const { return m_duration; };
	int getReleaseWhenDone( void ) const { return m_release_when_done; };

  private:
	string		m_id;
	int			m_duration;
	bool		m_release_when_done;
};

void LeaseManagerLease_FreeList( list<LeaseManagerLease *> &lease_list );

#endif//__LEASE_MANAGER_LEASE_H__
