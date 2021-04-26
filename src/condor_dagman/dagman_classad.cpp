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
	Qmgr_connection *queue = ConnectQ( _schedd->addr(), 0, false,
				&errstack, NULL, _schedd->version() );
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
ScheddClassad::SetAttribute( const char *attrName, const MyString &value ) const
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
ScheddClassad::GetAttribute( const char *attrName, MyString &attrVal,
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
	int val;
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
DagmanClassad::DagmanClassad( const CondorID &DAGManJobId, DCSchedd *schedd ) :
	_valid( false )
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
void
DagmanClassad::Initialize( int maxJobs, int maxIdle, int maxPreScripts,
			int maxPostScripts, int maxHoldScripts )
{
	Qmgr_connection *queue = OpenConnection();
	if ( !queue ) {
		return;
	}

	SetAttribute( ATTR_DAGMAN_MAXJOBS, maxJobs );
	SetAttribute( ATTR_DAGMAN_MAXIDLE, maxIdle );
	SetAttribute( ATTR_DAGMAN_MAXPRESCRIPTS, maxPreScripts );
	SetAttribute( ATTR_DAGMAN_MAXPOSTSCRIPTS, maxPostScripts );
	SetAttribute( ATTR_DAGMAN_MAXHOLDSCRIPTS, maxHoldScripts );

	CloseConnection( queue );
}

//---------------------------------------------------------------------------
void
DagmanClassad::Update( int total, int done, int pre, int submitted,
			int post, int hold, int ready, int failed, int unready,
			DagStatus dagStatus, bool recovery, const DagmanStats &stats,
			int &maxJobs, int &maxIdle, int &maxPreScripts, int &maxPostScripts,
			int &maxHoldScripts )
{
	if ( !_valid ) {
		debug_printf( DEBUG_VERBOSE,
					"Skipping ClassAd update -- DagmanClassad object is invalid\n" );
		return;
	}

	Qmgr_connection *queue = OpenConnection();
	if ( !queue ) {
		return;
	}

	SetAttribute( ATTR_DAG_NODES_TOTAL, total );
	SetAttribute( ATTR_DAG_NODES_DONE, done );
	SetAttribute( ATTR_DAG_NODES_PRERUN, pre );
	SetAttribute( ATTR_DAG_NODES_QUEUED, submitted );
	SetAttribute( ATTR_DAG_NODES_POSTRUN, post );
	SetAttribute( ATTR_DAG_NODES_HOLDRUN, hold );
	SetAttribute( ATTR_DAG_NODES_READY, ready );
	SetAttribute( ATTR_DAG_NODES_FAILED, failed );
	SetAttribute( ATTR_DAG_NODES_UNREADY, unready );
	SetAttribute( ATTR_DAG_STATUS, (int)dagStatus );
	SetAttribute( ATTR_DAG_IN_RECOVERY, recovery );

	// Publish DAGMan stats to a classad, then update those also
	ClassAd stats_ad;
	stats.Publish( stats_ad );
	SetAttribute( ATTR_DAG_STATS, stats_ad );

	// Certain DAGMan properties (MaxJobs, MaxIdle, etc.) can be changed by
	// users. Start by declaring variables for these properties.
	int jobAdMaxIdle;
	int jobAdMaxJobs;
	int jobAdMaxPreScripts;
	int jobAdMaxPostScripts;
	int jobAdMaxHoldScripts;

	// Look up the current values of these properties in the condor_dagman job ad.
	GetAttribute( ATTR_DAGMAN_MAXIDLE, jobAdMaxIdle );
	GetAttribute( ATTR_DAGMAN_MAXJOBS, jobAdMaxJobs );
	GetAttribute( ATTR_DAGMAN_MAXPRESCRIPTS, jobAdMaxPreScripts );
	GetAttribute( ATTR_DAGMAN_MAXPOSTSCRIPTS, jobAdMaxPostScripts );
	GetAttribute( ATTR_DAGMAN_MAXHOLDSCRIPTS, jobAdMaxHoldScripts );

	// Update our internal dag values according to whatever is in the job ad.
	maxIdle = jobAdMaxIdle;
	maxJobs = jobAdMaxJobs;
	maxPreScripts = jobAdMaxPreScripts;
	maxPostScripts = jobAdMaxPostScripts;
	maxHoldScripts = jobAdMaxHoldScripts;


	CloseConnection( queue );
}

//---------------------------------------------------------------------------
void
DagmanClassad::GetInfo( MyString &owner, MyString &nodeName )
{
	if ( !_valid ) {
		debug_printf( DEBUG_VERBOSE,
					"Skipping ClassAd query -- DagmanClassad object is invalid\n" );
		return;
	}

	Qmgr_connection *queue = OpenConnection();
	if ( !queue ) {
		return;
	}

	if ( !GetAttribute( ATTR_OWNER, owner ) ) {
		check_warning_strictness( DAG_STRICT_1 );
		owner = "undef";
	}

	if ( !GetAttribute( ATTR_DAG_NODE_NAME, nodeName ) ) {
		// We should only get this value if we're a sub-DAG.
		nodeName = "undef";
	}

	CloseConnection( queue );

	return;
}

//---------------------------------------------------------------------------
void
DagmanClassad::GetSetBatchId( std::string &batchId )
{
	if ( !_valid ) {
		debug_printf( DEBUG_VERBOSE,
					"Skipping ClassAd query -- DagmanClassad object is invalid\n" );
		return;
	}

	Qmgr_connection *queue = OpenConnection();
	if ( !queue ) {
		return;
	}

	if ( !GetAttribute( ATTR_JOB_BATCH_ID, batchId, false ) ) {
		batchId = std::to_string( _jobId._cluster );
		batchId += ".";
		batchId += std::to_string( _jobId._proc );
		SetAttribute( ATTR_JOB_BATCH_ID, batchId );
	}

	CloseConnection( queue );

	debug_printf( DEBUG_VERBOSE, "Workflow batch-id: <%s>\n",
				batchId.c_str() );
}

//---------------------------------------------------------------------------
void
DagmanClassad::GetSetBatchName( const MyString &primaryDagFile,
			MyString &batchName )
{
	if ( !_valid ) {
		debug_printf( DEBUG_VERBOSE,
					"Skipping ClassAd query -- DagmanClassad object is invalid\n" );
		return;
	}

	Qmgr_connection *queue = OpenConnection();
	if ( !queue ) {
		return;
	}

	if ( !GetAttribute( ATTR_JOB_BATCH_NAME, batchName, false ) ) {
			// Default batch name is top-level DAG's primary
			// DAG file (base name only).
		batchName = condor_basename( primaryDagFile.c_str() );
		batchName += "+";
		batchName += std::to_string( _jobId._cluster );
		SetAttribute( ATTR_JOB_BATCH_NAME, batchName );
	}

	CloseConnection( queue );

	debug_printf( DEBUG_VERBOSE, "Workflow batch-name: <%s>\n",
				batchName.c_str() );
}

//---------------------------------------------------------------------------
void
DagmanClassad::GetAcctInfo( MyString &group, MyString &user )
{
	if ( !_valid ) {
		debug_printf( DEBUG_VERBOSE,
					"Skipping ClassAd query -- DagmanClassad object is invalid\n" );
		return;
	}

	Qmgr_connection *queue = OpenConnection();
	if ( !queue ) {
		return;
	}

	GetAttribute( ATTR_ACCT_GROUP, group, false );
	debug_printf( DEBUG_VERBOSE, "Workflow accounting_group: <%s>\n",
				group.c_str() );

	GetAttribute( ATTR_ACCT_GROUP_USER, user, false );
	debug_printf( DEBUG_VERBOSE, "Workflow accounting_group_user: <%s>\n",
				user.c_str() );

	CloseConnection( queue );

	return;
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
	}

	CloseConnection( queue );

	DagmanMetrics::SetDagmanIds( _jobId, parentDagmanCluster );
}

//---------------------------------------------------------------------------
ProvisionerClassad::ProvisionerClassad( const CondorID &JobId, DCSchedd *schedd ) :
	_valid( false )
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
