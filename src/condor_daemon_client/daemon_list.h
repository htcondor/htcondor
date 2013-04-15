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


#ifndef _CONDOR_DAEMON_LIST_H
#define _CONDOR_DAEMON_LIST_H

#include "condor_common.h"
#include "daemon.h"
#include "simplelist.h"
#include "condor_classad.h"
#include "condor_query.h"


class DCCollector;
class CondorQuery;

/** Basically, a SimpleList of Daemon objects.  This is slightly
	more complicated than that, and provides some useful, shared
	functionality that would otherwise have to be duplicated in all
	the places that need a list of Daemon object.  In particular, the
	code to initialize the list and instantiate all the Daemon
	objects, and the destructor which properly cleans up all the
	memory and destroys all the objects.
*/

class DaemonList {
public:

	DaemonList();
	virtual ~DaemonList();

		/** Initialize the list with Daemons of the given type,
			specified with the given list of contact info
		*/
	void init( daemon_t type, const char* host_list, const char* pools=NULL );

		// Provide the same interface that both SimpleList and
		// StringList do (which, unfortunately, are not the same). 

		// General
    bool append( Daemon* );
    bool Append( Daemon* );
	bool isEmpty( void );
	bool IsEmpty( void );
    int number( void );
    int Number( void );

		// Scans
	void rewind( void );
	void Rewind( void );
    bool current( Daemon* & );
    bool Current( Daemon* & );
    bool next( Daemon* &);
    bool Next( Daemon* &);
    bool atEnd();
    bool AtEnd();

		// Removing
	void deleteCurrent();
	void DeleteCurrent();

 protected:
	SimpleList<Daemon*> list;


 private:

		/** Helper which constructs a Daemon object of the given type
			and contact info.  This is used to initalize the list. 
		*/
	Daemon* buildDaemon( daemon_t type, const char* host, char const *pool );

		// I can't be copied (yet)
	DaemonList( const DaemonList& );
	DaemonList& operator = ( const DaemonList& );
};


class CollectorList : public DaemonList {
 public:
	CollectorList();
	virtual ~CollectorList();

		// Create the list of collectors for the pool
		// based on configruation settings
	static CollectorList * create(const char * pool = NULL);

		// Resort a collector list for locality (for negotiator)
	int resortLocal( const char *preferred_collector );

		// Send updates to all the collectors
		// return - number of successfull updates
	int sendUpdates (int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking);
	
		// Try querying all the collectors until you get a good one
	QueryResult query (CondorQuery & query, ClassAdList & adList, CondorError *errstack = 0);

    bool next( DCCollector* &);
    bool Next( DCCollector* &);
    bool next( Daemon* &);
    bool Next( Daemon* &);

};



#endif /* _CONDOR_DAEMON_LIST_H */
