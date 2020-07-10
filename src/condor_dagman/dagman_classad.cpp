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
DagmanClassad::DagmanClassad( const CondorID &DAGManJobId ) :
	_valid( false ),
	_schedd( NULL )
{
	CondorID defaultCondorId;
	if ( DAGManJobId == defaultCondorId ) {
		debug_printf( DEBUG_QUIET, "No HTCondor ID available for DAGMan (running on command line?); DAG status will not be reported to ClassAd\n" );
		return;
	}

	_dagmanId = DAGManJobId;

	_schedd = new DCSchedd( NULL, NULL );
	if ( !_schedd || !_schedd->locate() ) {
		const char *errMsg = _schedd ? _schedd->error() : "?";
		debug_printf( DEBUG_QUIET,
					"WARNING: can't find address of local schedd for ClassAd updates (%s)\n",
					errMsg );
		check_warning_strictness( DAG_STRICT_3 );
		return;
	}

	_valid = true;

	InitializeMetrics();
}

//---------------------------------------------------------------------------
DagmanClassad::~DagmanClassad()
{
	_valid = false;

	delete _schedd;
}

//---------------------------------------------------------------------------
void
DagmanClassad::Initialize( int maxJobs, int maxIdle, int maxPreScripts,
			int maxPostScripts )
{
	Qmgr_connection *queue = OpenConnection();
	if ( !queue ) {
		return;
	}

	SetDagAttribute( ATTR_DAGMAN_MAXJOBS, maxJobs );
	SetDagAttribute( ATTR_DAGMAN_MAXIDLE, maxIdle );
	SetDagAttribute( ATTR_DAGMAN_MAXPRESCRIPTS, maxPreScripts );
	SetDagAttribute( ATTR_DAGMAN_MAXPOSTSCRIPTS, maxPostScripts );

	CloseConnection( queue );
}

//---------------------------------------------------------------------------
void
DagmanClassad::Update( int total, int done, int pre, int submitted,
			int post, int ready, int failed, int unready,
			Dag::dag_status dagStatus, bool recovery, const DagmanStats &stats,
			int &maxJobs, int &maxIdle, int &maxPreScripts, int &maxPostScripts )
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

	SetDagAttribute( ATTR_DAG_NODES_TOTAL, total );
	SetDagAttribute( ATTR_DAG_NODES_DONE, done );
	SetDagAttribute( ATTR_DAG_NODES_PRERUN, pre );
	SetDagAttribute( ATTR_DAG_NODES_QUEUED, submitted );
	SetDagAttribute( ATTR_DAG_NODES_POSTRUN, post );
	SetDagAttribute( ATTR_DAG_NODES_READY, ready );
	SetDagAttribute( ATTR_DAG_NODES_FAILED, failed );
	SetDagAttribute( ATTR_DAG_NODES_UNREADY, unready );
	SetDagAttribute( ATTR_DAG_STATUS, (int)dagStatus );
	SetDagAttribute( ATTR_DAG_IN_RECOVERY, recovery );

	// Publish DAGMan stats to a classad, then update those also
	ClassAd stats_ad;
	stats.Publish( stats_ad );
	SetDagAttribute( ATTR_DAG_STATS, stats_ad );

	// Certain DAGMan properties (MaxJobs, MaxIdle, etc.) can be changed by
	// users. Start by declaring variables for these properties.
	int jobAdMaxIdle;
	int jobAdMaxJobs;
	int jobAdMaxPreScripts;
	int jobAdMaxPostScripts;

	// Look up the current values of these properties in the condor_dagman job ad.
	GetDagAttribute( ATTR_DAGMAN_MAXIDLE, jobAdMaxIdle );
	GetDagAttribute( ATTR_DAGMAN_MAXJOBS, jobAdMaxJobs );
	GetDagAttribute( ATTR_DAGMAN_MAXPRESCRIPTS, jobAdMaxPreScripts );
	GetDagAttribute( ATTR_DAGMAN_MAXPOSTSCRIPTS, jobAdMaxPostScripts );

	// Update our internal dag values according to whatever is in the job ad.
	maxIdle = jobAdMaxIdle;
	maxJobs = jobAdMaxJobs;
	maxPreScripts = jobAdMaxPreScripts;
	maxPostScripts = jobAdMaxPostScripts;


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

	if ( !GetDagAttribute( ATTR_OWNER, owner ) ) {
		check_warning_strictness( DAG_STRICT_1 );
		owner = "undef";
	}

	if ( !GetDagAttribute( ATTR_DAG_NODE_NAME, nodeName ) ) {
		// We should only get this value if we're a sub-DAG.
		nodeName = "undef";
	}

	CloseConnection( queue );

	return;
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

	if ( !GetDagAttribute( ATTR_JOB_BATCH_NAME, batchName, false ) ) {
			// Default batch name is top-level DAG's primary
			// DAG file (base name only).
		batchName = condor_basename( primaryDagFile.Value() );
		batchName += "+";
		batchName += IntToStr( _dagmanId._cluster );
		SetDagAttribute( ATTR_JOB_BATCH_NAME, batchName );
	}

	CloseConnection( queue );

	debug_printf( DEBUG_VERBOSE, "Workflow batch-name: <%s>\n",
				batchName.Value() );
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

	GetDagAttribute( ATTR_ACCT_GROUP, group, false );
	debug_printf( DEBUG_VERBOSE, "Workflow accounting_group: <%s>\n",
				group.Value() );

	GetDagAttribute( ATTR_ACCT_GROUP_USER, user, false );
	debug_printf( DEBUG_VERBOSE, "Workflow accounting_group_user: <%s>\n",
				user.Value() );

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
	if ( GetAttributeInt( _dagmanId._cluster, _dagmanId._proc,
				ATTR_DAGMAN_JOB_ID, &parentDagmanCluster ) != 0 ) {
		debug_printf( DEBUG_DEBUG_1,
					"Can't get parent DAGMan cluster\n" );
		parentDagmanCluster = -1;
	} else {
		debug_printf( DEBUG_DEBUG_1, "Parent DAGMan cluster: %d\n",
					parentDagmanCluster );
	}

	CloseConnection( queue );

	DagmanMetrics::SetDagmanIds( _dagmanId, parentDagmanCluster );
}

//---------------------------------------------------------------------------
Qmgr_connection *
DagmanClassad::OpenConnection()
{
		// Open job queue
	CondorError errstack;
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
DagmanClassad::CloseConnection( Qmgr_connection *queue )
{
	if ( !DisconnectQ( queue ) ) {
		debug_printf( DEBUG_QUIET,
					"WARNING: queue transaction failed.  No attributes were set.\n" );
		check_warning_strictness( DAG_STRICT_3 );
	}
}

//---------------------------------------------------------------------------
void
DagmanClassad::SetDagAttribute( const char *attrName, int attrVal ) const
{
	if ( SetAttributeInt( _dagmanId._cluster, _dagmanId._proc,
						  attrName, attrVal ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					  "WARNING: failed to set attribute %s\n", attrName );
		check_warning_strictness( DAG_STRICT_3 );
	}
}

//---------------------------------------------------------------------------
void
DagmanClassad::SetDagAttribute( const char *attrName, const MyString &value ) const
{
	if ( SetAttributeString( _dagmanId._cluster, _dagmanId._proc,
						  attrName, value.Value() ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					  "WARNING: failed to set attribute %s\n", attrName );
		check_warning_strictness( DAG_STRICT_3 );
	}
}

//---------------------------------------------------------------------------
void
DagmanClassad::SetDagAttribute( const char *attrName, const ClassAd &ad ) const
{
	if ( SetAttributeExpr( _dagmanId._cluster, _dagmanId._proc,
						  attrName, &ad ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					  "WARNING: failed to set attribute %s\n", attrName );
		check_warning_strictness( DAG_STRICT_3 );
	}
}

//---------------------------------------------------------------------------
bool
DagmanClassad::GetDagAttribute( const char *attrName, MyString &attrVal,
			bool printWarning ) const
{
	char *val;
	if ( GetAttributeStringNew( _dagmanId._cluster, _dagmanId._proc,
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
DagmanClassad::GetDagAttribute( const char *attrName, int &attrVal,
			bool printWarning ) const
{
	int val;
	if ( GetAttributeInt( _dagmanId._cluster, _dagmanId._proc,
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
