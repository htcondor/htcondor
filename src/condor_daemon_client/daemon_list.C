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
DaemonList::deleteCurrent() { list.DeleteCurrent(); } 

void
DaemonList::DeleteCurrent() { list.DeleteCurrent(); } 


CollectorList::CollectorList() {
}

CollectorList::~CollectorList() {
}

CollectorList *
CollectorList::create() {

	CollectorList * result = new CollectorList();
	DCCollector * collector = NULL;


		// Read the new names from config file
	StringList collector_name_list;
	char * collector_name_param = NULL;
	Daemon::getCmHostFromConfig ( "COLLECTOR", collector_name_param );
	
	if (collector_name_param  && *collector_name_param) {
		collector_name_list.initializeFromString(collector_name_param);
	
			// Create collector objects
		collector_name_list.rewind();
		char * collector_name = NULL;
		while ((collector_name = collector_name_list.next()) != NULL) {
			collector = new DCCollector (collector_name);
			result->append (collector);
		}
	} else {
			// Otherwise, just return an empty list
		dprintf (D_ALWAYS, "ERROR: Unable to find collector info in configuration file!!!\n");
	}

	if (collector_name_param)
		free ( collector_name_param );

	return result;
}




int
CollectorList::sendUpdates (int cmd, ClassAd * ad1, ClassAd* ad2) {
	int success_count = 0;

	this->rewind();
	Daemon * daemon;
	while (this->next(daemon)) {
		dprintf (D_FULLDEBUG, 
				 "Trying to update collector %s\n", 
				 daemon->addr());
		if (((DCCollector*)daemon)->sendUpdate (cmd, ad1, ad2)) {
			success_count++;
		} 
	}

	return success_count;
}

QueryResult
CollectorList::query(CondorQuery & query, ClassAdList & adList) {

	int total_size = this->number();
	if (total_size < 1) {
		return Q_NO_COLLECTOR_HOST;
	}

	Daemon * daemon;
	int i=0;
	QueryResult result;

	this->rewind();
	while (this->next(daemon)) {
		dprintf (D_FULLDEBUG, 
				 "Trying to query collector %s\n", 
				 daemon->addr());

		result = 
			query.fetchAds (adList, daemon->addr());
		
		if (result == Q_OK) {
			return result;
		}
		
			// This collector is bad, it should be moved
			// to the end of the list
		this->deleteCurrent();
		this->append (daemon);
		this->rewind();
		
			// Make sure we don't keep rotating through the list
		if (++i >= total_size) break;
	}
			

		// If we've gotten here, there are no good collectors
	return Q_COMMUNICATION_ERROR;
}

