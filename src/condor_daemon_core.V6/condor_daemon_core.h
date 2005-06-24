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
#include "condor_ipverify.h"
#include "condor_commands.h"
#include "command_strings.h"
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
#include "self_monitor.h"
#include "stdsoap2.h"
#include "condor_pidenvid.h"


#define DEBUG_SETTABLE_ATTR_LISTS 0

static const int KEEP_STREAM = 100;
static const int CLOSE_STREAM = 101;
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
typedef int     (*PipeHandler)(Service*,int);

///
typedef int     (Service::*PipeHandlercpp)(int);

///
typedef int     (*ReaperHandler)(Service*,int pid,int exit_status);

///
typedef int     (Service::*ReaperHandlercpp)(int pid,int exit_status);

///
typedef int		(*ThreadStartFunc)(void *,Stream*);

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
const int DCJOBOPT_NO_ENV_INHERIT   = (1<<2);

#define HAS_DCJOBOPT_SUSPEND_ON_EXEC(mask)  ((mask)&DCJOBOPT_SUSPEND_ON_EXEC)
#define HAS_DCJOBOPT_NO_ENV_INHERIT(mask)  ((mask)&DCJOBOPT_NO_ENV_INHERIT)
#define HAS_DCJOBOPT_ENV_INHERIT(mask)  (!(HAS_DCJOBOPT_NO_ENV_INHERIT(mask)))


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
  friend DWORD WINAPI FindWinstaThread( LPVOID lpParam );
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
                int SocSize = 0, int ReapSize = 0, int PipeSize = 0);
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
        @return -1 if iosock is NULL, -2 is reregister, 0 or above on success
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
        @return -1 if iosock is NULL, -2 is reregister, 0 or above on success
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
        @return -1 if iosock is NULL, -2 is reregister, 0 or above on success
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

	/// Cancel and close all registed sockets.
	int Cancel_And_Close_All_Sockets(void);


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
        @param pipefd           Not_Yet_Documented
        @param iosock_descrip   Not_Yet_Documented
        @param handler          Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @param perm             Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Pipe (int		           pipefd,
                         char *            iosock_descrip,
                         PipeHandler       handler,
                         char *            handler_descrip,
                         Service *         s                = NULL,
                         HandlerType       handler_type     = HANDLE_READ,    
                         DCpermission      perm             = ALLOW);

    /** Not_Yet_Documented
        @param pipefd           Not_Yet_Documented
        @param iosock_descrip   Not_Yet_Documented
        @param handlercpp       Not_Yet_Documented
        @param handler_descrip  Not_Yet_Documented
        @param s                Not_Yet_Documented
        @param perm             Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Register_Pipe (int					  pipefd,
                         char *               iosock_descrip,
                         PipeHandlercpp       handlercpp,
                         char *               handler_descrip,
                         Service*             s,
                         HandlerType          handler_type = HANDLE_READ,    
                         DCpermission         perm = ALLOW);


    /** Not_Yet_Documented
        @param pipefd           Not_Yet_Documented
        @return Not_Yet_Documented
    */
    int Cancel_Pipe ( int pipefd );

	/// Cancel and close all registed pipes.
	int Cancel_And_Close_All_Pipes(void);

	/** Create an anonymous pipe.
	*/
	int Create_Pipe( int *pipefds, bool nonblocking_read = false, 
		bool nonblocking_write = false, unsigned int psize = 0);

	/** Close an anonymous pipe.  This function will also call 
	 * Cancel_Pipe() on behalf of the caller if the pipefd had
	 * been registed via Register_Pipe().
	*/
	int Close_Pipe(int piepfd);

	/** Close a pipe stream 
	*/
	int Close_Pipe(FILE *pipefp);

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
        @param want_command_port Well, do you?  Default = TRUE.  If
			   want_command_port it TRUE, the child process will be
			   born with a daemonCore command socket on a dynamic port.
			   If it is FALSE, the child process will not have a command
			   socket.  If it is any integer > 1, then it will have a
			   command socket on a port well-known port 
			   specified by want_command_port.
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
        @param job_opt_mask A bitmask which defines other optional
               behavior we might for this process.  The bits are
               defined above, and always begin with "DCJOBOPT".  In
               addition, each bit should also define a macro to test a
               mask if a given bit is set ("HAS_DCJOBOPT_...")
		@param fd_inherit_list An array of fds which you want the child to
		       inherit. The last element must be 0.
        @return On success, returns the child pid.  On failure, returns FALSE.
    */
    int Create_Process (
        const char  *name,
        const char  *args,
        priv_state  priv                 = PRIV_UNKNOWN,
        int         reaper_id            = 1,
        int         want_commanand_port  = TRUE,
        const char  *env                 = NULL,
        const char  *cwd                 = NULL,
        int         new_process_group    = FALSE,
        Stream      *sock_inherit_list[] = NULL,
        int         std[]                = NULL,
        int         nice_inc             = 0,
        int         job_opt_mask         = 0,
		int			fd_inherit_list[]	 = NULL
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

	///
	int Suspend_Thread(int tid);

	///
	int Continue_Thread(int tid);

	///
	int Kill_Thread(int tid);
	//@}

	/** Public method to allow things that fork() themselves without
		using Create_Thread() to set the magic DC variable so that our
		version of exit() uses exec() instead of exit() and we don't
		call destructors when the child exits.
	*/
	void Forked_Child_Wants_Exit_By_Exec( bool exit_by_exec );

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
	bool CheckConfigAttrSecurity( const char* attr, Sock* sock );


    /** Invalidate all session related cache since the configuration
     */
    void invalidateSessionCache();

	/* manage the secret cookie data */
	bool set_cookie( int len, unsigned char* data );
	bool get_cookie( int &len, unsigned char* &data );
	bool cookie_is_valid( unsigned char* data );

	/** The peaceful shutdown toggle controls whether graceful shutdown
	   avoids any hard killing.
	*/
	bool GetPeacefulShutdown();
	void SetPeacefulShutdown(bool value);

	/** Disable all daemon core callbacks for duration seconds, except for the
		processing of SOAP calls.
		@param seconds The number of seconds to only permit SOAP callbacks
	*/

	void Only_Allow_Soap(int duration);
	
    SelfMonitorData monitor_data;

  private:      
  	
	ReliSock* dc_rsock;	// tcp command socket
	SafeSock* dc_ssock;	// udp command socket

    void Inherit( void );  // called in main()
	void InitCommandSocket( int command_port );  // called in main()
	void InitHTTPSocket( int http_port );

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

    int Register_Pipe(int pipefd,
                        char *pipefd_descrip,
                        PipeHandler handler, 
                        PipeHandlercpp handlercpp,
                        char *handler_descrip,
                        Service* s, 
					    HandlerType handler_type, 
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
	
	int				  initial_http_sock;
	struct soap		  soap;
	time_t			  only_allow_soap;

	struct PidEntry;  // forward reference
    struct PipeEnt
    {
        int				pipefd;
        PipeHandler	   handler;
        PipeHandlercpp    handlercpp;
        int             is_cpp;
        DCpermission    perm;
        Service*        service; 
        char*           pipe_descrip;
        char*           handler_descrip;
        void*           data_ptr;
		bool			call_handler;
		PidEntry*		pentry;
		HandlerType		handler_type;
		bool			in_handler;
    };
    // void              DumpPipeTable(int, const char* = NULL);
    int               maxPipe;  // number of pipe handlers to start with
    int               nPipe;      // number of pipe handlers used
    ExtArray<PipeEnt> *pipeTable; // pipe table; grows dynamically if needed

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
		HANDLE hPipe;
		LONG pipeReady;
		LONG deallocate;
		HANDLE watcherEvent;
#endif
        char sinful_string[28];
        char parent_sinful_string[28];
        int is_local;
        int parent_is_local;
        int reaper_id;
        int hung_tid;   // Timer to detect hung processes
        int was_not_responding;

		/* the environment variables which allow me the track the pidfamily
			of this pid (where applicable) */
		PidEnvID penvid;
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

	int					_cookie_len, _cookie_len_old;
	unsigned char		*_cookie_data, *_cookie_data_old;

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
        
#ifndef WIN32
    static char **ParseArgsString(const char *env);
#endif

	priv_state Default_Priv_State;

	bool InitSettableAttrsList( const char* subsys, int i );
	StringList* SettableAttrsLists[LAST_PERM];

	bool peaceful_shutdown;
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

/** Call this function (inside your main_pre_dc_init() function) to
    bypass the authorization initialization in daemoncore.  This is for
    programs, such as condor_dagman, that are Condor daemons but should
    run with the user's credentials.
*/
extern void DC_Skip_Auth_Init();

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

// Prototype to get sinful string.
char *global_dc_sinful( void );

#endif /* _CONDOR_DAEMON_CORE_H_ */
