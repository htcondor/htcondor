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


#ifndef CONDOR_DAEMON_H
#define CONDOR_DAEMON_H

/* We have circular references between Daemon (declared in this file)
 * and DCMsg (declared in dc_message.h), so we need to foward declare
 * Daemon before the includes.
 */
class Daemon;

#include "condor_common.h"
#include "condor_io.h"
#include "condor_classad.h"
#include "condor_secman.h"
#include "daemon_types.h"
#include "KeyCache.h"
#include "CondorError.h"
#include "command_strings.h"
#include "dc_message.h"

#define COLLECTOR_PORT					9618

/** 
  Class used to pass around and store information about a given
  daemon.  You instantiate one of these objects and pass in the type
  of daemon you care about, and optionally, the name of the daemon and
  pool that it reports to.  The pool and name arguments both default
  to NULL, which means the local pool (the pool that the machine where
  the Daemon object was instantiated reports to) and the local  
  daemon, respectively.  

  <p>Once you have the object, you call the locate() method to have
  it actually try to find all the information about the daemon it can
  provide.  If you call member functions for info before you call
  locate(), the object will call locate() for you, though that can
  obscure errors, so it's generally discouraged.

  <p>If there are any errors, the error() method will stop returning
  NULL and start returning a string that describes the error, which
  can then be used for dprintf(), etc.

  <p>"The local daemon" means different things, depending on the type
  of daemon.  For central manager daemons (negotiator, collector,
  view_collector), it means the CM this machine is configured to use
  (which would normally be a remote host, unless you were
  instantiating the Daemon object on the CM itself).  For all other
  daemons (master, startd, schedd), it means the daemon running on the
  local host.

  <p>Information provided by the Daemon class includes the daemon's
  name (which could be a "condor" name such as
  "slot1@raven.cs.wisc.edu", or a full hostname), hostname, fully
  qualified hostname, address (sinful string), port, the type of the
  daemon (daemon_t enum), the pool it reports to (if it's a remote
  pool) and a bool that describes if the daemon is "local" (as
  described above).

  <p>We use this class to localize all the logic for finding daemons
  in one place, and all the rest of Condor now uses this class.  This
  way, when we add support for a new way of finding daemons (like
  address files, multiple network interface config parameters, etc),
  we can add the code to support it in one place, instead of peppered
  throughout the entire source tree.  */
class Daemon: public ClassyCountedPtr {

	friend class DaemonAllowLocateFull;

public:
		/** Constructor.  Takes the type of the daemon you want
		  (basically, the subsystem, though we use a daemon_t enum.
		  See daemon_types.h for details).  Also, you can pass in the
		  name (a hostname, or Condor-name, such as
		  "slot1@raven.cs.wisc.edu") of the particular daemon you care
		  about.  If you pass in a NULL (the default) for the name, we
		  assume you want the "local" daemon.  Alternatively, you can
		  pass in the "sinful string" of the daemon you care about.  
		  Finally, you can pass in the name of the pool you want to
		  query.  If you pass in a NULL (the default), we assume you
		  want the local pool.

		  @param type The type of the daemon, specified in a daemon_t
		  @param name The name (or sinful string) of the daemon, NULL if you want local
		  @param pool The name of the pool, NULL if you want local */
	Daemon( daemon_t type, const char* name = NULL, 
				const char* pool = NULL );

		/** Another version of the constructor that takes a ClassAd
			and gets all the info out of that instead of having to
			query a collector to locate it.  You can also optionally
			pass in the name of the collector you got it from.
		*/
	Daemon( const ClassAd* ad, daemon_t type, const char* pool );

		/// Copy constructor (implemented via deepCopy())
	Daemon( const Daemon &copy );

		/// Overloaded assignment operator (implemented via deepCopy())
	Daemon& operator = ( const Daemon& );

		/// Destructor.
	virtual ~Daemon();

		/** Find all information about the daemon.
		  All the different methods you might use to
		  find the info are supposed to be handled through here: fully
		  resolving hostnames, address files, querying the collector,
		  well-known addresses, DNS lookups, whatever it takes.  If
		  this fails, you can call error() to get a string describing
		  what went wrong.
		  LocateType method defaults to LOCATE_FOR_LOOKUP; if
		  you really need LOCATE_FULL (not likely), you will want
		  to use the DaemonAllowLocateFull subclass.
		  @return Success or failure of getting all the info.
		*/
	enum LocateType {LOCATE_FULL, LOCATE_FOR_LOOKUP};
	virtual bool locate( LocateType method=LOCATE_FOR_LOOKUP );

		/** Return the error string.  If there's ever a problem
		  enountered in the Daemon object, this will start returning a
		  string desribing it.  Returns NULL if there's no error.
		  */
	const char* error( void )	{ return _error; }

 		/** Return the result code of the previous action.  If there's
			a problem and the error() string above is set, this result
			code will specify the type of error encountered.  The
			CAResult enum and some helper functions to convert it
			to/from strings are in condor_utils/command_string.[Ch]
		*/
	CAResult errorCode( void ) { return _error_code; }

		// //////////////////////////////////////////////////////////
		/// Methods for getting information about the daemon.
		// //////////////////////////////////////////////////////////

		/** Return the daemon's name.  This will return the name of
		  the daemon, which is usually the fully qualified hostname,
		  but might be something else.  For schedds (submit-only
		  schedds, in particular), there might be a user name in front
		  of the hostname, like "wright@raven.cs.wisc.edu".  For SMP
		  startds, you might have a slot id in front, like
		  "slot2@raven.cs.wisc.edu".  Even if you instantiate the
		  Daemon object with a NULL as the argument for the name (to
		  get info on the local daemon), this function will always
		  return the name of the specified daemon (it never returns
		  NULL).  @return The name of this daemon (not necessarily the
		  hostname).
		  */
	const char* name( void );

		/** Return the hostname where the daemon is running.  This is
		  just the hostname, without the domain.  For example,
		  "raven".  This function will return NULL only if there was
		  an error in the locate() method.
		  @return Just the hostname where the daemon is running.
		  */
	const char* hostname( void );

		/** 
			@return The version string of this daemon, or NULL if we
			don't know it.
		*/
	const char* version( void );

		/** 
			@return The platform string of this daemon, or NULL if we
			don't know it.
		*/
private:
	const char* platform( void );
public:

		/** Return the full hostname where the daemon is running.
		  This is the fully qualified hostname, including the domain
		  name.  For example, "raven.cs.wisc.edu".  This function will
		  return NULL only if there was an error in the locate()
		  method. 
		  @return The fully-qualified hostname where the daemon is running.
		  */
	const char* fullHostname( void );

		/** Return the address of the daemon.  This is given as a
		  "sinful string", which is used throughout Condor to specify
		  IP/port pairs.  It is just a character string, of the form:
		  "<numeric.ip.addr:port>", for example,
		  "<128.105.101.17:4363>".  This function will return NULL
		  only if there was an error in the locate() method.
		  @return The sinful string of the daemon.
		  */
	const char* addr( void );

		/** Return the remote pool this daemon reports to.  If the
		  requested daemon is reporting to a remote pool (not the
		  local pool for the machine that instantiated the daemon
		  object), this full hostname of the collector for the pool is
		  given here.  This function returns NULL if the daemon is
		  reporting to the local pool.
		  @return The name of the collector for the daemon.
		  */
	const char* pool( void );

		/** Return the port the daemon is listening on.  This is the
		  integer port number that daemon has bound to.  It will
		  always be a positive, non-zero integer, unless there was an
		  error in locate(), in which case we return -1.
		  @return The port the daemon is listening on.
		  */
	int port( void );

		/** Return the type of the daemon.  This returns a daemon_t
		  enum that specifies the type of the requested daemon (which
		  is a required parameter to the constructor).  See
		  daemon_types.h for details on this enum.
		  @return The type of the daemon.
		  */
	daemon_t type( void )		{ return _type; }

		/** Return whether this daemon is local.  See the top-level
		  documentation for this class for details on exactly what
		  "local" means for the different types of daemons.
		  */
	bool isLocal( void ) const			{ return _is_local; }

		/** Returns a descriptive string for error messages.  This has
		  all the logic about printing out an appropriate string to
		  describe the daemon, it's type, and it's location.  For
		  example: "the local master", "the startd on
		  raven.cs.wisc.edu", etc.
		  */
	const char* idStr( void );

		/** Dump all info about this daemon to the given debug
		  level.
		  */
	void display( int debugflag );

		/** Dump all info about this daemon to the given FILE*
		  */
	void display( FILE* fp );

		// //////////////////////////////////////////////////////////
		/// Methods for communicating with the daemon.
		// //////////////////////////////////////////////////////////

		/**	Create a new ReliSock object, connected to the daemon.
		  Callers can optionally specify a timeout to use for the
		  connect().  If there was a failure in connect(), we delete
		  the object and return NULL.
		  @param timeout Number of seconds for the timeout on connect().
		  @param deadline Time at which to give up (0 if never).
		  @return A new ReliSock object connected to the daemon.  
		  */
	ReliSock* reliSock( int timeout = 0, time_t deadline = 0,
						CondorError* errstack = 0, bool non_blocking = false,
						bool ignore_timeout_multiplier = false );

		/**	Create a new SafeSock object, connected to the daemon.
		  Callers can optionally specify a timeout to use for the
		  connect().  If there was a failure in connect(), we delete 
		  the object and return NULL.
		  @param timeout Number of seconds for the timeout on connect().
		  @param deadline Time at which to give up (0 if never).
		  @return A new SafeSock object connected to the daemon.  
		  */
	SafeSock* safeSock( int timeout = 0, time_t deadline = 0,
						CondorError* errstack = 0, bool non_blocking = false );

public:
		/**	Create a new Sock object connected to the daemon.
		  Callers can optionally specify a timeout to use for the
		  connect().  If there was a failure in connect(), we delete 
		  the object and return NULL.
		  @param timeout Number of seconds for the timeout on connect().
		  @param deadline Time at which to give up (0 if never).
		  @return A new Sock object connected to the daemon.  
		  */
	Sock *makeConnectedSocket( Stream::stream_type st = Stream::reli_sock,
							   int timeout = 0, time_t deadline = 0,
							   CondorError* errstack = NULL,
							   bool non_blocking = false );

		/**	Connects a socket to the daemon.
		  Callers can optionally specify a timeout to use for the
		  connect().  If there was a failure in connect(), we delete 
		  the object and return NULL.
		  @param sec Number of seconds for the timeout on connect().
		             (If 0, then uses timeout already set on socket, if any.)
		  @return true if connection attempt successful
		  */
	bool connectSock(Sock *sock, int sec=0, CondorError* errstack=NULL, bool non_blocking=false, bool ignore_timeout_multiplier=false );

		/** Send the given command to the daemon.  The caller gives
		  the command they want to send, the type of Sock they
		  want to use to send it over, and an optional timeout.  
		  We then instantiate a new Sock of the right type and
		  timeout, send the command, and finally, the end_of_message().  
		  The Sock is then destroyed.
		  @param cmd The command you want to send.
		  @param st The type of the Sock you want to use.
		  @param sec The timeout you want to use on your Sock.
		  @return Success or failure.
		  */
	bool sendCommand( int cmd, 
					   Stream::stream_type st = Stream::reli_sock,
					   int sec = 0, CondorError* errstack = NULL,
					  char const *cmd_description=NULL );
	
		/** Send the given command to the daemon.  The caller gives
		  the command they want to send, a pointer to the Sock they
		  want us to use to send it over, and an optional timeout.
		  This method will then put the desired timeout on that sock,
		  place it in encode() mode, send the command, and finally,
		  the end_of_message().  The sock is otherwise left alone 
		  (i.e. not destroyed)
		  @param cmd The command you want to send.
		  @param sock The Sock you want to use.
		  @param sec The timeout you want to use on your Sock.
		  @return Success or failure.
		  */
	bool sendCommand( int cmd, Sock* sock, int sec = 0, CondorError* errstack = NULL, char const *cmd_description=NULL );

		/** Start sending the given command to the daemon.  The caller
		  gives the command they want to send, and the type of Sock
		  they want to use to send it over.  This method will then
		  allocate a new Sock of the right type, send the command, and
		  return a pointer to the Sock while it is still in encode()
		  mode.  If there is a failure, it will return NULL.
		  THE CALLER IS RESPONSIBLE FOR DELETING THE SOCK.
		  @param cmd The command you want to send.
		  @param st The type of the Sock you want to use.
		  @param sec The timeout you want to use on your Sock.
		  @param errstack NULL or error stack to dump errors into.
		  @param raw_protocol to bypass all security negotiation, set to true
		  @param sec_session_id use specified session if available
		  @return NULL on error, or the Sock object to use for the
		  rest of the command on success.
		  */
	Sock* startCommand( int cmd, 
				Stream::stream_type st = Stream::reli_sock,
				int sec = 0, CondorError* errstack = NULL,
				char const *cmd_description = NULL,
				bool raw_protocol=false, char const *sec_session_id=NULL );


		/** Start sending the given command and subcommand to the daemon. The caller
		  gives the command they want to send, and the type of Sock
		  they want to use to send it over.  This method will then
		  allocate a new Sock of the right type, send the command, and
		  return a pointer to the Sock while it is still in encode()
		  mode.  If there is a failure, it will return NULL.
		  THE CALLER IS RESPONSIBLE FOR DELETING THE SOCK.
		  @param cmd The command you want to send.
		  @param subcmd The sub command you want to send with DC_AUTHENTICATE
		  @param st The type of the Sock you want to use.
		  @param sec The timeout you want to use on your Sock.
		  @param errstack NULL or error stack to dump errors into.
		  @param raw_protocol to bypass all security negotiation, set to true
		  @param sec_session_id use specified session if available
		  @return NULL on error, or the Sock object to use for the
		  rest of the command on success.
		  */
	Sock* startSubCommand( int cmd, int subcmd,
				Stream::stream_type st = Stream::reli_sock,
				int sec = 0, CondorError* errstack = NULL,
				char const *cmd_description = NULL,
				bool raw_protocol=false, char const *sec_session_id=NULL );

		/** Start sending the given command to the daemon.  The caller
		  gives the command they want to send, and a pointer to the
		  Sock they want us to use to send it over.  This method will
		  then place that Sock in encode() mode, send the command, and
		  return true on success, false on failure.  See
		  startCommand_nonblocking for a non-blocking interface.
		  @param cmd The command you want to send.
		  @param sock The Sock you want to use.
		  @param sec The timeout you want to use on your Sock.
		  @param errstack NULL or error stack to dump errors into.
		  @param raw_protocol to bypass all security negotiation, set to true
		  @param sec_session_id use specified session if available
		  @return false on error, true on success.
		*/
	bool startCommand( int cmd, Sock* sock,
			int sec = 0, CondorError* errstack = NULL,
			char const *cmd_description=NULL,
			bool raw_protocol=false, char const *sec_session_id=NULL );

		/** Start sending the given command and subcommand to the daemon.  The caller
		  gives the command they want to send, and a pointer to the
		  Sock they want us to use to send it over.  This method will
		  then place that Sock in encode() mode, send the command, and
		  return true on success, false on failure.  See
		  startCommand_nonblocking for a non-blocking interface.
		  @param cmd The command you want to send.
		  @param subcmd The sub command you want to send with DC_AUTHENTICATE
		  @param sock The Sock you want to use.
		  @param sec The timeout you want to use on your Sock.
		  @param errstack NULL or error stack to dump errors into.
		  @param raw_protocol to bypass all security negotiation, set to true
		  @param sec_session_id use specified session if available
		  @return false on error, true on success.
		*/
	bool startSubCommand( int cmd, int subcmd, Sock* sock,
			int sec = 0, CondorError* errstack = NULL,
			char const *cmd_description=NULL,
			bool raw_protocol=false, char const *sec_session_id=NULL );
			
		/** Start sending the given command to the daemon.  This
			command claims to be nonblocking, but currently it only
			uses nonblocking connects; everything else is blocking.
			The caller gives the command they want to send, and the
			type of Sock they want to use to send it over.  This
			method will then allocate a new Sock of the right type,
			send the command, and callback the specified function with
			success indicator and a pointer to the sock while it is
			still in encode() mode. THE CALLBACK FUNCTION (if any) IS
			RESPONSIBLE FOR DELETING THE SOCK.
			The caller MUST ensure that sock and errstack do not get
			deleted before this operation completes.  It is ok if the
			daemon client object itself gets deleted before then.
			Note that this function will only work in DaemonCore
			applications, because it relies on the DaemonCore non-blocking
			event callbacks.
			Note that the callback may occur inside the call to
			startCommand(), mainly in error cases.
			@param cmd The command you want to send.
			@param st The type of the Sock you want to use.
			@param sec The timeout you want to use on your Sock.
			@param errstack NULL or error stack to dump errors into.
			@param callback_fn function to call when finished
			                   Must be non-NULL
			@param misc_data any data caller wants passed to callback_fn
			@param raw_protocol to bypass all security negotiation, set to true
			@param sec_session_id use specified session if available
			@return see definition of StartCommandResult enumeration.
		  */
	StartCommandResult startCommand_nonblocking( int cmd, Stream::stream_type st, int timeout, CondorError *errstack, StartCommandCallbackType *callback_fn, void *misc_data, char const *cmd_description=NULL, bool raw_protocol=false, char const *sec_session_id=NULL );

		/** Start sending the given command to the daemon.  This
			command claims to be nonblocking, but currently it only
			uses nonblocking connects; everything else is blocking.
			The caller gives the command they want to send, and a
			pointer to the Sock they want us to use to send it over.
			This method will then place that Sock in encode() mode,
			send the command, and callback the specified function with
			true on success, false on failure.
			The caller MUST ensure that sock and errstack do not get
			deleted before this operation completes.  It is ok if the
			daemon client object itself gets deleted before then.
			Note that this function will only work in DaemonCore
			applications, because it relies on the DaemonCore non-blocking
			event callbacks.
			Note that the callback may occur inside the call to
			startCommand(), mainly in error cases.
			@param cmd The command you want to send.
			@param sock The	Sock you want to use.
			@param timeout The number of seconds you want to use on your Sock.
			@param errstack NULL or errstack to dump errors into
			@param callback_fn NULL or function to call when finished
			                   If NULL and sock is UDP, will return
							   StartCommandWouldBlock if TCP session key
							   setup is in progress.
			@param misc_data any data caller wants passed to callback_fn
			@param raw_protocol to bypass all security negotiation, set to true
			@param sec_session_id use specified session if available
			@return see definition of StartCommandResult enumeration.
		*/
	StartCommandResult startCommand_nonblocking( int cmd, Sock* sock, int timeout, CondorError *errstack, StartCommandCallbackType *callback_fn, void *misc_data, char const *cmd_description=NULL, bool raw_protocol=false, char const *sec_session_id=NULL );

		/**
		 * Asynchronously send a message (command + whatever) to the
		 * daemon.  Both this daemon object and the msg object should
		 * be allocated on the heap so that they are not deleted
		 * before this operation completes.  Garbage collection is
		 * handled via reference-counting ala ClassyCountedPtr.
		 * @param msg - the message to send
		 * @return void - all error handling should happen in DCMsg
		 */
	void sendMsg( classy_counted_ptr<DCMsg> msg );

		/**
		 * Synchronously send a message (command + whatever) to the
		 * daemon.  Both this daemon object and the msg object should
		 * be allocated on the heap so that they are not deleted
		 * before this operation completes.  Garbage collection is
		 * handled via reference-counting ala ClassyCountedPtr.
		 * @param msg - the message to send
		 * @return void - all error handling should happen in DCMsg
		 */
	void sendBlockingMsg( classy_counted_ptr<DCMsg> msg );

		/**
		 * returns true if target daemon has a UDP command port.
		 * Currently, we assume the answer is "yes" unless the target
		 * daemon is being accessed via CCB or a shared port.
		 * Eventually, we could be smarter.
		 */
	bool hasUDPCommandPort();

		/**
		 * Contact another daemon and initiate the time offset range 
		 * determination logic. We create a socket connection, pass the
		 * DC_TIME_OFFSET command then pass the Stream to the cedar stub
		 * code for time offset. If this method returns false, then
		 * that means we were not able to coordinate our communications
		 * with the remote daemon
		 * 
		 * @param offset - the reference placeholder for the range
		 * @return true if it was able to contact the other Daemon
		 **/
 	bool getTimeOffset( long &offset );
 	
		/**
		 * Contact another daemon and initiate the time offset range 
		 * determination logic. We create a socket connection, pass the
		 * DC_TIME_OFFSET command then pass the Stream to the cedar stub
		 * code for time offset. The min/max range value placeholders
		 * are passed in by reference. If this method returns false, then
		 * that means for some reason we could not get the range and the
		 * range values will default to a known value.
		 * 
		 * @param min_range - the minimum range value for the time offset
		 * @param max_range - the maximum range value for the time offset
		 * @return true if it was able to contact the other Daemon and get range
		 **/
	bool getTimeOffsetRange( long &min_range, long &max_range );

		/**
		 * Set the name of the subsystem
		 *
		 * @param subsys - The subsystem string for this daemon
		 **/
	bool setSubsystem( const char* subsys );

		/*
		 * Interate to the next CM daemon in a case where
		 * more than 1 central manager is configured
		 **/
	bool nextValidCm();

		/*
		 * Reset the list of CM daemons to the first one
		 **/
	void rewindCmList();

		/*
		 * Contact another daemon and get its instance ID, which is a
		 * random number generated once in the first response to this query.
		 */
	bool getInstanceID( std::string & instanceID );

		/*
		 * Request a token from the remote daemon.
		 *
		 * Caller can optionally request a maximum token lifetime; if none is desired,
		 * then set `lifetime` to -1.
		 */
	bool getSessionToken( const std::vector<std::string> &authz_bounding_limit, int lifetime,
		std::string &token, CondorError *err=NULL );

		/*
		 * Start a token request workflow from the remote daemon, potentially as an
		 * unauthenticated user.
		 *
		 * As with `getSessionToken`, the user can request limits on the token's
		 * capabilities (maximum lifetime, bounding set) and additionally request a
		 * specific identity.
		 *
		 * The caller should provide a human-friendly ID (not necessarily unique; may
		 * be the hostname) that will help the request approver to identify the request.
		 *
		 * If we are able to authenticate, then the token may be issued immediately;
		 * in that case, the function will set the `token` parameter to a non-empty string
		 * and return true.  If we are unable to authenticate (or have insufficient
		 * permissions at the server-side), then the `request_id` parameter will be
		 * set; use `finishTokenRequest` to poll for completion.
		 *
		 * Returns true on success or false on failure.
		 */
	bool startTokenRequest( const std::string &identity,
		const std::vector<std::string> &authz_bounding_set, int lifetime,
		const std::string &client_id, std::string &token, std::string &request_id,
		CondorError *err=NULL ) noexcept;

		/**
		 * Poll for and finish a token request.
		 *
		 * The `request_id` is the ID returned by a prior call to `startTokenRequest`.
		 */
	bool finishTokenRequest( const std::string &client_id, const std::string &request_id,
		std::string &token, CondorError *err=nullptr ) noexcept;

		/**
		 * List all the token requests in the system.
		 *
		 * The `request_id`, if non-empty, causes only a given request to be returned;
		 * otherwise, all requests are returned.
		 */
	bool listTokenRequest( const std::string &request_id, std::vector<classad::ClassAd> &results,
		CondorError *err=nullptr ) noexcept;

		/**
		 * Approve a specific token request.
		 *
		 * The `request_id` and `client_id` must match a given request.
		 */
	bool approveTokenRequest( const std::string &client_id, const std::string &request_id,
		CondorError *err=nullptr ) noexcept;

		/**
		 * Create a rule to auto-approve future requests.
		 *
		 * The `netblock` (example: 192.168.0.1/24) and `lifetime` (seconds)
		 * define the tool we are to install.
		 */
	bool autoApproveTokens( const std::string &netblock, time_t lifetime,
		CondorError *err=nullptr ) noexcept;

		/**
		 * Exchange a SciToken for a HTCondor token.
		 */
	bool exchangeSciToken( const std::string &scitoken, std::string &token, CondorError &err ) noexcept;

		/**
		 * When authentication fails - but TOKEN is a valid method - this is set to true.
		 */
	bool shouldTryTokenRequest() const {return m_should_try_token_request;};

		/**
		 * Last recorded trust domain from this daemon.
		 */
	std::string getTrustDomain() const {return m_trust_domain;}

		// Set the owner for this daemon; if possible, always
		// authenticate with the remote daemon as this owner.
	void setOwner(const std::string &owner) {m_owner = owner;}
	const std::string &getOwner() const {return m_owner;}

		// Set the authentication methods to use with this daemon object;
		// overrides those built-in to the param table.
	void setAuthenticationMethods(const std::vector<std::string> &methods) {m_methods = methods;}
	const std::vector<std::string> &getAuthenticationMethods() const {return m_methods;}

protected:
	// Data members
	char* _name;
	char* _hostname;
	char* _full_hostname;
	char* _addr;
	char* _alias;
	bool m_has_udp_command_port;
	char* _version;
	char* _platform;
	char* _pool;
	char* _error;
	CAResult _error_code;
	char* _id_str;
	char* _subsys;
	int _port;
	daemon_t _type;
	bool _is_local;
	bool _tried_locate;
	bool _tried_init_hostname;
	bool _tried_init_version;
	bool _is_configured;
	bool m_should_try_token_request{false};
	SecMan _sec_man;
	StringList daemon_list;



		// //////////////////////////////////////////////////////////
		/// Helper methods.
		// //////////////////////////////////////////////////////////

		/** Initializes the object by setting everything to NULL.
			Shared by all the different constructors.
		  */
	void common_init();

		/**
		   Make a deep copy of this Daemon, used by copy constructor
		   and assignment operator.
		*/
	void deepCopy( const Daemon& copy );

		/** Helper for regular daemons (schedd, startd, master).
		  This does all the real work of finding the right address,
		  port, ip_addr, etc.  We check for address files, and query
		  the appropriate collector if that doesn't work.
		  @param adtype The type of ClassAd we'll query.
		  @parma query_collector Whether to query collector if all else fails
		  */
	bool getDaemonInfo( AdTypes adtype, bool query_collector, LocateType method );

		/** Helper for central manager daemons (collector and
		  negotiator).  These are a special case since they have
		  well-known ports, instead of needing to query the central
		  manager (of course).  Uses findCmDaemon to determine the
		  parameters for central manager daemons.  This is useful
		  when we're trying to find the condor_view collector,
		  b/c if we can't find condor_view-specific entries, we fall
		  back and try to just find the default collector.  
		  @param subsys The subsystem string for this daemon
		  @return Whether or not we found the info we want
		  */
	bool getCmInfo( const char* subsys );

		/** Deal with the fact that the CM might have multiple
		  network interfaces, and we need to be sure to connect
		  to the right one.  We return success or failure on whether
		  we found any of the parameters we were looking for that
		  describe where the CM is.
		  @param name The hostname of a central manager
		  @return Whether all the parameters for the CM were found
		  */
	bool findCmDaemon( const char* name );


		/** Helper to initialize the hostname if we don't have it
			already, but we do have an IP address.  Usually, when we
			locate(), we can get all the hostname info for free, but
			if we're instantiated just with a sinful string, we don't
			bother looking that IP up in the appropriate hostinfo
			database unless we need it.
		*/
	bool initHostname( void );

		/** Helper to initialize the hostname once we have the full
			hostname.  This is shared in a few places, so it now lives
			in a seperate method.
		*/
	bool initHostnameFromFull( void );

		/** Helper to initialize the version and platform string if we
			don't have it already.  Usually, when we locate(), we can
			get all this info for free, either from the local address
			file or the ClassAd we got back from querying a collector.
			However, if we don't have it, we don't go out of our way
			to find it (like calling ident on the binary) unless we
			need it.
		*/
	bool initVersion( void );

		/** Get the default port based on what type of Daemon we are */
	int getDefaultPort( void );

		/** Set a new value for the error string.  If the error string
		  is already set, deallocate the existing string.  Then, make
		  a copy of the given string and store that in _error.
		  */
	void newError( CAResult, const char* );

		/** Returns a string containing the local daemon's name.  If
		  the <subsys>_NAME parameter is set in the config file, we
		  use that, and pass it to build_valid_daemon_name() to make
		  sure we have a fully-qualified hostname.  If not, we just
		  use get_local_fqdn().  The string we return is newly
		  allocated and should be deallocated with free(). 
		  */
	char* localName( void );

		/** Code for parsing a locally-written address file to find
			the IP, port (and version, if available) for our daemon.
			This is shared in a couple of places, so it now lives in a
			seperate helper method.
			@return true if we found the address in the file, false if not
		*/
	bool readAddressFile( const char* subsys );

		/** Should we try to access the daemon via the super port?
		    @return true if we are a client tool running as root or
			via the condor_super tool, false if not
		*/
	bool useSuperPort();

		/** Code for parsing a locally-written classad, which should
			contain everything about the daemon
			@return true if we found everthing in the ad, false if not
		*/
	bool readLocalClassAd( const char* subsys );

		/** Initialize any daemon values we can from the given classad.
			This function should be called whenever we obtain a daemon ad
			and want to extract information about the daemon from it.
			@param ad The ClassAd you want to extract daemon information
			    from
			@return true if we found everything in the ad, false if not
		 */
	bool getInfoFromAd( const ClassAd* ad );

		/** Initialize one of our values from the given ClassAd* and
			attribute name.  This is shared code when we query the
			collector to locate a daemon and get the address, version
			and platform, so i'm putting it in a seperate method.
			If the attribute wasn't found, it takes care of setting
			our error string, dprintf()ing, and returns false.
			Otherwise, it safely stores the value in the string you
			pass in, which should be one of the data members of the
			object (e.g. "&_platform").
			@param ad The ClassAd you want to look up in
			@param attrname The name of the string attribute in the ad
			@param value_str Pointer to the place to store the result
			@return true on success, false on failure (can't find attr)
		*/
	bool initStringFromAd( const ClassAd* ad, const char* attrname,
						   char** value_str );

		/* 
		   These helpers prevent memory leaks.  Whenever we want to
		   set one of these strings, you just use the helper, which
		   will delete any existing value of the string, and set it to
		   the value you pass in.  Unlike newError(), this DOES NOT
		   make a copy of what you pass (since so many of our util lib
		   functions already allocate a string), so the string you
		   pass in should be a strdup()'ed or equivalent string.  
		   We simply return the value you pass in.
		*/
	char* New_full_hostname( char* );
	char* New_hostname( char* );
	char* New_name( char* );
	char* New_version( char* );
	char* New_platform( char* );
	void New_addr( char* );
	char* New_pool( char* );
	const char* New_alias( char* );

		/**
		   Set a string so we know what command we're inside for use
		   in constructing error messages, and so we know the last
		   command we tried to perform.
		 */
	void setCmdStr( const char* cmd );
	char* _cmd_str;

		/** 
 		   Helper method for the client-side of the ClassAd-only
		   protocol.  This method will try to: locate our daemon,
		   connect(), send the CA_CMD int (or CA_AUTH_CMD is
		   force_auth is true), send a ClassAd and an EOM,  
		   read back a ClassAd and EOM, lookup the ATTR_RESULT in the
		   reply, and if it's FALSE, lookup ATTR_ERROR_STRING.  This
		   deals with everything for you, so all you have to do if you
		   want to use this protocol is define a method that sets up
		   up the right request ad and calls this.
		   @param req Pointer to the request ad (you fill it in)
		   @param reply Pointer to the reply ad (from the server)
		   @param force_auth Should we force authentication for this cmd?
		   @param timeout Network timeout to use (ignored if < 0 )
		   @param sec_session_id Security session to use.
		   @return false if there were any network errors, if
		   ATTR_ERROR_STRING is defined, and/or if ATTR_RESULT is not
		   CA_SUCCESS.  Otherwise, true.   
		*/
	bool sendCACmd( ClassAd* req, ClassAd* reply, bool force_auth,
					int timeout = -1, char const *sec_session_id=NULL );

		/** Same as above, except the socket for the command is passed
			in as an argument.  This way, you can keep the ReliSock
			object around and the connection open once the command is
			over, in case you need that.  Otherwise, you should just
			call the above version.
		*/
	bool sendCACmd( ClassAd* req, ClassAd* reply, ReliSock* sock,
					bool force_auth, int timeout = -1,
					char const *sec_session_id=NULL );

		/** 
		   Helper method for commands to see if we've already got the
		   right address for our daemon.  If not, we try to locate
		   it.  
		   @return true if we've got the address or found it, false 
		   if we failed to locate it.
		*/
	bool checkAddr( void );

		/**
           Helper method for commands to see if we've already
           authenticated this socket, and if not, to try to do so.
		*/
    bool forceAuthentication( ReliSock* rsock, CondorError* errstack );

		/**
		   Internal function used by public versions of startCommand().
		   It may be either blocking or nonblocking, depending on the
		   nonblocking flag.  This version uses an existing socket.

		   This function previously was also named `startCommand`; it
		   was fairly confusing because there is also the non-static
		   `startCommand`; the `_internal` suffix was added to help
		   differentiate between the 6 different variants (besides the
		   13 argument signature!).
		 */
	static StartCommandResult startCommand_internal( const SecMan::StartCommandRequest &req, int timeout, SecMan *sec_man );

		/**
		   Internal function used by public versions of startCommand().
		   It may be either blocking or nonblocking, depending on the
		   nonblocking flag.  This version creates a socket of the
		   specified type and connects it.
		 */
	StartCommandResult startCommand( int cmd, Stream::stream_type st,Sock **sock,int timeout, CondorError *errstack, int subcmd, StartCommandCallbackType *callback_fn, void *misc_data, bool nonblocking, char const *cmd_description=NULL, bool raw_protocol=false, char const *sec_session_id=NULL );

		/**
		   Class used internally to handle non-blocking connects for
		   startCommand().
		*/
	friend struct StartCommandConnectCallback;
	friend class DCMessenger;

	
private:

	// Note: we want to keep the m_daemon_ad_ptr data member private!
	// Not even protected is good enough, because we don't want to expose
	// this data member to child classes like DCSchedd. If someone wants access
	// to the full classad, they must use the friend class DaemonAllowLocateFull.
	// This way we can assume all calls to locate() are LOCATE_FOR_LOOKUP,
	// and folks who try to do something different will get a compile-time error
	// unless they use DaemonAllowLocateFull::locate().

	ClassAd *m_daemon_ad_ptr;

	std::string m_trust_domain;

		// The virtual 'owner' of this collector object
	std::string m_owner;

		// Authentication method overrides
	std::vector<std::string> m_methods;
};

/** This helper class is derived from the Daemon class; it allows
    the caller to invoke method daemonAd() to get the complete ad
	if invoked with locate(Daemon::LOCATE_FULL).
*/
class DaemonAllowLocateFull: public Daemon {
public:

	DaemonAllowLocateFull( daemon_t type, const char* name = NULL,
				const char* pool = NULL );

	DaemonAllowLocateFull( const ClassAd* ad, daemon_t type, const char* pool );


	DaemonAllowLocateFull( const DaemonAllowLocateFull &copy );

	DaemonAllowLocateFull& operator = ( const DaemonAllowLocateFull& );

	bool locate( Daemon::LocateType method  );

		/** Return the classad for this daemon. We may not have the
			ad, if we found the daemon from the config file or the 
			address file, or it doesn't have a classad. So the caller
			should be prepared for this method to return NULL.
			The caller must copy the classad it gets back!
		  */
	ClassAd *daemonAd() { locate(LOCATE_FULL); return m_daemon_ad_ptr; }

};

// Prototype to get sinful string.
char const *global_dc_sinful( void );

/** Helper to get the *_HOST or *_IP_ADDR param for the appropriate
	subsystem.  It just returns whatever param() would.  So, if it's
	NULL, we failed to find anything.  If it's non-NULL, param()
	allocated the space you you have to free() the result.
*/
char* getCmHostFromConfig( const char * subsys );


#endif /* CONDOR_DAEMON_H */
