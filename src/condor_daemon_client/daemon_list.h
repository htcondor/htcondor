/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.
 * No use of the CONDOR Software Program Source Code is authorized
 * without the express consent of the CONDOR Team.  For more
 * information contact: CONDOR Team, Attention: Professor Miron Livny,
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

#ifndef _CONDOR_DAEMON_LIST_H
#define _CONDOR_DAEMON_LIST_H

#include "daemon.h"
#include "simplelist.h"


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
	~DaemonList();

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


#endif /* _CONDOR_DAEMON_LIST_H */
