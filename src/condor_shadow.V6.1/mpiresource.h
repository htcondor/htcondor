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

#ifndef MPIRESOURCE_H
#define MPIRESOURCE_H

#include "condor_common.h"
#include "remoteresource.h"

/** Here are some basic states that an mpi resource can be in. */ 
enum Resource_State {
		/// Before the job begins execution
	PRE, 
		/// While it's running (after requestIt() succeeds...)
	EXECUTING,
		/** We've told the job to go away, but haven't received 
			confirmation that it's really dead.  This state is 
		    skipped if the job exits on its own. */
	PENDING_DEATH,
		/// After it has stopped (for whatever reason...)
	FINISHED 
};

static char *Resource_State_String [] = {
	"PRE", 
	"EXECUTING", 
	"PENDING_DEATH", 
	"FINISHED"
};

/** Here we have a remote resource that is specific to an MPI job.
	The differences are in the way it prints itself out, and it 
	also has the notion of special "states" that it can be in, that
	aren't needed in a normal job. */

class MpiResource : public RemoteResource {

 public:

		/** See the RemoteResource's constructor.
		*/
	MpiResource( BaseShadow *shadow );

		/** See the RemoteResource's constructor.
		*/
	MpiResource( BaseShadow *shadow, const char * executingHost, 
				 const char * capability );

		/// Destructor
	~MpiResource() {};

		/** Call RemoteResource::requestIt(), set state to EXECUTING
		   if it succeeds */
	virtual int requestIt( int starterVersion = 2 );

		/** Call RemoteResource::killStarter, set state to PENDING_DEATH */
	virtual int killStarter();

		/** Overridden so you can set the state to FINISHED */
	virtual void setExitStatus( int status );

		/** Add state... */
	virtual void dprintfSelf( int debugLevel);

		/** Special format... */
	virtual void printExit( FILE *fp );

	/** Return the state that this resource is in. */
	Resource_State getResourceState() { return state; };

		/** Change this resource's state */
	void setResourceState( Resource_State s );

	int node( void ) { return node_num; };
	void setNode( int node ) { node_num = node; };

	void resourceExit( int exit_reason, int exit_status );

 private:

		// Making these private PREVENTS copying.
	MpiResource( const MpiResource& );
	MpiResource& operator = ( const MpiResource& );

	Resource_State state;

	int node_num;
};


#endif
