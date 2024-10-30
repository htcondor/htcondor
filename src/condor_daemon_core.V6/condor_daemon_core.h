/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

// //////////////////////////////////////////////////////////////////////////
//
// This file contains the definition for class DaemonCore. This is the
// central structure for every daemon in condor. The daemon core triggers
// preregistered handlers for corresponding events. class Service is the base
// class of the classes that daemon_core can serve. In order to use a class
// with the DaemonCore, it has to be a derived class of Service.
//
//
// //////////////////////////////////////////////////////////////////////////

#ifndef _CONDOR_DAEMON_CORE_H_
#define _CONDOR_DAEMON_CORE_H_

#include "condor_common.h"
#include "condor_uid.h"
#include "condor_io.h"
#include "dc_service.h"
#include "condor_timer_manager.h"
#include "condor_commands.h"
#include "command_strings.h"
#include "condor_classad.h"
#include "condor_secman.h"
#include "KeyCache.h"
#include "MapFile.h"
#ifdef WIN32
#include "ntsysinfo.WINDOWS.h"
#endif
#include "self_monitor.h"
#include "condor_pidenvid.h"
#include "condor_arglist.h"
#include "env.h"
#include "daemon.h"
#include "daemon_list.h"
#include "limit.h"
#include "ccb_listener.h"
#include "condor_sinful.h"
#include "condor_sockaddr.h"
#include "generic_stats.h"
#include "filesystem_remap.h"
#include "daemon_keep_alive.h"

#include <vector>
#include <memory>
#include <deque>

#include "../condor_procd/proc_family_io.h"
class ProcFamilyInterface;

#if defined(WIN32)
#include "pipe.WINDOWS.h"
#endif

#define DEBUG_SETTABLE_ATTR_LISTS 0

class Probe;

#define USE_MIRON_PROBE_FOR_DC_RUNTIME_STATS

static const int KEEP_STREAM = 100;
static const int CLOSE_STREAM = 101;
static const int PASS_STREAM = 102;
static const int MAX_SOCKS_INHERITED = 4;

/**
   Magic fd to include in the 'std' array argument to Create_Process()
   which means you want a pipe automatically created and handled.
   If you do this to stdin, you get a writeable pipe end (exposed as a FILE*
   Write pipes created like this are stored as string objects, which
   you can access via the Get_Pipe_Data() call.
*/
static const int DC_STD_FD_PIPE = -10;

/**
   Constant used to indicate no pipe (or fd at all) should be remapped.
*/
static const int DC_STD_FD_NOPIPE = -1;


int dc_main( int argc, char **argv );
bool dc_args_is_background(int argc, char** argv); // return true if we should run in background
// set the default for -f / -b flag for this daemon, used by the master to default to backround, all other daemons default to foreground.
bool dc_args_default_to_background(bool background);
// Disable default log setup and ignore -l log directory. Must be called before dc_main()
void DC_Disable_Default_Log();

#ifndef WIN32
// call in the forked child of a HTCondor daemon that is started in backgroun mode when it is ok for the fork parent to exit
bool dc_release_background_parent(int status);
bool dc_set_background_parent_mode(bool is_master = true);
#endif


// External protos
extern void (*dc_main_init)(int argc, char *argv[]);	// old main
extern void (*dc_main_config)();
extern void (*dc_main_shutdown_fast)();
extern void (*dc_main_shutdown_graceful)();
extern void (*dc_main_pre_dc_init)(int argc, char *argv[]);
extern void (*dc_main_pre_command_sock_init)();

/** @name Typedefs for Callback Procedures
 */
//@{
///
typedef int     (*CommandHandler)(int,Stream*);

///
typedef int     (Service::*CommandHandlercpp)(int,Stream*);

///
typedef int     (*SignalHandler)(int);

///
typedef int     (Service::*SignalHandlercpp)(int);

///
typedef int     (*SocketHandler)(Stream*);

///
typedef int     (Service::*SocketHandlercpp)(Stream*);

///
typedef int     (*PipeHandler)(int);

///
typedef int     (Service::*PipeHandlercpp)(int);

///
typedef int     (*ReaperHandler)(int pid,int exit_status);

///
typedef int     (Service::*ReaperHandlercpp)(int pid,int exit_status);

///
typedef int		(*ThreadStartFunc)(void *,Stream*);

///
typedef int     (*PumpWorkCallback)(void* cls, void* data);

/// Register with RegisterTimeSkipCallback. Call when clock skips.  First
//variable is opaque data pointer passed to RegisterTimeSkipCallback.  Second
//variable is the _rough_ unexpected change in time (negative for backwards).
typedef void	(*TimeSkipFunc)(void *,int);

/** Does work in thread.  For Create_Thread_With_Data.
	@see Create_Thread_With_Data
*/

typedef int	(*DataThreadWorkerFunc)(int data_n1, int data_n2, void * data_vp);

/** Reports to parent when thread finishes.  For Create_Thread_With_Data.
	@see Create_Thread_With_Data
*/
typedef int	(*DataThreadReaperFunc)(int data_n1, int data_n2, void * data_vp, int exit_status);
//@}

typedef enum { 
	HANDLE_READ=1,
	HANDLE_WRITE,
	HANDLE_READ_WRITE
} HandlerType;

// some constants for HandleSig().
#define _DC_RAISESIGNAL 1
#define _DC_BLOCKSIGNAL 2
#define _DC_UNBLOCKSIGNAL 3

// constants for the DaemonCore::Create_Process job_opt_mask bitmask

const int DCJOBOPT_SUSPEND_ON_EXEC  = (1<<1);
const int DCJOBOPT_NO_ENV_INHERIT   = (1<<2);        // do not pass our env or CONDOR_INHERIT to the child
const int DCJOBOPT_NEVER_USE_SHARED_PORT   = (1<<3);
const int DCJOBOPT_NO_UDP           = (1<<4);
const int DCJOBOPT_NO_CONDOR_ENV_INHERIT = (1<<5);   // do not pass CONDOR_INHERIT to the child
const int DCJOBOPT_USE_SYSTEMD_INET_SOCKET = (1<<6);	     // Pass the reli sock from systemd as the command socket.
const int DCJOBOPT_INHERIT_FAMILY_SESSION = (1<<7);

const int DC_STATUS_OOM_KILLED = (1 << 24);

#define HAS_DCJOBOPT_SUSPEND_ON_EXEC(mask)  ((mask)&DCJOBOPT_SUSPEND_ON_EXEC)
#define HAS_DCJOBOPT_NO_ENV_INHERIT(mask)  ((mask)&DCJOBOPT_NO_ENV_INHERIT)
#define HAS_DCJOBOPT_ENV_INHERIT(mask)  (!(HAS_DCJOBOPT_NO_ENV_INHERIT(mask)))
#define HAS_DCJOBOPT_NEVER_USE_SHARED_PORT(mask) ((mask)&DCJOBOPT_NEVER_USE_SHARED_PORT)
#define HAS_DCJOBOPT_NO_UDP(mask) ((mask)&DCJOBOPT_NO_UDP)
#define HAS_DCJOBOPT_CONDOR_ENV_INHERIT(mask)  (!((mask)&DCJOBOPT_NO_CONDOR_ENV_INHERIT))
#define HAS_DCJOBOPT_INHERIT_FAMILY_SESSION(mask)  ((mask)&DCJOBOPT_INHERIT_FAMILY_SESSION)
#define HAS_DCJOBOPT_USE_SYSTEMD_INET_SOCKET(mask) ((mask)&DCJOBOPT_USE_SYSTEMD_INET_SOCKET)

// structure to be used as an argument to Create_Process for tracking process
// families
struct FamilyInfo {

	int max_snapshot_interval{-1};
	const char* login{nullptr};
#if defined(LINUX)
	gid_t* group_ptr{nullptr};
#endif
	bool want_pid_namespace{false};
	const char* cgroup{nullptr};
	uint64_t cgroup_memory_limit{0};
	uint64_t cgroup_memory_limit_low{0};      // limit after which kernel aggressively evicts memory
	uint64_t cgroup_memory_and_swap_limit{0}; // limit of swap INclusive of memory. i.e.  
											 // if same as cgroup_memory_limit, then
											 // use memory but no swap
	int cgroup_cpu_shares{0};
#if defined(LINUX)
	std::vector<dev_t> cgroup_hide_devices;
#endif
	bool cgroup_active {false}; // are we actually using a cgroup?

	FamilyInfo() = default;
};

// Create_Process takes a sigset_t* as an argument for the signal mask to be
// applied to the child, which is meaningless on Windows
//
#if defined(WIN32)
typedef void sigset_t;
#endif

/** helper function for finding available port for both 
    TCP and UDP command socket.  Only promised to be meaningful
	for the local host.  No promises are made about what 
	protocol will be used.
	*/
int BindAnyLocalCommandPort(ReliSock *rsock, SafeSock *ssock);
/** helper function for finding available port for both 
    TCP and UDP command socket */
int BindAnyCommandPort(ReliSock *rsock, SafeSock *ssock, condor_protocol proto);

// Multiple senders of DCSignalMsg assume it won't block when resuming a
// session. This is false if the server is expected to send a reply to the
// resumed session and the server is blocked (possibly trying to connect to
// this process). This can result in a timeout-limited deadlock.
// Since these messages are local to the machine and there's no data sent
// after the security communication, disabling the server response to the
// resume session request is acceptable.
class DCSignalMsg: public DCMsg {
 public:
	DCSignalMsg(pid_t pid, int s): DCMsg(DC_RAISESIGNAL)
		{m_pid = pid; m_signal = s; m_messenger_delivery = false; setResumeResponse(false);}

	int theSignal() const {return m_signal;}
	pid_t thePid() const {return m_pid;}

	char const *signalName() const;

	bool codeMsg( DCMessenger *messenger, Sock *sock );

	bool writeMsg( DCMessenger *messenger, Sock *sock )
		{return codeMsg(messenger,sock);}
	bool readMsg( DCMessenger *messenger, Sock *sock )
		{return codeMsg(messenger,sock);}

	virtual void reportFailure( DCMessenger *messenger );
	virtual void reportSuccess( DCMessenger *messenger );

	bool messengerDelivery() const {return m_messenger_delivery;}
	void messengerDelivery(bool flag) {m_messenger_delivery = flag;}

 private:
	pid_t m_pid;
	int m_signal;
		// true if DaemonCore sends the signal through some means
		// other than delivery of this network message through DCMessenger
	bool m_messenger_delivery;
};


/**
 * This is the class that internally manages the token request process;
 * the actual logic implementation is in a separate class in daemon_core_main;
 * this provides a clean, separate interface to that class.
 */

// Typedef of function we will call when a token request finished.
typedef void RequestCallbackFn(bool success, void *miscdata);

class DCTokenRequester {
public:
	DCTokenRequester(RequestCallbackFn callback, void *miscdata)
	  : m_callback(callback),
	    m_callback_data(miscdata)
	{}

	static const std::string default_identity;

		// Remove/stop any pending token requests.
	void clearRequests();

	void *createCallbackData(const std::string &daemon_addr, const std::string &identity,
		const std::string &authz_name);

	static void
	daemonUpdateCallback(bool success, Sock *sock, CondorError *, const std::string &trust_domain,
		bool should_try_token_request, void *miscdata);

	static void
	tokenRequestCallback(bool success, void *miscdata);

	struct DCTokenRequesterData {
		std::string m_addr;
		std::string m_identity;
		std::string m_authz_name;
		RequestCallbackFn *m_callback_fn;
		void *m_callback_data;
	};

private:
	RequestCallbackFn *m_callback{nullptr};
	void *m_callback_data{nullptr};
};


class OptionalCreateProcessArgs;
class OptionalCreateProcessArgs {
  friend class DaemonCore;

  public:
    OptionalCreateProcessArgs(/*very not-const*/ std::string &_err_return_msg) :
        _priv(PRIV_UNKNOWN), reaper_id(1), want_command_port(TRUE),
        want_udp_command_port(TRUE), _env(NULL), _cwd(NULL), family_info(NULL),
        socket_inherit_list(NULL), _std(NULL), fd_inherit_list(NULL),
        nice_inc(0), sig_mask(NULL), job_opt_mask(0), core_hard_limit(NULL),
        affinity_mask(NULL), daemon_sock(NULL),
        err_return_msg(_err_return_msg), _remap(NULL), as_hard_limit(0l)
	{}
    OptionalCreateProcessArgs() :
        _priv(PRIV_UNKNOWN), reaper_id(1), want_command_port(TRUE),
        want_udp_command_port(TRUE), _env(NULL), _cwd(NULL), family_info(NULL),
        socket_inherit_list(NULL), _std(NULL), fd_inherit_list(NULL),
        nice_inc(0), sig_mask(NULL), job_opt_mask(0), core_hard_limit(NULL),
        affinity_mask(NULL), daemon_sock(NULL),
        err_return_msg(default_return_msg), _remap(NULL), as_hard_limit(0l)
    {}

    OptionalCreateProcessArgs(
        priv_state        priv,
        int               reaper_id,
        int               want_command_port,
        int               want_udp_command_port,
        const Env *       env,
        const char *      cwd,
        FamilyInfo *      family_info,
        Stream **         socket_inherit_list,
        int *             std,
        int *             fd_inherit_list,
        int               nice_inc,
        sigset_t *        sig_mask,
        int               job_opt_mask,
        size_t *          core_hard_limit,
        int *             affinity_mask,
        char const *      daemon_sock,
        std::string &     err_return_msg,
        FilesystemRemap * remap,
        long              as_hard_limit
    ) :
        _priv(priv), reaper_id(reaper_id), want_command_port(want_command_port),
        want_udp_command_port(want_udp_command_port), _env(env), _cwd(cwd),
        family_info(family_info), socket_inherit_list(socket_inherit_list),
        _std(std), fd_inherit_list(fd_inherit_list), nice_inc(nice_inc),
        sig_mask(sig_mask), job_opt_mask(job_opt_mask),
        core_hard_limit(core_hard_limit), affinity_mask(affinity_mask),
        daemon_sock(daemon_sock), err_return_msg(err_return_msg),
        _remap(remap), as_hard_limit(as_hard_limit)
    {}

    OptionalCreateProcessArgs & priv(priv_state p) { this->_priv = p; return *this; }
    OptionalCreateProcessArgs & wantCommandPort(int wcp) { this->want_command_port = wcp; return *this; }
    OptionalCreateProcessArgs & wantUDPCommandPort(int wucp) { this->want_udp_command_port = wucp; return *this; }
    OptionalCreateProcessArgs & env(const Env * env) { this->_env = env; return *this; }
    OptionalCreateProcessArgs & cwd(const char * cwd) { this->_cwd = cwd; return *this; }
    OptionalCreateProcessArgs & familyInfo(FamilyInfo * family_info) { this->family_info = family_info; return *this; }
    OptionalCreateProcessArgs & socketInheritList(Stream ** socket_inherit_list) { this->socket_inherit_list = socket_inherit_list; return *this; }
    OptionalCreateProcessArgs & std(int * std) { this->_std = std; return *this; }
    OptionalCreateProcessArgs & fdInheritList(int * fd_inherit_list) { this->fd_inherit_list = fd_inherit_list; return *this; }
    OptionalCreateProcessArgs & niceInc(int nice_inc) { this->nice_inc = nice_inc; return *this; }
    OptionalCreateProcessArgs & sigMask(sigset_t * sig_mask) { this->sig_mask = sig_mask; return *this; }
    OptionalCreateProcessArgs & jobOptMask(int job_opt_mask) { this->job_opt_mask = job_opt_mask; return *this; }
    OptionalCreateProcessArgs & coreHardLimit(size_t * core_hard_limit ) { this->core_hard_limit = core_hard_limit; return *this; }
    OptionalCreateProcessArgs & affinityMask(int * affinity_mask) { this->affinity_mask = affinity_mask; return *this; }
    OptionalCreateProcessArgs & daemonSock(const char * daemon_sock) { this->daemon_sock = daemon_sock; return *this; }
    OptionalCreateProcessArgs & remap(FilesystemRemap * fsr) { this->_remap = fsr; return *this; }
    OptionalCreateProcessArgs & asHardLimit(long ahl) { this->as_hard_limit = ahl; return *this; }
    OptionalCreateProcessArgs & reaperID(int ri) { this->reaper_id = ri; return *this; }


    // Special case for usability; may be a bad idea, but allows you to
    // pass the default OptionalCreateProcessArgs and still get the error
    // message back.
    std::string & getErrorReturnMsg() const { return err_return_msg; }

  private:

    priv_state        _priv;
    int               reaper_id;
    int               want_command_port;
    int               want_udp_command_port;
    const Env *       _env;
    const char *      _cwd;
    FamilyInfo *      family_info;
    Stream **         socket_inherit_list;
    int *             _std;
    int *             fd_inherit_list;
    int               nice_inc;
    sigset_t *        sig_mask;
    int               job_opt_mask;
    size_t *          core_hard_limit;
    int *             affinity_mask;
    char const *      daemon_sock;
    std::string &     err_return_msg;
    FilesystemRemap * _remap;
    long              as_hard_limit;

    // We'd like to use std::optional for the std::string reference, but
    // we can't do that until C++17 is a thing for us.  Instead, initialize
    // the default reference to this member variable.
    std::string       default_return_msg;
};


//-----------------------------------------------------------------------------
/** This class badly needs documentation, doesn't it? 
    This file contains the definition for class DaemonCore. This is the
    central structure for every daemon in Condor. The daemon core triggers
    preregistered handlers for corresponding events. class Service is the base
    class of the classes that daemon_core can serve. In order to use a class
    with the DaemonCore, it has to be a derived class of Service.
*/
class DaemonCore : public Service
{
  friend class TimerManager; 
  friend class CreateProcessForkit;
#ifdef WIN32
  friend unsigned pidWatcherThread(void*);
#endif
  friend int dc_main(int, char**);
  friend class DaemonCommandProtocol;
  friend class DaemonKeepAlive;
    
  public:
    
  	/* These are all declarations for methods which never need to be
	 * called by the end user.  Thus they are not docified.
	 * Typically these methods are invoked from functions inside 
	 * of daemon_core_main.C.
	 */
    DaemonCore (int ComSize = 0, int SigSize = 0,
                int SocSize = 0, int ReapSize = 0);
    ~DaemonCore();
    void Driver();

		/**
		   Re-read anything that the daemonCore object got from the
		   config file that might have changed on a reconfig.
		*/
    void reconfig();

	void refreshDNS( int timerID = -1 );

    /** Not_Yet_Documented
        @param perm Not_Yet_Documented
        @param sin  Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Verify (char const *command_descrip, DCpermission perm, const condor_sockaddr& addr, const char * fqu, int log_level=D_ALWAYS);

	/** Given a socket, check to see whether it has a given permission.
	    This checks:
	    - the authentication was done with an appropriate settings and methods.
	    - any permission limits on the authentication (such as used with fine-grained tokens)
	      are applied correctly.
	    - the user / IP address has authorization (e.g., applies ALLOW_* / DENY_* from the IpVerify
	      class).

	    The `command_descrip` is used to generate a useful logging message on an authorization allowed
	    or denied; log_level controls how "loud" the log message is.
	 */
    int Verify (char const *command_descrip, DCpermission perm, const Sock &sock, int log_level=D_ALWAYS);

    int AddAllowHost( const char* host, DCpermission perm );

    /** clear all sessions associated with the child 
     */
    void clearSession(pid_t pid);

	/** @name Command Events
	 */
	//@{

    /** Not_Yet_Documented
        @param command         Not_Yet_Documented
        @param com_descrip     Not_Yet_Documented
        @param handler         Not_Yet_Documented
        @param handler_descrip Not_Yet_Documented
        @param s               Not_Yet_Documented
        @param perm            Not_Yet_Documented
		@param force_authentication This command _must_ be authenticated.
        @param wait_for_payload Do a non-blocking select for read for this
                                many seconds (0 means none) before calling
                                command handler.
        @return Not_Yet_Documented
    */
    int Register_Command (int             command,
                          const char *    com_descrip,
                          CommandHandler  handler, 
                          const char *    handler_descrip,
                          DCpermission    perm,
                          bool            force_authentication = false,
                          int             wait_for_payload = 0,
                          std::vector<DCpermission> *alternate_perms = nullptr);
    
    /** Not_Yet_Documented
        @param command         Not_Yet_Documented
        @param com_descrip     Not_Yet_Documented
        @param handlercpp      Not_Yet_Documented
        @param handler_descrip Not_Yet_Documented
        @param s               Not_Yet_Documented
        @param perm            Not_Yet_Documented
		@param force_authentication This command _must_ be authenticated.
        @param wait_for_payload Do a non-blocking select for read for this
                                many seconds (0 means none) before calling
                                command handler.
        @return Not_Yet_Documented
    */
    int Register_Command (int                command,
                          const char *       com_descript,
                          CommandHandlercpp  handlercpp, 
                          const char *       handler_descrip,
                          Service *          s,
                          DCpermission       perm,
                          bool               force_authentication = false,
                          int                wait_for_payload = 0,
                          std::vector<DCpermission> * alternate_perms = nullptr);

    int Register_UnregisteredCommandHandler (
		CommandHandlercpp handlercpp,
		const char       *handler_descrip,
		Service          *s,
		bool              include_auth);
    bool HandleUnregistered() const {return m_unregisteredCommand.num;}
    bool HandleUnregisteredDCAuth() const {return HandleUnregistered() && m_unregisteredCommand.is_cpp;}

	/** Register_CommandWithPayload is the same as Register_Command
		but with a different default for wait_for_payload.  By
		default, a non-blocking read will be performed before calling
		the command-handler.  This reduces the threat of having the
		command handler block while waiting for the client to send the
		rest of the command input.  The command handler can therefore
		set a small timeout when reading.
	*/
    int Register_CommandWithPayload (
                          int             command,
                          const char *    com_descrip,
                          CommandHandler  handler, 
                          const char *    handler_descrip,
                          DCpermission    perm,
                          bool            force_authentication = false,
                          int             wait_for_payload = STANDARD_COMMAND_PAYLOAD_TIMEOUT,
                          std::vector<DCpermission> *alternate_perms = nullptr);

    int Register_CommandWithPayload (
                          int                command,
                          const char *       com_descript,
                          CommandHandlercpp  handlercpp, 
                          const char *       handler_descrip,
                          Service *          s,
                          DCpermission       perm,
                          bool               force_authentication = false,
                          int                wait_for_payload = STANDARD_COMMAND_PAYLOAD_TIMEOUT,
                          std::vector<DCpermission> * alternate_perms = nullptr);


    /** Not_Yet_Documented
        @param command Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Cancel_Command (int command);

    /** Gives the port of the DaemonCore
		command socket of this process.
        @return The port number, or -1 on error */
    int InfoCommandPort();

    /** Returns the Sinful String <host:port> of the DaemonCore
		command socket of this process
        as a default.  If given a pid, it returns the sinful
        string of that pid.
        @param pid The pid to ask about.  -1 (Default) means
                   the calling process
        @return A pointer into a <b>static buffer</b>, or NULL on error */
    char const* InfoCommandSinfulString (int pid = -1);

	/**
	 * Return a vector of the sinful strings of all known command sockets.
	 *
	 * ONLY returns the public IP address.
	 *
	 * This function was 'born deprecated'; use case is when rewriting all
	 * addresses in a collector (in the future, we hope to have a more
	 * expressive sinful string variant).
	 */
	const std::vector<Sinful> &InfoCommandSinfulStringsMyself(); 

	void daemonContactInfoChanged();

    /** Returns the Sinful String <host:port> of the DaemonCore
		command socket of this process
		@param usePrivateAddress
			- If false, return our address
			- If true, return our local address, which
			  may not be valid outside of this machine
		@return A pointer into a <b>static buffer</b>, or NULL on error */
	char const * InfoCommandSinfulStringMyself(bool usePrivateAddress);

		/**
		   @return Pointer to a static buffer containing the daemon's
		   public IP address and port.  Same as InfoCommandSinfulString()
		   but with a more obvious name which matches the underlying
		   ClassAd attribute where it is advertised.
		*/
	const char* publicNetworkIpAddr(void);

		/**
		   @return Pointer to a static buffer containing the daemon's
		   true, local IP address and port if CCB is involved,
		   otherwise NULL.  Nearly the same as
		   InfoCommandSinfulStringMyself(true) but with a more obvious
		   name which matches the underlying ClassAd attribute where
		   it is advertised.
		*/
	const char* privateNetworkIpAddr(void);

		/**
		   @return Pointer to a static buffer containing the daemon's
		   superuser local IP address and port (aka sinful string),
		   otherwise NULL.
		*/
	const char* superUserNetworkIpAddr(void);

		/** Determine if a Stream passed to a command handler
			originated via the condor_sos command (i.e. via super user socket)
			@return true if super user command, else false
		*/
	bool Is_Command_From_SuperUser( Stream *s ) const;

		/**
		   @return The daemon's private network name, or NULL if there
		   is none (i.e., it's on the public internet).
		*/
	const char* privateNetworkName(void);

	void SetInheritParentSinful( const char *sinful ) {
		m_inherit_parent_sinful = sinful ? sinful : "";
	}

	/** Returns a pointer to the penvid passed in if successful
		in determining the environment id for the pid, or NULL if unable
		to determine.
        @param pid The pid to ask about.  -1 (Default) means
                   the calling process
		@param penvid Address of a structure to be filled in with the 
			environment id of the pid. Left in undefined state after function
			call.
	*/
	PidEnvID* InfoEnvironmentID(PidEnvID *penvid, int pid = -1);


    /** Not_Yet_Documented
        @return Not_Yet_Documented
		*/
    int ServiceCommandSocket();

    /** Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int InServiceCommandSocket() const {
        return inServiceCommandSocket_flag;
    }
    
    void SetDelayReconfig(bool value) {
        m_delay_reconfig = value;
    }

    bool GetDelayReconfig() const {
        return m_delay_reconfig;
    }

    void SetNeedReconfig(bool value) {
        m_need_reconfig = value;
    }

    bool GetNeedReconfig() const {
        return m_need_reconfig;
    }   
	//@}
    


	/** @name Signal events
	 */
	//@{

    /** Not_Yet_Documented
        @param sig              Not_Yet_Documented
        @param sig_descrip      Not_Yet_Documented
        @param handler          Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Signal (int             sig,
                         const char *    sig_descrip,
                         SignalHandler   handler, 
                         const char *    handler_descrip);
    
    /** Not_Yet_Documented
        @param sig              Not_Yet_Documented
        @param sig_descrip      Not_Yet_Documented
        @param handlercpp       Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Signal (int                 sig,
                         const char *        sig_descript,
                         SignalHandlercpp    handlercpp, 
                         const char *        handler_descrip,
                         Service*            s);
    
    /** Not_Yet_Documented
        @param sig Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Cancel_Signal (int sig);


    /** Not_Yet_Documented
        @param sig Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Block_Signal (int sig) {
        return(HandleSig(_DC_BLOCKSIGNAL,sig));
    }

    /** Not_Yet_Documented
        @param sig Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Unblock_Signal (int sig) {
        return(HandleSig(_DC_UNBLOCKSIGNAL,sig));
    }

    /** Send a signal to daemonCore processes or non-DC process
        @param pid The receiving process ID
        @param sig The signal to send
        @return Not_Yet_Documented
    */
    bool Send_Signal (pid_t pid, int sig);
    bool Signal_Myself(int sig);

	/**  Send a signal to a process, as a nonblocking operation.
		 If the caller cares about the success/failure status, this
		 can be obtained by subclassing DCSignalMsg and overriding
		 the message delivery hooks.
		 @param msg The pid and signal to send as a DCSignalMsg
	 */
	void Send_Signal_nonblocking(classy_counted_ptr<DCSignalMsg> msg);
	//@}


	/** @name Reaper events.
	 */
	//@{


    /** This method selects the reaper to be called when a process exits
        and no reaper is registered.  This can be used, for example,
        to catch the exit of processes that were created by other facilities
        than DaemonCore.

        @param reaper_id The already-registered reaper number to use.
     */

    void Set_Default_Reaper( int reaper_id );

    /** Not_Yet_Documented
        @param reap_descrip     Not_Yet_Documented
        @param handler          Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Reaper (const char *    reap_descrip,
                         ReaperHandler   handler,
                         const char *    handler_descrip);
    
    /** Not_Yet_Documented
        @param reap_descrip     Not_Yet_Documented
        @param handlercpp       Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return Not_Yet_Documented
    */
     int Register_Reaper (const char *      reap_descript,
                          ReaperHandlercpp  handlercpp, 
                          const char *      handler_descrip,
                          Service*          s);

    /** Not_Yet_Documented
        @param rid The Reaper ID
        @param reap_descrip     Not_Yet_Documented
        @param handler          Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Reset_Reaper (int              rid,
                      const char *     reap_descrip,
                      ReaperHandler    handler, 
                      const char *     handler_descrip);
    
    /** Not_Yet_Documented
        @param rid The Reaper ID
        @param reap_descrip     Not_Yet_Documented
        @param handlercpp       Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Reset_Reaper (int                    rid,
                      const char *           reap_descript,
                      ReaperHandlercpp       handlercpp, 
                      const char *           handler_descrip,
                      Service*               s);

    /** Not_Yet_Documented
        @param rid The Reaper ID
        @return Not_Yet_Documented
    */
    int Cancel_Reaper (int rid);


    int numRegisteredReapers();
    int countTimersByDescription( const char * description ) { return t.countTimersByDescription(description); }


    /** Not_Yet_Documented
        @param signal signal
        @return Not_Yet_Documented
    */
	static const char *GetExceptionString(int signal);

    /** Not_Yet_Documented
        @param pid pid
        @return Not_Yet_Documented
    */
    int Was_Not_Responding(pid_t pid);
    int Got_Alive_Messages(pid_t pid, bool & not_responding); // returns number of DC_CHILDALIVE messages received

	/* What signal should be sent to the given child pid when the current
	 * daemon exits? Default is SIGKILL. 0 means no signal is sent.
	 */
	void Set_Cleanup_Signal(pid_t pid, int signum);

	//@}

	/** @name Socket events.
	 */
	//@{
    /** Not_Yet_Documented
        @param iosock           Not_Yet_Documented
        @param iosock_descrip   Not_Yet_Documented
        @param handler          Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return -1 if iosock is NULL, -2 is reregister, 0 or above on success
    */
    int Register_Socket (Stream*           iosock,
                         const char *      iosock_descrip,
                         SocketHandler     handler,
                         const char *      handler_descrip,
                         HandlerType       handler_type = HANDLE_READ,
                         void **           prev_entry = NULL);

    /** Not_Yet_Documented
        @param iosock           Not_Yet_Documented
        @param iosock_descrip   Not_Yet_Documented
        @param handlercpp       Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return -1 if iosock is NULL, -2 is reregister, 0 or above on success
    */
    int Register_Socket (Stream*              iosock,
                         const char *         iosock_descrip,
                         SocketHandlercpp     handlercpp,
                         const char *         handler_descrip,
                         Service*             s,
                         HandlerType          handler_type = HANDLE_READ,
                         void **              prev_entry = NULL);

    /** Not_Yet_Documented
        @param iosock           Not_Yet_Documented
        @param descrip          Not_Yet_Documented
        @return -1 if iosock is NULL, -2 is reregister, 0 or above on success
    */
    int Register_Command_Socket (Stream*      iosock,
                                 const char * descrip = NULL ) {
	m_dirty_command_sock_sinfuls = true;
        return Register_Socket (iosock,
                                descrip,
                                (SocketHandler)NULL,
                                (SocketHandlercpp)NULL,
                                "DC Command Handler",
                                NULL,
				HANDLE_READ,
                                0); 
    }

    /** Not_Yet_Documented
        @param insock           Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Cancel_Socket ( Stream * insock, void *prev_entry = NULL );

		// Returns true if the given socket is already registered.
	bool SocketIsRegistered( Stream *sock );

		// Call the registered socket handler for this socket
		// sock - previously registered socket
		// default_to_HandleCommand - true if HandleCommand() should be called
		//                          if there is no other callback function
	void CallSocketHandler( Stream *sock, bool default_to_HandleCommand=false );

		// Call the registered command handler.
		// returns the return code of the handler
		// if delete_stream is true and the command handler does not return
		// KEEP_STREAM, the stream is deleted
	int CallCommandHandler(int req,Stream *stream,bool delete_stream=true,bool check_payload=true,float time_spent_on_sec=0,float time_spent_waiting_for_payload=0);
	int CallUnregisteredCommandHandler(int req, Stream *stream);


		// This function is called in order to have
		// TooManyRegisteredSockets() take into account an extra socket
		// that is waiting for a timer or other callback to complete.
	void incrementPendingSockets() { nPendingSockets++; }

		// This function must be called after incrementPendingSockets()
		// when the socket is being (or about to be) destroyed.
	void decrementPendingSockets() { nPendingSockets--; }

	/**
	   @return Number of currently registered sockets.
	 */
	int RegisteredSocketCount() const;

	/** This function is specifically tailored for testing if it is
		ok to register a new socket (e.g. for non-blocking connect).
	   @param fd Recently opened file descriptor, helpful, but
	             not required for determining if we are in danger.
	   @param msg Optional string into which this function writes
	              a human readable description of why it was decided
	              that there are too many open sockets.
	   @param num_fds Number of file descriptors that the caller plans
	                  to register. This will usually be 1, but may be
					  2 for UDP (UDP socket plus TCP socket for
					  establishing the security session).
	   @return true of in danger of running out of file descriptors
	 */
	bool TooManyRegisteredSockets(int fd=-1,std::string *msg=NULL,int num_fds=1);

	/**
	   @return Maximum number of persistent file descriptors that
	           we should ever attempt having open at the same time.
	 */
	int FileDescriptorSafetyLimit();

    /** Not_Yet_Documented
        @param insock           Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Lookup_Socket ( Stream * iosock );
	//@}

	/** @name Pipe events.
	 */
	//@{
    /** Not_Yet_Documented
        @param pipe_end            Not_Yet_Documented
        @param pipe_descrip     Not_Yet_Documented
        @param handler          Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Pipe (int		           pipe_end,
                         const char *      pipe_descrip,
                         PipeHandler       handler,
                         const char *      handler_descrip,
                         HandlerType       handler_type     = HANDLE_READ);

    /** Not_Yet_Documented
        @param pipe_end         Not_Yet_Documented
        @param pipe_descrip     Not_Yet_Documented
        @param handlercpp       Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Pipe (int					  pipe_end,
                         const char *         pipe_descrip,
                         PipeHandlercpp       handlercpp,
                         const char *         handler_descrip,
                         Service*             s,
                         HandlerType          handler_type = HANDLE_READ);

    /** Not_Yet_Documented
        @param pipe_end           Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Cancel_Pipe ( int pipe_end );

	/// Cancel and close all pipes.
	int Cancel_And_Close_All_Pipes(void);

	/** Create an anonymous pipe.
	*/
	int Create_Pipe( int *pipe_ends,
			 bool can_register_read = false, bool can_register_write = false,
			 bool nonblocking_read = false, bool nonblocking_write = false,
			 unsigned int psize = 4096);

	/** Create a named pipe
	*/
	int Create_Named_Pipe( int *pipe_ends,
			 bool can_register_read = false, bool can_register_write = false,
			 bool nonblocking_read = false, bool nonblocking_write = false,
			 unsigned int psize = 4096, const char* pipe_name = NULL);
	/** Make DaemonCore aware of an inherited pipe.
	*/
	int Inherit_Pipe( int p, bool write, bool can_register, bool nonblocking, int psize = 4096);

	int Read_Pipe(int pipe_end, void* buffer, int len);

	int Write_Pipe(int pipe_end, const void* buffer, int len);
					 
	/** Close an anonymous pipe.  This function will also call 
	 * Cancel_Pipe() on behalf of the caller if the pipe_end had
	 * been registed via Register_Pipe().
	*/
	int Close_Pipe(int pipe_end);

	int Get_Max_Pipe_Buffer() const { return maxPipeBuffer; };

#if !defined(WIN32)
	/** Get the FD underlying the given pipe end. Returns FALSE
	 *  if not given a valid pipe end.
	*/
	int Get_Pipe_FD(int pipe_end, int* fd);
#endif

	/** Close an anonymous pipe or file depending on its value
		relative to PIPE_INDEX_OFFSET.  If the fd's value is 
		>= PIPE_INDEX_OFFSET then it is a pipe; otherwise, it
		is an file.
		*/
	int Close_FD(int fd);

	/**
	   Gain access to data written to a given DC process's std(out|err) pipe.

	   @param pid
	     DC process id of the process to read the data for.
	   @param std_fd
	     The fd to identify the pipe to read: 1 for stdout, 2 for stderr.
	   @return
	     Pointer to a string object containing all the data written so far.
	*/
	std::string* Read_Std_Pipe(int pid, int std_fd);

	/**
	   Write data to the given DC process's stdin pipe.
	   @see pipeFullWrite()
	*/
	int Write_Stdin_Pipe(int pid, const void* buffer, int len);

	/**
	   Close a given DC process's stdin pipe.

	   @return true if the given pid was found and had a DC-managed
	     stdin pipe, false if not. 

	   @see Close_Pipe()
	*/
	bool Close_Stdin_Pipe(int pid);

	//@}

	/** @name Timer events.
	 */
	//@{
    /** Not_Yet_Documented
        @param deltawhen       Not_Yet_Documented
        @param event           Not_Yet_Documented
        @param event_descrip   Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Timer (time_t     deltawhen,
                        TimerHandler handler,
                        const char * event_descrip);

	/** Not_Yet_Documented
        @param deltawhen       Not_Yet_Documented
        @param event           Not_Yet_Documented
        @param event_descrip   Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Timer (time_t     deltawhen,
                        TimerHandler handler,
						Release      release,
                        const char * event_descrip);
    
    /** Not_Yet_Documented
        @param deltawhen       Not_Yet_Documented
        @param period          Not_Yet_Documented
        @param event           Not_Yet_Documented
        @param event_descrip   Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Timer (time_t     deltawhen,
                        time_t       period,
                        TimerHandler handler,
                        const char * event_descrip);

    /** Not_Yet_Documented
        @param deltawhen       Not_Yet_Documented
        @param event           Not_Yet_Documented
        @param event_descrip   Not_Yet_Documented
        @param s               Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Timer (time_t     deltawhen,
                        TimerHandlercpp handler,
                        const char * event_descrip,
                        Service*     s);

    // register a callback from any thread that will be called on the main thread
    // as soon as we are back in the daemon core pump (just before timers are handled).
    // In effect, this is a thread safe register of a zero length one-shot timer
    int Register_PumpWork_TS (
        PumpWorkCallback handler,
        void* cls, // intended to be the 'this' pointer when registering a static method in a class
        void* data); // intended for use as the data pointer

    /** Not_Yet_Documented
        @param deltawhen       Not_Yet_Documented
        @param period          Not_Yet_Documented
        @param event           Not_Yet_Documented
        @param event_descrip   Not_Yet_Documented
        @param s               Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Timer (time_t     deltawhen,
                        time_t       period,
                        TimerHandlercpp handler,
                        const char * event_descrip,
                        Service *    s);

    /** 
        @param timeslice       Timeslice object specifying interval parameters
        @param event           Function to call when timer fires.
        @param event_descrip   String describing the function.
        @return                Timer id or -1 on error
    */
    int Register_Timer (const Timeslice &timeslice,
                        TimerHandler handler,
                        const char * event_descrip);

	/** 
        @param timeslice       Timeslice object specifying interval parameters
        @param event           Function to call when timer fires.
        @param event_descrip   String describing the function.
        @param s               Service object of which function is a member.
        @return                Timer id or -1 on error
    */
    int Register_Timer (const Timeslice &timeslice,
                        TimerHandlercpp handler,
                        const char * event_descrip,
                        Service*     s);

    /** Not_Yet_Documented
        @param id The timer's ID
        @return Not_Yet_Documented
    */
    int Cancel_Timer ( int id );

    /** Not_Yet_Documented
        @param id The timer's ID
        @param deltawhen   Not_Yet_Documented
        @param period Not_Yet_Documented
        @return 0 if successful, -1 on failure (timer not found)
    */
    int Reset_Timer ( int id, time_t deltawhen, time_t  period = 0 );

    /** Change a timer's period.  Recompute time to fire next based on this
		new period and how long this timer has been waiting.
        @param id The timer's ID
        @param period New period for this timer.
        @return 0 on success
    */
    int Reset_Timer_Period ( int id, time_t period );

    /** Change a timer's timeslice settings.
        @param id The timer's ID
        @param new_timeslice New timeslice settings to use.
        @return 0 on success
    */
    int ResetTimerTimeslice ( int id, Timeslice const &new_timeslice );

    /** Get a timer's timeslice settings.
        @param id The timer's ID
        @param timeslice Object to receive a copy of the timeslice settings.
        @return false if no timeslice associated with this timer
    */
    bool GetTimerTimeslice ( int id, Timeslice &timeslice );

    /** Get the timestamp of when the timer is expected to fire
     *  @param id The timer's ID
     *  @return timestamp; 0 if the timer does not exist.
     */
    time_t GetNextRuntime(int id) {return t.GetNextRuntime(id);}
	//@}

    /** Not_Yet_Documented
        @param flag   Not_Yet_Documented
        @param indent Not_Yet_Documented
    */
    void Dump (int flag, const char* indent = NULL );


    /** @name Process management.
        These work on any process, not just daemon core processes.
    */
    //@{

    /** Not_Yet_Documented
        @return Not_Yet_Documented
    */
    inline pid_t getpid() const { return this->mypid; };

    /** Not_Yet_Documented
        @return Not_Yet_Documented
    */
    inline pid_t getppid() const { return ppid; };

    /** Not_Yet_Documented
        @param pid Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Is_Pid_Alive (pid_t pid);

		// returns true if we have called waitpid but
		// have still queued the call to the reaper
	bool ProcessExitedButNotReaped(pid_t pid);

    /** Not_Yet_Documented
        @param pid Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Shutdown_Fast(pid_t pid, bool want_core = false );

    /** Not_Yet_Documented
        @param pid Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Shutdown_Graceful(pid_t pid);

    /** Not_Yet_Documented
        @param pid Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Suspend_Process(pid_t pid) const;

    /** Not_Yet_Documented
        @param pid Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Continue_Process(pid_t pid);

    bool setChildSharedPortID( pid_t pid, const char * sock );

    /** Create a process.  Works for NT and Unix.  On Unix, a
        fork and exec are done.  Read the source for ugly 
        details - it takes care of most everything.

        @param name The full path name of the executable.  If this 
               is a relative path name AND cwd is specified, then we
               prepend the result of getcwd() to 'name' and pass 
               that to exec().
        @param args The list of args as an ArgList object.  The 
               first arg is argv[0], the name you want to appear in 
               'ps'.  If you don't specify args, then 'name' is 
               used as argv[0].
        @param priv The priv state to change into right before
               the exec.  Default = no action.
        @param reaper_id The reaper number to use.  Default = 1. If a
			   reaper_id of 0 is used, no reaper callback will be invoked.
        @param want_command_port Well, do you?  Default = TRUE.  If
			   want_command_port it TRUE, the child process will be
			   born with a daemonCore command socket on a dynamic port.
			   If it is FALSE, the child process will not have a command
			   socket.  If it is any integer > 1, then it will have a
			   command socket on a port well-known port 
			   specified by want_command_port.
        @param env Environment values to place in job environment.
        @param cwd Current Working Directory
        @param new_process_group Do you want to make one? Default = FALSE.
               This will be forced to FALSE on unix if USE_PROCESS_GROUPS
               is set to false in the config file.
        @param sock_inherit_list A list of socks to inherit.
        @param std An array of three file descriptors to map
               to stdin, stdout, stderr respectively.  If this array
               is NULL, don't perform remapping.  If any one of these
			   is negative, it is ignored and *not* mapped.
			   There's a special case if you use DC_STD_FD_PIPE as the
               value for any of these fds -- DaemonCore will create a
               pipe and register everything for you automatically. If
               you use this for stdin, you can use Write_Std_Pipe() to
               write to the stdin of the child. If you use this for
               std(out|err) then you can get a pointer to a string
               with all the data written by the child using Read_Std_Pipe().
        @param nice_inc The value to be passed to nice() in the
               child.  0 < nice < 20, and greater numbers mean
               less priority.  This is an addition to the current
               nice value.
        @param job_opt_mask A bitmask which defines other optional
               behavior we might for this process.  The bits are
               defined above, and always begin with "DCJOBOPT".  In
               addition, each bit should also define a macro to test a
               mask if a given bit is set ("HAS_DCJOBOPT_...")
		@param fd_inherit_list An array of fds which you want the child to
		       inherit. The last element must be 0.
        ...
        @param err_return_msg Returned with error message pertaining to
               failure inside the method.  Ignored if NULL (default).
        @param remap Performs bind mounts for the child process.
               Ignored if NULL (default).  Ignored on non-Linux.
        @return On success, returns the child pid.  On failure, returns FALSE.
    */
    int Create_Process (
        const char      *name,
        ArgList const   &arglist,
        priv_state      priv                 = PRIV_UNKNOWN,
        int             reaper_id            = 1,
        int             want_commanand_port  = TRUE,
        int             want_udp_comm_port   = TRUE,
        Env const       *env                 = NULL,
        const char      *cwd                 = NULL,
        FamilyInfo      *family_info         = NULL,
        Stream          *sock_inherit_list[] = NULL,
        int             std[]                = NULL,
        int             fd_inherit_list[]    = NULL,
        int             nice_inc             = 0,
        sigset_t        *sigmask             = NULL,
        int             job_opt_mask         = 0,
        size_t          *core_hard_limit     = NULL,
        int             *affinity_mask       = NULL,
        char const      *daemon_sock         = NULL,
        std::string     *err_return_msg      = NULL,
        FilesystemRemap *remap               = NULL,
        long            as_hard_limit        = 0l
        );

    int CreateProcessNew(
        const std::string & name,
        const ArgList & argsList,
        const OptionalCreateProcessArgs & ocpa = {} );

    int CreateProcessNew(
        const std::string & name,
        const std::vector< std::string > args,
        const OptionalCreateProcessArgs & ocpa = {} );

    //@}

	/** "Special" errno values that may be set when Create_Process fails
	*/
	static const int ERRNO_EXEC_AS_ROOT;
	static const int ERRNO_PID_COLLISION;
	static const int ERRNO_REGISTRATION_FAILED;
	static const int ERRNO_EXIT;

	const static char* DEFAULT_INDENT;

    /** Methods for operating on a process family
    */
    int Get_Family_Usage(pid_t, ProcFamilyUsage&, bool full = false);
    int Suspend_Family(pid_t);
    int Continue_Family(pid_t);
    int Kill_Family(pid_t);
    int Extend_Family_Lifetime(pid_t);
    int Signal_Process(pid_t,int);
    
	// This method should go away in the long term.
	// Daemon Core should be responsible for unregistering any subfamily
	// that it creates when asked to monitor subfamilies as a side-effect
	// of Create_Process.  It does this after calling the Reaper, but in
	// the starter, the starter exit's in the reaper before it returns
	// to daemon core, so subfamilies never get unregistered, and thus
	// cgroups are never deleted.  This should only be called from
	// the starter, or any other daemon where the reaper calls exit
	void Unregister_subfamily(pid_t pid);

	/** Used to explicitly cleanup our ProcFamilyInterface object
	    (used by the Master before it restarts, since in that
	    case the DaemonCore destructor won't be called.
	*/
	void Proc_Family_Cleanup();

	/** Used to explicitly stop a condor_procd child process if there is one
		(used by the Master before it restarts or exits so that it can be sure to reap the child process)
		returns true if the procd is stopping and the notify callback will be called when it is reaped
	*/
	bool Proc_Family_QuitProcd(void(*notify)(void*me,int pid,int status), void*me);

    /** @name Data pointer functions.
        These functions deal with
        associating a pointer to data with a registered callback.
    */
    //@{
    /** Set the data pointer when you're <b>inside</b> the handler
        function.  It will not work outside.

        @param data The desired pointer to set...
        @return Not_Yet_Documented
    */
    int SetDataPtr( void *data );

    /** "Register" a data pointer.  You want to do this immediately
        after you register a Command/Socket/Timer/etc.  When you enter 
        the handler for this registered function, the pointer you 
        specify will be magically available to (G|S)etDataPtr().

        @param data The desired pointer to set...
        @return Not_Yet_Documented
    */
    int Register_DataPtr( void *data );

    /** Get the data pointer when you're <b>inside</b> the handler
        function.  It will not work outside.  You must have done a 
        Register_DataPtr after the function was registered for this
        to work.

        @return The desired pointer to set...
    */
    void *GetDataPtr();
    //@}
    
	/** @name Key management.
	 */
	//@{
    /** Access to the SecMan object;
        @return Pointer to this daemon's SecMan
    */
    SecMan* getSecMan();
	IpVerify* getIpVerify() {return getSecMan()->getIpVerify();}
    KeyCache* getKeyCache();
	//@}

     
	/** @name Thread management.
	 */
	//@{
		/** Create a "poor man's" thread.  Works for NT (via
			CreateThread) and Unix (via fork).  The new thread does
			not have a DaemonCore command socket.
			NOTE: if DoFakeCreateThread() is true, this will just
			call the specified function directly and then call the
			reaper later in a timer.  Currently, this is the default
			under Windows until thread-safety can be assured.
			@param start_func Function to execute in the thread.
			   This function must not access any shared memory.
			   The only DaemonCore command which may be used in this
			   thread is Send_Signal.
			   When the function returns, the thread exits and the reaper
			   is called in the parent, giving the return value as
			   the thread exit status.
			   The function must take a single argument of type
			   (void *) and return an int.  The value of the return int
			   should ONLY be a 0 or a 1.... the thread reaper can only
			   distinguish between 0 and non-zero.
			@param arg The (void *) argument to be passed to the thread.
			   If not NULL, this must point to a buffer malloc()'ed by
			   the caller.  DaemonCore will free() this memory when
			   appropriate.
			@param sock A socket to be handed off to the thread.  This
			   socket is duplicated; one copy is given to the thread
			   and the other is kept by the caller (parent).  The
			   caller must not access this socket after handing it off
			   while the thread is still alive.  DaemonCore will close
			   the copy given to the thread when the thread exits; the
			   caller (parent) is responsible for eventually closing
			   the copy kept with the caller.  
			@param reaper_id The reaper number to use.  Default = 1.  
			   IMPORTANT: the exit_status passed to the reaper will
			   be a 0 if the start_func returned with 0, and the reaper
			   exit_status will be non-zero if the start_func returns with
			   a 1.  Example: start_func returns 1, the reaper exit_status
			   could be set to 1, or 128, or -1, or 255.... or anything 
			   except 0.  Example: start_func returns 0, the reaper exit_status
			   will be 0, and only 0.
			@return The tid of the newly created thread.
		*/
	int Create_Thread(
		ThreadStartFunc	start_func,
		void			*arg = NULL,
		Stream			*sock = NULL,
		int				reaper_id = 1
		);

		// On some platforms (currently Windows), we do not want
		// Create_Thread() to actually create a thread, because
		// just about nothing in Condor is thread safe at this time.
	bool DoFakeCreateThread() const { return m_fake_create_thread; }

	///
	int Suspend_Thread(int tid);

	///
	int Continue_Thread(int tid);

	///
	int Kill_Thread(int tid);
	//@}


	/** Public method to allow things that fork() themselves without
		using Create_Thread() to set the magic DC variable so that our
		version of exit() uses _exit() instead of exit() and we don't
		call destructors when the child exits.
	*/
	void Forked_Child_Wants_Fast_Exit( bool exit_by_exec );

	// If you call this, you own the inherited sockets, and are
	// responsible for deleting them
    Stream **GetInheritedSocks() { return (Stream **)inheritedSocks; }


		/** Register the Priv state this Daemon wants to be in, every time
		    DaemonCore is returned to the Handler DaemonCore will check to see
			that this is the current priv state.
			@param priv_state The new priv state you want to be in. DaemonCore
			initializes this to be CONDOR_PRIV.
			@return The priv state you were in before.
		*/
	priv_state Register_Priv_State( priv_state priv );
	
		/** Check the table of attributes we're willing to allow users
			at hosts of different permission levels to change to see
			if we should allow the given request to succeed.
			@param config String containing the configuration request
			@param sock The sock that we're handling this command with
			@return true if we should allow this, false if not
		*/
	bool CheckConfigSecurity( const char* config, Sock* sock );
	bool CheckConfigAttrSecurity( const char* attr, Sock* sock );


    /** Invalidate all session related cache since the configuration
     */
    void invalidateSessionCache();

	/* manage the secret cookie data */
	bool set_cookie( int len, const unsigned char* data );
	bool get_cookie( int &len, unsigned char* &data );
	bool cookie_is_valid( const unsigned char* data );

	/** The peaceful shutdown toggle controls whether graceful shutdown
	   avoids any hard killing.
	*/
	bool GetPeacefulShutdown() const;
	void SetPeacefulShutdown(bool value);

	static void TimerHandler_main_shutdown_fast(int tid);
	static int handle_dc_sigterm(int sig);
	static int handle_dc_sigquit(int sig);

	/** Called when it looks like the clock jumped unexpectedly.

	data is opaquely passed to the TimeSkipFunc.

	If you register a callback multiple times, it will be called multiple
	times.  Also, you'll need to call Unregister repeatedly if you want to
	totally silence it.  You should probably avoid registering the same
	callback function / data pointer combation multiple times.

	When Unregistering a callback, both fnc and data are considered; it
	must be an exact match for the Registered pair.
	*/
	void RegisterTimeSkipCallback(TimeSkipFunc fnc, void * data);
	void UnregisterTimeSkipCallback(TimeSkipFunc fnc, void * data);
	
    SelfMonitorData monitor_data;

	char 	*localAdFile;
	void	UpdateLocalAd(ClassAd *daemonAd,char const *fname=NULL); 

		/**
		   Publish all DC-specific attributes into the given ClassAd.
		   Every daemon should call this method on its own copy of its
		   ClassAd, so that these attributes are available in the
		   daemon itself (e.g. in the startd's ClassAd used for
		   evaluating the policy expressions), instead of calling this
		   automatically just when the ad is sent to the collector.
		*/
	void	publish(ClassAd *ad);

		/**
		   @return A pointer to the CollectorList object that holds
		   all of the collectors this daemons is configured to
		   communicate with.
		*/
	CollectorList* getCollectorList(void);

		/**
		   Send the given ClassAd(s) to all of the collectors defined
		   in the COLLECTOR_HOST setting. Now that DaemonCore manages
		   this list of collectors, daemons only need to invoke this
		   method to send out their updates.
		   @param cmd The update command to send (e.g. UPDATE_STARTD_AD)
		   @param ad1 The primary ClassAd to send.
		   @param ad2 The secondary ClassAd to send. Usually NULL,
		     except in the case of startds sending the "private" ad.
		   @param nonblock Should the update use non-blocking communication.
		   @return The number of successful updates that were sent.
		*/
	int sendUpdates(int cmd, ClassAd* ad1, ClassAd* ad2 = NULL, bool nonblock = false,
		DCTokenRequester *requester = nullptr, const std::string &identity = "",
		const std::string &authz_name = "");

	DCCollectorAdSequences & getUpdateAdSeq() { return m_collector_list->getAdSeq(); }

	time_t getStartTime() const {return m_startup_time;}

		/**
		   Indicates if this daemon wants to be restarted by its
		   parent or not.  Usually true, unless one of the
		   DAEMON_SHUTDOWN expressions was TRUE, in which case we do
		   not want to be automatically restarted.
		*/
	bool wantsRestart( void ) const;

		/**
		   Tell the daemon to send itself the shutdown signal
		*/
	void beginDaemonRestart(bool fast = false, bool restart = true);
	void beginDaemonShutdown(bool fast = false) { beginDaemonRestart(fast, false); }

		/**
		   Set whether this daemon should should send CHILDALIVE commands
		   to its daemoncore parent. Must be called from
		   main_pre_command_sock_init() to have any effect. The default
		   is to send CHILDALIVEs.
		*/
	void WantSendChildAlive( bool send_alive )
		{ m_DaemonKeepAlive.WantSendChildAlive(send_alive); }
	
		/**
			Returns true if a wake signal was sent to the master's select.
			Does nothing and returns false if called by the master or by a thread 
			that is not a condor_thread. 
		*/
	bool Wake_up_select();

		/**
			Wakes up the main condor select, does not check to see if the caller
			is a condor_thread or the main thread. returns true if the main thread
			will wake, or false if unable to send the wake signal.
		*/
	bool Do_Wake_up_select();

		/**
			gather info about the wakeup select pipe from a thread that may be async to the main thread
		*/
	bool AsyncInfo_Wake_up_select(void * &dst, int & dst_fd, void* &src, int & src_fd);

		/**
			check hotness of wakeup select socket from an async thread. used for debugging
		*/
	int Async_test_Wake_up_select(void * &dst, int & dst_fd, void* &src, int & src_fd, std::string & status);

	/** Registers a socket for read and then calls HandleReq to
			process a command on the socket once one becomes
			available.
		*/
	void HandleReqAsync(Stream *stream);

		/** Force a reload of the shared port server address.
			Called, for example, after the master has started up the
			shared port server.
		*/
	void ReloadSharedPortServerAddr();
		/** Unset the shared port server address.
			Called, for example, before the master starts up the
			shared port server, to ensure a sensible parent address.
		 */
	void ClearSharedPortServerAddr();

	void InstallAuditingCallback( void (*fn)(int, Sock&, bool) ) { audit_log_callback_fn = fn; }

	//-----------------------------------------------------------------------------
	/*
  	 Statistical values for the operation of DaemonCore, to be published in the
  	 ClassAd for the daemon.
	*/
	class Stats {
	public:
	   // published values
	   time_t StatsLifetime;       // total time the daemon has been collecting statistics. (uptime)
	   time_t StatsLastUpdateTime; // last time that statistics were last updated. (a freshness time)
	   time_t RecentStatsLifetime; // actual time span of current DCRecentXXX data.
   
	   stats_entry_recent<double> SelectWaittime; //  total time spent waiting in select
	   stats_entry_recent<double> SignalRuntime;  //  total time spent handling signals
	   stats_entry_recent<double> TimerRuntime;   //  total time spent handling timers
	   stats_entry_recent<double> SocketRuntime;  //  total time spent handling socket messages
	   stats_entry_recent<double> PipeRuntime;    //  total time spent handling pipe messages

	   stats_entry_recent<int> Signals;        //  number of signals handlers called
	   stats_entry_abs<int> TimersFired;    //  number of timer handlers called
	   stats_entry_recent<int> SockMessages;   //  number of socket handlers called
	   stats_entry_recent<int> PipeMessages;   //  number of pipe handlers called
	   //stats_entry_recent<int64_t> SockBytes;      //  number of bytes passed though the socket (can we do this?)
	   //stats_entry_recent<int64_t> PipeBytes;      //  number of bytes passed though the socket
	   stats_entry_recent<int> DebugOuts;      //  number of dprintf calls that were written to output.
      #ifdef WIN32
	   stats_entry_recent<int> AsyncPipe;      //  number of times async_pipe was signalled
      #endif
	   stats_entry_abs<int> UdpQueueDepth;  // Unread bytes for the UDP command port 

		
       stats_entry_recent<Probe> PumpCycle;   // count of pump cycles plus sum of cycle time with min/max/avg/std 
       stats_entry_sum_ema_rate<int> Commands;

       StatisticsPool          Pool;          // pool of statistics probes and Publish attrib names
	   std::shared_ptr<stats_ema_config> ema_config;	// Exponential moving average config for this pool.

	   time_t InitTime;            // last time we init'ed the structure
	   time_t RecentStatsTickTime; // time of the latest recent buffer Advance
	   int    RecentWindowMax;     // size of the time window over which RecentXXX values are calculated.
       int    RecentWindowQuantum;
       int    PublishFlags;        // verbositiy of publishing
	   bool   enabled;            // set to true to enable statistics, otherwise the pool will be empty and AddProbe calls will quietly fail.

	   // helper methods
	   //Stats();
	   //~Stats();
	   void Init(bool enable);
       void Reconfig();
	   void Clear();
	   time_t Tick(time_t now=0); // call this when time may have changed to update StatsLastUpdateTime, etc.
	   void SetWindowSize(int window);
	   void Publish(ClassAd & ad) const;
	   void Publish(ClassAd & ad, int flags) const;
       void Publish(ClassAd & ad, const char * config) const;
	   void Unpublish(ClassAd & ad) const;
       void* NewProbe(const char * category, const char * name, int as);
       void AddToProbe(const char * name, int val);
       void AddToProbe(const char * name, int64_t val);
       void AddToSumEmaRate(const char * name, int val);
       void AddToAnyProbe(const char * name, int val);
       double AddSample(const char * name, int as, double val);
       double AddRuntime(const char * name, double before); // returns current time.
       double AddRuntimeSample(const char * name, int as, double before);

	} dc_stats;

	bool wants_dc_udp_self() const { return m_wants_dc_udp_self;}

		// Create a session that permits a remote entity to be an administrator
		// of this daemon.
		// - duration: Lifetime of the session.
		// - capability (output): Resulting serialized session string.
		// - Return: true on success.
		// This is public so the collector process can invoke this directly for
		// its selfAd.
	bool SetupAdministratorSession(unsigned duration, std::string &capability);

	void kill_immediate_children();

  private:

		// do and our parents/children want/have a udp comment socket?
	bool m_wants_dc_udp;
		// Send_Signal should use UDP rather than TCP if possible (the standard pre 9.0 behavior)
	bool m_use_udp_for_dc_signals;
		// DaemonCore::Send_Signal should never use kill when the target is a daemon core daemon
	bool m_never_use_kill_for_dc_signals;
		// do we ourself want/have a udp comment socket?
	bool m_wants_dc_udp_self;
	bool m_invalidate_sessions_via_tcp;

	// This pairing should representing the "same" socket, just on UDP and TCP.
	// It's okay for parts to be NULL.  Safe to copy, although all of the
	// copies will be sharing the same sockets.  
  public:
	class SockPair {
	public:

		SockPair() : m_rsock(NULL), m_ssock(NULL) {}
		SockPair(const SockPair & src) : m_rsock(src.m_rsock), m_ssock(src.m_ssock) {}

		SockPair & operator=(const SockPair & src) {
			m_rsock = src.m_rsock;
			m_ssock = src.m_ssock;
			return *this;
		}

			// Strictly unnecessary, but proved helpful for debugging.
		~SockPair() {
			m_rsock = std::shared_ptr<ReliSock>(nullptr);
			m_ssock = std::shared_ptr<SafeSock>(nullptr);
		}

		bool has_relisock() const { return (bool)m_rsock; }
		bool has_safesock() const { return (bool)m_ssock; }
		bool not_empty() const { return has_relisock() || has_safesock(); }

		// If you really need a non-shared_ptr version, use .get(). Avoid
		// doing so if possible.  If you must, keep the usage brief.
		std::shared_ptr<ReliSock> rsock() { return m_rsock; }
		std::shared_ptr<SafeSock> ssock() { return m_ssock; }

		// Associate a ReliSock or SafeSock with this SockPair. Does nothing
		// if one is already associated. b must always be true and always
		// returns true.
		bool has_relisock(bool b);
		bool has_safesock(bool b);
	private:
		std::shared_ptr<ReliSock> m_rsock;	// tcp command socket
		std::shared_ptr<SafeSock> m_ssock;	// udp command socket
	};
	typedef std::vector<SockPair> SockPairVec;

	bool m_create_family_session;
	std::string m_family_session_id;
	std::string m_family_session_key;

  private:
	SockPairVec dc_socks;
	ReliSock* super_dc_rsock;	// super user tcp command socket
	SafeSock* super_dc_ssock;	// super user udp command socket
	int m_super_dc_port;		// super user listen port
    int m_iMaxAcceptsPerCycle; ///< maximum number of inbound connections to accept per loop
	int m_iMaxReapsPerCycle; // maximum number reapers to invoke per event loop
	int m_MaxTimeSkip;
	int m_iMaxUdpMsgsPerCycle;	// max number of udp messages read per loop

    void Inherit( void );  // called in main()
	void InitDCCommandSocket( int command_port );  // called in main()
	void SetDaemonSockName( char const *sock_name );

    int HandleSigCommand(int command, Stream* stream);
    int HandleReq(size_t socki, Stream* accepted_sock=NULL);
	int HandleReq(Stream *insock, Stream* accepted_sock=NULL);
	int HandleReqSocketTimerHandler();
	int HandleReqSocketHandler(Stream *stream);
	int HandleReqPayloadReady(Stream *stream);
    int HandleSig(int command, int sig);

	bool RegisterSocketForHandleReq(Stream *stream);

	friend class FakeCreateThreadReaperCaller;
	void CallReaper(int reaper_id, char const *whatexited, pid_t pid, int exit_status);

    int HandleProcessExitCommand(int command, Stream* stream);
    int HandleProcessExit(pid_t pid, int exit_status);
    int HandleDC_SIGCHLD(int sig);
	int HandleDC_SERVICEWAITPIDS(int sig);
 
    int Register_Command(int command, const char *com_descip,
                         CommandHandler handler, 
                         CommandHandlercpp handlercpp,
                         const char *handler_descrip,
                         Service* s, 
                         DCpermission perm,
                         int is_cpp,
                         bool force_authentication,
                         int wait_for_payload,
                         std::vector<DCpermission> *alternate_perm);

    int Register_Signal(int sig,
                        const char *sig_descip,
                        SignalHandler handler, 
                        SignalHandlercpp handlercpp,
                        const char *handler_descrip,
                        Service* s, 
                        int is_cpp);

    int Register_Socket(Stream* iosock,
                        const char *iosock_descrip,
                        SocketHandler handler, 
                        SocketHandlercpp handlercpp,
                        const char *handler_descrip,
                        Service* s, 
			HandlerType handler_type,
                        int is_cpp,
                        void **prev_entry = NULL);

    int Register_Pipe(int pipefd,
                        const char *pipefd_descrip,
                        PipeHandler handler, 
                        PipeHandlercpp handlercpp,
                        const char *handler_descrip,
                        Service* s, 
					    HandlerType handler_type, 
                        int is_cpp);

    int Register_Reaper(int rid,
                        const char *reap_descip,
                        ReaperHandler handler, 
                        ReaperHandlercpp handlercpp,
                        const char *handler_descrip,
                        Service* s, 
                        int is_cpp);

	bool Register_Family(pid_t child_pid,
	                     pid_t parent_pid,
	                     int max_snapshot_interval,
	                     PidEnvID* penvid,
	                     const char* login,
	                     gid_t* group,
	                     FamilyInfo* fi);

	void CheckForTimeSkip(time_t time_before, time_t okay_delta);

		// If this platform supports clone() as a faster alternative to fork(), use it (or not).
#ifdef HAVE_CLONE
	bool m_use_clone_to_create_processes;
	bool UseCloneToCreateProcesses() const { return m_use_clone_to_create_processes; }
#else
	bool UseCloneToCreateProcesses() { return false; }
#endif

	void Send_Signal(classy_counted_ptr<DCSignalMsg> msg, bool nonblocking);

	std::string GetCommandsInAuthLevel(DCpermission perm,bool is_authenticated);

	// Returns first command socket in our list. In general, you
	// probably want to spin over sockTable looking for it->command_sock==true,
	// this is a transitional function for the old initial_command_sock 
	// variable.  Returns index into sockTable, -1 if none available.
	int initial_command_sock() const;

	struct CommandEnt
	{
		int             num{0};
		bool            is_cpp{true};
		bool            force_authentication{false};
		CommandHandler  handler{nullptr};
		CommandHandlercpp   handlercpp{nullptr};
		DCpermission    perm{ALLOW};
		Service*        service{nullptr};
		char*           command_descrip{nullptr};
		char*           handler_descrip{nullptr};
		void*           data_ptr{nullptr};
		int             wait_for_payload{0};
		// If there are alternate permission levels where the
		// command is permitted, they will be listed here.
		std::vector<DCpermission> *alternate_perm{nullptr};
    };

    void                DumpCommandTable(int, const char* = NULL);
	std::vector<CommandEnt>         comTable;       // command table
    CommandEnt          m_unregisteredCommand;

    struct SignalEnt 
    {
        int             num;
        bool            is_cpp;
        bool            is_blocked;
        // Note: is_pending must be volatile because it could be set inside
        // of a Unix asynchronous signal handler (such as SIGCHLD).
        volatile bool   is_pending;
        SignalHandler   handler;
        SignalHandlercpp    handlercpp;
        Service*        service; 
        char*           sig_descrip;
        char*           handler_descrip;
        void*           data_ptr;
    };
    void                DumpSigTable(int, const char* = NULL);
	std::vector<SignalEnt> sigTable;    // signal table
    volatile int        sent_signal; // TRUE if a signal handler sends a signal

    struct SockEnt
    {
        Sock*           iosock;
        SocketHandler   handler;
        SocketHandlercpp    handlercpp;
        Service*        service; 
        char*           iosock_descrip;
        char*           handler_descrip;
        void*           data_ptr;
        bool            is_cpp;
		bool			is_connect_pending;
		bool			is_reverse_connect_pending;
		bool			call_handler;
		bool			waiting_for_data;
		bool			remove_asap;	// remove when being_serviced==false
		HandlerType		handler_type;
		int				servicing_tid;	// tid servicing this socket
		bool            is_command_sock;
    };
    void              DumpSocketTable(int, const char* = NULL);
	int				  nRegisteredSocks; // number of sockets registered, always < nSock
	int               nPendingSockets; // number of sockets waiting on timers or any other callbacks
	std::vector<SockEnt> sockTable; // socket table; grows dynamically if needed

		// number of file descriptors in use past which we should start
		// avoiding the creation of new persistent sockets.  Do not use
		// this value directly.  Call FileDescriptorSafetyLimit().
	int file_descriptor_safety_limit;

	bool m_fake_create_thread;

#if defined(WIN32)
	typedef PipeEnd* PipeHandle;
#else
	typedef int PipeHandle;
#endif
	std::vector<PipeHandle> pipeHandleTable;
	size_t pipeHandleTableInsert(PipeHandle);
	void pipeHandleTableRemove(size_t);
	int pipeHandleTableLookup(size_t, PipeHandle* = NULL);
	int maxPipeBuffer;

	// this table is for dispatching registered pipes
	class PidEntry;  // forward reference
    struct PipeEnt
    {
        PipeHandler		handler;
        PipeHandlercpp  handlercpp;
        Service*        service; 
        char*           pipe_descrip;
        char*           handler_descrip;
        void*           data_ptr;
		PidEntry*		pentry;
        int				index;		// index into the pipeHandleTable
		HandlerType		handler_type;
        bool            is_cpp;
		bool			call_handler;
		bool			in_handler;
    };
	std::vector<PipeEnt> pipeTable; // pipe table; grows dynamically if needed

    struct ReapEnt
    {
        int             num;
        bool            is_cpp;
        ReaperHandler   handler;
        ReaperHandlercpp    handlercpp;
        Service*        service; 
        char*           reap_descrip;
        char*           handler_descrip;
        void*           data_ptr;
    };
    void                DumpReapTable(int, const char* = NULL);
    size_t              nReap;          // number of reaper handlers used
    int                 nextReapId;     // next reaper id to use
	std::vector<ReapEnt>  reapTable;      // reaper table
    int                 defaultReaper;

    class PidEntry : public Service
    {
	public:
		PidEntry();
		~PidEntry();
		int pipeHandler(int pipe_fd);
		int pipeFullWrite(int pipe_fd);

        pid_t pid;
        int new_process_group;
#ifdef WIN32
        HANDLE hProcess;
        HANDLE hThread;
        DWORD tid;
		LONG pipeReady;
		PipeEnd *pipeEnd;
		HANDLE watcherEvent;
		LONG deallocate;
#endif
		volatile unsigned int process_exited;   // set to true if the process has exited, set before reaper callback
		std::string sinful_string;
        int is_local;
        int parent_is_local;
        int reaper_id;
        int cleanup_signal;
        int std_pipes[3];  // Pipe handles for automagic DC std pipes.
        std::string* pipe_buf[3];  // Buffers for data written to DC std pipes.
        int stdin_offset;

		// these three data members are set/used by the DaemonKeepAlive class
        time_t hung_past_this_time;   // if >0, child is hung if time() > this value
        int was_not_responding;
        int got_alive_msg; // number of child alive messages received

		/* the environment variables which allow me the track the pidfamily
			of this pid (where applicable) */
		PidEnvID penvid;
		std::string shared_port_fname;
		//Session ID and key for child process.
		char* child_session_id;
    };

	int m_refresh_dns_timer;

	std::map<pid_t, PidEntry> pidTable;
    pid_t mypid;
    pid_t ppid;

    ProcFamilyInterface* m_proc_family;

#ifdef WIN32
    // note: as of WinNT 4.0, MAXIMUM_WAIT_OBJECTS == 64
    struct PidWatcherEntry
    {
        PidEntry* pidentries[MAXIMUM_WAIT_OBJECTS - 1];
        HANDLE event;
        HANDLE hThread;
        CRITICAL_SECTION crit_section;
        int nEntries;
    };

	std::vector<PidWatcherEntry *> PidWatcherList;

    int                 WatchPid(PidEntry *pidentry);

    // PumpWorkItem is an item in the PumpWorkCallback list
    // note that you should use __aligned_malloc TO to allocate these
    // in order to gurantee alignment
    struct __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) PumpWorkItem
    {
        SLIST_ENTRY      slist;
        PumpWorkCallback callback;
        void *           cls;
        void *           data;
    };

    __declspec(align(MEMORY_ALLOCATION_ALIGNMENT))
    SLIST_HEADER        PumpWorkHead; // list head for async PumpWorkCallback items.
#else
    // implement for non windows?

#endif
    int  DoPumpWork(); // call on main thread to handle all of work in the PumpWork list, returns number of callbacks handled
            
    TimerManager &t;
    void                DumpTimerList(int, const char* = NULL);

	SecMan	    		*sec_man;

	int					_cookie_len, _cookie_len_old;
	unsigned char		*_cookie_data, *_cookie_data_old;

	void (*audit_log_callback_fn)( int, Sock &, bool);

#ifdef WIN32
    // the thread id of the thread running the main daemon core
    DWORD   dcmainThreadId;
    CSysinfo ntsysinfo;     // class to do undocumented NT process management

    // set files inheritable or not on NT
    int SetFDInheritFlag(int fd, int flag);
#endif

	// Setup an async_pipe, used to wake up select from another thread.
	// Implemented on Unix via a pipe, on Win32 via a pair of connected tcp sockets.
#ifndef WIN32
    int async_pipe[2];  // 0 for reading, 1 for writing
    volatile int async_sigs_unblocked;
	volatile bool async_pipe_signal;
#else
	ReliSock async_pipe[2];  // 0 for reading, 1 for writing
	volatile unsigned int async_pipe_signal;
#endif

	// Data memebers for queuing up waitpid() events
	struct WaitpidEntry_s
	{
		pid_t child_pid;
		int exit_status;
		bool operator==( const struct WaitpidEntry_s &target) const { 
			if(child_pid == target.child_pid) {
				return true;
			}
			return false;
		}

	};
	typedef struct WaitpidEntry_s WaitpidEntry;
	std::deque<WaitpidEntry> WaitpidQueue;

    Stream *inheritedSocks[MAX_SOCKS_INHERITED+1];

	// Methods to detect and kill hung processes are consolidated
	// in the DaemonKeepAlive helper friend class.
	DaemonKeepAlive m_DaemonKeepAlive;

	// Method to check on and possibly recover from a bad connection
	// to the procd. Suitable to be registered as a one-shot timer.
	int CheckProcInterface();
	void CheckProcInterfaceFromTimer( int /* timerID */ ) { (void)CheckProcInterface(); }

	// misc helper functions
	void CheckPrivState( void );

		// invoked by CondorThreads upon thread context switch
	static void thread_switch_callback(void* & ptr);

		// Call the registered socket handler for this socket
		// i - index of registered socket
		// default_to_HandleCommand - true if HandleCommand() should be called
		//                          if there is no other callback function
	void CallSocketHandler( const size_t i, bool default_to_HandleCommand );
	static void CallSocketHandler_worker_demarshall(void *args);
	void CallSocketHandler_worker( int i, bool default_to_HandleCommand, Stream* asock );
	

		// Returns index of registered socket or -1 if not found.
	int GetRegisteredSocketIndex( Stream *sock );

    int inServiceCommandSocket_flag;
    bool m_need_reconfig;
    bool m_delay_reconfig;
#ifndef WIN32
    static char **ParseArgsString(const char *env);
#endif

	priv_state Default_Priv_State;

		/** Initialize our array of StringLists that we use to verify
			if a given request to set a configuration variable with
			condor_config_val should be allowed.
		*/
	void InitSettableAttrsLists( void );
	bool InitSettableAttrsList( const char* subsys, int i );
	std::vector<std::string>* SettableAttrsLists[LAST_PERM];

	bool peaceful_shutdown;

	struct TimeSkipWatcher {
		TimeSkipFunc fn;
		void * data;
	};

	std::vector<TimeSkipWatcher *> m_TimeSkipWatchers;

		/**
		   Evaluate a DC-specific policy expression and return the
		   result.  Prints a message to the log if the expr evaluates
		   to TRUE.
		   @param ad ClassAd to evaluate the expression in.
		   @param param_name Name of the config file parameter that
		     defines the expression.
		   @param attr_name Name of the ClassAd attribute for this
		     expression.
		   @param message Additional text to include in the log
		     message when the expression evaluates to TRUE.
		   @return true if the expression exists and evaluates to
		     TRUE, false otherwise.
		*/
	bool evalExpr( ClassAd* ad, const char* param_name,
				   const char* attr_name, const char* message );

	CollectorList* m_collector_list;

		/**
		   Initialize the list of collectors this daemon is configured
		   to communicate with.  This is handled automatically by
		   DaemonCore, so individual daemons should not all this
		   themselves.
		*/
	void initCollectorList(void);

	// Inform a client that they attempted to resume a session that
	// we don't have.
	// If the client is version 8.8.0 or later, extended information
	// can be provided in the info_ad. Older clients will assume the
	// ad is part of the session id to invalidate.
	void send_invalidate_session ( const char* sinful, const char* sessid,
	                               const ClassAd* info_ad = NULL ) const;

	bool m_wants_restart{true};
	bool m_in_shutdown_peaceful{false};
	bool m_in_shutdown_graceful{false};
	bool m_in_shutdown_fast{false};

	char* m_private_network_name;

	// This is the argument passed to InitDCCommandSocket(), taken from
	// the -port command line argument (default -1). We need this to
	// correctly handle configuration of the shared port endpoint for
	// the command socket (if there is one).
	int m_command_port_arg;

	class CCBListeners *m_ccb_listeners;
	class SharedPortEndpoint *m_shared_port_endpoint;
	std::string m_daemon_sock_name;
	Sinful m_sinful;     // full contact info (public, private, ccb, etc.)
	bool m_dirty_sinful; // true if m_sinful needs to be reinitialized
	std::vector<Sinful> m_command_sock_sinfuls; // Cached copy of our command sockets' sinful strings.
	bool m_dirty_command_sock_sinfuls; // true if m_command_sock_sinfuls needs to be reinitialized.

	//
	// For compabitility with existing configurations and code, when we
	// enabled mixed-mode (IPv4 and IPv6) by default, we decided to change
	// to advertising IPv4 addresses first.  Since older mixed-mode clients
	// will always use the earliest of the addresses it can, this avoids
	// a number of problems with host-based ALLOW settings.  Of course,
	// this is against the RFC, and may not be what administrators want to
	// do as they continue to migrate to IPv6, so now we have a new knob.
	//
	bool m_advertise_ipv4_first;

	bool CommandNumToTableIndex(int cmd,int *cmd_index);

	void InitSharedPort(bool in_init_dc_command_socket=false);

	std::string m_inherit_parent_sinful;

		// Enable remote administration for this daemon.
	void SetRemoteAdmin(bool remote_admin);

	static unsigned m_remote_admin_seq;
	static time_t m_startup_time;
	bool m_enable_remote_admin{false};
	time_t m_remote_admin_last_time{0};
	std::string m_remote_admin_last;
};

/**
 * Helper function to initialize command ports.
 *
 * This function is call for both the initial command sockets and when
 * creating sockets to inherit before DC:Create_Process().  It calls
 * bind() or BindAnyCommandPort()  as needed, listen() and
 * setsockopt() (for SO_REUSEADDR and TCP_NODELAY).
 *
 * @param tcp_port
 *   What TCP port you want to bind to, or -1 if you don't care.
 * @param udp_port
 *   What UDP port you want to bind to, or -1 if you don't care.
 *   If tcp_port is positive, then udp_port must be the same value.
 *   If udp_port is positive, then tcp_port may still be -1.
 *   This allows us to listen on a well-known udp port but let another
 *   daemon (shared_port) listen on the corresponding tcp port.
 * @param socks
 *   Created socks will be pushed onto this list.
 * @param fatal
 *   Should errors be considered fatal (EXCEPT) or not (dprintf).
 *
 * @return
 *   true if everything worked, false if there were any errors.
 *
 * @see BindAnyCommandPort()
 * @see DaemonCore::InitDCCommandSock
 * @see DaemonCore::Create_Process
 */
bool InitCommandSockets(int tcp_port, int udp_port, DaemonCore::SockPairVec & socks, bool want_udp, bool fatal);

// helper function to extract the parent address and inherited socket information from
// the inherit string that is normally passed via the CONDOR_INHERIT environment variable
// This function extracts parent & socket info then tokenizes the remaining items from
// the string into the supplied vector.
//
// @return
//    number of entries in the socks[] array that were populated.
//    negative values for failure.
//
// note: the size of the socks array should be 1 more than the maximum if you want to have room
// for a terminating null.
//
int extractInheritedSocks (
	const char * inherit,  // in: inherit string, usually from CONDOR_INHERIT environment variable
	pid_t & ppid,          // out: pid of the parent
	std::string & psinful, // out: sinful of the parent
	Stream* socks[],   // out: filled in with items from the inherit string
	int     cMaxSocks, // in: number of items in the socks array
	std::vector<std::string> & remaining_items); // out: unparsed items from the inherit string are appended

// helper class that uses C++ constructor/destructor to automatically
// time a function call. 
class dc_stats_auto_runtime_probe
{
public:
    dc_stats_auto_runtime_probe(const char * name, int as);
    ~dc_stats_auto_runtime_probe();
    stats_entry_recent<Probe> * probe;
    double                    begin;
};

#define DC_AUTO_RUNTIME_PROBE(n,a) dc_stats_auto_runtime_probe a(n, IF_VERBOSEPUB)
#define DC_AUTO_FUNCTION_RUNTIME(a) DC_AUTO_RUNTIME_PROBE(__FUNCTION__,a)


#ifndef _NO_EXTERN_DAEMON_CORE

/** Function to call when you want your daemon to really exit.
    This allows DaemonCore a chance to clean up.  This function should be
    used in place of the standard <tt>exit()</tt> function!
    @param status The return status upon exit.  The value you would normally
           pass to the regular <tt>exit()</tt> function if you weren't using
           Daemon Core.
    @param shutdown_program Optional program to exec() instead of permforming
		   a normal "exit".  If the exec() fails, the normal exit() will
		   occur.
*/
[[noreturn]]
extern PREFAST_NORETURN void DC_Exit( int status, const char *shutdown_program = NULL ) GCC_NORETURN;

/** Call this function (inside your main_pre_dc_init() function) to
    bypass the core limit initialization in daemoncore.  This is for
    programs, such as condor_dagman, that are Condor daemons but should
    run with the user's limits.
*/
extern void DC_Skip_Core_Init();


extern void dc_reconfig();

/** The main DaemonCore object.  This pointer will be automatically instatiated
    for you.  A perfect place to use it would be in your main_init, to access
    Daemon Core's wonderful services, like <tt>Register_Signal()</tt> or
    <tt>Register_Timer()</tt>.
*/

extern DaemonCore* daemonCore;
#endif


/**
	Spawn a thread (process on Unix) to do some work.  "Worker"
	is the function called in the seperate thread/process.  When
	it exits, "Reaper" will be called in the parent
	thread/process.  Both the Worker and Reaper functions will be
	passed three optional opaque data variables, two integers and
	a void *.  The Reaper is also optional.

	@param Worker   Called in seperate thread or process.  Passed
		data_n1, data_n2, and data_vp.  Must be a valid function
		pointer.

	@param Reaper   Called in parent thread or process after the
		Worker exists.  Passed data_n1, data_n2, data_vp and the
		return code of Worker.  Also passed is the "exit status".
		The exit status if false (0) or true (any non-0 result) based
		on if the result from Worker is false or true.  If the result
		is true (non 0), the specific value is undefined beyond "not
		0".  You do not necessarily get the exact value returned by
		the worker.  If you don't want this notification, pass in
		NULL (0).

	@param data_n1   (also data_n2) Opaque values passed to
		worker and reaper.  Useful place to store a cluster and proc,
		but this code doesn't look at the values in any way.

	@param data_n2   see data_n1

	@param data_vp   Opaque value passed to Worker and Reaper.
		For safety this should be in the heap (malloc/new) or a
		pointer to a static data member.  You cannot assume that this
		memory space is shared by the parent and child, it may or may
		not be copied for the child.  If it's heap memory, be sure to
		release it somewhere in the parent, probably in Reaper.

	@return   TID, thread id, as returned by DaemonCore::Create_Thread.
*/

int Create_Thread_With_Data(DataThreadWorkerFunc Worker, DataThreadReaperFunc Reaper, 
	int data_n1 = 0, int data_n2 = 0, void * data_vp = 0);

#define MAX_INHERIT_SOCKS 10
#define MAX_INHERIT_FDS   32

// Prototype to get sinful string.
char const *global_dc_sinful( void );

#endif /* _CONDOR_DAEMON_CORE_H_ */
