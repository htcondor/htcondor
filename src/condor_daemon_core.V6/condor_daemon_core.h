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

#ifndef WIN32
#if defined(Solaris)
#define __EXTENSIONS__
#endif
#include <sys/types.h>
#include <sys/time.h>
#include "condor_fdset.h"
#if defined(Solaris)
#undef __EXTENSIONS__
#endif
#endif  /* ifndef WIN32 */

#include "condor_uid.h"
#include "condor_io.h"
#include "condor_timer_manager.h"
#include "condor_commands.h"

// enum for Daemon Core socket/command/signal permissions
enum DCpermission { ALLOW, READ, WRITE, NEGOTIATOR };

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

// defines for signals; compatibility with traditional UNIX values maintained
#define	DC_SIGHUP	1	/* hangup */
#define	DC_SIGINT	2	/* interrupt (rubout) */
#define	DC_SIGQUIT	3	/* quit (ASCII FS) */
#define	DC_SIGILL	4	/* illegal instruction (not reset when caught) */
#define	DC_SIGTRAP	5	/* trace trap (not reset when caught) */
#define	DC_SIGIOT	6	/* IOT instruction */
#define	DC_SIGABRT 6	/* used by abort, replace DC_SIGIOT in the future */
#define	DC_SIGEMT	7	/* EMT instruction */
#define	DC_SIGFPE	8	/* floating point exception */
#define	DC_SIGKILL	9	/* kill (cannot be caught or ignored) */
#define	DC_SIGBUS	10	/* bus error */
#define	DC_SIGSEGV	11	/* segmentation violation */
#define	DC_SIGSYS	12	/* bad argument to system call */
#define	DC_SIGPIPE	13	/* write on a pipe with no one to read it */
#define	DC_SIGALRM	14	/* alarm clock */
#define	DC_SIGTERM	15	/* software termination signal from kill */
#define	DC_SIGUSR1	16	/* user defined signal 1 */
#define	DC_SIGUSR2	17	/* user defined signal 2 */
#define	DC_SIGCLD	18	/* child status change */
#define	DC_SIGCHLD	18	/* child status change alias (POSIX) */
#define	DC_SIGPWR	19	/* power-fail restart */
#define	DC_SIGWINCH 20	/* window size change */
#define	DC_SIGURG	21	/* urgent socket condition */
#define	DC_SIGPOLL 22	/* pollable event occured */
#define	DC_SIGIO	DC_SIGPOLL	/* socket I/O possible (DC_SIGPOLL alias) */
#define	DC_SIGSTOP 23	/* stop (cannot be caught or ignored) */
#define	DC_SIGTSTP 24	/* user stop requested from tty */
#define	DC_SIGCONT 25	/* stopped process has been continued */
#define	DC_SIGTTIN 26	/* background tty read attempted */
#define	DC_SIGTTOU 27	/* background tty write attempted */
#define	DC_SIGVTALRM 28	/* virtual timer expired */
#define	DC_SIGPROF 29	/* profiling timer expired */
#define	DC_SIGXCPU 30	/* exceeded cpu limit */
#define	DC_SIGXFSZ 31	/* exceeded file size limit */
#define	DC_SIGWAITING 32	/* process's lwps are blocked */
#define	DC_SIGLWP	33	/* special signal used by thread library */
#define	DC_SIGFREEZE 34	/* special signal used by CPR */
#define	DC_SIGTHAW 35	/* special signal used by CPR */
#define	DC_SIGCANCEL 36	/* thread cancellation signal used by libthread */


class DaemonCore : public Service
{
	friend class TimerManager;
	
	public:
		DaemonCore(int ComSize = 0, int SigSize = 0, int SocSize = 0);
		~DaemonCore();

		void	Driver();
		

		
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
			
		int		Register_Socket(Stream* iosock, char *iosock_descrip, SocketHandler handler,
					char *handler_descrip, Service* s = NULL, DCpermission perm = ALLOW);
		int		Register_Socket(Stream* iosock, char *iosock_descrip, SocketHandlercpp handlercpp,
					char *handler_descrip, Service* s, DCpermission perm = ALLOW);
		int		Register_Command_Socket( Stream* iosock, char *descrip = NULL ) {
					return(Register_Socket(iosock,descrip,(SocketHandler)NULL,(SocketHandlercpp)NULL,"DC Command Handler",NULL,ALLOW,0)); 
				}
		int		Cancel_Socket( Stream* );

		int		Register_Timer(unsigned deltawhen, Event event, char *event_descrip, 
					Service* s = NULL, int id = -1);
		int		Register_Timer(unsigned deltawhen, unsigned period, Event event, 
					char *event_descrip, Service* s = NULL, int id = -1);
		int		Register_Timer(unsigned deltawhen, Eventcpp event, char *event_descrip, 
					Service* s, int id = -1);
		int		Register_Timer(unsigned deltawhen, unsigned period, Eventcpp event, 
					char *event_descrip, Service* s, int id = -1);
		int		Cancel_Timer( int id );

		void	Dump(int, char* = NULL );

		inline int getpid() { return 0; };

		int		Send_Signal(int pid, int sig);

		int		SetDataPtr( void * );
		int		Register_DataPtr( void * );
		void	*GetDataPtr();
		
#ifdef FUTURE		
		int		Block_Signal()
		int		Unblock_Signal()
		int		Register_Reaper(Service* s, ReaperHandler handler);
		int		Create_Process()
		int		Create_Thread()
		int		Kill_Process()
		int		Kill_Thread()
#endif

		int		HandleSigCommand(int command, Stream* stream);
		
	private:

		void	HandleReq(int socki);

		int		HandleSig(int command, int sig);

		int		Register_Command(int command, char *com_descip, CommandHandler handler, 
					CommandHandlercpp handlercpp, char *handler_descrip, Service* s, 
					DCpermission perm, int is_cpp);
		int		Register_Signal(int sig, char *sig_descip, SignalHandler handler, 
					SignalHandlercpp handlercpp, char *handler_descrip, Service* s, 
					DCpermission perm, int is_cpp);
		int		Register_Socket(Stream* iosock, char *iosock_descrip, SocketHandler handler, 
					SocketHandlercpp handlercpp, char *handler_descrip, Service* s, 
					DCpermission perm, int is_cpp);

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
			int				is_pending;
			char*			sig_descrip;
			char*			handler_descrip;
			void*			data_ptr;
		};
		void				DumpSigTable(int, const char* = NULL);
		int					maxSig;		// max number of signal handlers
		int					nSig;		// number of signal handlers used
		SignalEnt*			sigTable;		// signal table
		int					sent_signal;	// TRUE if a signal handler sends a signal

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

		static				TimerManager t;
		void				DumpTimerList(int, char* = NULL);

		void				free_descrip(char *p) { if(p &&  p != EMPTY_DESCRIP) free(p); }
	
		fd_set				readfds; 

		// these need to be in thread local storage someday
		void **curr_dataptr;
		void **curr_regdataptr;
		// end of thread local storage
};

#ifndef _NO_EXTERN_DAEMON_CORE
extern DaemonCore* daemonCore;
#endif


#endif
