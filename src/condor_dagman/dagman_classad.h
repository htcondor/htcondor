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
#include "dagman_stats.h"
#include "condor_qmgr.h"
#include "../condor_utils/dagman_utils.h"

class DCSchedd;

class ScheddClassad {

  public:
		/** Open a connection to the schedd.
		@return Qmgr_connection An opaque connection object -- NULL
				if connection fails.
		*/
	Qmgr_connection *OpenConnection() const;

		/** Close the given connection to the schedd.
			@param Qmgr_connection An opaque connection object.
		*/
	void CloseConnection( Qmgr_connection *queue );

		/** Set an attribute in this DAGMan's classad.
			@param attrName The name of the attribute to set.
			@param attrVal The value of the attribute.
		*/
	void SetAttribute( const char *attrName, int attrVal ) const;

		/** Set an attribute in this DAGMan's classad.
			@param attrName The name of the attribute to set.
			@param attrVal The value of the attribute.
		*/
	void SetAttribute( const char *attrName, const MyString &value ) const;

		/** Set a nested ClassAd attribute in this DAGMan's classad.
			@param attrName The name of the attribute to set.
			@param ad The ClassAd to set.
		*/
	void SetAttribute( const char *attrName, const ClassAd &ad ) const;

		/** Get the specified attribute (string) value from our ClassAd.
			@param attrName: The name of the attribute.
			@param attrVal: A MyString to receive the attribute value.
			@param printWarning: Whether to print a warning if we
				can't get the requested attribute.
			@return true if we got the requested attribute, false otherwise
		*/
	bool GetAttribute( const char *attrName, MyString &attrVal,
				bool printWarning = true ) const;

	bool GetAttribute( const char *attrName, std::string &attrVal,
				bool printWarning = true ) const;

	bool GetAttribute( const char *attrName, int &attrVal,
				bool printWarning = true ) const;

		// The condor ID for this connection client.
	CondorID _jobId;

		// The schedd we need to talk to to update the classad.
	DCSchedd *_schedd = NULL;

};

class DagmanClassad : public ScheddClassad {
  public:
	/** Constructor.
	*/
	DagmanClassad( const CondorID &DAGManJobId, DCSchedd *schedd );
	
	/** Destructor.
	*/
	~DagmanClassad();

	/** Initialize the DAGMan job's classad.
		@param maxJobs Maximum number of submitted jobs
		@param maxIdle Maximum number of idle jobs
		@param maxPreScripts Maximum number of active pre scripts
		@param maxPostScripts Maximum number of active post scripts
		@param maxHoldScripts Maximum number of active hold scripts
	**/
	void Initialize( int maxJobs, int maxIdle, int maxPreScripts,
			int maxPostScripts, int maxHoldScripts );


	/** Update the status information in the DAGMan job's classad.
		@param The total number of nodes
		@param The number of nodes that are done
		@param The number of nodes in the prerun state
		@param The number of nodes submitted/queued
		@param The number of nodes in the postrun state
		@param The number of nodes in the hold state
		@param The number of nodes that are ready (but not submitted, etc.)
		@param The number of nodes that have failed
		@param The number of nodes that are unready
		@param The overall DAG status
		@param Whether the DAG is in recovery mode
		@param A ClassAd of DAGMan runtime statistics
	*/
	void Update( int total, int done, int pre, int submitted, int post,
				int hold, int ready, int failed, int unready,
				DagStatus dagStatus, bool recovery, const DagmanStats &stats,
				int &maxJobs, int &maxIdle, int &maxPreScripts, 
				int &maxPostScripts, int &maxHoldScripts );

		/** Get information we need from our own ClassAd.
			@param owner: A MyString to receive the Owner value.
			@param nodeName: A MyString to receive the DAGNodeName value.
		*/
	void GetInfo( MyString &owner, MyString &nodeName );

		/** Get the JobBatchId value from our ClassAd (setting it
		    to the default if it's not already set).
			@param batchId: An int to receive the JobBatchId value
		*/
	void GetSetBatchId( std::string &batchId );

		/** Get the JobBatchName value from our ClassAd (setting it
		    to the default if it's not already set).
			@param batchName: A MyString to receive the JobBatchName value
		*/
	void GetSetBatchName( const MyString &primaryDagFile,
				MyString &batchName );

		/** Get the AcctGroup and AcctGroupUser values from our ClassAd.
			@param group: A MyString to receive the AcctGroup value
			@param user: A MyString to receive the AcctGroupUser value
		*/
	void GetAcctInfo( MyString &group, MyString &user );

  private:
		/** Initialize metrics information related to our classad.
		*/
	void InitializeMetrics();


		// Whether this object is valid.
	bool _valid;

		// The HTCondor ID for this DAGMan -- that's the classad we'll
		// update.
	CondorID _dagmanId;
};


class ProvisionerClassad : public ScheddClassad {
  public:
	/** Constructor.
	*/
	ProvisionerClassad( const CondorID &JobId, DCSchedd *schedd );

	/** Destructor.
	*/
	~ProvisionerClassad();

		// Returns the state of a provisioner, represented as a string
	int GetProvisionerState();

		// Whether this object is valid.
	bool _valid;

};


#endif /* #ifndef DAGMAN_CLASSAD_H */
