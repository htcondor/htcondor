/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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
#include "condor_timer_manager.h"
#include "condor_ipverify.h"
#include "condor_commands.h"
#include "condor_classad.h"
#include "condor_secman.h"
#include "HashTable.h"
#include "KeyCache.h"
#include "list.h"
#include "extArray.h"
#include "Queue.h"
#ifdef WIN32
#include "ntsysinfo.h"
#endif

#define DEBUG_SETTABLE_ATTR_LISTS 0

static const int KEEP_STREAM = 100;
static const int MAX_SOCKS_INHERITED = 4;
static char* EMPTY_DESCRIP = "<NULL>";

/** @name Typedefs for Callback Procedures
 */
//@{
///
typedef int     (*CommandHandler)(Service*,int,Stream*);

///
typedef int     (Service::*CommandHandlercpp)(int,Stream*);

///
typedef int     (*SignalHandler)(Service*,int);

///
typedef int     (Service::*SignalHandlercpp)(int);

///
typedef int     (*SocketHandler)(Service*,Stream*);

///
typedef int     (Service::*SocketHandlercpp)(Stream*);

///
typedef int     (*ReaperHandler)(Service*,int pid,int exit_status);

///
typedef int     (Service::*ReaperHandlercpp)(int pid,int exit_status);

///
typedef int		(*ThreadStartFunc)(void *,Stream*);
//@}

// other macros and protos needed on WIN32 for exit status
#ifdef WIN32
#define WEXITSTATUS(stat) ((int)(stat))
#define WTERMSIG(stat) ((int)(stat))
#define WIFSTOPPED(stat) ((int)(0))
///
int WIFEXITED(DWORD stat);
///
int WIFSIGNALED(DWORD stat);
#endif  // of ifdef WIN32

// some constants for HandleSig().
#define _DC_RAISESIGNAL 1
#define _DC_BLOCKSIGNAL 2
#define _DC_UNBLOCKSIGNAL 3

// If WANT_DC_PM is defined, it means we want DaemonCore Process Management.
// We _always_ want it on WinNT; on Unix, some daemons still do their own 
// Process Management (just until we get around to changing them to use 
// daemon core).
#if defined(WIN32) && !defined(WANT_DC_PM)
#define WANT_DC_PM
#endif

/** helper function for finding available port for both 
    TCP and UDP command socket */
int BindAnyCommandPort(ReliSock *rsock, SafeSock *ssock);

/** This global should be defined in your subsystems's main.C file.
    Here are some examples:<p>

    <UL>
      <LI><tt>char* mySubSystem = "DAGMAN";</tt>
      <LI><tt>char* mySubSystem = "SHADOW";</tt>
      <LI><tt>char* mySubSystem = "SCHEDD";</tt>
    </UL>
*/
extern char *mySubSystem;

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
#ifdef WIN32
  friend int dc_main( int argc, char** argv );
  friend unsigned pidWatcherThread(void*);
  friend BOOL CALLBACK DCFindWindow(HWND, LPARAM);
  friend BOOL CALLBACK DCFindWinSta(LPTSTR, LPARAM);
#else
  friend int main(int, char**);
#endif
    
  public:
    
  	/* These are all declarations for methods which never need to be
	 * called by the end user.  Thus they are not docified.
	 * Typically these methods are invoked from functions inside 
	 * of daemon_core_main.C.
	 */
    DaemonCore (int PidSize = 0, int ComSize = 0, int SigSize = 0,
                int SocSize = 0, int ReapSize = 0);
    ~DaemonCore();
    void Driver();

    /** Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int ReInit();

    /** Not_Yet_Documented
        @param perm Not_Yet_Documented
        @param sin  Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Verify (DCpermission perm, const struct sockaddr_in *sin, const char * fqu);
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
        @param dprintf_flag    Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Command (int             command,
                          char *          com_descrip,
                          CommandHandler  handler, 
                          char *          handler_descrip,
                          Service *       s                = NULL,
                          DCpermission    perm             = ALLOW,
                          int             dprintf_flag     = D_COMMAND);
    
    /** Not_Yet_Documented
        @param command         Not_Yet_Documented
        @param com_descrip     Not_Yet_Documented
        @param handlercpp      Not_Yet_Documented
        @param handler_descrip Not_Yet_Documented
        @param s               Not_Yet_Documented
        @param perm            Not_Yet_Documented
        @param dprintf_flag    Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Command (int                command,
                          char *             com_descript,
                          CommandHandlercpp  handlercpp, 
                          char *             handler_descrip,
                          Service *          s,
                          DCpermission       perm             = ALLOW,
                          int                dprintf_flag     = D_COMMAND);
    
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
    char* InfoCommandSinfulString (int pid = -1);

    /** Not_Yet_Documented
        @return Not_Yet_Documented
		*/
    int ServiceCommandSocket();

    /** Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int InServiceCommandSocket() {
        return inServiceCommandSocket_flag;
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
        @param perm             Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Signal (int             sig,
                         char *          sig_descrip,
                         SignalHandler   handler, 
                         char *          handler_descrip,
                         Service*        s                = NULL,
                         DCpermission    perm             = ALLOW);
    
    /** Not_Yet_Documented
        @param sig              Not_Yet_Documented
        @param sig_descrip      Not_Yet_Documented
        @param handlercpp       Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @param perm             Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Signal (int                 sig,
                         char *              sig_descript,
                         SignalHandlercpp    handlercpp, 
                         char *              handler_descrip,
                         Service*            s,
                         DCpermission        perm = ALLOW);
    
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
    int Send_Signal (pid_t pid, int sig);
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
    int Register_Reaper (char *          reap_descrip,
                         ReaperHandler   handler,
                         char *          handler_descrip,
                         Service*        s = NULL);
    
    /** Not_Yet_Documented
        @param reap_descrip     Not_Yet_Documented
        @param handlercpp       Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return Not_Yet_Documented
    */
     int Register_Reaper (char *            reap_descript,
                          ReaperHandlercpp  handlercpp, 
                          char *            handler_descrip,
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
                      char *           reap_descrip,
                      ReaperHandler    handler, 
                      char *           handler_descrip,
                      Service *        s = NULL);
    
    /** Not_Yet_Documented
        @param rid The Reaper ID
        @param reap_descrip     Not_Yet_Documented
        @param handlercpp       Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Reset_Reaper (int                    rid,
                      char *                 reap_descript,
                      ReaperHandlercpp       handlercpp, 
                      char *                 handler_descrip,
                      Service*               s);

    /** Not_Yet_Documented
        @param rid The Reaper ID
        @return Not_Yet_Documented
    */
    int Cancel_Reaper (int rid);
   

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
        @param perm             Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Socket (Stream*           iosock,
                         char *            iosock_descrip,
                         SocketHandler     handler,
                         char *            handler_descrip,
                         Service *         s                = NULL,
                         DCpermission      perm             = ALLOW);

    /** Not_Yet_Documented
        @param iosock           Not_Yet_Documented
        @param iosock_descrip   Not_Yet_Documented
        @param handlercpp       Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @param perm             Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Socket (Stream*              iosock,
                         char *               iosock_descrip,
                         SocketHandlercpp     handlercpp,
                         char *               handler_descrip,
                         Service*             s,
                         DCpermission         perm = ALLOW);

    /** Not_Yet_Documented
        @param iosock           Not_Yet_Documented
        @param descrip          Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Command_Socket (Stream*      iosock,
                                 char *       descrip = NULL ) {
        return Register_Socket (iosock,
                                descrip,
                                (SocketHandler)NULL,
                                (SocketHandlercpp)NULL,
                                "DC Command Handler",
                                NULL,
                                ALLOW,
                                0); 
    }

    /** Not_Yet_Documented
        @param insock           Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Cancel_Socket ( Stream * insock );

    /** Not_Yet_Documented
        @param insock           Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Lookup_Socket ( Stream * iosock );
	//@}

	/** @name Timer events.
	 */
	//@{
    /** Not_Yet_Documented
        @param deltawhen       Not_Yet_Documented
        @param event           Not_Yet_Documented
        @param event_descrip   Not_Yet_Documented
        @param s               Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Timer (unsigned     deltawhen,
                        Event        event,
                        char *       event_descrip, 
                        Service*     s = NULL);
    
    /** Not_Yet_Documented
        @param deltawhen       Not_Yet_Documented
        @param period          Not_Yet_Documented
        @param event           Not_Yet_Documented
        @param event_descrip   Not_Yet_Documented
        @param s               Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Timer (unsigned     deltawhen,
                        unsigned     period,
                        Event        event,
                        char *       event_descrip,
                        Service*     s = NULL);

    /** Not_Yet_Documented
        @param deltawhen       Not_Yet_Documented
        @param event           Not_Yet_Documented
        @param event_descrip   Not_Yet_Documented
        @param s               Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Timer (unsigned   deltawhen,
                        Eventcpp   event,
                        char *     event_descrip,
                        Service*   s);

    /** Not_Yet_Documented
        @param deltawhen       Not_Yet_Documented
        @param period          Not_Yet_Documented
        @param event           Not_Yet_Documented
        @param event_descrip   Not_Yet_Documented
        @param s               Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Timer (unsigned  deltawhen,
                        unsigned  period,
                        Eventcpp  event,
                        char *    event_descrip,
                        Service * s);

    /** Not_Yet_Documented
        @param id The timer's ID
        @return Not_Yet_Documented
    */
    int Cancel_Timer ( int id );

    /** Not_Yet_Documented
        @param id The timer's ID
        @param when   Not_Yet_Documented
        @param period Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Reset_Timer ( int id, unsigned when, unsigned period = 0 );
	//@}

    /** Not_Yet_Documented
        @param flag   Not_Yet_Documented
        @param indent Not_Yet_Documented
    */
    void Dump (int flag, char* indent = NULL );


    /** @name Process management.
        These work on any process, not just daemon core processes.
    */
    //@{

    /** Not_Yet_Documented
        @return Not_Yet_Documented
    */
    inline pid_t getpid() { return mypid; };

    /** Not_Yet_Documented
        @return Not_Yet_Documented
    */
    inline pid_t getppid() { return ppid; };

    /** Not_Yet_Documented
        @param pid Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Is_Pid_Alive (pid_t pid);

    /** Not_Yet_Documented
        @param pid Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Shutdown_Fast(pid_t pid);

    /** Not_Yet_Documented
        @param pid Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Shutdown_Graceful(pid_t pid);

    /** Not_Yet_Documented
        @param pid Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Suspend_Process(pid_t pid);

    /** Not_Yet_Documented
        @param pid Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Continue_Process(pid_t pid);

    /** Create a process.  Works for NT and Unix.  On Unix, a
        fork and exec are done.  Read the source for ugly 
        details - it takes care of most everything.

        @param name The full path name of the executable.  If this 
               is a relative path name AND cwd is specified, then we
               prepend the result of getcwd() to 'name' and pass 
               that to exec().
        @param args The list of args, separated by spaces.  The 
               first arg is argv[0], the name you want to appear in 
               'ps'.  If you don't specify agrs, then 'name' is 
               used as argv[0].
        @param priv The priv state to change into right before
               the exec.  Default = no action.
        @param reaper_id The reaper number to use.  Default = 1.
        @param want_command_port Well, do you?  Default = TRUE
        @param env A colon-separated list of stuff to be put into
               the environment of the new process
        @param cwd Current Working Directory
        @param new_process_group Do you want to make one? Default = FALSE
        @param sock_inherit_list A list of socks to inherit.
        @param std An array of three file descriptors to map
               to stdin, stdout, stderr respectively.  If this array
               is NULL, don't perform remapping.  If any one of these
			   is -1, it is ignored and *not* mapped.
        @param nice_inc The value to be passed to nice() in the
               child.  0 < nice < 20, and greater numbers mean
               less priority.  This is an addition to the current
               nice value.
        @return On success, returns the child pid.  On failure, returns FALSE.
    */
    int Create_Process (
        char        *name,
        char        *args,
        priv_state  priv                 = PRIV_UNKNOWN,
        int         reaper_id            = 1,
        int         want_commanand_port  = TRUE,
        char        *env                 = NULL,
        char        *cwd                 = NULL,
        int         new_process_group    = FALSE,
        Stream      *sock_inherit_list[] = NULL,
        int         std[]                = NULL,
        int         nice_inc             = 0
        );

    //@}

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
    KeyCache* getKeyCache();
	//@}

 	/** @name Environment management.
	 */
	//@{
    /** Put the {key, value} pair into the environment
        @param key
        @param value
        @return Not_Yet_Documented
    */
    int SetEnv(char *key, char *value);

    /** Put env_var into the environment
        @param env_var Desired string to go into environment;
        must be of the form 'name=value' 
    */
    int SetEnv(char *env_var);
	//@}
     
	/** @name Thread management.
	 */
	//@{
		/** Create a "poor man's" thread.  Works for NT (via
			CreateThread) and Unix (via fork).  The new thread does
			not have a DaemonCore command socket.
			@param start_func Function to execute in the thread.
			   This function must not access any shared memory.
			   The only DaemonCore command which may be used in this
			   thread is Send_Signal.
			   When the function returns, the thread exits and the reaper
			   is called in the parent, giving the return value as
			   the thread exit status.
			   The function must take a single argument of type
			   (void *) and return an int.
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
			@return The tid of the newly created thread.
		*/
	int Create_Thread(
		ThreadStartFunc	start_func,
		void			*arg = NULL,
		Stream			*sock = NULL,
		int				reaper_id = 1
		);

	///
	int Suspend_Thread(int tid);

	///
	int Continue_Thread(int tid);

	///
	int Kill_Thread(int tid);
	//@}

    Stream **GetInheritedSocks() { return (Stream **)inheritedSocks; }


		/** Register the Priv state this Daemon wants to be in, every time
		    DaemonCore is returned to the Handler DaemonCore will check to see
			that this is the current priv state.
			@param priv_state The new priv state you want to be in. DaemonCore
			initializes this to be CONDOR_PRIV.
			@return The priv state you were in before.
		*/
	priv_state Register_Priv_State( priv_state priv );
	
		/** Initialize our array of StringLists that we use to verify
			if a given request to set a configuration variable with
			condor_config_val should be allowed.
		*/
	void InitSettableAttrsLists( void );

		/** Check the table of attributes we're willing to allow users
			at hosts of different permission levels to change to see
			if we should allow the given request to succeed.
			@param config String containing the configuration request
			@param sock The sock that we're handling this command with
			@return true if we should allow this, false if not
		*/
	bool CheckConfigSecurity( const char* config, Sock* sock );


    /** Invalidate all session related cache since the configuration
     */
    void invalidateSessionCache();

  private:      

	ReliSock* dc_rsock;	// tcp command socket
	SafeSock* dc_ssock;	// udp command socket

    int getAuthBitmask( char* methods );

    void Inherit( void );  // called in main()
	void InitCommandSocket( int command_port );  // called in main()

    int HandleSigCommand(int command, Stream* stream);
    int HandleReq(int socki);
    int HandleSig(int command, int sig);

    int HandleProcessExitCommand(int command, Stream* stream);
    int HandleProcessExit(pid_t pid, int exit_status);
    int HandleDC_SIGCHLD(int sig);
	int HandleDC_SERVICEWAITPIDS(int sig);
 
    int Register_Command(int command, char *com_descip,
                         CommandHandler handler, 
                         CommandHandlercpp handlercpp,
                         char *handler_descrip,
                         Service* s, 
                         DCpermission perm,
                         int dprintf_flag,
                         int is_cpp);

    int Register_Signal(int sig,
                        char *sig_descip,
                        SignalHandler handler, 
                        SignalHandlercpp handlercpp,
                        char *handler_descrip,
                        Service* s, 
                        DCpermission perm,
                        int is_cpp);

    int Register_Socket(Stream* iosock,
                        char *iosock_descrip,
                        SocketHandler handler, 
                        SocketHandlercpp handlercpp,
                        char *handler_descrip,
                        Service* s, 
                        DCpermission perm,
                        int is_cpp);

    int Register_Reaper(int rid,
                        char *reap_descip,
                        ReaperHandler handler, 
                        ReaperHandlercpp handlercpp,
                        char *handler_descrip,
                        Service* s, 
                        int is_cpp);

	MyString DaemonCore::GetCommandsInAuthLevel(DCpermission perm);

    struct CommandEnt
    {
        int             num;
        CommandHandler  handler;
        CommandHandlercpp   handlercpp;
        int             is_cpp;
        DCpermission    perm;
        Service*        service; 
        char*           command_descrip;
        char*           handler_descrip;
        void*           data_ptr;
        int             dprintf_flag;
    };

    void                DumpCommandTable(int, const char* = NULL);
    int                 maxCommand;     // max number of command handlers
    int                 nCommand;       // number of command handlers used
    CommandEnt*         comTable;       // command table

    struct SignalEnt 
    {
        int             num;
        SignalHandler   handler;
        SignalHandlercpp    handlercpp;
        int             is_cpp;
        DCpermission    perm;
        Service*        service; 
        int             is_blocked;
        // Note: is_pending must be volatile because it could be set inside
        // of a Unix asynchronous signal handler (such as SIGCHLD).
        volatile int    is_pending; 
        char*           sig_descrip;
        char*           handler_descrip;
        void*           data_ptr;
    };
    void                DumpSigTable(int, const char* = NULL);
    int                 maxSig;      // max number of signal handlers
    int                 nSig;        // number of signal handlers used
    SignalEnt*          sigTable;    // signal table
    volatile int        sent_signal; // TRUE if a signal handler sends a signal

    struct SockEnt
    {
        Stream*         iosock;
        SOCKET          sockd;
        SocketHandler   handler;
        SocketHandlercpp    handlercpp;
        int             is_cpp;
        DCpermission    perm;
        Service*        service; 
        char*           iosock_descrip;
        char*           handler_descrip;
        void*           data_ptr;
		bool			is_connect_pending;
		bool			call_handler;
    };
    void              DumpSocketTable(int, const char* = NULL);
    int               maxSocket;  // number of socket handlers to start with
    int               nSock;      // number of socket handlers used
    ExtArray<SockEnt> *sockTable; // socket table; grows dynamically if needed
    int               initial_command_sock;  

    struct ReapEnt
    {
        int             num;
        ReaperHandler   handler;
        ReaperHandlercpp    handlercpp;
        int             is_cpp;
        Service*        service; 
        char*           reap_descrip;
        char*           handler_descrip;
        void*           data_ptr;
    };
    void                DumpReapTable(int, const char* = NULL);
    int                 maxReap;        // max number of reaper handlers
    int                 nReap;          // number of reaper handlers used
    ReapEnt*            reapTable;      // reaper table
    int                 defaultReaper;

    struct PidEntry
    {
        pid_t pid;
        int new_process_group;
#ifdef WIN32
        HANDLE hProcess;
        HANDLE hThread;
        DWORD tid;
        HWND hWnd;
#endif
        char sinful_string[28];
        char parent_sinful_string[28];
        int is_local;
        int parent_is_local;
        int reaper_id;
        int hung_tid;   // Timer to detect hung processes
        int was_not_responding;
    };
    typedef HashTable <pid_t, PidEntry *> PidHashTable;
    PidHashTable* pidTable;
    pid_t mypid;
    pid_t ppid;

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

    List<PidWatcherEntry> PidWatcherList;

    int                 WatchPid(PidEntry *pidentry);
#endif
            
    static              TimerManager t;
    void                DumpTimerList(int, char* = NULL);

	SecMan	    		*sec_man;


    IpVerify            ipverify;   

    void free_descrip(char *p) { if(p &&  p != EMPTY_DESCRIP) free(p); }
    
    fd_set              readfds,writefds,exceptfds; 

    struct in_addr      negotiator_sin_addr;    // used by Verify method

#ifdef WIN32
    // the thread id of the thread running the main daemon core
    DWORD   dcmainThreadId;
    CSysinfo ntsysinfo;     // class to do undocumented NT process management

    // set files inheritable or not on NT
    int SetFDInheritFlag(int fd, int flag);
#endif

#ifndef WIN32
    int async_pipe[2];  // 0 for reading, 1 for writing
    volatile int async_sigs_unblocked;

	// Data memebers for queuing up waitpid() events
	struct WaitpidEntry_s
	{
		pid_t child_pid;
		int exit_status;
		bool operator==( const struct WaitpidEntry_s &target) { 
			if(child_pid == target.child_pid) {
				return true;
			}
			return false;
		}

	};
	typedef struct WaitpidEntry_s WaitpidEntry;
	Queue<WaitpidEntry> WaitpidQueue;

#endif

    Stream *inheritedSocks[MAX_SOCKS_INHERITED+1];

    // methods to detect and kill hung processes
    int HandleChildAliveCommand(int command, Stream* stream);
    int HungChildTimeout();
    int SendAliveToParent();
    unsigned int max_hang_time;
    int send_child_alive_timer;

	// misc helper functions
	void CheckPrivState( void );

    // these need to be in thread local storage someday
    void **curr_dataptr;
    void **curr_regdataptr;
    int inServiceCommandSocket_flag;
    // end of thread local storage
        
#ifdef WIN32
    static char *ParseEnvArgsString(char *env, char *inheritbuf);
#else
    static char **ParseEnvArgsString(char *env, bool env);
#endif

	priv_state Default_Priv_State;

	bool InitSettableAttrsList( const char* subsys, int i );
	StringList* SettableAttrsLists[LAST_PERM];
};

#ifndef _NO_EXTERN_DAEMON_CORE

/** Function to call when you want your daemon to really exit.
    This allows DaemonCore a chance to clean up.  This function should be
    used in place of the standard <tt>exit()</tt> function!
    @param status The return status upon exit.  The value you would normally
           pass to the regular <tt>exit()</tt> function if you weren't using
           Daemon Core.
*/
extern void DC_Exit( int status );  

/** The main DaemonCore object.  This pointer will be automatically instatiated
    for you.  A perfect place to use it would be in your main_init, to access
    Daemon Core's wonderful services, like <tt>Register_Signal()</tt> or
    <tt>Register_Timer()</tt>.
*/
extern DaemonCore* daemonCore;
#endif

#define MAX_INHERIT_SOCKS 10
#define _INHERITBUF_MAXSIZE 500


#endif /* _CONDOR_DAEMON_CORE_H_ */
