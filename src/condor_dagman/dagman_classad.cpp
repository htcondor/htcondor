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

#include "condor_common.h"
#include "condor_attributes.h"
#include "dagman_classad.h"
#include "dc_schedd.h"
#include "condor_qmgr.h"
#include "debug.h"
#include "dagman_metrics.h"
#include "basename.h"
#include "dagman_main.h"

namespace shallow = DagmanShallowOptions;

//---------------------------------------------------------------------------
Qmgr_connection *
ScheddClassad::OpenConnection() const
{
		// Open job queue
	CondorError errstack;
	if ( !_schedd ) {
		debug_printf( DEBUG_QUIET, "ERROR: Queue manager not initialized, "
			"cannot publish updates to ClassAd.\n");
		check_warning_strictness( DAG_STRICT_3 );
		return NULL;
	}
	Qmgr_connection *queue = ConnectQ( *_schedd, 0, false, &errstack );
	if ( !queue ) {
		debug_printf( DEBUG_QUIET,
					"WARNING: failed to connect to queue manager (%s)\n",
					errstack.getFullText().c_str() );
		check_warning_strictness( DAG_STRICT_3 );
		return NULL;
	}

	return queue;
}

//---------------------------------------------------------------------------
void
ScheddClassad::CloseConnection( Qmgr_connection *queue )
{
	if ( !DisconnectQ( queue ) ) {
		debug_printf( DEBUG_QUIET,
					"WARNING: queue transaction failed.  No attributes were set.\n" );
		check_warning_strictness( DAG_STRICT_3 );
	}
}

//---------------------------------------------------------------------------
void
ScheddClassad::SetAttribute( const char *attrName, int attrVal ) const
{
	if ( SetAttributeInt( _jobId._cluster, _jobId._proc,
						  attrName, attrVal ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					  "WARNING: failed to set attribute %s\n", attrName );
		check_warning_strictness( DAG_STRICT_3 );
	}
}

//---------------------------------------------------------------------------
void
ScheddClassad::SetAttribute( const char *attrName, const std::string &value ) const
{
	if ( SetAttributeString( _jobId._cluster, _jobId._proc,
						  attrName, value.c_str() ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					  "WARNING: failed to set attribute %s\n", attrName );
		check_warning_strictness( DAG_STRICT_3 );
	}
}

//---------------------------------------------------------------------------
void
ScheddClassad::SetAttribute( const char *attrName, const ClassAd &ad ) const
{
	if ( SetAttributeExpr( _jobId._cluster, _jobId._proc,
						  attrName, &ad ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					  "WARNING: failed to set attribute %s\n", attrName );
		check_warning_strictness( DAG_STRICT_3 );
	}
}

//---------------------------------------------------------------------------
bool
ScheddClassad::GetAttribute( const char *attrName, std::string &attrVal,
			bool printWarning ) const
{
	char *val;
	if ( GetAttributeStringNew( _jobId._cluster, _jobId._proc,
				attrName, &val ) == -1 ) {
		if ( printWarning ) {
			debug_printf( DEBUG_QUIET,
					"Warning: failed to get attribute %s\n", attrName );
		}
		return false;
	}

	attrVal = val;
	free( val );
	return true;
}

//---------------------------------------------------------------------------
bool
ScheddClassad::GetAttribute( const char *attrName, int &attrVal,
			bool printWarning ) const
{
	int val = 0;
	if ( GetAttributeInt( _jobId._cluster, _jobId._proc,
				attrName, &val ) == -1 ) {
		if ( printWarning ) {
			debug_printf( DEBUG_QUIET,
				"Warning: failed to get attribute %s\n", attrName );
		}
		return false;
	}

	attrVal = val;
	return true;
}

//---------------------------------------------------------------------------
bool ScheddClassad::GetAttributeExpr(const char* attrName, std::string& attrVal) const {
	char* val;
	if (GetAttributeExprNew(_jobId._cluster, _jobId._proc, attrName, &val) == -1) {
		debug_printf(DEBUG_NORMAL, "Error: Failed to get attribute %s\n", attrName);
		return false;
	}
	attrVal = val;
	free(val);
	return true;
}

//---------------------------------------------------------------------------
DagmanClassad::DagmanClassad( const CondorID &DAGManJobId, DCSchedd *schedd )
{
	CondorID defaultCondorId;
	if ( DAGManJobId == defaultCondorId ) {
		debug_printf( DEBUG_QUIET, "No HTCondor ID available for DAGMan (running on command line?); DAG status will not be reported to ClassAd\n" );
		return;
	}

	_jobId = DAGManJobId;
	_schedd = schedd;
	_valid = true;

	InitializeMetrics();
}

//---------------------------------------------------------------------------
DagmanClassad::~DagmanClassad()
{
	_valid = false;
}

//---------------------------------------------------------------------------
void DagmanClassad::Initialize(DagmanOptions& dagOpts) {
	Qmgr_connection *queue = OpenConnection();
	if ( ! queue) { return; }


	SetAttribute(ATTR_DAGMAN_MAXJOBS, dagOpts[shallow::i::MaxJobs]);
	SetAttribute(ATTR_DAGMAN_MAXIDLE, dagOpts[shallow::i::MaxIdle]);
	SetAttribute(ATTR_DAGMAN_MAXPRESCRIPTS, dagOpts[shallow::i::MaxPre]);
	SetAttribute(ATTR_DAGMAN_MAXPOSTSCRIPTS, dagOpts[shallow::i::MaxPost]);
	SetAttribute(ATTR_DAGMAN_MAXHOLDSCRIPTS, dagOpts[shallow::i::MaxHold]);

	if (_valid) {
		using namespace DagmanDeepOptions;
		std::string batchId, batchName, acctGroup, acctUser;
		if ( ! GetAttribute(ATTR_JOB_BATCH_ID, batchId, false)) {
			batchId = std::to_string(_jobId._cluster);
			batchId += ".";
			batchId += std::to_string(_jobId._proc);
			SetAttribute(ATTR_JOB_BATCH_ID, batchId);
		}
		dagOpts[str::BatchId] = batchId;
		debug_printf(DEBUG_VERBOSE, "Workflow batch-id: <%s>\n", batchId.c_str());

		if ( ! GetAttribute(ATTR_JOB_BATCH_NAME, batchName, false)) {
			// Default batch name is top-level DAG's primary DAG file (base name only).
			batchName = condor_basename(dagOpts.primaryDag().c_str());
			batchName += "+";
			batchName += std::to_string(_jobId._cluster);
			SetAttribute(ATTR_JOB_BATCH_NAME, batchName);
		}
		dagOpts[str::BatchName] = batchName;
		debug_printf(DEBUG_VERBOSE, "Workflow batch-name: <%s>\n", batchName.c_str());

		GetAttribute(ATTR_ACCT_GROUP, acctGroup, false);
		dagOpts[str::AcctGroup] = acctGroup;
		debug_printf(DEBUG_VERBOSE, "Workflow accounting_group: <%s>\n", acctGroup.c_str());

		GetAttribute(ATTR_ACCT_GROUP_USER, acctUser, false);
		dagOpts[str::AcctGroupUser] = acctUser;
		debug_printf(DEBUG_VERBOSE, "Workflow accounting_group_user: <%s>\n", acctUser.c_str());

	} else {
		debug_printf(DEBUG_VERBOSE, "Skipping ClassAd query -- DagmanClassad object is invalid\n");
	}

	CloseConnection(queue);
}

//---------------------------------------------------------------------------
void
DagmanClassad::Update(Dagman &dagman)
{
	if ( ! _valid) {
		debug_printf(DEBUG_VERBOSE, "Skipping ClassAd update -- DagmanClassad object is invalid\n");
		return;
	}

	Qmgr_connection *queue = OpenConnection();
	if ( ! queue) {
		return;
	}

	//Get counts for DAG job process states: idle, held, running
	int jobProcsIdle, jobProcsHeld, jobProcsRunning;
	dagman.dag->NumJobProcStates(&jobProcsHeld,&jobProcsIdle,&jobProcsRunning);

	SetAttribute(ATTR_DAG_AD_UPDATE_TIME, time(nullptr)); // Set the time that update occurred
	SetAttribute(ATTR_DAG_NODES_TOTAL, dagman.dag->NumNodes(true));
	SetAttribute(ATTR_DAG_NODES_DONE, dagman.dag->NumNodesDone(true));
	SetAttribute(ATTR_DAG_NODES_PRERUN, dagman.dag->PreRunNodeCount());
	SetAttribute(ATTR_DAG_NODES_QUEUED, dagman.dag->NumNodesSubmitted());
	SetAttribute(ATTR_DAG_NODES_POSTRUN, dagman.dag->PostRunNodeCount());
	SetAttribute(ATTR_DAG_NODES_HOLDRUN, dagman.dag->HoldRunNodeCount());
	SetAttribute(ATTR_DAG_NODES_READY, dagman.dag->NumNodesReady());
	SetAttribute(ATTR_DAG_NODES_FAILED, dagman.dag->NumNodesFailed());
	SetAttribute(ATTR_DAG_NODES_UNREADY, dagman.dag->NumNodesUnready(true));
	SetAttribute(ATTR_DAG_NODES_FUTILE, dagman.dag->NumNodesFutile());
	SetAttribute(ATTR_DAG_STATUS, (int)dagman.dag->_dagStatus);
	SetAttribute(ATTR_DAG_IN_RECOVERY, dagman.dag->Recovery());
	SetAttribute(ATTR_DAG_JOBS_SUBMITTED, dagman.dag->TotalJobsSubmitted());
	SetAttribute(ATTR_DAG_JOBS_IDLE, jobProcsIdle);
	SetAttribute(ATTR_DAG_JOBS_HELD, jobProcsHeld);
	SetAttribute(ATTR_DAG_JOBS_RUNNING, jobProcsRunning);
	SetAttribute(ATTR_DAG_JOBS_COMPLETED, dagman.dag->TotalJobsCompleted());

	// Publish DAGMan stats to a classad, then update those also
	ClassAd stats_ad;
	dagman._dagmanStats.Publish(stats_ad);
	SetAttribute(ATTR_DAG_STATS, stats_ad);
	
	// Certain DAGMan properties (MaxJobs, MaxIdle, etc.) can be changed by
	// users. Start by declaring variables for these properties.
	int oldMaxJobs = dagman.options[shallow::i::MaxJobs];

	// Look up the current values of these properties in the condor_dagman job ad.
	GetAttribute(ATTR_DAGMAN_MAXIDLE, dagman.options[shallow::i::MaxIdle]);
	GetAttribute(ATTR_DAGMAN_MAXJOBS, dagman.options[shallow::i::MaxJobs]);
	GetAttribute(ATTR_DAGMAN_MAXPRESCRIPTS, dagman.options[shallow::i::MaxPre]);
	GetAttribute(ATTR_DAGMAN_MAXPOSTSCRIPTS, dagman.options[shallow::i::MaxPost]);
	GetAttribute(ATTR_DAGMAN_MAXHOLDSCRIPTS, dagman.options[shallow::i::MaxHold]);

	int newMaxJobs = dagman.options[shallow::i::MaxJobs];
	if (newMaxJobs != 0 && newMaxJobs != oldMaxJobs && dagman.enforceNewJobsLimit) {
		dagman.dag->EnforceNewJobsLimit();
	}
	// It's possible that certain DAGMan attributes were changed in the job ad.
	// If this happened, update the internal values in our dagman data structure.
	// Update our internal dag values according to whatever is in the job ad.

	CloseConnection(queue);
}

//---------------------------------------------------------------------------
void DagmanClassad::GetInfo(std::string &owner, std::string &nodeName) {
	if ( ! _valid) {
		debug_printf(DEBUG_VERBOSE, "Skipping ClassAd query -- DagmanClassad object is invalid\n");
		return;
	}

	Qmgr_connection *queue = OpenConnection();
	if ( ! queue) { return; }

	if ( ! GetAttribute(ATTR_OWNER, owner)) {
		check_warning_strictness(DAG_STRICT_1);
		owner = "undef";
	}

	if ( ! GetAttribute(ATTR_DAG_NODE_NAME, nodeName)) {
		// We should only get this value if we're a sub-DAG.
		nodeName = "undef";
	}

	CloseConnection(queue);

	return;
}

//---------------------------------------------------------------------------
void DagmanClassad::GetRequestedAttrs(std::map<std::string, std::string>& inheritAttrs, const char* prefix) {
	Qmgr_connection *queue = OpenConnection();
	if ( ! queue) { return; }

	std::vector<std::string> removeList;
	for (auto& [key, val] : inheritAttrs) {
		std::string queryKey(key);
		if ( ! isSubDag && prefix) {
			// Remove any prefixes if this is the root DAG
			queryKey.erase(0, strlen(prefix));
		}
		if ( ! GetAttributeExpr(queryKey.c_str(), val)) {
			// Failure to query removes key from map
			removeList.push_back(key);
		}
	}

	for (const auto& key : removeList) { inheritAttrs.erase(key); }

	CloseConnection(queue);
}
//---------------------------------------------------------------------------
void
DagmanClassad::InitializeMetrics()
{

	Qmgr_connection *queue = OpenConnection();
	if ( !queue ) {
		return;
	}

	int parentDagmanCluster;
	if ( GetAttributeInt( _jobId._cluster, _jobId._proc,
				ATTR_DAGMAN_JOB_ID, &parentDagmanCluster ) != 0 ) {
		debug_printf( DEBUG_DEBUG_1,
					"Can't get parent DAGMan cluster\n" );
		parentDagmanCluster = -1;
	} else {
		debug_printf( DEBUG_DEBUG_1, "Parent DAGMan cluster: %d\n",
					parentDagmanCluster );
		isSubDag = true;
	}

	CloseConnection( queue );

	DagmanMetrics::SetDagmanIds( _jobId, parentDagmanCluster );
}

//---------------------------------------------------------------------------
ProvisionerClassad::ProvisionerClassad( const CondorID &JobId, DCSchedd *schedd )
{
	CondorID defaultCondorId;
	if ( JobId == defaultCondorId ) {
		debug_printf( DEBUG_QUIET, "No HTCondor ID available for this job." );
		return;
	}

	_jobId = JobId;
	_schedd = schedd;
	_valid = true;
}

//---------------------------------------------------------------------------
ProvisionerClassad::~ProvisionerClassad()
{
	_valid = false;
}

//---------------------------------------------------------------------------
int
ProvisionerClassad::GetProvisionerState()
{
	int state = -1;

	if ( !_valid ) {
		debug_printf( DEBUG_VERBOSE,
					"Skipping ClassAd query -- ProvisionerClassad object is invalid\n" );
		return state;
	}

	Qmgr_connection *queue = OpenConnection();

	if ( !queue ) {
		return state;
	}

	GetAttribute( ATTR_PROVISIONER_STATE, state, false );
	debug_printf( DEBUG_VERBOSE, "Provisioner job state: <%d>\n",
				state );

	CloseConnection( queue );

	return state;
}
