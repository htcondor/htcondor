/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

#ifndef _CONDOR_DC_LEASE_MANAGER_H
#define _CONDOR_DC_LEASE_MANAGER_H

#include <list>
#include <string>
#include "condor_common.h"
#include "stream.h"
#include "daemon.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;

#include "dc_lease_manager_lease.h"

/** The subclass of the Daemon object for talking to a lease manager daemon
*/
class DCLeaseManager : public Daemon
{
  public:

		/** Constructor.  Same as a Daemon object.
		  @param name The name (or sinful string) of the daemon, NULL
		              if you want local
		  @param pool The name of the pool, NULL if you want local
		*/
	DCLeaseManager( const char* const name = NULL, const char* pool = NULL );

		/// Destructor.
	~DCLeaseManager( );


		/** Get lease(s) which to match the requirements passed in
			@param requestor_name The logical name of the requestor
			@param num The number of of leases requested
			@param duration The requested duration (in seconds) of the leases
			@param requirements The requirements expression for the match
			@param rank The rank expression for the match (ignored for now)
			@param leases STL List of lease information
			The list pointers should be delete()ed when no longer used
			@return true on success, false on invalid input (NULL)
		*/
	bool getLeases( const char *requestor_name,
					int num, int duration,
					const char* requirements, const char *rank,
					list< DCLeaseManagerLease *> &leases );


		/** Get lease(s) which to match the requirements passed in
			@param ad (New) ClassAd which discribe the request
			@param leases STL List of lease information
			The list pointers should be delete()ed when no longer used
			@return true on success, false on invalid input (NULL)
		*/
	bool getLeases( const classad::ClassAd &ad,
					list< DCLeaseManagerLease *> &leases );


		/** Renew the leases specified
			@param leases STL List of leases to renew
			Lease ID & duration are required
			@param out_leases STL list of renewed leases
			The list pointers should be delete()ed when no longer used
		*/
	bool renewLeases( list< const DCLeaseManagerLease *> &leases,
					  list< DCLeaseManagerLease *> &out_leases );


		/** Release the leases specified
			@param leases STL list of lease information on leases to release
			@return true on success, false on invalid input (NULL)
		*/
	bool releaseLeases( list <DCLeaseManagerLease *> &leases );


 private:

		// I can't be copied (yet)
	DCLeaseManager( const DCLeaseManager& );
	DCLeaseManager& operator = ( const DCLeaseManager& );

	// Helper methods to get/send leases
	bool SendLeases(
		Stream								*stream,
		list< const DCLeaseManagerLease *> &l_list
		);
	bool GetLeases(
		Stream								*stream,
		std::list< DCLeaseManagerLease *>	&l_list
		);

};

#endif /* _CONDOR_DC_LEASE_MANAGER_H */
