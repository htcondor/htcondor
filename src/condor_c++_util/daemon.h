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

typedef struct in_addr sin_addr_t;


/** 
  Class used to pass around and store information about a given
  daemon.  You instantiate one of these objects and pass in the type
  of daemon you care about, and optionally, the name of the daemon and
  pool that it reports to.  The pool and name arguments both default
  to NULL, which means the local pool (the pool that the machine where
  the Daemon object was instantiated reports to) and the local  
  daemon, respectively.  "The local daemon" means different things,
  depending on the type of daemon.  For central manager daemons
  (negotiator, collector, view_collector), it means the CM this
  machine is configured to use (which would normally be a remote host,
  unless you were instantiating the Daemon object on the CM itself).
  For all other daemons (master, startd, schedd), it means the daemon
  running on the local host.

  <p>Information provided by the Daemon class includes the daemon's
  name (which could be a "condor" name such as
  "vm1@raven.cs.wisc.edu", or a full hostname), hostname, fully
  qualified hostname, address (sinful string), ip_addr (sin_addr_t,
  typedef'ed above), port, the type of the daemon (daemon_t enum), the
  pool it reports to (if it's a remote pool) and a bool that describes
  if the daemon is "local" (as described above).

  <p>We use this class to localize all the logic for finding daemons
  in one place, and all the rest of Condor now uses this class.  This
  way, when we add support for a new way of finding daemons (like
  address files, multiple network interface config parameters, etc),
  we can add the code to support it in one place, instead of peppered
  throughout the entire source tree.  */
class Daemon {
public:
		/** Constructor.  Takes the type of the daemon you want
		  (basically, the subsystem, though we use a daemon_t enum.
		  See daemon_types.h for details.  Also, you can pass in the
		  name (a hostname, or Condor-name, such as
		  "vm1@raven.cs.wisc.edu") of the particular daemon you care
		  about.  If you pass in a NULL (the default) for the name, we
		  assume you want the "local" daemon.  Finally, you can pass
		  in the name of the pool you want to query.  If you pass in a
		  NULL (the default), we assume you want the local pool.
		  @param type The type of the daemon, specified in a daemon_t
		  @param name The name of the daemon, NULL if you want local
		  @param pool The name of the pool, NULL if you want local
		  */	
	Daemon( daemon_t type, const char* name = NULL, 
				const char* pool = NULL );

		/// Destructor.
	~Daemon();

		/** Return the daemon's name.  This will return the name of
		  the daemon, which is usually the fully qualified hostname,
		  but might be something else.  For schedds (submit-only
		  schedds, in particular), there might be a user name in front
		  of the hostname, like "wright@raven.cs.wisc.edu".  For SMP
		  startds, you might have a virtual machine id in front, like
		  "vm2@raven.cs.wisc.edu".  Even if you instantiate the Daemon
		  object with a NULL as the argument for the name (to get info
		  on the local daemon), this function will always return the
		  name of the specified daemon (it never returns NULL).
		  @return The name of this daemon (not necessarily the
		  hostname).
		  */
	char* name()			{ return _name; };

		/** Return the hostname where the daemon is running.  This is
		  just the hostname, without the domain.  For example,
		  "raven".  
		  @return Just the hostname where the daemon is running.
		  */
	char* hostname()		{ return _hostname; };

		/** Return the full hostname where the daemon is running.
		  This is the fully qualified hostname, including the domain
		  name.  For example, "raven.cs.wisc.edu".  
		  @return The fully-qualified hostname where the daemon is running.
		  */
	char* fullHostname()	{ return _full_hostname; };

		/** Return the address of the daemon.  This is given as a
		  "sinful string", which is used throughout Condor to specify
		  IP/port pairs.  It is just a character string, of the form:
		  "<numeric.ip.addr:port>", for example,
		  "<128.105.101.17:4363>".  This function will always return a
		  value, never NULL. 
		  @return The sinful string of the daemon.
		  */
	char* addr()			{ return _addr; };

		/** Return the remote pool this daemon reports to.  If the
		  requested daemon is reporting to a remote pool (not the
		  local pool for the machine that instantiated the daemon
		  object), this full hostname of the collector for the pool is
		  given here.  This function returns NULL if the daemon is
		  reporting to the local pool.
		  @return The name of the collector for the daemon.
		  */
	char* pool()			{ return _pool; };

		/** Return the port the daemon is listening on.  This is the
		  integer port number that daemon has bound to.  It will
		  always be a positive, non-zero integer.
		  @return The port the daemon is listening on.
		  */
	int port()				{ return _port; };

		/** Return the IP address the daemon is listening on.  This
		  returns a pointer to a sin_addr_t (typedef'ed version of
		  struct in_addr), stored within the Daemon object itself,
		  which holds the IP address.  This structure is used for many
		  network system calls to specify an IP addr.  The pointer
		  returned will always be a valid address, never NULL.
		  @return The IP address the daemon is listening on.
		  */
	sin_addr_t* sinAddr()	{ return &_sin_addr; };

		/** Return the type of the daemon.  This returns a daemon_t
		  enum that specifies the type of the requested daemon (which
		  is a required parameter to the constructor).  See
		  daemon_types.h for details on this enum.
		  @return The type of the daemon.
		  */
	daemon_t type()			{ return _type; };

		/** Return whether this daemon is local.  We return a bool
		  that specifies whether or not this daemon is "local".  
		  See the top-level documentation for this class for details
		  on exactly what "local" means for the different types of
		  daemons.  
		  @return If this daemon is "local".
		  */
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
	sin_addr_t _sin_addr;
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
		  need to be sure to connect to the right one.  We return
		  success or failure on whether we found any of the parameters
		  we were looking for that describe where the CM is.  This is
		  useful when we're trying to find the condor_view collector,
		  b/c if we can't find condor_view-specific entries, we fall
		  back and try to just find the default collector.  
		  @param subsys The subsystem string for this daemon
		  @param port The fixed port for this daemon
		  @return Whether or not we found the info we want
		  */
	bool get_cm_info( const char* subsys, int port );
};

#endif /* CONDOR_DAEMON_H */
