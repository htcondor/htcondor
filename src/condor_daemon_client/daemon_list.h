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

#ifndef _CONDOR_DAEMON_LIST_H
#define _CONDOR_DAEMON_LIST_H

#include "condor_common.h"
#include "daemon.h"
#include "simplelist.h"
#include "condor_classad.h"
#include "condor_query.h"


class CondorQuery;
class ClassAdList;

/** Basically, a SimpleList of Daemon objects.  This is slightly
	more complicated than that, and provides some useful, shared
	functionality that would otherwise have to be duplicated in all
	the places that need a list of Daemon objects (the flocking
	schedd and the SECONDARY_COLLECTORS list in the master, for
	example).  In particular, the code to initialize the list and
	instantiate all the Daemon objects, and the destructor which
	properly cleans up all the memory and destroys all the objects. 
*/

class DaemonList {
public:

	DaemonList();
	virtual ~DaemonList();

		/** Initialize the list with Daemons of the given type,
			specified with the given list of contact info
		*/
	void init( daemon_t type, const char* host_list );

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


 private:

		/** Helper which constructs a Daemon object of the given type
			and contact info.  This is used to initalize the list. 
		*/
	Daemon* buildDaemon( daemon_t type, const char* str );

	SimpleList<Daemon*> list;

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
	static CollectorList * create();

	static CollectorList * createForNegotiator();

		// Send updates to all the collectors
		// return - number of successfull updates
	int sendUpdates (int cmd, ClassAd* ad1, ClassAd* ad2 = NULL);
	
		// Try querying all the collectors until you get a good one
	QueryResult query (CondorQuery & query, ClassAdList & adList);


};



#endif /* _CONDOR_DAEMON_LIST_H */
