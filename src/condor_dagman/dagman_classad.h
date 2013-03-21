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


#ifndef DAGMAN_CLASSAD_H
#define DAGMAN_CLASSAD_H

#include "condor_common.h"
#include "condor_id.h"
#include "dag.h"

class DCSchedd;

class DagmanClassad {
  public:
	/** Constructor.
	*/
	DagmanClassad( const CondorID &DAGManJobId );
	
	/** Destructor.
	*/
	~DagmanClassad();

	/** Update the status information in the DAGMan job's classad.
		@param The total number of nodes
		@param The number of nodes that are done
		@param The number of nodes in the prerun state
		@param The number of nodes submitted/queued
		@param The number of nodes in the postrun state
		@param The number of nodes that are ready (but not submitted, etc.)
		@param The number of nodes that have failed
		@param The number of nodes that are unready
		@param The overall DAG status
		@param Whether the DAG is in recovery mode
	*/
	void Update( int total, int done, int pre, int submitted, int post,
				int ready, int failed, int unready,
				Dag::dag_status dagStatus, bool recovery );

  private:
	void SetDagAttribute( const char *attrName, int attrVal );

		// Whether this object is valid.
	bool _valid;

		// The Condor ID for this DAGMan -- that's the classad we'll
		// update.
	CondorID _dagmanId;

		// The schedd we need to talk to to update the classad.
	DCSchedd *_schedd;
};

#endif /* #ifndef DAGMAN_CLASSAD_H */
