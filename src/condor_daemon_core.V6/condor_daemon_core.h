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
////////////////////////////////////////////////////////////////////////////////
//
// This file contains the definition for class DaemonCore. This is the
// central structure for every daemon in condor. The daemon core triggers
// preregistered handlers for corresponding events. class Service is the base
// class of the classes that daemon_core can serve. In order to use a class
// with the DaemonCore, it has to be a derived class of Service.
//
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _CONDOR_DAEMON_CORE_H_
#define _CONDOR_DAEMON_CORE_H_

#include "condor_common.h"
#include "condor_uid.h"
#include "condor_io.h"
#include "condor_timer_manager.h"
#include "condor_ipverify.h"
#include "condor_commands.h"
#include "HashTable.h"
#include "list.h"

#if defined(GSS_AUTHENTICATION)
#include "auth_sock.h"
#else
#define AuthSock ReliSock
#endif

static const int KEEP_STREAM = 100;
static char* EMPTY_DESCRIP = "<NULL>";

// typedefs for callback procedures
typedef int		(*CommandHandler)(Service*,int,Stream*);
typedef int		(Service::*CommandHandlercpp)(int,Stream*);

typedef int		(*SignalHandler)(Service*,int);
typedef int		(Service::*SignalHandlercpp)(int);

typedef int		(*SocketHandler)(Service*,Stream*);
typedef int		(Service::*SocketHandlercpp)(Stream*);

typedef int		(*ReaperHandler)(Service*,int pid,int exit_status);
typedef int		(Service::*ReaperHandlercpp)(int pid,int exit_status);

// other typedefs and macros needed on WIN32
#ifdef WIN32
typedef DWORD pid_t;
#define WIFEXITED(stat) ((int)(1))
#define WEXITSTATUS(stat) ((int)(stat))
#define WIFSIGNALED(stat) ((int)(0))
#define WTERMSIG(stat) ((int)(0))
#endif  // of ifdef WIN32

// some constants for HandleSig().
#define _DC_RAISESIGNAL 1
#define _DC_BLOCKSIGNAL 2
#define _DC_UNBLOCKSIGNAL 3

// If WANT_DC_PM is defined, it means we want DaemonCore Process Management.
// We _always_ want it on WinNT; on Unix, some daemons still do their own 
// Process Management (just until we get around to changing them to use daemon core).
#if defined(WIN32) && !defined(WANT_DC_PM)
#define WANT_DC_PM
#endif

class DaemonCore : public Service
{
	friend class TimerManager; 
#ifdef WIN32
	friend dc_main( int argc, char** argv );
	friend DWORD pidWatcherThread(void*);	
#else
	friend main(int, char**);
#endif

	public:
		DaemonCore(int PidSize = 0, int ComSize = 0, int SigSize = 0, int SocSize = 0, int ReapSize = 0);
		~DaemonCore();

		void	Driver();

		int		ReInit();

		int		Verify(DCpermission perm, const struct sockaddr_in *sin );
		
		int		Register_Command(int command, char *com_descrip, CommandHandler handler, 
					char *handler_descrip, Service* s = NULL, DCpermission perm = ALLOW);
		int		Register_Command(int command, char *com_descript, CommandHandlercpp handlercpp, 
					char *handler_descrip, Service* s, DCpermission perm = ALLOW);
		int		Cancel_Command( int command );
		int		InfoCommandPort();
		char*	InfoCommandSinfulString();

		int		Register_Signal(int sig, char *sig_descrip, SignalHandler handler, 
					char *handler_descrip, Service* s = NULL, DCpermission perm = ALLOW);
		int		Register_Signal(int sig, char *sig_descript, SignalHandlercpp handlercpp, 
					char *handler_descrip, Service* s, DCpermission perm = ALLOW);
		int		Cancel_Signal( int sig );
		int		Block_Signal(int sig) { return(HandleSig(_DC_BLOCKSIGNAL,sig)); }
		int		Unblock_Signal(int sig) { return(HandleSig(_DC_UNBLOCKSIGNAL,sig)); }

		int		Register_Reaper(char *reap_descrip, ReaperHandler handler, 
					char *handler_descrip, Service* s = NULL);
		int		Register_Reaper(char *reap_descript, ReaperHandlercpp handlercpp, 
					char *handler_descrip, Service* s);
		int		Reset_Reaper(int rid, char *reap_descrip, ReaperHandler handler, 
					char *handler_descrip, Service* s = NULL);
		int		Reset_Reaper(int rid, char *reap_descript, ReaperHandlercpp handlercpp, 
					char *handler_descrip, Service* s);
		int		Cancel_Reaper( int rid );
		
		int		Register_Socket(Stream* iosock, char *iosock_descrip, SocketHandler handler,
					char *handler_descrip, Service* s = NULL, DCpermission perm = ALLOW);
		int		Register_Socket(Stream* iosock, char *iosock_descrip, SocketHandlercpp handlercpp,
					char *handler_descrip, Service* s, DCpermission perm = ALLOW);
		int		Register_Command_Socket( Stream* iosock, char *descrip = NULL ) {
					return(Register_Socket(iosock,descrip,(SocketHandler)NULL,(SocketHandlercpp)NULL,"DC Command Handler",NULL,ALLOW,0)); 
				}
		int		Cancel_Socket( Stream* );

		int		Lookup_Socket( Stream *iosock );

		int		Register_Timer(unsigned deltawhen, Event event, char *event_descrip, 
					Service* s = NULL);
		int		Register_Timer(unsigned deltawhen, unsigned period, Event event, 
					char *event_descrip, Service* s = NULL);
		int		Register_Timer(unsigned deltawhen, Eventcpp event, char *event_descrip, 
					Service* s);
		int		Register_Timer(unsigned deltawhen, unsigned period, Eventcpp event, 
					char *event_descrip, Service* s);
		int		Cancel_Timer( int id );
		int		Reset_Timer( int id, unsigned when, unsigned period = 0 );

		void	Dump(int, char* = NULL );

		inline pid_t getpid() { return mypid; };
		inline pid_t getppid() { return ppid; };

		int		Send_Signal(pid_t pid, int sig);

		int		SetDataPtr( void * );
		int		Register_DataPtr( void * );
		void	*GetDataPtr();
		
		int		Create_Process(
			char		*name,
			char		*args,
			priv_state	condor_priv = PRIV_UNKNOWN,
			int			reaper_id = 1,
			int			want_commanand_port = TRUE,
			char		*env = NULL,
			char		*cwd = NULL,
		//	unsigned int std[3] = { 0, 0, 0 },
			int			new_process_group = FALSE,
			Stream		*sock_inherit_list[] = NULL 			
			);

#ifdef FUTURE		
		int		Create_Thread()
		int		Kill_Process()
		int		Kill_Thread()
#endif


		
	private:
		int		HandleSigCommand(int command, Stream* stream);
		int		HandleReq(int socki);
		int		HandleSig(int command, int sig);
		void	Inherit( AuthSock* &rsock, SafeSock* &ssock );  // called in main()
		int		HandleProcessExitCommand(int command, Stream* stream);
		int		HandleProcessExit(pid_t pid, int exit_status);

		int		Register_Command(int command, char *com_descip, CommandHandler handler, 
					CommandHandlercpp handlercpp, char *handler_descrip, Service* s, 
					DCpermission perm, int is_cpp);
		int		Register_Signal(int sig, char *sig_descip, SignalHandler handler, 
					SignalHandlercpp handlercpp, char *handler_descrip, Service* s, 
					DCpermission perm, int is_cpp);
		int		Register_Socket(Stream* iosock, char *iosock_descrip, SocketHandler handler, 
					SocketHandlercpp handlercpp, char *handler_descrip, Service* s, 
					DCpermission perm, int is_cpp);
		int		Register_Reaper(int rid, char *reap_descip, ReaperHandler handler, 
					ReaperHandlercpp handlercpp, char *handler_descrip, Service* s, 
					int is_cpp);


		struct CommandEnt
		{
		    int				num;
		    CommandHandler	handler;
			CommandHandlercpp	handlercpp;
			int				is_cpp;
			DCpermission	perm;
			Service*		service; 
			char*			command_descrip;
			char*			handler_descrip;
			void*			data_ptr;
		};
		void				DumpCommandTable(int, const char* = NULL);
		int					maxCommand;		// max number of command handlers
		int					nCommand;		// number of command handlers used
		CommandEnt*			comTable;		// command table

		struct SignalEnt 
		{
			int				num;
		    SignalHandler	handler;
			SignalHandlercpp	handlercpp;
			int				is_cpp;
			DCpermission	perm;
			Service*		service; 
			int				is_blocked;
			// Note: is_pending must be volatile because it could be set inside
			// of a Unix asynchronous signal handler (such as SIGCHLD).
			volatile int	is_pending;	
			char*			sig_descrip;
			char*			handler_descrip;
			void*			data_ptr;
		};
		void				DumpSigTable(int, const char* = NULL);
		int					maxSig;		// max number of signal handlers
		int					nSig;		// number of signal handlers used
		SignalEnt*			sigTable;		// signal table
		volatile int		sent_signal;	// TRUE if a signal handler sends a signal

		struct SockEnt
		{
		    Stream*			iosock;
			SOCKET			sockd;
		    SocketHandler	handler;
			SocketHandlercpp	handlercpp;
			int				is_cpp;
			DCpermission	perm;
			Service*		service; 
			char*			iosock_descrip;
			char*			handler_descrip;
			void*			data_ptr;
		};
		void				DumpSocketTable(int, const char* = NULL);
		int					maxSocket;		// max number of socket handlers
		int					nSock;		// number of socket handlers used
		SockEnt*			sockTable;		// socket table
		int					initial_command_sock;  

		struct ReapEnt
		{
		    int				num;
		    ReaperHandler	handler;
			ReaperHandlercpp	handlercpp;
			int				is_cpp;
			Service*		service; 
			char*			reap_descrip;
			char*			handler_descrip;
			void*			data_ptr;
		};
		void				DumpReapTable(int, const char* = NULL);
		int					maxReap;		// max number of reaper handlers
		int					nReap;			// number of reaper handlers used
		ReapEnt*			reapTable;		// reaper table

		struct PidEntry
		{
			pid_t pid;
#ifdef WIN32
			HANDLE hProcess;
			HANDLE hThread;
#endif
			char sinful_string[28];
			char parent_sinful_string[28];
			int is_local;
			int parent_is_local;
			int	reaper_id;
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

		int					WatchPid(PidEntry *pidentry);
#endif
			
		static				TimerManager t;
		void				DumpTimerList(int, char* = NULL);

		IpVerify			ipverify;	

		void				free_descrip(char *p) { if(p &&  p != EMPTY_DESCRIP) free(p); }
	
		fd_set				readfds; 

		struct in_addr		negotiator_sin_addr;	// used by Verify method

		int					reinit_timer_id;

#ifdef WIN32
		DWORD	dcmainThreadId;		// the thread id of the thread running the main daemon core
#endif

#ifndef WIN32
		int async_pipe[2];  // 0 for reading, 1 for writing
		volatile int async_sigs_unblocked;
#endif

		// these need to be in thread local storage someday
		void **curr_dataptr;
		void **curr_regdataptr;
		// end of thread local storage
};

#ifndef _NO_EXTERN_DAEMON_CORE

	// Function to call when you want your daemon to really exit.
	// This allows DaemonCore a chance to clean up.
extern void DC_Exit( int status );	

extern DaemonCore* daemonCore;
#endif

#define MAX_INHERIT_SOCKS 10
#define _INHERITBUF_MAXSIZE 500


#endif /* _CONDOR_DAEMON_CORE_H_ */
