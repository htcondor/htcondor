/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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

#ifndef MPISHADOW_H
#define MPISHADOW_H

#include "condor_common.h"
#include "baseshadow.h"
#include "remoteresource.h"
#include "list.h"

/** This is the MPI Shadow class.  It acts as a shadow for an MPI
	job submitted to Condor.<p>

	More to come...

	@author Mike Yoder
*/
class MPIShadow : public BaseShadow
{
 public:

		/// Constructor
	MPIShadow();

		/// Destructor
	virtual ~MPIShadow();

		/**	Does the following:
			<ul>
			 <li>Checks some parameters
			 <li>Talks to the schedd to get all the hosts and capabilities
			       we'll need for this mpi job
			 <li>Calls BaseShadow::init()
			 <li>Requests all the remote resources
			 <li>Makes a log execute event
			 <li>Registers the RemoteResource's claimSock
			</ul>
			The parameters passed are all gotten from the 
			command line and should be easy to figure out.
		 */
	void init( ClassAd *jobAd, char schedd_addr[], char host[], 
			   char capability[], char cluster[], char proc[]);

		/** Shut down properly.  Send email to the user if requested.
			Calls DC_Exit(), therefore it doesn't return.
			@param reason The reason this mpi job exited.
			@param exitStatus The Status upon exit. 
		*/
	void shutDown( int reason, int exitStatus );

		/** Handle job removal. */
	int handleJobRemoval( int sig );

 private:

		// the list of remote (mpi) resources
		// Perhaps use STL soon.
	List<RemoteResource *> ResourceList;
};


#endif
