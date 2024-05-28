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
class Dagman;

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
	void SetAttribute( const char *attrName, const std::string &value ) const;

		/** Set a nested ClassAd attribute in this DAGMan's classad.
			@param attrName The name of the attribute to set.
			@param ad The ClassAd to set.
		*/
	void SetAttribute( const char *attrName, const ClassAd &ad ) const;

		/** Get the specified attribute (string) value from our ClassAd.
			@param attrName: The name of the attribute.
			@param attrVal: A string to receive the attribute value.
			@param printWarning: Whether to print a warning if we
				can't get the requested attribute.
			@return true if we got the requested attribute, false otherwise
		*/
	bool GetAttribute( const char *attrName, std::string &attrVal,
				bool printWarning = true ) const;

	bool GetAttribute( const char *attrName, int &attrVal,
				bool printWarning = true ) const;

		// The condor ID for this connection client.
	CondorID _jobId;

	bool _valid{false}; // Whether Object is is Valid

		// The schedd we need to talk to to update the classad.
	DCSchedd *_schedd{nullptr};

};

class DagmanClassad : public ScheddClassad {
  public:
	/** Constructor.
	*/
	DagmanClassad( const CondorID &DAGManJobId, DCSchedd *schedd );
	
	/** Destructor.
	*/
	~DagmanClassad();

	// Initialize the DAGMan job's classad.
	void Initialize(DagmanOptions& dagOpts);

	/** Update the status information in the DAGMan job's classad.
		@param dagman: Dagman object to pull status information from
	*/
	void Update(Dagman &dagman );

		/** Get information we need from our own ClassAd.
			@param owner: A string to receive the Owner value.
			@param nodeName: A string to receive the DAGNodeName value.
		*/
	void GetInfo( std::string &owner, std::string &nodeName );

  private:
		/** Initialize metrics information related to our classad.
		*/
	void InitializeMetrics();
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
};


#endif /* #ifndef DAGMAN_CLASSAD_H */
