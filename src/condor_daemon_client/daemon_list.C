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
DaemonList::deleteCurrent() { this->DeleteCurrent(); }

/*
  NOTE: SimpleList does NOT delete the Daemon objects themselves,
  DeleteCurrent() is only going to delete the pointer itself.  Since
  we're responsible for all this memory (we create the Daemon objects 
  in DaemonList::init() and DaemonList::buildDaemon()), we have to be
  responsbile to deallocate it when we're done.  We're already doing
  this correctly in the DaemonList destructor, and we have to do it
  here in the DeleteCurrent() interface, too.
*/
void
DaemonList::DeleteCurrent() {
	Daemon* cur = NULL;
	if( list.Current(cur) && cur ) {
		delete cur;
	}
	list.DeleteCurrent();
}

