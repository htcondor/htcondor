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
#ifndef _CONDOR_DC_MATCH_MAKER_H
#define _CONDOR_DC_MATCH_MAKER_H

#include <list>
#include <string>
#include "condor_common.h"
#include "stream.h"
#include "daemon.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;

#include "dc_match_maker_lease.h"

/** The subclass of the Daemon object for talking to a match maker daemon
*/
class DCMatchMaker : public Daemon
{
  public:

		/** Constructor.  Same as a Daemon object.
		  @param name The name (or sinful string) of the daemon, NULL
		              if you want local  
		  @param pool The name of the pool, NULL if you want local
		*/
	DCMatchMaker( const char* const name = NULL, const char* pool = NULL );

		/// Destructor.
	~DCMatchMaker( );


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
	bool getMatches( const char *requestor_name,
					 int num, int duration,
					 const char* requirements, const char *rank,
					 list< DCMatchMakerLease *> &leases );


		/** Get lease(s) which to match the requirements passed in
			@param ad (New) ClassAd which discribe the request
			@param leases STL List of lease information
			The list pointers should be delete()ed when no longer used
			@return true on success, false on invalid input (NULL)
		*/
	bool getMatches( const classad::ClassAd &ad,
					 list< DCMatchMakerLease *> &leases );


		/** Renew the leases specified
			@param leases STL List of leases to renew
			Lease ID & duration are required
			@param out_leases STL list of renewed leases
			The list pointers should be delete()ed when no longer used
		*/
	bool renewLeases( list< const DCMatchMakerLease *> &leases,
					  list< DCMatchMakerLease *> &out_leases );


		/** Release the leases specified
			@param leases STL list of lease information on leases to release
			@return true on success, false on invalid input (NULL)
		*/
	bool releaseLeases( list <const DCMatchMakerLease *> &leases );


 private:

		// I can't be copied (yet)
	DCMatchMaker( const DCMatchMaker& );
	DCMatchMaker& operator = ( const DCMatchMaker& );

	// Helper methods to get/send leases
	bool SendLeases(
		Stream							*stream,
		list< const DCMatchMakerLease *> &l_list
		);
	bool GetLeases(
		Stream							*stream,
		std::list< DCMatchMakerLease *>	&l_list
		);

};

#endif /* _CONDOR_DC_MATCH_MAKER_H */
