/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department,
 *University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.
 *No use of the CONDOR Software Program Source Code is authorized
 *without the express consent of the CONDOR Team.  For more
 *information contact: CONDOR Team, Attention: Professor Miron Livny,
 *7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685,
 *(608) 262-0856 or miron@cs.wisc.edu.
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

#ifndef CONDOR_DAEMON_H
#define CONDOR_DAEMON_H

#include "condor_common.h"
#include "condor_io.h"
#include "condor_classad.h"
#include "condor_collector.h"
#include "daemon_types.h"

/** 
  Class used to pass around and store information about a given
  daemon, such as its name, address (sinful string), ip_addr, and
  port. 
*/
class Daemon {
public:
		/** Constructor.  Takes the type of the daemon you want
		  (basically, the subsystem, though we use a daemon_t enum.
		  See daemon_types.h for details.  Also, you can pass in the
		  name (a hostname, or Condor-name, such as
		  "vm1@raven.cs.wisc.edu") of the particular daemon you care
		  about.  If you pass in a NULL (the default) for the name, we
		  assume you want the local daemon.  Finally, you can pass in
		  the name of the pool you want to query.  If you pass in a
		  NULL (the default), we assume you want the local pool.
		  @param type The type of the daemon, specified in a daemon_t
		  @param name The name of the daemon, NULL if you want local
		  @param pool The name of the pool, NULL if you want local
		  */	
	Daemon( daemon_t type, const char* name = NULL, 
				const char* pool = NULL );

		/// Destructor
	~Daemon();

		/// @return The name of this daemon (not necessarily the hostname). 
	char* name()			{ return _name; };

		/// @return Just the hostname where the daemon is running.
	char* hostname()		{ return _hostname; };

		/// @return The fully-qualified hostname where the daemon is running.
	char* fullHostname()	{ return _full_hostname; };

		/// @return The sinful string of the daemon.
	char* addr()			{ return _addr; };

		/// @return The name of the collector for the daemon.
	char* pool()			{ return _pool; };

		/// @return The port the daemon is listening on.
	int port()				{ return _port; };

		/// @return The IP address the daemon is listening on.
	struct in_addr* sinAddr()	{ return &_sin_addr; };

		/// @return The type of the daemon (See daemon_types.h for details).
	daemon_t type()			{ return _type; };

		/// @return If this daemon is the local daemon or not.
	bool is_local()			{ return _is_local; };

		/**	Create a new ReliSock object, connected to the daemon.
		  Callers can optionally specify a timeout to use for the
		  connect().  If there was a failure in connect(), we delete
		  the object and return NULL.
		  @param sec Number of seconds for the timeout on connect().
		  @return A new ReliSock object connected to the daemon.  
		  */
	ReliSock* reliSock( int sec = 0 );

		/**	Create a new SafeSock object, connected to the daemon.
		  Callers can optionally specify a timeout to use for the
		  connect().  If there was a failure in connect(), we delete 
		  the object and return NULL.
		  @param sec Number of seconds for the timeout on connect().
		  @return A new SafeSock object connected to the daemon.  
		  */
	SafeSock* safeSock( int sec = 0 );

private:
	// Data members

	char* _name;
	char* _hostname;
	char* _full_hostname;
	char* _addr;
	char* _pool;
	int _port;
	struct in_addr _sin_addr;
	daemon_t _type;
	bool _is_local;

	// Helper functions

		/** Initialization code.  Figures out what kind of daemon we
		  want info on and calls the appropriate helper.
		  */
	void init();

		/** Helper for regular daemons (schedd, startd, master).
		  This does all the real work of finding the right address,
		  port, ip_addr, etc.  We check for address files, and query
		  the appropriate collector if that doesn't work.
		  @param subsys The subsystem string for this daemon
		  @param attrirbute The attribute from the classad that stores
		  the address we want
		  @param adtype The type of ClassAd we'll query.
		  */
	void get_daemon_info( const char* subsys, 
						  const char* attribute, AdTypes adtype );

		/** Helper for central manager daemons (collector and
		  negotiator).  These are a special case since they have
		  well-known ports, instead of needing to query the central
		  manager (of course).   Also, we have to deal with the fact
		  that the CM might have multiple network interfaces, and we
		  need to be sure to connect to the right one.
		  @param subsys The subsystem string for this daemon
		  @param port The fixed port for this daemon
		  */
	bool get_cm_info( const char* subsys, int port );
};

#endif /* CONDOR_DAEMON_H */
