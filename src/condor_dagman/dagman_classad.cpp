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

//---------------------------------------------------------------------------
DagmanClassad::DagmanClassad( const CondorID &DAGManJobId ) :
	_valid( false ),
	_schedd( NULL )
{
	CondorID defaultCondorId;
	if ( DAGManJobId == defaultCondorId ) {
		debug_printf( DEBUG_QUIET, "No Condor ID available for DAGMan (running on command line?); DAG status will not be reported to classad\n" );
		return;
	}

	_dagmanId = DAGManJobId;

	_schedd = new DCSchedd( NULL, NULL );
	if ( !_schedd || !_schedd->locate() ) {
		const char *errMsg = _schedd ? _schedd->error() : "?";
		debug_printf( DEBUG_QUIET,
					"WARNING: can't find address of local schedd for classad updates (%s)\n",
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
DagmanClassad::Update( int total, int done, int pre, int submitted,
			int post, int ready, int failed, int unready,
			Dag::dag_status dagStatus, bool recovery )
{

	if ( !_valid ) {
		debug_printf( DEBUG_VERBOSE,
					"Skipping classad update -- DagmanClassad object is invalid\n" );
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

	CloseConnection( queue );
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
DagmanClassad::SetDagAttribute( const char *attrName, int attrVal )
{
	if ( SetAttributeInt( _dagmanId._cluster, _dagmanId._proc,
						  attrName, attrVal ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					  "WARNING: failed to set attribute %s\n", attrName );
		check_warning_strictness( DAG_STRICT_3 );
	}
}
