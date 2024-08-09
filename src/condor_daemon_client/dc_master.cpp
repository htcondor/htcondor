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
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"

#include "daemon.h"
#include "dc_master.h"
#include "internet.h"


DCMaster::DCMaster( const char* tName ) : Daemon( DT_MASTER, tName, NULL )
{
	m_is_initialized = false;
	m_master_safesock = NULL;
}


DCMaster::~DCMaster( void )
{
	if( m_master_safesock ) {
		delete m_master_safesock;
	}
}


bool
DCMaster::sendMasterCommand( bool insure_update, int my_cmd )
{
	CondorError errstack;
	int master_cmd = my_cmd;
	dprintf( D_FULLDEBUG, "DCMaster::sendMasterCommand: Just starting... \n"); 

	/* have we located the required master yet? */
	if( _addr.empty() ) {
		locate();
	}

	if( ! m_master_safesock && ! insure_update ) {
		m_master_safesock = new SafeSock;
		m_master_safesock->timeout(20);   // years of research... :)
		if( ! m_master_safesock->connect(_addr.c_str()) ) {
			dprintf( D_ALWAYS, "sendMasterCommand: Failed to connect to master " 
					 "(%s)\n", _addr.c_str() );
			delete m_master_safesock;
			m_master_safesock = NULL;
			return false;
		}
	}

	ReliSock reli_sock;
	bool  result;

	if( insure_update ) {
			// For now, if we have to ensure that the update gets
			// there, we use a ReliSock (TCP).
		reli_sock.timeout(20);   // years of research... :)
		if( ! reli_sock.connect(_addr.c_str()) ) {
			dprintf( D_ALWAYS, "sendMasterCommand: Failed to connect to master " 
					 "(%s)\n", _addr.c_str() );
			return false;
		}

		result = sendCommand( master_cmd, (Sock*)&reli_sock, 0, &errstack );
	} else {
		result = sendCommand( master_cmd, (Sock*)m_master_safesock, 0, &errstack );
	}
	if( ! result ) {
		dprintf( D_FULLDEBUG, 
				 "Failed to send %d command to master\n",master_cmd );
		if( m_master_safesock ) {
			delete m_master_safesock;
			m_master_safesock = NULL;
		}
		if( errstack.code() != 0 ) {
		        dprintf( D_ALWAYS, "ERROR: %s\n", errstack.getFullText().c_str() );
		}
		return false;
	}
	return true;
}

bool 
DCMaster::sendMasterOff( bool insure_update )
{
	dprintf( D_FULLDEBUG, "DCMaster: Just starting..<MASTER_OFF>.. \n"); 
	return sendMasterCommand( insure_update, MASTER_OFF );
}

