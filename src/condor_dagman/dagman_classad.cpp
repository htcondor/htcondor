//TEMPTEMP -- verify that I didn't make any unintended changes in the daemoncore files, etc.
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
#include "dagman_classad.h"
#include "dc_schedd.h"
#include "condor_qmgr.h"
#include "debug.h"

//---------------------------------------------------------------------------
DagmanClassad::DagmanClassad( const CondorID &DAGManJobId )
{
	_valid = false;

	//TEMPTEMP -- check for -1.-1.-1 for &DAGManJobId

	_dagmanId = DAGManJobId;

	_schedd = new DCSchedd( NULL, NULL );
	if ( !_schedd || !_schedd->locate() ) {
		const char *errMsg = _schedd ? _schedd->error() : "?";
		debug_printf( DEBUG_QUIET,
					"ERROR: can't find address of local schedd (%s)\n",
					errMsg );
		return;
	}

	_valid = true;
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
			int post, int ready, int failed, int unready, int dagStatus,
			bool recovery )
{
	if ( !_valid ) {
		//TEMPTEMP -- debug output
		return;
	}

		// Open job queue
	//TEMPTEMP -- might want to pass CondorError* (first NULL)
	Qmgr_connection *queue = ConnectQ( _schedd->addr(), 0, false,
				NULL, NULL, _schedd->version() );
	if ( !queue ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: failed to connect to queue manager\n" );
		return;
	}

	//TEMPTEMP -- parameterize attr names?
	SetDagAttribute( "DAG_NodesTotal", total );
	SetDagAttribute( "DAG_NodesDone", done );
	SetDagAttribute( "DAG_NodesPrerun", pre );
	SetDagAttribute( "DAG_NodesQueued", submitted );
	SetDagAttribute( "DAG_NodesPostrun", post );
	SetDagAttribute( "DAG_NodesReady", ready );
	SetDagAttribute( "DAG_NodesFailed", failed );
	SetDagAttribute( "DAG_NodesUnready", unready );
	SetDagAttribute( "DAG_Status", dagStatus );
	SetDagAttribute( "DAG_InRecovery", recovery );

	if ( !DisconnectQ( queue ) ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: queue transaction failed.  No attributes were set.\n" );
		
	}
}

//---------------------------------------------------------------------------
void
DagmanClassad::SetDagAttribute( const char *attrName, int attrVal )
{
	if ( SetAttributeInt( _dagmanId._cluster, _dagmanId._proc,
				attrName, attrVal, SETDIRTY|SHOULDLOG ) != 0 ) {
			// Try again without SETDIRTY|SHOULDLOG.
		if ( SetAttributeInt( _dagmanId._cluster, _dagmanId._proc,
					attrName, attrVal ) != 0 ) {
			debug_printf( DEBUG_QUIET,
						"ERROR: failed to set attribute %s\n", attrName );
		}
	}
}
