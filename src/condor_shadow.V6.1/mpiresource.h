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

#ifndef MPIRESOURCE_H
#define MPIRESOURCE_H

#include "condor_common.h"
#include "remoteresource.h"

/** Here we have a remote resource that is specific to an MPI job. */

class MpiResource : public RemoteResource {

 public:

		/** See the RemoteResource's constructor.
		*/
	MpiResource( BaseShadow *shadow );

		/// Destructor
	~MpiResource() {};

		/** Special format... */
	virtual void printExit( FILE *fp );

	int node( void ) { return node_num; };
	void setNode( int node ) { node_num = node; };

		/** Call RemoteResource::resourceExit() and log a
			NodeTerminatedEvent to the UserLog
		*/
	void resourceExit( int reason, int status );

		/** Before we log anything to the UserLog, we want to
			initialize the UserLog with our node number.  
		*/
	virtual bool writeULogEvent( ULogEvent* event );

		/** Our job on the remote resource started to execute, so we
			want to log a NodeExecuteEvent.
		*/
	virtual void beginExecution( void );

	virtual void reconnect( void );

	virtual void attemptReconnect( void );

	virtual bool supportsReconnect( void );


 private:

		// Making these private PREVENTS copying.
	MpiResource( const MpiResource& );
	MpiResource& operator = ( const MpiResource& );

	int node_num;
};


#endif
