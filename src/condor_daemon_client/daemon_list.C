/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "daemon_list.h"
#include "dc_collector.h"


DaemonList::DaemonList()
{
}


DaemonList::~DaemonList( void )
{
	Daemon* tmp;
	list.Rewind();
	while( list.Next(tmp) ) {
		delete tmp;
	}
}


void
DaemonList::init( daemon_t type, const char* host_list )
{
	Daemon* tmp;
	char* host;
	StringList foo;
	foo.initializeFromString( host_list );
	foo.rewind();
	while( (host = foo.next()) ) {
		tmp = buildDaemon( type, host );
		append( tmp );
	}
}


Daemon*
DaemonList::buildDaemon( daemon_t type, const char* str )
{
	Daemon* tmp;
	switch( type ) {
	case DT_COLLECTOR:
		tmp = new DCCollector( str );
		break;
	default:
		tmp = new Daemon( type, str );
		break;
	}
	return tmp;
}


/*************************************************************
 ** SimpleList API
 ************************************************************/


bool
DaemonList::append( Daemon* d) { return list.Append(d); }

bool
DaemonList::Append( Daemon* d) { return list.Append(d); }

bool
DaemonList::isEmpty( void ) { return list.IsEmpty(); } 

bool
DaemonList::IsEmpty( void ) { return list.IsEmpty(); } 

int
DaemonList::number( void ) { return list.Number(); } 

int
DaemonList::Number( void ) { return list.Number(); } 

void
DaemonList::rewind( void ) { list.Rewind(); } 

void
DaemonList::Rewind( void ) { list.Rewind(); } 

bool
DaemonList::current( Daemon* & d ) { return list.Current(d); } 

bool
DaemonList::Current( Daemon* & d ) { return list.Current(d); } 

bool
DaemonList::next( Daemon* & d ) { return list.Next(d); } 

bool
DaemonList::Next( Daemon* & d ) { return list.Next(d); } 

bool
DaemonList::atEnd() { return list.AtEnd(); } 

bool
DaemonList::AtEnd() { return list.AtEnd(); } 

void
DaemonList::deleteCurrent() { list.DeleteCurrent(); } 

void
DaemonList::DeleteCurrent() { list.DeleteCurrent(); } 
