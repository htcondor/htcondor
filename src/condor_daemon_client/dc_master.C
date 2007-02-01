/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"

#include "daemon.h"
#include "dc_master.h"
#include "internet.h"


DCMaster::DCMaster( const char* name ) : Daemon( DT_MASTER, name, NULL )
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
	if( ! _addr ) {
		locate();
	}

	if( ! m_master_safesock && ! insure_update ) {
		m_master_safesock = new SafeSock;
		m_master_safesock->timeout(20);   // years of research... :)
		if( ! m_master_safesock->connect(_addr) ) {
			dprintf( D_ALWAYS, "sendMasterCommand: Failed to connect to master " 
					 "(%s)\n", _addr );
			delete m_master_safesock;
			m_master_safesock = NULL;
			return false;
		}
	}

	ReliSock reli_sock;
	Sock* tmp;
	bool  result;

	if( insure_update ) {
			// For now, if we have to ensure that the update gets
			// there, we use a ReliSock (TCP).
		reli_sock.timeout(20);   // years of research... :)
		if( ! reli_sock.connect(_addr) ) {
			dprintf( D_ALWAYS, "sendMasterCommand: Failed to connect to master " 
					 "(%s)\n", _addr );
			return false;
		}

		result = sendCommand( master_cmd, (Sock*)&reli_sock, 0, &errstack );
		tmp = &reli_sock;
	} else {
		result = sendCommand( master_cmd, (Sock*)m_master_safesock, 0, &errstack );
		tmp = m_master_safesock;
	}
	if( ! result ) {
		dprintf( D_FULLDEBUG, 
				 "Failed to send %d command to master\n",master_cmd );
		if( m_master_safesock ) {
			delete m_master_safesock;
			m_master_safesock = NULL;
		}
		if( errstack.code() != 0 ) {
		        dprintf( D_ALWAYS, "ERROR: %s\n", errstack.getFullText() );
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

