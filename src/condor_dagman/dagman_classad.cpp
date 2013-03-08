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
#include "condor_config.h"//TEMPTEMP
#include "dagman_classad.h"
#include "dc_schedd.h"
#include "condor_qmgr.h"
#include "debug.h"

//---------------------------------------------------------------------------
DagmanClassad::DagmanClassad( const CondorID &DAGManJobId )
{
	debug_printf( DEBUG_QUIET, "DagmanClassad::DagmanClassad()\n" );//TEMPTEMP
	printf( "DIAG DagmanClassad::DagmanClassad()\n" );//TEMPTEMP

	const char *tmp = param( "SCHEDD_ADDRESS_FILE" );//TEMPTEMP
	printf( "DIAG SCHEDD_ADDRESS_FILE: <%s>\n", tmp );//TEMPTEMP
	tmp = param( "SCHEDD_DAEMON_AD_FILE" );//TEMPTEMP
	printf( "DIAG SCHEDD_DAEMON_AD_FILE: <%s>\n", tmp );//TEMPTEMP
	tmp = param( "LOG" );//TEMPTEMP
	printf( "DIAG LOG: <%s>\n", tmp );//TEMPTEMP
	tmp = getenv( "_CONDOR_SCHEDD_ADDRESS_FILE" );
	printf( "DIAG _CONDOR_SCHEDD_ADDRESS_FILE: <%s>\n", tmp );//TEMPTEMP

	_valid = false;

	//TEMPTEMP -- check for -1.-1.-1 for &DAGManJobId

	_dagmanId = DAGManJobId;

#if 1 //TEMPTEMP
	printf( "DIAG 2010\n" );//TEMPTEMP
	_schedd = new DCSchedd( NULL, NULL );
	printf( "DIAG 2011\n" );//TEMPTEMP
	if ( !_schedd || !_schedd->locate() ) {
	printf( "DIAG 2012\n" );//TEMPTEMP
		const char *errMsg = _schedd ? _schedd->error() : "?";
		debug_printf( DEBUG_QUIET,
					"ERROR: can't find address of local schedd (%s)\n",
					errMsg );
		return;
	}
#endif //TEMPTEMP

	_valid = true;
	printf( "  DIAG end of DagmanClassad::DagmanClassad()\n" );//TEMPTEMP
}

//---------------------------------------------------------------------------
DagmanClassad::~DagmanClassad()
{
	debug_printf( DEBUG_QUIET, "DagmanClassad::~DagmanClassad()\n" );//TEMPTEMP
}

//---------------------------------------------------------------------------
void
DagmanClassad::Update( int total, int done, int pre, int submitted,
			int post, int ready, int failed, int unready )
{
	debug_printf( DEBUG_QUIET, "DagmanClassad::Update()\n" );//TEMPTEMP

	if ( !_valid ) {
		//TEMPTEMP -- debug output
		return;
	}

		// Open job queue
	printf( "DIAG 2110\n" );//TEMPTEMP
	//TEMPTEMP -- might want to pass CondorError* (first NULL)
	Qmgr_connection *queue = ConnectQ( _schedd->addr(), 0, false,
				NULL, NULL, _schedd->version() );
	//Qmgr_connection *queue = ConnectQ( "<128.105.167.40:54506>", 0, false,//TEMPTEMP!!!
				//NULL, NULL, "<$CondorVersion: 7.9.5 Mar 06 2013 BuildID: UW_development PRE-RELEASE-UWCS $>" );//TEMPTEMP!!!!
	printf( "DIAG 2111\n" );//TEMPTEMP
	if ( !queue ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: failed to connect to queue manager\n" );
		return;
	}

	//TEMPTEMP -- parameterize attr names?
	SetDagAttribute( "DAGNodesTotal", total );
	SetDagAttribute( "DAGNodesDone", done );
	SetDagAttribute( "DAGNodesPrerun", pre );
	SetDagAttribute( "DAGNodesQueued", submitted );
	SetDagAttribute( "DAGNodesPostrun", post );
	SetDagAttribute( "DAGNodesReady", ready );
	SetDagAttribute( "DAGNodesFailed", failed );
	SetDagAttribute( "DAGNodesUnready", unready );

	if ( !DisconnectQ( queue ) ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: queue transaction failed.  No attributes were set.\n" );
		
	}
}

//---------------------------------------------------------------------------
void
DagmanClassad::SetDagAttribute( const char *attrName, int attrVal )
{
	//TEMPTEMP -- what about SetAttributeFlags_t?
	if ( SetAttributeInt( _dagmanId._cluster, _dagmanId._proc,
				attrName, attrVal ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: failed to set attribute %s\n", attrName );
	}
}
