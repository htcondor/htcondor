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


#ifndef MPIRESOURCE_H
#define MPIRESOURCE_H

#include "condor_common.h"
#include "remoteresource.h"

/** Here we have a remote resource that is specific to an MPI job. */

class MpiResource : public RemoteResource {

 public:

		/** See the RemoteResource's constructor.
		*/
	MpiResource( BaseShadow *base_shadow );

		/// Destructor
	~MpiResource() {};

	virtual void hadContact();

		/** Special format... */
	virtual void printExit( FILE *fp );

	int getNode( void ) const { return node_num; };
	void setNode( int node_arg ) { node_num = node_arg; };

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
