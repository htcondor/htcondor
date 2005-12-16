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
// //////////////////////////////////////////////////////////////////////
//
// Implementation of DaemonCore.
//
//
// //////////////////////////////////////////////////////////////////////

#include "condor_common.h"
#if HAVE_EXT_GCB
extern "C" {
void Generic_stop_logging();
}
#endif

static const int DEFAULT_MAXCOMMANDS = 255;
static const int DEFAULT_MAXSIGNALS = 99;
static const int DEFAULT_MAXSOCKETS = 8;
static const int DEFAULT_MAXPIPES = 8;
static const int DEFAULT_MAXREAPS = 100;
static const int DEFAULT_PIDBUCKETS = 11;
static const int ERRNO_EXEC_AS_ROOT = 666666;
static const int ERRNO_PID_COLLISION = 666667;
static const int DEFAULT_MAX_PID_COLLISIONS = 9;
static const char* DEFAULT_INDENT = "DaemonCore--> ";
static const int MAX_TIME_SKIP = (60*20); //20 minutes


#include "authentication.h"
#include "daemon.h"
#include "reli_sock.h"
#include "condor_daemon_core.h"
#include "condor_io.h"
#include "internet.h"
#include "KeyCache.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include "my_hostname.h"
#include "condor_commands.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "strupr.h"
#include "sig_name.h"
#include "env.h"
#include "condor_secman.h"
#include "condor_distribution.h"
#include "condor_environ.h"
#include "condor_version.h"
#include "setenv.h"
#ifdef WIN32
#include "exphnd.WIN32.h"
#include "condor_fix_assert.h"
typedef unsigned (__stdcall *CRT_THREAD_HANDLER) (void *);
CRITICAL_SECTION Big_fat_mutex; // coarse grained mutex for debugging purposes
#endif
#include "directory.h"
#include "../condor_io/condor_rw.h"
#include "httpget.h"

#ifdef COMPILE_SOAP_SSL
#include "MapFile.h"
#endif

// Make this the last include to fix assert problems on Win32 -- see
// the comments about assert at the end of condor_debug.h to understand
// why.
// DO NOT include any system header files after this!
#include "condor_debug.h"

#if 0
	// Lord help us -- here we define some CRT internal data structure info.
	// If you compile Condor NT with anything other than VC++ 6.0, you
	// need to check the C runtime library (CRT) source to make certain the below
	// still makes sense (in particular, the ioinfo struct).  In the CRT,
	// look at INTERNAL.H and MSDOS.H.  Good Luck.
	typedef struct {
			long osfhnd;    /* underlying OS file HANDLE */
			char osfile;    /* attributes of file (e.g., open in text mode?) */
			char pipech;    /* one char buffer for handles opened on pipes */
			#ifdef _MT
			int lockinitflag;
			CRITICAL_SECTION lock;
			#endif  /* _MT */
		}   ioinfo;
	#define IOINFO_L2E          5
	#define IOINFO_ARRAY_ELTS   (1 << IOINFO_L2E)
	#define IOINFO_ARRAYS       64
	#define _pioinfo(i) ( __pioinfo[(i) >> IOINFO_L2E] + ((i) & (IOINFO_ARRAY_ELTS - \
								  1)) )
	#define _osfile(i)  ( _pioinfo(i)->osfile )
	#define _pipech(i)  ( _pioinfo(i)->pipech )
	extern _CRTIMP ioinfo * __pioinfo[];
	extern int _nhandle;
	#define FOPEN           0x01    /* file handle open */
	#define FPIPE           0x08    /* file handle refers to a pipe */
	#define FDEV            0x40    /* file handle refers to device */
	extern void __cdecl _lock_fhandle(int);
	extern void __cdecl _unlock_fhandle(int);
#endif

// We should only need to include the libTDP header once
// the library is made portable. For now, the TDP process
// control stuff is in here
#define TDP 1
#if defined( LINUX ) && defined( TDP )

#include <sys/ptrace.h>
#include <sys/wait.h>

int
tdp_wait_stopped_child (pid_t pid)
{

    int wait_val;

    if (waitpid(pid, &wait_val, 0) == -1) {
	dprintf(D_ALWAYS,"Wait for Stopped Child wait failed: %d (%s) \n", errno, strerror (errno));
	return -1;
    }

    if(!WIFSTOPPED(wait_val)) {
	return -1;  /* Something went wrong with application exec. */
    }

    if (kill(pid, SIGSTOP) < 0) {
	dprintf(D_ALWAYS, "Wait for Stopped Child kill failed: %d (%s) \n", errno, strerror(errno));
	return -1;
    }

    if (ptrace(PTRACE_DETACH, pid, 0, 0) < 0) {
	dprintf(D_ALWAYS, "Wait for Stopped Child detach failed: %d (%s) \n", errno, strerror(errno));
	return -1;
    }

    return 0;
}

#endif /* LINUX && TDP */

#define SECURITY_HACK_ENABLE
void zz2printf(KeyInfo *k) {
	if (k) {
		char hexout[260];  // holds (at least) a 128 byte key.
		const unsigned char* dataptr = k->getKeyData();
		int   length  =  k->getKeyLength();

		for (int i = 0; (i < length) && (i < 24); i++) {
			sprintf (&hexout[i*2], "%02x", *dataptr++);
		}

    	dprintf (D_SECURITY, "ZKM: [%i] %s\n", length, hexout);
	}
}

static unsigned int ZZZZZ = 0;
int ZZZ_always_increase() {
	return ZZZZZ++;
}

static int _condor_exit_with_exec = 0;

extern int soap_serve(struct soap*);
extern int LockFd;
extern void drop_addr_file( void );
extern char* mySubSystem;	// the subsys ID, such as SCHEDD

TimerManager DaemonCore::t;

// Hash function for pid table.
static int compute_pid_hash(const pid_t &key, int numBuckets)
{
	return ( key % numBuckets );
}


// DaemonCore constructor.

DaemonCore::DaemonCore(int PidSize, int ComSize,int SigSize,
				int SocSize,int ReapSize,int PipeSize)
{

	if(ComSize < 0 || SigSize < 0 || SocSize < 0 || PidSize < 0)
	{
		EXCEPT("Invalid argument(s) for DaemonCore constructor");
	}

	if ( PidSize == 0 )
		PidSize = DEFAULT_PIDBUCKETS;
	pidTable = new PidHashTable(PidSize, compute_pid_hash);
	ppid = 0;
#ifdef WIN32
	// init the mutex
	InitializeCriticalSection(&Big_fat_mutex);
	EnterCriticalSection(&Big_fat_mutex);

	mypid = ::GetCurrentProcessId();
#else
	mypid = ::getpid();
#endif

	maxCommand = ComSize;
	maxSig = SigSize;
	maxSocket = SocSize;
	maxReap = ReapSize;
	maxPipe = PipeSize;

	if(maxCommand == 0)
		maxCommand = DEFAULT_MAXCOMMANDS;

	comTable = new CommandEnt[maxCommand];
	if(comTable == NULL) {
		EXCEPT("Out of memory!");
	}
	nCommand = 0;
	memset(comTable,'\0',maxCommand*sizeof(CommandEnt));

	if(maxSig == 0)
		maxSig = DEFAULT_MAXSIGNALS;

	sigTable = new SignalEnt[maxSig];
	if(sigTable == NULL)
	{
		EXCEPT("Out of memory!");
	}
	nSig = 0;
	memset(sigTable,'\0',maxSig*sizeof(SignalEnt));

	if(maxSocket == 0)
		maxSocket = DEFAULT_MAXSOCKETS;

	sec_man = new SecMan();

	sockTable = new ExtArray<SockEnt>(maxSocket);
	if(sockTable == NULL)
	{
		EXCEPT("Out of memory!");
	}
	nSock = 0;
	SockEnt blankSockEnt;
	memset(&blankSockEnt,'\0',sizeof(SockEnt));
	sockTable->fill(blankSockEnt);

	initial_command_sock = -1;
	soap_ssl_sock = -1;

	if(maxPipe == 0)
		maxPipe = DEFAULT_MAXPIPES;

	pipeTable = new ExtArray<PipeEnt>(maxPipe);
	if(pipeTable == NULL)
	{
		EXCEPT("Out of memory!");
	}
	nPipe = 0;
	PipeEnt blankPipeEnt;
	memset(&blankPipeEnt,'\0',sizeof(PipeEnt));
	pipeTable->fill(blankPipeEnt);

	if(maxReap == 0)
		maxReap = DEFAULT_MAXREAPS;

	reapTable = new ReapEnt[maxReap];
	if(reapTable == NULL)
	{
		EXCEPT("Out of memory!");
	}
	nReap = 0;
	memset(reapTable,'\0',maxReap*sizeof(ReapEnt));
	defaultReaper=-1;

	curr_dataptr = NULL;
	curr_regdataptr = NULL;

	send_child_alive_timer = -1;

#ifdef WIN32
	dcmainThreadId = ::GetCurrentThreadId();
#endif

#ifndef WIN32
	async_sigs_unblocked = FALSE;
#endif
	async_pipe_empty = TRUE;

	dc_rsock = NULL;
	dc_ssock = NULL;

	inheritedSocks[0] = NULL;
	inServiceCommandSocket_flag = FALSE;

		// Initialize our array of StringLists used to authorize
		// condor_config_val -set and friends.
	int i;
	for( i=0; i<LAST_PERM; i++ ) {
		SettableAttrsLists[i] = NULL;
	}

	Default_Priv_State = PRIV_CONDOR;

	_cookie_len_old  = _cookie_len  = 0;
	_cookie_data_old = _cookie_data = NULL;

	peaceful_shutdown = false;

	only_allow_soap = 0;
}

// DaemonCore destructor. Delete the all the various handler tables, plus
// delete/free any pointers in those tables.
DaemonCore::~DaemonCore()
{
	int		i;

#ifndef WIN32
	close(async_pipe[1]);
	close(async_pipe[0]);
#endif

	if (comTable != NULL )
	{
		for (i=0;i<maxCommand;i++) {
			free_descrip( comTable[i].command_descrip );
			free_descrip( comTable[i].handler_descrip );
		}
		delete []comTable;
	}

	if (sigTable != NULL)
	{
		for (i=0;i<maxSig;i++) {
			free_descrip( sigTable[i].sig_descrip );
			free_descrip( sigTable[i].handler_descrip );
		}
		delete []sigTable;
	}

	if (sockTable != NULL)
	{

			// There may be CEDAR objects stored in the table, but we
			// don't want to delete them here.  People who register
			// sockets in our table have to be responsible for
			// cleaning up after themselves.  "He who creates should
			// delete", otherwise the socket(s) may get deleted
			// multiple times.  The only things we created are the UDP
			// and TCP command sockets, but we'll delete those down
			// below, so we just need to delete the table entries
			// themselves, not the CEDAR objects.  Origional wisdom by
			// Todd, cleanup of DC command sockets by Derek on 2/26/01

		for (i=0;i<nSock;i++) {
			free_descrip( (*sockTable)[i].iosock_descrip );
			free_descrip( (*sockTable)[i].handler_descrip );
		}
		delete sockTable;
	}

	if (sec_man) {
		// the reference counting in sec_man is currently disabled,
		// so we need to clean up after it quite explicitly.  ZKM.
		KeyCache * tmp_kt = sec_man->session_cache;
		HashTable<MyString,MyString>* tmp_cm = sec_man->command_map;

		delete sec_man;
		delete tmp_kt;
		delete tmp_cm;
	}

		// Since we created these, we need to clean them up.
	if( dc_rsock ) {
		delete dc_rsock;
	}
	if( dc_ssock ) {
		delete dc_ssock;
	}

	if (reapTable != NULL)
	{
		for (i=0;i<maxReap;i++) {
			free_descrip( reapTable[i].reap_descrip );
			free_descrip( reapTable[i].handler_descrip );
		}
		delete []reapTable;
	}

	// Delete all entries from the pidTable, and the table itself
	PidEntry* pid_entry;
	pidTable->startIterations();
	while (pidTable->iterate(pid_entry))
	{
		if ( pid_entry ) delete pid_entry;
	}
	delete pidTable;

	for( i=0; i<LAST_PERM; i++ ) {
		if( SettableAttrsLists[i] ) {
			delete SettableAttrsLists[i];
		}
	}

	if( pipeTable ) {
		delete( pipeTable );
	}
	t.CancelAllTimers();

	if (_cookie_data) {
		free(_cookie_data);
	}
	if (_cookie_data_old) {
		free(_cookie_data_old);
	}

#ifdef WIN32
	 DeleteCriticalSection(&Big_fat_mutex);
#endif
}

void DaemonCore::Set_Default_Reaper( int reaper_id )
{
	defaultReaper = reaper_id;
}

/********************************************************
 Here are a bunch of public methods with parameter overloading.
 These methods here just call the actual method implementation with a
 default parameter set.
 ********************************************************/
int	DaemonCore::Register_Command(int command, char* com_descrip,
				CommandHandler handler, char* handler_descrip, Service* s,
				DCpermission perm, int dprintf_flag)
{
	return( Register_Command(command, com_descrip, handler,
							(CommandHandlercpp)NULL, handler_descrip, s,
							perm, dprintf_flag, FALSE) );
}

int	DaemonCore::Register_Command(int command, char *com_descrip,
				CommandHandlercpp handlercpp, char* handler_descrip,
				Service* s, DCpermission perm, int dprintf_flag)
{
	return( Register_Command(command, com_descrip, NULL, handlercpp,
							handler_descrip, s, perm, dprintf_flag, TRUE) );
}

int	DaemonCore::Register_Signal(int sig, char* sig_descrip,
				SignalHandler handler, char* handler_descrip,
				Service* s, DCpermission perm)
{
	return( Register_Signal(sig, sig_descrip, handler,
							(SignalHandlercpp)NULL, handler_descrip, s,
							perm, FALSE) );
}

int	DaemonCore::Register_Signal(int sig, char *sig_descrip,
				SignalHandlercpp handlercpp, char* handler_descrip,
				Service* s, DCpermission perm)
{
	return( Register_Signal(sig, sig_descrip, NULL, handlercpp,
							handler_descrip, s, perm, TRUE) );
}

int	DaemonCore::Register_Socket(Stream* iosock, char* iosock_descrip,
				SocketHandler handler, char* handler_descrip,
				Service* s, DCpermission perm)
{
	return( Register_Socket(iosock, iosock_descrip, handler,
							(SocketHandlercpp)NULL, handler_descrip, s,
							perm, FALSE) );
}

int	DaemonCore::Register_Socket(Stream* iosock, char* iosock_descrip,
				SocketHandlercpp handlercpp, char* handler_descrip,
				Service* s, DCpermission perm)
{
	return( Register_Socket(iosock, iosock_descrip, NULL, handlercpp,
							handler_descrip, s, perm, TRUE) );
}

int	DaemonCore::Register_Pipe(int pipefd, char* pipefd_descrip,
				PipeHandler handler, char* handler_descrip,
				Service* s, HandlerType handler_type, DCpermission perm)
{
	return( Register_Pipe(pipefd, pipefd_descrip, handler,
							(PipeHandlercpp)NULL, handler_descrip, s,
							handler_type, perm, FALSE) );
}

int	DaemonCore::Register_Pipe(int pipefd, char* piepfd_descrip,
				PipeHandlercpp handlercpp, char* handler_descrip,
				Service* s, HandlerType handler_type, DCpermission perm)
{
	return( Register_Pipe(pipefd, piepfd_descrip, NULL, handlercpp,
							handler_descrip, s, handler_type, perm, TRUE) );
}

int	DaemonCore::Register_Reaper(char* reap_descrip, ReaperHandler handler,
				char* handler_descrip, Service* s)
{
	return( Register_Reaper(-1, reap_descrip, handler,
							(ReaperHandlercpp)NULL, handler_descrip,
							s, FALSE) );
}

int	DaemonCore::Register_Reaper(char* reap_descrip,
				ReaperHandlercpp handlercpp, char* handler_descrip, Service* s)
{
	return( Register_Reaper(-1, reap_descrip, NULL, handlercpp,
							handler_descrip, s, TRUE) );
}

int	DaemonCore::Reset_Reaper(int rid, char* reap_descrip,
				ReaperHandler handler, char* handler_descrip, Service* s)
{
	return( Register_Reaper(rid, reap_descrip, handler,
							(ReaperHandlercpp)NULL, handler_descrip,
							s, FALSE) );
}

int	DaemonCore::Reset_Reaper(int rid, char* reap_descrip,
				ReaperHandlercpp handlercpp, char* handler_descrip, Service* s)
{
	return( Register_Reaper(rid, reap_descrip, NULL, handlercpp,
							handler_descrip, s, TRUE) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, Event event,
				char *event_descrip, Service* s)
{
	return( t.NewTimer(s, deltawhen, event, event_descrip, 0, -1) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, unsigned period,
				Event event, char *event_descrip, Service* s)
{
	return( t.NewTimer(s, deltawhen, event, event_descrip, period, -1) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, Eventcpp eventcpp,
				char *event_descrip, Service* s)
{
	return( t.NewTimer(s, deltawhen, eventcpp, event_descrip, 0, -1) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, unsigned period,
				Eventcpp event, char *event_descrip, Service* s )
{
	return( t.NewTimer(s, deltawhen, event, event_descrip, period, -1) );
}

int	DaemonCore::Cancel_Timer( int id )
{
	return( t.CancelTimer(id) );
}

int DaemonCore::Reset_Timer( int id, unsigned when, unsigned period )
{
	return( t.ResetTimer(id,when,period) );
}

/************************************************************************/


int DaemonCore::Register_Command(int command, char* command_descrip,
				CommandHandler handler, CommandHandlercpp handlercpp,
				char *handler_descrip, Service* s, DCpermission perm,
				int dprintf_flag, int is_cpp)
{
    int     i;		// hash value
    int     j;		// for linear probing

    if( handler == 0 && handlercpp == 0 ) {
		dprintf(D_DAEMONCORE, "Can't register NULL command handler\n");
		return -1;
    }

    if(nCommand >= maxCommand) {
		EXCEPT("# of command handlers exceeded specified maximum");
    }

	// We want to allow "command" to be a negative integer, so
	// be careful about sign when computing our simple hash value
    if(command < 0) {
        i = -command % maxCommand;
    } else {
        i = command % maxCommand;
    }

	// See if our hash landed on an empty bucket...
    if ( (comTable[i].handler) || (comTable[i].handlercpp) ) {
		// occupied
        if(comTable[i].num == command) {
			// by the same signal
			EXCEPT("DaemonCore: Same command registered twice");
        }
		// by some other signal, so scan thru the entries to
		// find the first empty one
        for(j = (i + 1) % maxCommand; j != i; j = (j + 1) % maxCommand) {
            if( (comTable[j].handler == 0) && (comTable[j].handlercpp == 0) )
            {
				i = j;
				break;
            }
        }
    }

	// Found a blank entry at index i. Now add in the new data.
	comTable[i].num = command;
	comTable[i].handler = handler;
	comTable[i].handlercpp = handlercpp;
	comTable[i].is_cpp = is_cpp;
	comTable[i].perm = perm;
	comTable[i].service = s;
	comTable[i].data_ptr = NULL;
	comTable[i].dprintf_flag = dprintf_flag;
	free_descrip(comTable[i].command_descrip);
	if ( command_descrip )
		comTable[i].command_descrip = strdup(command_descrip);
	else
		comTable[i].command_descrip = EMPTY_DESCRIP;
	free_descrip(comTable[i].handler_descrip);
	if ( handler_descrip )
		comTable[i].handler_descrip = strdup(handler_descrip);
	else
		comTable[i].handler_descrip = EMPTY_DESCRIP;

	// Increment the counter of total number of entries
	nCommand++;

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &(comTable[i].data_ptr);

	// Conditionally dump what our table looks like
	DumpCommandTable(D_FULLDEBUG | D_DAEMONCORE);

	return(command);
}

int DaemonCore::Cancel_Command( int )
{
	// stub

	return TRUE;
}

int DaemonCore::InfoCommandPort()
{
	if ( initial_command_sock == -1 ) {
		// there is no command sock!
		return -1;
	}

	// this will return a -1 on error
	return( ((Sock*)((*sockTable)[initial_command_sock].iosock))->get_port() );
}

// NOTE: InfoCommandSinfulString always returns a pointer to a _static_ buffer!
// This means you'd better copy or strdup the result if you expect it to never
// change on you.  Plus, realize static buffers aren't exactly thread safe!
char * DaemonCore::InfoCommandSinfulString(int pid)
{
	static char *myown_sinful_string = NULL;
	static char somepid_sinful_string[28];

	// if pid is -1, we want info on our own process, else we want info
	// on a process created with Create_Process().
	if ( pid == -1 ) {
		if ( initial_command_sock == -1 ) {
			// there is no command sock!
			return NULL;
		}

		if ( myown_sinful_string == NULL ) {
			myown_sinful_string = strdup(
				sock_to_string( (*sockTable)[initial_command_sock].sockd ) );
		}

		return myown_sinful_string;
	} else {
		PidEntry *pidinfo = NULL;
		if ((pidTable->lookup(pid, pidinfo) < 0)) {
			// we have no information on this pid
			return NULL;
		}
		if ( pidinfo->sinful_string[0] == '\0' ) {
			// this pid is apparently not a daemon core process
			return NULL;
		}
		strncpy(somepid_sinful_string,pidinfo->sinful_string,
			sizeof(somepid_sinful_string) );
		return somepid_sinful_string;
	}
}

// Lookup the environment id set for a particular pid, or if -1 then the
// getpid() in question.  Returns penvid or NULL of can't be found.
PidEnvID* DaemonCore::InfoEnvironmentID(PidEnvID *penvid, int pid)
{
	extern char **environ;

	if (penvid == NULL) {
		return NULL;
	}

	/* just in case... */
	pidenvid_init(penvid);

	/* handle the base case of my own pid */
	if ( pid == -1 ) {

		if (pidenvid_filter_and_insert(penvid, environ) == 
			PIDENVID_OVERSIZED)
		{
			EXCEPT( "DaemonCore::InfoEnvironmentID: Programmer error. "
				"Tried to overstuff a PidEntryID array.\n" );
		}

	} else {

		// If someone else was asked for, give them the info for that pid.
		PidEntry *pidinfo = NULL;
		if ((pidTable->lookup(pid, pidinfo) < 0)) {
			// we have no information on this pid
			return NULL;
		}

		// copy over the information to the passed in array
		pidenvid_copy(penvid, &pidinfo->penvid);
	}

	return penvid;
}

int DaemonCore::Register_Signal(int sig, char* sig_descrip, 
				SignalHandler handler, SignalHandlercpp handlercpp, 
				char* handler_descrip, Service* s,	DCpermission perm, 
				int is_cpp)
{
    int     i;		// hash value
    int     j;		// for linear probing


    if( handler == 0 && handlercpp == 0 ) {
		dprintf(D_DAEMONCORE, "Can't register NULL signal handler\n");
		return -1;
    }

	// Semantics dictate that certain signals CANNOT be caught!
	// In addition, allow SIGCHLD to be automatically replaced (for backwards
	// compatibility), so cancel any previous registration for SIGCHLD.
	switch (sig) {
		case SIGKILL:
		case SIGSTOP:
		case SIGCONT:
			EXCEPT("Trying to Register_Signal for sig %d which cannot be caught!",sig);
			break;
		case SIGCHLD:
			Cancel_Signal(SIGCHLD);
			break;
		default:
			break;
	}

    if(nSig >= maxSig) {
		EXCEPT("# of signal handlers exceeded specified maximum");
    }

	// We want to allow "command" to be a negative integer, so
	// be careful about sign when computing our simple hash value
    if(sig < 0) {
        i = -sig % maxSig;
    } else {
        i = sig % maxSig;
    }

	// See if our hash landed on an empty bucket...  We identify an empty
	// bucket by checking of there is a handler (or a c++ handler) defined;
	// if there is no handler, then it is an empty entry.
    if( sigTable[i].handler || sigTable[i].handlercpp ) {
		// occupied...
        if(sigTable[i].num == sig) {
			// by the same signal
			EXCEPT("DaemonCore: Same signal registered twice");
        }
		// by some other signal, so scan thru the entries to
		// find the first empty one
        for(j = (i + 1) % maxSig; j != i; j = (j + 1) % maxSig) {
            if( (sigTable[j].handler == 0) && (sigTable[j].handlercpp == 0) )
            {
				i = j;
				break;
            }
        }
    }

	// Found a blank entry at index i. Now add in the new data.
	sigTable[i].num = sig;
	sigTable[i].handler = handler;
	sigTable[i].handlercpp = handlercpp;
	sigTable[i].is_cpp = is_cpp;
	sigTable[i].perm = perm;
	sigTable[i].service = s;
	sigTable[i].is_blocked = FALSE;
	sigTable[i].is_pending = FALSE;
	free_descrip(sigTable[i].sig_descrip);
	if ( sig_descrip )
		sigTable[i].sig_descrip = strdup(sig_descrip);
	else
		sigTable[i].sig_descrip = EMPTY_DESCRIP;
	free_descrip(sigTable[i].handler_descrip);
	if ( handler_descrip )
		sigTable[i].handler_descrip = strdup(handler_descrip);
	else
		sigTable[i].handler_descrip = EMPTY_DESCRIP;

	// Increment the counter of total number of entries
	nSig++;

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &(sigTable[i].data_ptr);

	// Conditionally dump what our table looks like
	DumpSigTable(D_FULLDEBUG | D_DAEMONCORE);

	return sig;
}

int DaemonCore::Cancel_Signal( int sig )
{
	int i,j;
	int found = -1;

	// We want to allow "command" to be a negative integer, so
	// be careful about sign when computing our simple hash value
    if(sig < 0) {
        i = -sig % maxSig;
    } else {
        i = sig % maxSig;
    }

	// find this signal in our table
	j = i;
	do {
		if ( (sigTable[j].num == sig) &&
			 ( sigTable[j].handler || sigTable[j].handlercpp ) ) {
			found = j;
		} else {
			j = (j + 1) % maxSig;
		}
	} while ( j != i && found == -1 );

	// Check if found
	if ( found == -1 ) {
		dprintf(D_DAEMONCORE,"Cancel_Signal: signal %d not found\n",sig);
		return FALSE;
	}

	// Clear entry
	sigTable[found].num = 0;
	sigTable[found].handler = NULL;
	sigTable[found].handlercpp = (SignalHandlercpp)NULL;
	free_descrip( sigTable[found].handler_descrip );
	sigTable[found].handler_descrip = NULL;

	// Decrement the counter of total number of entries
	nSig--;

	// Clear any data_ptr which go to this entry we just removed
	if ( curr_regdataptr == &(sigTable[found].data_ptr) )
		curr_regdataptr = NULL;
	if ( curr_dataptr == &(sigTable[found].data_ptr) )
		curr_dataptr = NULL;

	// Log a message and conditionally dump what our table now looks like
	dprintf(D_DAEMONCORE,
					"Cancel_Signal: cancelled signal %d <%s>\n",
					sig,sigTable[found].sig_descrip);
	free_descrip( sigTable[found].sig_descrip );
	sigTable[found].sig_descrip = NULL;

	DumpSigTable(D_FULLDEBUG | D_DAEMONCORE);

	return TRUE;
}

int DaemonCore::Register_Socket(Stream *iosock, char* iosock_descrip,
				SocketHandler handler, SocketHandlercpp handlercpp,
				char *handler_descrip, Service* s, DCpermission perm,
				int is_cpp)
{
    int     i;
    int     j;

    // In sockTable, unlike the others handler tables, we allow for a NULL
	// handler and a NULL handlercpp - this means a command socket, so use
	// the default daemon core socket handler which strips off the command.
	// SO, a blank table entry is defined as a NULL iosock field.

	// And since FD_ISSET only allows us to probe, we do not bother using a
	// hash table for sockets.  We simply store them in an array.

    if ( !iosock ) {
		dprintf(D_DAEMONCORE, "Can't register NULL socket \n");
		return -1;
    }

	i = nSock;

	// Make certain that entry i is empty.
	if ( (*sockTable)[i].iosock ) {
        dprintf ( D_ALWAYS, "Socket table fubar.  nSock = %d\n", nSock );
        DumpSocketTable( D_ALWAYS );
		EXCEPT("DaemonCore: Socket table messed up");
	}

	// Verify that this socket has not already been registered
	int fd_to_register = ((Sock *)iosock)->get_file_desc();
	for ( j=0; j < nSock; j++ )
	{
		bool duplicate_found = false;

		if ( (*sockTable)[j].iosock == iosock ) {
			duplicate_found = true;
        }

		if ( (*sockTable)[j].iosock ) { 	// if valid entry
			if ( ((Sock *)(*sockTable)[j].iosock)->get_file_desc() ==
								fd_to_register ) {
				duplicate_found = true;
			}
		}

		if (duplicate_found) {
			dprintf(D_ALWAYS, "DaemonCore: Attempt to register socket twice\n");

			return -2;
		}
	}

	// Found a blank entry at index i. Now add in the new data.
	(*sockTable)[i].call_handler = false;
	(*sockTable)[i].iosock = iosock;
	switch ( iosock->type() ) {
		case Stream::reli_sock :
			(*sockTable)[i].sockd = ((ReliSock *)iosock)->get_file_desc();
			(*sockTable)[i].is_connect_pending =
								((ReliSock *)iosock)->is_connect_pending();
			break;
		case Stream::safe_sock :
			(*sockTable)[i].sockd = ((SafeSock *)iosock)->get_file_desc();
				// SafeSock connect never blocks....
			(*sockTable)[i].is_connect_pending = false;
			break;
		default:
			EXCEPT("Adding CEDAR socket of unknown type\n");
			break;
	}
	(*sockTable)[i].handler = handler;
	(*sockTable)[i].handlercpp = handlercpp;
	(*sockTable)[i].is_cpp = is_cpp;
	(*sockTable)[i].perm = perm;
	(*sockTable)[i].service = s;
	(*sockTable)[i].data_ptr = NULL;
	free_descrip((*sockTable)[i].iosock_descrip);
	if ( iosock_descrip )
		(*sockTable)[i].iosock_descrip = strdup(iosock_descrip);
	else
		(*sockTable)[i].iosock_descrip = EMPTY_DESCRIP;
	free_descrip((*sockTable)[i].handler_descrip);
	if ( handler_descrip )
		(*sockTable)[i].handler_descrip = strdup(handler_descrip);
	else
		(*sockTable)[i].handler_descrip = EMPTY_DESCRIP;

	// Increment the counter of total number of entries
	nSock++;

	// If this is the first command sock, set initial_command_sock
	// NOTE: When we remove sockets, the intial_command_sock can change!
	if ( initial_command_sock == -1 && handler == 0 && handlercpp == 0 )
		initial_command_sock = i;

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &((*sockTable)[i].data_ptr);

	// Conditionally dump what our table looks like
	DumpSocketTable(D_FULLDEBUG | D_DAEMONCORE);

	return i;
}


int
DaemonCore::Cancel_And_Close_All_Sockets(void)
{
	// This method will cancel *and delete* all registered sockets.
	// It will return the number of sockets cancelled + closed.
	int i = 0;

	while ( nSock > 0 ) {
		if ( (*sockTable)[0].iosock ) {	// if a valid entry....
			Stream* insock = (*sockTable)[0].iosock;
				// Note:  calling Cancel_Socket will decrement
				// variable nSock (number of registered Sockets)
				// by one.
			Cancel_Socket( insock );
			delete insock;
			if( insock == (Stream*)dc_rsock ) {
				dc_rsock = NULL;
			}
			if( insock == (Stream*)dc_ssock ) {
				dc_ssock = NULL;
			}
			i++;
		}
	}

	return i;
}


int
DaemonCore::Cancel_And_Close_All_Pipes(void)
{
	// This method will cancel *and delete* all registered pipes.
	// It will return the number of pipes cancelled + closed.
	int i = 0;

	while ( nPipe > 0 ) {
		if ( (*pipeTable)[0].pipefd ) {	// if a valid entry....
				// Note:  calling Close_Pipe will decrement
				// variable nPipe (number of registered Sockets)
				// by one.
			Close_Pipe( (*pipeTable)[0].pipefd );
			i++;
		}
	}

	return i;
}



int DaemonCore::Cancel_Socket( Stream* insock)
{
	int i,j;

	i = -1;
	for (j=0;j<nSock;j++) {
		if ( (*sockTable)[j].iosock == insock ) {
			i = j;
			break;
		}
	}

	if ( i == -1 ) {
		dprintf( D_ALWAYS,"Cancel_Socket: called on non-registered socket!\n");
		dprintf( D_ALWAYS,"Offending socket number %d\n", i );
		DumpSocketTable( D_DAEMONCORE );
		return FALSE;
	}

	// Remove entry at index i by moving the last one in the table here.

	// Clear any data_ptr which go to this entry we just removed
	if ( curr_regdataptr == &( (*sockTable)[i].data_ptr) )
		curr_regdataptr = NULL;
	if ( curr_dataptr == &( (*sockTable)[i].data_ptr) )
		curr_dataptr = NULL;

	// Log a message
	dprintf(D_DAEMONCORE,"Cancel_Socket: cancelled socket %d <%s> %p\n",
			i,(*sockTable)[i].iosock_descrip, (*sockTable)[i].iosock );

	// Remove entry, move the last one in the list into this spot
	(*sockTable)[i].iosock = NULL;
	free_descrip( (*sockTable)[i].iosock_descrip );
	(*sockTable)[i].iosock_descrip = NULL;
	free_descrip( (*sockTable)[i].handler_descrip );
	(*sockTable)[i].handler_descrip = NULL;
	if ( i < nSock - 1 ) {
            // if not the last entry in the table, move the last one here
		(*sockTable)[i] = (*sockTable)[nSock - 1];
		(*sockTable)[nSock - 1].iosock = NULL;
		(*sockTable)[nSock - 1].iosock_descrip = NULL;
		(*sockTable)[nSock - 1].handler_descrip = NULL;
	}
	nSock--;

	DumpSocketTable(D_FULLDEBUG | D_DAEMONCORE);

	return TRUE;
}


int DaemonCore::Register_Pipe(int pipefd, char* pipefd_descrip,
				PipeHandler handler, PipeHandlercpp handlercpp,
				char *handler_descrip, Service* s,
				HandlerType handler_type, DCpermission perm,
				int is_cpp)
{
    int     i;
    int     j;

	// Since FD_ISSET only allows us to probe, we do not bother using a
	// hash table for pipes.  We simply store them in an array.

	// ckireyev: this used to be:
	// if (pipefd < 1)
    //	 why? what about stdin(0)?
    if ( pipefd < 0 ) {
		dprintf(D_DAEMONCORE, "Register_Pipe: invalid pipefd \n");
		return -1;
    }

#if 0
	_lock_fhandle(pipefd);
	if ( ((unsigned)pipefd >= (unsigned)_nhandle) ||
			!(_osfile(pipefd) & FOPEN) ||
			!(_osfile(pipefd) & (FPIPE|FDEV)) )
	{
		dprintf(D_DAEMONCORE, "Register_Pipe: invalid pipefd \n");
		_unlock_fhandle(pipefd);
		return -1;
	}
	_unlock_fhandle(pipefd);
#endif

#ifdef WIN32
	HANDLE dupped_pipe_handle;
	if ( _get_osfhandle(pipefd) == -1L ) {
		dprintf(D_DAEMONCORE, "Register_Pipe: invalid pipefd \n");
		return -1;
    }
	if (!DuplicateHandle(GetCurrentProcess(),
        (HANDLE)_get_osfhandle(pipefd),
        GetCurrentProcess(),
        (HANDLE*)&dupped_pipe_handle,
        0,
        0, // inheritable flag
        DUPLICATE_SAME_ACCESS)) {
			// failed to duplicate
			dprintf(D_ALWAYS,
				"ERROR: DuplicateHandle() failed in Register_Pipe\n");
			return -1;
	}
#endif

	i = nPipe;

	// Make certain that entry i is empty.
	if ( (*pipeTable)[i].pipefd ) {
        EXCEPT("Pipe table fubar!  nPipe = %d\n", nPipe );
	}

	// Verify that this piepfd has not already been registered
	for ( j=0; j < nPipe; j++ )
	{
		if ( (*pipeTable)[j].pipefd == pipefd ) {
			EXCEPT("DaemonCore: Same pipe registered twice");
        }
	}

	// Found a blank entry at index i. Now add in the new data.
	(*pipeTable)[i].pentry = NULL;
	(*pipeTable)[i].call_handler = false;
	(*pipeTable)[i].in_handler = false;
	(*pipeTable)[i].pipefd = pipefd;
	(*pipeTable)[i].handler = handler;
	(*pipeTable)[i].handler_type = handler_type;
	(*pipeTable)[i].handlercpp = handlercpp;
	(*pipeTable)[i].is_cpp = is_cpp;
	(*pipeTable)[i].perm = perm;
	(*pipeTable)[i].service = s;
	(*pipeTable)[i].data_ptr = NULL;
	free_descrip((*pipeTable)[i].pipe_descrip);
	if ( pipefd_descrip )
		(*pipeTable)[i].pipe_descrip = strdup(pipefd_descrip);
	else
		(*pipeTable)[i].pipe_descrip = EMPTY_DESCRIP;
	free_descrip((*pipeTable)[i].handler_descrip);
	if ( handler_descrip )
		(*pipeTable)[i].handler_descrip = strdup(handler_descrip);
	else
		(*pipeTable)[i].handler_descrip = EMPTY_DESCRIP;

	// Increment the counter of total number of entries
	nPipe++;

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &((*pipeTable)[i].data_ptr);

#ifdef WIN32
	// On Win32, make a "pid entry" and pass it to our Pid Watcher thread.
	// This thread will then watch over the pipe handle and notify us
	// when there is something to read.
	// NOTE: WatchPid() must be called at the very end of this function.
	(*pipeTable)[i].pentry = new PidEntry;
	(*pipeTable)[i].pentry->hPipe = dupped_pipe_handle;
	(*pipeTable)[i].pentry->hProcess = 0;
	(*pipeTable)[i].pentry->hThread = 0;
	(*pipeTable)[i].pentry->pipeReady = 0;
	(*pipeTable)[i].pentry->deallocate = 0;
	WatchPid((*pipeTable)[i].pentry);
#endif

	return pipefd;
}

int DaemonCore::Create_Pipe( int *filedes, bool nonblocking_read,
		bool nonblocking_write, unsigned int psize)
{
	dprintf(D_DAEMONCORE,"Entering Create_Pipe()\n");

	if ( psize == 0 ) {
		// set default pipe size at 4k, the size of one page on most platforms
		psize = 1024 * 4;
	}

	bool failed = false;

#ifdef WIN32
	// WIN32
	long handle;
	DWORD Mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
	if ( _pipe(filedes, psize, _O_BINARY) == -1 ) {
		dprintf(D_ALWAYS,"Create_Pipe(): call to pipe() failed\n");
		return FALSE;
	}
	if ( nonblocking_read ) {
		handle = _get_osfhandle( filedes[0] );
		if ( SetNamedPipeHandleState(
				(HANDLE) handle,					// handle to pipe
				&Mode,	// new pipe mode
				NULL,  // maximum collection count
				NULL   // time-out value
				) == 0 ) {
			failed = true;
		}
	}
	if ( nonblocking_write ) {
		handle = _get_osfhandle( filedes[1] );
		if ( SetNamedPipeHandleState(
				(HANDLE) handle,					// handle to pipe
				&Mode,	// new pipe mode
				NULL,  // maximum collection count
				NULL   // time-out value
				) == 0 ) {
			failed = true;
		}
	}
#else
	// Unix
	if ( pipe(filedes) == -1 ) {
		dprintf(D_ALWAYS,"Create_Pipe(): call to pipe() failed\n");
		return FALSE;
	}

	if ( nonblocking_read ) {
		int fcntl_flags;
		if ( (fcntl_flags=fcntl(filedes[0], F_GETFL)) < 0 ) {
			failed = true;
		} else {
			fcntl_flags |= O_NONBLOCK;	// set nonblocking mode
			if ( fcntl(filedes[0],F_SETFL,fcntl_flags) == -1 ) {
				failed = true;
			}
		}
	}
	if ( nonblocking_write ) {
		int fcntl_flags;
		if ( (fcntl_flags=fcntl(filedes[1], F_GETFL)) < 0 ) {
			failed = true;
		} else {
			fcntl_flags |= O_NONBLOCK;	// set nonblocking mode
			if ( fcntl(filedes[1],F_SETFL,fcntl_flags) == -1 ) {
				failed = true;
			}
		}
	}
#endif

	if ( failed == true ) {
		close(filedes[0]);
		filedes[0] = -1;
		close(filedes[1]);
		filedes[1] = -1;
		dprintf(D_ALWAYS,"Create_Pipe() failed to set non-blocking mode\n");
		return FALSE;
	} else {
		dprintf(D_DAEMONCORE,"Create_Pipe() success readfd=%d writefd=%d\n",
			filedes[0],filedes[1]);
		return TRUE;
	}
}


int DaemonCore::Close_Pipe( FILE* pipefp ) {

	int pipefd = fileno(pipefp);

	// First, call Cancel_Pipe on this pipefd.
	int i,j;
	i = -1;
	for (j=0;j<nPipe;j++) {
		if ( (*pipeTable)[j].pipefd == pipefd ) {
			i = j;
			break;
		}
	}

	if ( i != -1 ) {
		// We now know that this pipefd is registed.  Cancel it.
		int result = Cancel_Pipe(pipefd);
		// ASSERT that it did not fail, because the only reason it should
		// fail is if it is not registered.  And we already checked that.
		ASSERT( result == TRUE );
	}

	// since its a stream, call fclose() instead of close()

	if ( 0 == fclose(pipefp) ) {
		return TRUE;
	} else {
		dprintf(D_ALWAYS, "fclose() failed in Pipe_Close() with errno=%d\n",
			errno);
		return FALSE;
	}
}

int DaemonCore::Close_Pipe( int pipefd )
{
	// First, call Cancel_Pipe on this pipefd.
	int i,j;
	i = -1;
	for (j=0;j<nPipe;j++) {
		if ( (*pipeTable)[j].pipefd == pipefd ) {
			i = j;
			break;
		}
	}
	if ( i != -1 ) {
		// We now know that this pipefd is registed.  Cancel it.
		int result = Cancel_Pipe(pipefd);
		// ASSERT that it did not fail, because the only reason it should
		// fail is if it is not registered.  And we already checked that.
		ASSERT( result == TRUE );
	}

	// Now, close the fd.
	if ( close(pipefd) < 0 ) {
		dprintf(D_ALWAYS,
			"Close_Pipe(pipefd=%d) failed, errno=%d\n",pipefd,errno);
		return FALSE;  // probably a bad fd
	} else {
		dprintf(D_DAEMONCORE,
			"Close_Pipe(pipefd=%d) succeeded\n",pipefd);
		return TRUE;
	}
}


int DaemonCore::Cancel_Pipe( int pipefd )
{
	int i,j;

	i = -1;
	for (j=0;j<nPipe;j++) {
		if ( (*pipeTable)[j].pipefd == pipefd ) {
			i = j;
			break;
		}
	}

	if ( i == -1 ) {
		dprintf( D_ALWAYS,"Cancel_Pipe: called on non-registered pipe!\n");
		dprintf( D_ALWAYS,"Offending pipe fd number %d\n", pipefd );
		return FALSE;
	}

	// Remove entry at index i by moving the last one in the table here.

	// Clear any data_ptr which go to this entry we just removed
	if ( curr_regdataptr == &( (*pipeTable)[i].data_ptr) )
		curr_regdataptr = NULL;
	if ( curr_dataptr == &( (*pipeTable)[i].data_ptr) )
		curr_dataptr = NULL;

	// Log a message
	dprintf(D_DAEMONCORE,
			"Cancel_Pipe: cancelled pipe fd %d <%s> (entry=%d)\n",
			pipefd,(*pipeTable)[i].pipe_descrip, i );

	// Remove entry, move the last one in the list into this spot
	(*pipeTable)[i].pipefd = 0;
	free_descrip( (*pipeTable)[i].pipe_descrip );
	(*pipeTable)[i].pipe_descrip = NULL;
	free_descrip( (*pipeTable)[i].handler_descrip );
	(*pipeTable)[i].handler_descrip = NULL;
#ifdef WIN32
	// Instead of deallocating, just set deallocate flag and then
	// the pointer to NULL --
	// actual memory will get dealloacted by a seperate thread.
	// note: we must acccess the deallocate flag in a thread-safe manner.
	ASSERT( (*pipeTable)[i].pentry );
	InterlockedExchange(&((*pipeTable)[i].pentry->deallocate),1L);
	// Now if we are called from inside of our handler, we have nothing more to
	// do because when we return to DaemonCore, DaemonCore will call WatchPid() again
	// to hand this PidEntry off to a watcher thread which will then deallocate.
	// However, if we are not called from inside of our handler, we need to call
	// SetEvent to get the attention of the thread watching this entry.
	if ( (*pipeTable)[i].in_handler == false &&
		 (*pipeTable)[i].pentry->watcherEvent )
	{
		::SetEvent((*pipeTable)[i].pentry->watcherEvent);
	}
#endif
	(*pipeTable)[i].pentry = NULL;
	if ( i < nPipe - 1 ) {
            // if not the last entry in the table, move the last one here
		(*pipeTable)[i] = (*pipeTable)[nPipe - 1];
		(*pipeTable)[nPipe - 1].pipefd = 0;
		(*pipeTable)[nPipe - 1].pipe_descrip = NULL;
		(*pipeTable)[nPipe - 1].handler_descrip = NULL;
		(*pipeTable)[nPipe - 1].pentry = NULL;
	}
	nPipe--;

	return TRUE;
}


int DaemonCore::Register_Reaper(int rid, char* reap_descrip,
				ReaperHandler handler, ReaperHandlercpp handlercpp,
				char *handler_descrip, Service* s, int is_cpp)
{
    int     i;
    int     j;

    // In reapTable, unlike the others handler tables, we allow for a
	// NULL handler and a NULL handlercpp - this means just reap
	// with no handler, so use the default daemon core reaper handler
	// which reaps the exit status on unix and frees the handle on Win32.

	// An incoming rid of -1 means choose a new rid; otherwise we want to
	// replace a table entry, resulting in a new entry with the same rid.

	// No hash table; just store in an array

    // Set i to be the entry in the table we're going to modify.  If the rid
	// is -1, then find an empty entry.  If the rid is > 0, assert that this
	// is  valid entry.
	if ( rid == -1 ) {
		// a brand new entry in the table
		if(nReap >= maxReap) {
			EXCEPT("# of reaper handlers exceeded specified maximum");
		}
		// scan thru table to find a new entry. scan in such a way
		// that we do not re-use rid's until we have to.
		for(i = nReap % maxReap, j=0; j < maxReap; j++, i = (i + 1) % maxReap)
		{
			if ( reapTable[i].num == 0 ) {
				break;
			} else {
				if ( reapTable[i].num != i + 1 ) {
					EXCEPT("reaper table messed up");
				}
			}
		}
		nReap++;	// this is a new entry, so increment our counter
		rid = i + 1;
	} else {
		if ( (rid < 1) || (rid > maxReap) )
			return FALSE;	// invalid rid passed to us
		if ( (reapTable[rid - 1].num) != rid )
			return FALSE;	// trying to re-register a non-existant entry
		i = rid - 1;
	}

	// Found the entry to use at index i. Now add in the new data.
	reapTable[i].num = rid;
	reapTable[i].handler = handler;
	reapTable[i].handlercpp = handlercpp;
	reapTable[i].is_cpp = is_cpp;
	reapTable[i].service = s;
	reapTable[i].data_ptr = NULL;
	free_descrip(reapTable[i].reap_descrip);
	if ( reap_descrip )
		reapTable[i].reap_descrip = strdup(reap_descrip);
	else
		reapTable[i].reap_descrip = EMPTY_DESCRIP;
	free_descrip(reapTable[i].handler_descrip);
	if ( handler_descrip )
		reapTable[i].handler_descrip = strdup(handler_descrip);
	else
		reapTable[i].handler_descrip = EMPTY_DESCRIP;

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &(reapTable[i].data_ptr);

	// Conditionally dump what our table looks like
	DumpReapTable(D_FULLDEBUG | D_DAEMONCORE);

	return rid;
}

int DaemonCore::Lookup_Socket( Stream *insock )
{
	for (int i=0; i < nSock; i++) {
		if ((*sockTable)[i].iosock == insock) {
			return i;
		}
	}
	return -1;
}

int DaemonCore::Cancel_Reaper( int )
{
	// stub

	// be certain to get through the pid table and edit the rids

	return TRUE;
}

// For debugging purposes
void DaemonCore::Dump(int flag, char* indent)
{
	DumpCommandTable(flag, indent);
	DumpSigTable(flag, indent);
	DumpSocketTable(flag, indent);
	t.DumpTimerList(flag, indent);
}

void DaemonCore::DumpCommandTable(int flag, const char* indent)
{
	int		i;
	char *descrip1, *descrip2;

	// we want to allow flag to be "D_FULLDEBUG | D_DAEMONCORE",
	// and only have output if _both_ are specified by the user
	// in the condor_config.  this is a little different than
	// what dprintf does by itself ( which is just
	// flag & DebugFlags > 0 ), so our own check here:
	if ( (flag & DebugFlags) != flag )
		return;

	if ( indent == NULL)
		indent = DEFAULT_INDENT;

	dprintf(flag,"\n");
	dprintf(flag, "%sCommands Registered\n", indent);
	dprintf(flag, "%s~~~~~~~~~~~~~~~~~~~\n", indent);
	for (i = 0; i < maxCommand; i++) {
		if( comTable[i].handler || comTable[i].handlercpp )
		{
			descrip1 = "NULL";
			descrip2 = descrip1;
			if ( comTable[i].command_descrip )
				descrip1 = comTable[i].command_descrip;
			if ( comTable[i].handler_descrip )
				descrip2 = comTable[i].handler_descrip;
			dprintf(flag, "%s%d: %s %s\n", indent, comTable[i].num,
							descrip1, descrip2);
		}
	}
	dprintf(flag, "\n");
}

MyString DaemonCore::GetCommandsInAuthLevel(DCpermission perm) {
	int		i;

	MyString res;
	char tbuf[16];

	for (i = 0; i < maxCommand; i++) {
		if( (comTable[i].handler || comTable[i].handlercpp) &&
			(comTable[i].perm == perm) )
		{

			sprintf (tbuf, "%i", comTable[i].num);
			if (res.Length()) {
				res += ",";
			}
			res += tbuf;
		}
	}

	return res;

}

void DaemonCore::DumpReapTable(int flag, const char* indent)
{
	int		i;
	char *descrip1, *descrip2;

	// we want to allow flag to be "D_FULLDEBUG | D_DAEMONCORE",
	// and only have output if _both_ are specified by the user
	// in the condor_config.  this is a little different than
	// what dprintf does by itself ( which is just
	// flag & DebugFlags > 0 ), so our own check here:
	if ( (flag & DebugFlags) != flag )
		return;

	if ( indent == NULL)
		indent = DEFAULT_INDENT;

	dprintf(flag,"\n");
	dprintf(flag, "%sReapers Registered\n", indent);
	dprintf(flag, "%s~~~~~~~~~~~~~~~~~~~\n", indent);
	for (i = 0; i < maxReap; i++) {
		if( reapTable[i].handler || reapTable[i].handlercpp ) {
			descrip1 = "NULL";
			descrip2 = descrip1;
			if ( reapTable[i].reap_descrip )
				descrip1 = reapTable[i].reap_descrip;
			if ( reapTable[i].handler_descrip )
				descrip2 = reapTable[i].handler_descrip;
			dprintf(flag, "%s%d: %s %s\n", indent, reapTable[i].num,
							descrip1, descrip2);
		}
	}
	dprintf(flag, "\n");
}

void DaemonCore::DumpSigTable(int flag, const char* indent)
{
	int		i;
	char *descrip1, *descrip2;

	// we want to allow flag to be "D_FULLDEBUG | D_DAEMONCORE",
	// and only have output if _both_ are specified by the user
	// in the condor_config.  this is a little different than
	// what dprintf does by itself ( which is just
	// flag & DebugFlags > 0 ), so our own check here:
	if ( (flag & DebugFlags) != flag )
		return;

	if ( indent == NULL)
		indent = DEFAULT_INDENT;

	dprintf(flag, "\n");
	dprintf(flag, "%sSignals Registered\n", indent);
	dprintf(flag, "%s~~~~~~~~~~~~~~~~~~\n", indent);
	for (i = 0; i < maxSig; i++) {
		if( sigTable[i].handler || sigTable[i].handlercpp ) {
			descrip1 = "NULL";
			descrip2 = descrip1;
			if ( sigTable[i].sig_descrip )
				descrip1 = sigTable[i].sig_descrip;
			if ( sigTable[i].handler_descrip )
				descrip2 = sigTable[i].handler_descrip;
			dprintf(flag, "%s%d: %s %s, Blocked:%d Pending:%d\n", indent,
							sigTable[i].num, descrip1, descrip2,
							sigTable[i].is_blocked, sigTable[i].is_pending);
		}
	}
	dprintf(flag, "\n");
}

void DaemonCore::DumpSocketTable(int flag, const char* indent)
{
	int		i;
	char *descrip1, *descrip2;

	// we want to allow flag to be "D_FULLDEBUG | D_DAEMONCORE",
	// and only have output if _both_ are specified by the user
	// in the condor_config.  this is a little different than
	// what dprintf does by itself ( which is just
	// flag & DebugFlags > 0 ), so our own check here:
	if ( (flag & DebugFlags) != flag )
		return;

	if ( indent == NULL)
		indent = DEFAULT_INDENT;

	dprintf(flag,"\n");
	dprintf(flag, "%sSockets Registered\n", indent);
	dprintf(flag, "%s~~~~~~~~~~~~~~~~~~~\n", indent);
	for (i = 0; i < nSock; i++) {
		if ( (*sockTable)[i].iosock ) {
			descrip1 = "NULL";
			descrip2 = descrip1;
			if ( (*sockTable)[i].iosock_descrip )
				descrip1 = (*sockTable)[i].iosock_descrip;
			if ( (*sockTable)[i].handler_descrip )
				descrip2 = (*sockTable)[i].handler_descrip;
			dprintf(flag, "%s%d: %d %s %s\n",
					indent, i, ((Sock *) (*sockTable)[i].iosock)->get_file_desc(), descrip1, descrip2 );
		}
	}
	dprintf(flag, "\n");
}

int
DaemonCore::ReInit()
{
	char *tmp;
	char buf[50];
	static int tid = -1;

		// Reset our IpVerify object
	ipverify.Init();

		// Handle our timer.  If this is the first time, we need to
		// register it.  Otherwise, we just reset its value to go off
		// 8 hours from now.  The reason we don't do this as a simple
		// periodic timer is that if we get sent a RECONFIG, we call
		// this anyway, and we don't want to clear out the cache soon
		// after that, just b/c of a periodic timer.  -Derek 1/28/99
	if( tid < 0 ) {
		tid = daemonCore->
			Register_Timer( 8*60*60, 0, (Eventcpp)&DaemonCore::ReInit,
							"DaemonCore::ReInit()", daemonCore );
	} else {
		daemonCore->Reset_Timer( tid, 8*60*60, 0 );
	}

	// Setup a timer to send child keepalives to our parent, if we have
	// a daemon core parent.
	if ( ppid ) {
		max_hang_time = 0;
		sprintf(buf,"%s_NOT_RESPONDING_TIMEOUT",mySubSystem);
		if ( !(tmp=param(buf)) ) {
			tmp = param("NOT_RESPONDING_TIMEOUT");
		}
		if ( tmp ) {
			max_hang_time = atoi(tmp);
			free(tmp);
		}
		if ( !max_hang_time ) {
			max_hang_time = 60 * 60;	// default to 1 hour
		}
		int send_update = (max_hang_time / 3) - 30;
		if ( send_update < 1 )
			send_update = 1;
		if ( send_child_alive_timer == -1 ) {
			send_child_alive_timer = Register_Timer(1, (unsigned)send_update,
					(TimerHandlercpp)&DaemonCore::SendAliveToParent,
					"DaemonCore::SendAliveToParent", this );
		} else {
			Reset_Timer(send_child_alive_timer, 1, send_update);
		}
	}

#ifdef COMPILE_SOAP_SSL
	MyString subsys = MyString(mySubSystem);
	bool enable_soap_ssl = param_boolean("ENABLE_SOAP_SSL", false);
	bool subsys_enable_soap_ssl =
		param_boolean((subsys + "_ENABLE_SOAP_SSL").GetCStr(), false);
	if (subsys_enable_soap_ssl ||
		(enable_soap_ssl &&
		 (!(NULL != param((subsys + "_ENABLE_SOAP_SSL").GetCStr())) ||
		  subsys_enable_soap_ssl))) {
		if (mapfile) {
			delete mapfile; mapfile = NULL;
		}
		mapfile = new MapFile;
		char * credential_mapfile;
		if (NULL == (credential_mapfile = param("CERTIFICATE_MAPFILE"))) {
			EXCEPT("DaemonCore: No CERTIFICATE_MAPFILE defined, "
				   "unable to identify users, required by ENABLE_SOAP_SSL");
		}
		char * user_mapfile;
		if (NULL == (user_mapfile = param("USER_MAPFILE"))) {
			EXCEPT("DaemonCore: No USER_MAPFILE defined, "
				   "unable to identify users, required by ENABLE_SOAP_SSL");
		}
		int line;
		if (0 != (line = mapfile->ParseCanonicalizationFile(credential_mapfile))) {
			EXCEPT("DaemonCore: Error parsing CERTIFICATE_MAPFILE at line %d",
				   line);
	}
		if (0 != (line = mapfile->ParseUsermapFile(user_mapfile))) {
			EXCEPT("DaemonCore: Error parsing USER_MAPFILE at line %d", line);
		}
	}
#endif // COMPILE_SOAP_SSL

	return TRUE;
}

int
DaemonCore::Verify(DCpermission perm, const struct sockaddr_in *sin, const char * fqu )
{
	/*
	 * Be Warned:  careful about parameter "sin" being NULL.  It could be, in
	 * which case we should return FALSE (unless perm is ALLOW)
	 *
	 */

	switch (perm) {
	case ALLOW:
		return TRUE;
		break;

	case IMMEDIATE_FAMILY:
		// TODO!!!  Implement IMMEDIATE_FAMILY someday!
		return TRUE;
		break;

	default:
		if ( sin ) {
			return ipverify.Verify(perm, sin, fqu);
		} else {
			return FALSE;
		}
		break;
	}

	// Should never make it here, but we return to satisfy C++
	EXCEPT("bad DC Verify");
	return FALSE;
}


int
DaemonCore::AddAllowHost( const char* host, DCpermission perm )
{
	return ipverify.AddAllowHost( host, perm );
}

void
DaemonCore::Only_Allow_Soap(int duration)
{
	if ( duration <= 0 ) {
		only_allow_soap = 0;
	} else {
		time_t now = time(NULL);
		only_allow_soap = now + duration + 1;  // +1 cuz Matt worries
	}
}

// This function never returns. It is responsible for monitor signals and
// incoming messages or requests and invoke corresponding handlers.
void DaemonCore::Driver()
{
	int			rv;					// return value from select
	int			i;
	int			tmpErrno;
	struct timeval	timer;
	struct timeval *ptimer;
	int temp;
	int result;
	time_t connect_timeout, min_connect_timeout;

#ifndef WIN32
	sigset_t fullset, emptyset;
	sigfillset( &fullset );
    // We do not want to block the following signals ----
		sigdelset(&fullset, SIGSEGV);    // so we get a core right away
		sigdelset(&fullset, SIGABRT);    // so assert() drops core right away
		sigdelset(&fullset, SIGILL);     // so we get a core right away
		sigdelset(&fullset, SIGBUS);     // so we get a core right away
		sigdelset(&fullset, SIGFPE);     // so we get a core right away
		sigdelset(&fullset, SIGTRAP);    // so gdb works when it uses SIGTRAP
	sigemptyset( &emptyset );
	char asyncpipe_buf[10];
#endif

	for(;;)
	{
		// call signal handlers for any pending signals
		sent_signal = FALSE;	// set to True inside Send_Signal()
		if ( !only_allow_soap ) {
			for (i=0;i<maxSig;i++) {
				if ( sigTable[i].handler || sigTable[i].handlercpp ) {
					// found a valid entry; test if we should call handler
					if ( sigTable[i].is_pending && !sigTable[i].is_blocked ) {
						// call handler, but first clear pending flag
						sigTable[i].is_pending = 0;
						// Update curr_dataptr for GetDataPtr()
						curr_dataptr = &(sigTable[i].data_ptr);
						// log a message
						dprintf(D_DAEMONCORE,
										"Calling Handler <%s> for Signal %d <%s>\n",
										sigTable[i].handler_descrip,sigTable[i].num,
										sigTable[i].sig_descrip);
						// call the handler
						if ( sigTable[i].is_cpp )
							(sigTable[i].service->*(sigTable[i].handlercpp))(sigTable[i].num);
						else
							(*sigTable[i].handler)(sigTable[i].service,sigTable[i].num);
						// Clear curr_dataptr
						curr_dataptr = NULL;
						// Make sure we didn't leak our priv state
						CheckPrivState();
					}
				}
			}
		}
#ifndef WIN32
		// Drain our async_pipe; we must do this before we unblock unix signals.
		// Just keep reading while something is there.  async_pipe is set to
		// non-blocking mode via fcntl, so the read below will not block.
		while( read(async_pipe[0],asyncpipe_buf,8) > 0 );
#endif
		async_pipe_empty = TRUE;

		// Prepare to enter main select()

		// call Timeout() - this function does 2 things:
		//   first, it calls any timer handlers whose time has arrived.
		//   second, it returns how many seconds until the next timer
		//   event so we use this as our select timeout _if_ sent_signal
		//   is not TRUE.  if sent_signal is TRUE, it means that we have
		//   a pending signal which we did not service above (likely because
		//   it was itself raised by a signal handler!).  so if sent_signal is
		//   TRUE, set the select timeout to zero so that we break thru select
		//   and service this outstanding signal and yet we do not
		//   starve commands...

		temp = 0;
		if ( !only_allow_soap ) {	// call timers unless only allowing soap
			temp = t.Timeout();
		}

		if ( sent_signal == TRUE ) {
			temp = 0;
		}
		timer.tv_sec = temp;
		timer.tv_usec = 0;
		if ( temp < 0 ) {
			ptimer = NULL;
		} else {
			ptimer = &timer;		// no timeout on the select() desired
		}

		// Setup what socket descriptors to select on.  We recompute this
		// every time because 1) some timeout handler may have removed/added
		// sockets, and 2) it ain't that expensive....
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);
		min_connect_timeout = 0;
        int maxfd = 0;
		for (i = 0; i < nSock; i++) {
			if ( (*sockTable)[i].iosock ) {	// if a valid entry....
					// Setup our fdsets
				if ( (*sockTable)[i].is_connect_pending ) {
						// we want to be woken when a non-blocking
						// connect is ready to write.  when connect
						// is ready, select will set the writefd set
						// on success, or the exceptfd set on failure.
					FD_SET( (*sockTable)[i].sockd,&writefds);
					FD_SET( (*sockTable)[i].sockd,&exceptfds);

					// If this connection attempt times out sooner than
					// our select timeout, adjust the select timeout.
					connect_timeout = (*sockTable)[i].iosock->connect_timeout_time();
					if(connect_timeout) { // If non-zero, there is a timeout.
						if(min_connect_timeout == 0 || \
						   min_connect_timeout > connect_timeout) {
							min_connect_timeout = connect_timeout;
						}
						connect_timeout -= time(NULL);
						if(connect_timeout < timer.tv_sec) {
							if(connect_timeout < 0) connect_timeout = 0;
							timer.tv_sec = connect_timeout;
						}
					}
				} else {
						// we want to be woken when there is something
						// to read.
					FD_SET( (*sockTable)[i].sockd,&readfds);
				}
                if ( (*sockTable)[i].sockd >= maxfd ) {
                    maxfd = (*sockTable)[i].sockd + 1;
                }
            }
		}


#if !defined(WIN32)
		// Add the registered pipe fds into the list of descriptors to
		// select on.
		for (i = 0; i < nPipe; i++) {
			if ( (*pipeTable)[i].pipefd ) {	// if a valid entry....
                if ( (*pipeTable)[i].pipefd >= maxfd ) {
                    maxfd = (*pipeTable)[i].pipefd + 1;
                }
				switch( (*pipeTable)[i].handler_type ) {
				case HANDLE_READ:
					FD_SET( (*pipeTable)[i].pipefd,&readfds);
					break;
				case HANDLE_WRITE:
					FD_SET( (*pipeTable)[i].pipefd,&writefds);
					break;
				case HANDLE_READ_WRITE:
					FD_SET( (*pipeTable)[i].pipefd,&readfds);
					FD_SET( (*pipeTable)[i].pipefd,&writefds);
					break;
				}
			}
        }

		// Add the read side of async_pipe to the list of file descriptors to
		// select on.  We write to async_pipe if a unix async signal
		// is delivered after we unblock signals and before we block on select.
		FD_SET(async_pipe[0],&readfds);
        if ( async_pipe[0] >= maxfd ) {
            maxfd = async_pipe[0] + 1;
        }
#endif

		if ( only_allow_soap ) {
			time_t now = time(NULL);
			if ( now >= only_allow_soap ) {
				// the time has past... let everything in
				only_allow_soap = 0;
				// and call continue so our timers get called at the start
				// of the infinite loop above.
				continue;
			} else {
				// only allow soap commands for a while longer
				timer.tv_sec = only_allow_soap - now;
				FD_ZERO(&readfds);
				FD_ZERO(&writefds);
				FD_ZERO(&exceptfds);
				FD_SET( (*sockTable)[initial_command_sock].sockd, &readfds );
					// This is ugly, and will break if the sockTable
					// is ever "compacted" or otherwise rearranged
					// after sockets are registered into it.
				if (-1 != soap_ssl_sock) {
					int soap_sock_fd = (*sockTable)[soap_ssl_sock].sockd;
					FD_SET( soap_sock_fd, &readfds );
					if ( soap_sock_fd >= maxfd ) {
						maxfd = soap_sock_fd + 1;
					}
				}
			}
		}

#if !defined(WIN32)
		// Set aync_sigs_unblocked flag to true so that Send_Signal()
		// knows to put info onto the async_pipe in order to wake up select().
		// We _must_ set this flag to TRUE before we unblock async signals, and
		// set it to FALSE after we block the signals again.
		async_sigs_unblocked = TRUE;

		// Unblock all signals so that we can get them during the
		// select.
		sigprocmask( SIG_SETMASK, &emptyset, NULL );
#else
		//Win32 - grab coarse-grained mutex
		LeaveCriticalSection(&Big_fat_mutex);
#endif

		errno = 0;
		time_t time_before = time(NULL);
		rv = select( maxfd, (SELECT_FDSET_PTR) &readfds,
					 (SELECT_FDSET_PTR) &writefds,
					 (SELECT_FDSET_PTR) &exceptfds, ptimer );
		CheckForTimeSkip(time_before, timer.tv_sec);


		tmpErrno = errno;

#ifndef WIN32
		// Unix

		// Block all signals until next select so that we don't
		// get confused.
		sigprocmask( SIG_SETMASK, &fullset, NULL );

		// We _must_ set async_sigs_unblocked flag to TRUE
		// before we unblock async signals, and
		// set it to FALSE after we block the signals again.
		async_sigs_unblocked = FALSE;

		if(rv < 0) {
			if(tmpErrno != EINTR)
			// not just interrupted by a signal...
			{
				EXCEPT("DaemonCore: select() returned an unexpected error: %d (%s)\n",tmpErrno,strerror(tmpErrno));
			}
		}
#else
		// Windoze
		EnterCriticalSection(&Big_fat_mutex);
		if ( rv == SOCKET_ERROR ) {
			EXCEPT("select, error # = %d",WSAGetLastError());
		}
#endif

		if(rv==0 && min_connect_timeout && min_connect_timeout < time(NULL)) {
			// No socket activity has happened, but a connection attempt
			// has timed out, so do enter the following section.
			rv = 1;
		}
		if (rv > 0) {	// connection requested

			bool recheck_status = false;
			//bool call_soap_handler = false;

			// scan through the socket table to find which ones select() set
			for(i = 0; i < nSock; i++) {
				if ( (*sockTable)[i].iosock ) {	// if a valid entry...
					// figure out if we should call a handler.  to do this,
					// if the socket was doing a connect(), we check the
					// writefds and excepfds.  otherwise, check readfds.
					(*sockTable)[i].call_handler = false;
					if ( (*sockTable)[i].is_connect_pending ) {

						connect_timeout =
							(*sockTable)[i].iosock->connect_timeout_time();
						bool connect_timed_out =
							connect_timeout != 0 && connect_timeout < time(NULL);

						if ( (FD_ISSET((*sockTable)[i].sockd, &writefds)) ||
							 (FD_ISSET((*sockTable)[i].sockd, &exceptfds)) ||
							 connect_timed_out )
						{
							// A connection pending socket has been
							// set or the connection attempt has timed out.
							// Only call handler if CEDAR confirms the
							// connect algorithm has completed.
							if ( ((Sock *)(*sockTable)[i].iosock)->
											do_connect_finish() )
							{
								(*sockTable)[i].call_handler = true;
							}
						}
					} else {
						if (FD_ISSET((*sockTable)[i].sockd, &readfds))
						{
							(*sockTable)[i].call_handler = true;
						}
					}
				}	// end of if valid sock entry
			}	// end of for loop through all sock entries

			// scan through the pipe table to find which ones select() set
			for(i = 0; i < nPipe; i++) {
				if ( (*pipeTable)[i].pipefd ) {	// if a valid entry...
					// figure out if we should call a handler.
					(*pipeTable)[i].call_handler = false;
#ifdef WIN32
					// For Windows, check if our pidwatcher thread set the flag
					ASSERT( (*pipeTable)[i].pentry );
					if (InterlockedExchange(&((*pipeTable)[i].pentry->pipeReady),0L))
					{
						// pipeReady flag was set by the pidwatcher thread.
						(*pipeTable)[i].call_handler = true;
					}
#else
					// For Unix, check if select set the bit
					if( FD_ISSET((*pipeTable)[i].pipefd, &readfds) )
					{
						(*pipeTable)[i].call_handler = true;
					}
					if (FD_ISSET((*pipeTable)[i].pipefd, &writefds))
					{
						(*pipeTable)[i].call_handler = true;
					}
#endif
				}	// end of if valid pipe entry
			}	// end of for loop through all pipe entries


			// Now loop through all pipe entries, calling handlers if required.
			for(i = 0; i < nPipe; i++) {
				if ( (*pipeTable)[i].pipefd ) {	// if a valid entry...

					if ( (*pipeTable)[i].call_handler ) {

						(*pipeTable)[i].call_handler = false;

						// save the pentry on the stack, since we'd otherwise lose it
						// if the user's handler call Cancel_Pipe().
						PidEntry* saved_pentry = (*pipeTable)[i].pentry;

						if ( recheck_status || saved_pentry ) {
							// we have already called at least one callback handler.  what
							// if this handler drained this registed pipe, so that another
							// read on the pipe could block?  to prevent this, we need
							// to check one more time to make certain the pipe is ready
							// for reading.
							// NOTE: we also enter this code if saved_pentry != NULL.
							//       why?  because that means we are on Windows, and
							//       on Windows we need to check because pipes are
							//       signalled not by select() but by our pidwatcher
							//       thread, which may have signaled this pipe ready
							//       when were in a timer handler or whatever.
#ifdef WIN32
							// WINDOWS
							DWORD num_bytes_avail = 0;
							if ( saved_pentry && saved_pentry->hPipe )
							{
								PeekNamedPipe(
								  saved_pentry->hPipe,	// handle to pipe
								  NULL,				// data buffer
								  0,				// size of data buffer
								  NULL,				// number of bytes read
								  &num_bytes_avail,	// number of bytes available
								  NULL  // unread bytes
								);
							}
							if ( num_bytes_avail == 0 ) {
								// there is no longer anything available to read
								// on the pipe.  try the next entry....
								continue;
							}
#else
							// UNIX
							FD_ZERO(&readfds);
							FD_SET( (*pipeTable)[i].pipefd,&readfds);
							struct timeval stimeout;
							stimeout.tv_sec = 0;	// set timeout for a poll
							stimeout.tv_usec = 0;
							int sresult = select( (*pipeTable)[i].pipefd + 1,
								(SELECT_FDSET_PTR) &readfds,
								(SELECT_FDSET_PTR) 0,(SELECT_FDSET_PTR) 0,
								&stimeout );
							if ( sresult == 0 ) {
								// nothing available, try the next entry...
								continue;
							}
#endif
						}	// end of if ( recheck_status || saved_pentry )

						(*pipeTable)[i].in_handler = true;


						// log a message
						dprintf(D_DAEMONCORE,
									"Calling Handler <%s> for Pipe fd=%d <%s>\n",
									(*pipeTable)[i].handler_descrip,
									(*pipeTable)[i].pipefd,
									(*pipeTable)[i].pipe_descrip);

						// Update curr_dataptr for GetDataPtr()
						curr_dataptr = &( (*pipeTable)[i].data_ptr);
						recheck_status = true;
						if ( (*pipeTable)[i].handler )
							// a C handler
							result = (*( (*pipeTable)[i].handler))( (*pipeTable)[i].service, (*pipeTable)[i].pipefd);
						else
						if ( (*pipeTable)[i].handlercpp )
							// a C++ handler
							result = ((*pipeTable)[i].service->*( (*pipeTable)[i].handlercpp))((*pipeTable)[i].pipefd);
						else
						{
							// no handler registered
							EXCEPT("No pipe handler callback");
						}

						// Make sure we didn't leak our priv state
						CheckPrivState();

						// Clear curr_dataptr
						curr_dataptr = NULL;

						(*pipeTable)[i].in_handler = false;

#ifdef WIN32
						// Ask a pid watcher thread to watch over this pipe
						// handle.  Note that if Cancel_Pipe() was called by the
						// handler above, the deallote flag will be set in the
						// pentry and the pidwatcher thread will cleanup.
						if ( saved_pentry ) {
							WatchPid(saved_pentry);
						}
#endif

						if ( (*pipeTable)[i].call_handler == true ) {
							// looks like the handler called Cancel_Pipe(),
							// and now entry i no longer points to what we
							// think it points to.  Decrement i now, so when
							// we loop back we do not miss calling a handler.
							i--;
						}

					}	// if call_handler is True
				}	// if valid entry in pipeTable
			}	// for 0 thru nPipe checking if call_handler is true


			// Now loop through all sock entries, calling handlers if required.
			for(i = 0; i < nSock; i++) {
				if ( (*sockTable)[i].iosock ) {	// if a valid entry...

					if ( (*sockTable)[i].call_handler ) {

						(*sockTable)[i].call_handler = false;

						if ( recheck_status &&
							 ((*sockTable)[i].is_connect_pending == false) )
						{
							// we have already called at least one callback handler.  what
							// if this handler drained this registed pipe, so that another
							// read on the pipe could block?  to prevent this, we need
							// to check one more time to make certain the pipe is ready
							// for reading.
							FD_ZERO(&readfds);
							FD_SET( (*sockTable)[i].sockd,&readfds);
							struct timeval stimeout;
							stimeout.tv_sec = 0;	// set timeout for a poll
							stimeout.tv_usec = 0;

							int sresult = select( (*sockTable)[i].sockd + 1,
								(SELECT_FDSET_PTR) &readfds,
								(SELECT_FDSET_PTR) 0,(SELECT_FDSET_PTR) 0,
								&stimeout );
							if ( sresult == 0 ) {
								// nothing available, try the next entry...
								continue;
							}
						}

						// ok, select says this socket table entry has new data.

						// if this sock is a safe_sock, then call the method
						// to enqueue this packet into the buffers.  if a complete
						// message is not yet ready, then do not yet call a handler.
						if ( (*sockTable)[i].iosock->type() == Stream::safe_sock )
						{
							SafeSock* ss = (SafeSock *)(*sockTable)[i].iosock;
							// call handle_incoming_packet to consume the packet.
							// it returns true if there is a complete message ready,
							// otherwise it returns false.
							if ( !(ss->handle_incoming_packet()) ) {
								// there is not yet a complete message ready.
								// so go back to the outer for loop - do not
								// call the user handler yet.
								continue;
							}
						}

						// if the user provided a handler for this socket, then
						// call it now.  otherwise, call the daemoncore
						// HandleReq() handler which strips off the command
						// request number and calls any registered command
						// handler.

						// log a message
						if ( (*sockTable)[i].handler || (*sockTable)[i].handlercpp )
						{
							dprintf(D_DAEMONCORE,
									"Calling Handler <%s> for Socket <%s>\n",
									(*sockTable)[i].handler_descrip,
									(*sockTable)[i].iosock_descrip);
						}

						// Update curr_dataptr for GetDataPtr()
						curr_dataptr = &( (*sockTable)[i].data_ptr);
						recheck_status = true;
						if ( (*sockTable)[i].handler )
							// a C handler
							result = (*( (*sockTable)[i].handler))( (*sockTable)[i].service, (*sockTable)[i].iosock);
						else
						if ( (*sockTable)[i].handlercpp )
							// a C++ handler
							result = ((*sockTable)[i].service->*( (*sockTable)[i].handlercpp))((*sockTable)[i].iosock);
						else
							// no handler registered, so this is a command
							// socket.  call the DaemonCore handler which
							// takes care of command sockets.
							result = HandleReq(i);

						// Make sure we didn't leak our priv state
						CheckPrivState();

						// Clear curr_dataptr
						curr_dataptr = NULL;

						// Check result from socket handler, and if
						// not KEEP_STREAM, then
						// delete the socket and the socket handler.
						if ( result != KEEP_STREAM ) {
							// delete the cedar socket
							delete (*sockTable)[i].iosock;
							// cancel the socket handler
							Cancel_Socket( (*sockTable)[i].iosock );
							// decrement i, since sockTable[i] may now
							// point to a new valid socket
							i--;
						}

					}	// if call_handler is True
				}	// if valid entry in sockTable
			}	// for 0 thru nSock checking if call_handler is true

		}	// if rv > 0

	}	// end of infinite for loop
}

void
DaemonCore::CheckPrivState( void )
{
		// We should always be Condor, so set to it here.  If we were
		// already in Condor priv, this is just a no-op.
	priv_state old_priv = set_priv( Default_Priv_State );

#ifdef WIN32
		// TODD - TEMPORARY HACK UNTIL WIN32 HAS FULL USER_PRIV SUPPORT
	if ( Default_Priv_State == PRIV_USER ) {
		return;
	}
#endif

		// See if our old state was something else.
	if( old_priv != Default_Priv_State ) {
		dprintf( D_ALWAYS,
				 "DaemonCore ERROR: Handler returned with priv state %d\n",
				 old_priv );
		dprintf( D_ALWAYS, "History of priv-state changes:\n" );
		display_priv_log();
		char* tmp = param( "EXCEPT_ON_ERROR" );
		if( tmp ) {
			if( tmp[0] == 'T' || tmp[0] == 't' ) {
				EXCEPT( "Priv-state error found by DaemonCore" );
			}
			free( tmp );
		}
	}
}

int DaemonCore::ServiceCommandSocket()
{
	int commands_served = 0;
	fd_set		fds;
	int			rv = 0;					// return value from select
	struct timeval	timer;

	// Just return if there is no command socket
	if ( initial_command_sock == -1 )
		return 0;
	if ( !( (*sockTable)[initial_command_sock].iosock) )
		return 0;

	inServiceCommandSocket_flag = TRUE;
	do {

		// Set select timer sec & usec to 0 means do not block, i.e.
		// just poll the socket
		timer.tv_sec = 0;
		timer.tv_usec = 0;

		FD_ZERO(&fds);
		FD_SET( (*sockTable)[initial_command_sock].sockd,&fds);
		errno = 0;
		rv = select((*sockTable)[initial_command_sock].sockd + 1,
                (SELECT_FDSET_PTR) &fds, NULL, NULL, &timer);
#ifndef WIN32
		// Unix
		if(rv < 0) {
			if(errno != EINTR) {
				// not just interrupted by a signal...
				EXCEPT("select, error # = %d", errno);
			}
		}
#else
		// Win32
		if ( rv == SOCKET_ERROR ) {
			EXCEPT("select, error # = %d",WSAGetLastError());
		}
#endif

		if ( rv > 0) {		// a connection was requested
			HandleReq( initial_command_sock );
			commands_served++;
				// Make sure we didn't leak our priv state
			CheckPrivState();
		}

	} while ( rv > 0 );		// loop until no more commands waiting on socket

	inServiceCommandSocket_flag = FALSE;
	return commands_served;
}

int DaemonCore::HandleReq(int socki)
{
	Stream				*stream = NULL;
	Stream				*insock;
	int					is_tcp;
	int                 req;
	int					index, j;
	int					reqFound = FALSE;
	int					result;
	int					old_timeout;
    int                 perm         = USER_AUTH_FAILURE;
    char                user[256];
	user[0] = '\0';
    ClassAd *the_policy     = NULL;
    KeyInfo *the_key        = NULL;
    char    *the_sid        = NULL;
    char    * who = NULL;   // Remote user
	bool is_http_post = false;	// must initialize to false
	bool is_http_get = false;   // must initialize to false


	insock = (*sockTable)[socki].iosock;

	switch ( insock->type() ) {
		case Stream::reli_sock :
			is_tcp = TRUE;
			break;
		case Stream::safe_sock :
			is_tcp = FALSE;
			break;
		default:
			// unrecognized Stream sock
			dprintf(D_ALWAYS,
						"DaemonCore: HandleReq(): unrecognized Stream sock\n");
			return FALSE;
	}

	CondorError errstack;

	// set up a connection for a tcp socket
	if ( is_tcp ) {

		// if the connection was received on a listen socket, do an accept.
		if ( ((ReliSock *)insock)->_state == Sock::sock_special &&
			((ReliSock *)insock)->_special_state == ReliSock::relisock_listen )
		{
			stream = (Stream *) ((ReliSock *)insock)->accept();
			if ( !stream ) {
				dprintf(D_ALWAYS, "DaemonCore: accept() failed!");
				// return KEEP_STEAM cuz insock is a listen socket
				return KEEP_STREAM;
			}
		}
		// if the not a listen socket, then just assign stream to insock
		else {
			stream = insock;
		}

	}
	// set up a connection for a udp socket
	else {
		// on UDP, we do not have a seperate listen and accept sock.
		// our "listen sock" is also our "accept" sock, so just
		// assign stream to the insock. UDP = connectionless, get it?
		stream = insock;
		// in UDP we cannot display who the command is from until
		// we read something off the socket, so we display who from
		// after we read the command below...

		dprintf ( D_SECURITY, "DC_AUTHENTICATE: received UDP packet from %s.\n",
				sin_to_string(((Sock*)stream)->endpoint()));


		// get the info, if there is any
		char * cleartext_info = (char*)((SafeSock*)stream)->isIncomingDataMD5ed();
		char * sess_id = NULL;
		char * return_address_ss = NULL;

		if (cleartext_info) {
			StringList info_list(cleartext_info);
			char * tmp = NULL;

			info_list.rewind();
			tmp = info_list.next();
			if (tmp) {
				sess_id = strdup(tmp);
				tmp = info_list.next();
				if (tmp) {
					return_address_ss = strdup(tmp);
					dprintf ( D_SECURITY, "DC_AUTHENTICATE: packet from %s uses MD5 session %s.\n",
							return_address_ss, sess_id);
				} else {
					dprintf ( D_SECURITY, "DC_AUTHENTICATE: packet uses MD5 session %s.\n", sess_id);
				}

			} else {
				// protocol violation... StringList didn't give us anything!
				// this is unlikely to work, but we may as well try... so, we
				// don't fail here.
			}
		}

		if (sess_id) {
			KeyCacheEntry *session = NULL;
			bool found_sess = sec_man->session_cache->lookup(sess_id, session);

			if (!found_sess) {
				dprintf ( D_ALWAYS, "DC_AUTHENTICATE: session %s NOT FOUND...\n", sess_id);
				// no session... we outta here!

				// but first, we should be nice and send a message back to
				// the people who sent us the wrong session id.
				sec_man->send_invalidate_packet ( return_address_ss, sess_id );

				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				result = FALSE;
				goto finalize;
			}

			if (!session->key()) {
				dprintf ( D_ALWAYS, "DC_AUTHENTICATE: session %s is missing the key!\n", sess_id);
				// uhm, there should be a key here!
				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				result = FALSE;
				goto finalize;
			}

			if (!stream->set_MD_mode(MD_ALWAYS_ON, session->key())) {
				dprintf (D_ALWAYS, "DC_AUTHENTICATE: unable to turn on message authenticator, failing.\n");
				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				result = FALSE;
				goto finalize;
			} else {
				dprintf (D_SECURITY, "DC_AUTHENTICATE: message authenticator enabled with key id %s.\n", sess_id);
#ifdef SECURITY_HACK_ENABLE
				zz2printf (session->key());
#endif
			}

            // Lookup remote user
            session->policy()->LookupString(ATTR_SEC_USER, &who);

			free( sess_id );

			if (return_address_ss) {
				free( return_address_ss );
			}
		}


		// get the info, if there is any
		cleartext_info = (char*)((SafeSock*)stream)->isIncomingDataEncrypted();
		sess_id = NULL;
		return_address_ss = NULL;

		if (cleartext_info) {
			StringList info_list(cleartext_info);
			char * tmp = NULL;

			info_list.rewind();
			tmp = info_list.next();
			if (tmp) {
				sess_id = strdup(tmp);

				tmp = info_list.next();
				if (tmp) {
					return_address_ss = strdup(tmp);
					dprintf ( D_SECURITY, "DC_AUTHENTICATE: packet from %s uses crypto session %s.\n",
							return_address_ss, sess_id);
				} else {
					dprintf ( D_SECURITY, "DC_AUTHENTICATE: packet uses crypto session %s.\n", sess_id);
				}

			} else {
				// protocol violation... StringList didn't give us anything!
				// this is unlikely to work, but we may as well try... so, we
				// don't fail here.
			}
		}


		if (sess_id) {
			KeyCacheEntry *session = NULL;
			bool found_sess = sec_man->session_cache->lookup(sess_id, session);

			if (!found_sess) {
				dprintf ( D_ALWAYS, "DC_AUTHENTICATE: session %s NOT FOUND...\n", sess_id);
				// no session... we outta here!

				// but first, see above behavior in MD5 code above.
				sec_man->send_invalidate_packet( return_address_ss, sess_id );

				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				result = FALSE;
				goto finalize;
			}

			if (!session->key()) {
				dprintf ( D_ALWAYS, "DC_AUTHENTICATE: session %s is missing the key!\n", sess_id);
				// uhm, there should be a key here!
				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				result = FALSE;
				goto finalize;
			}

			if (!stream->set_crypto_key(true, session->key())) {
				dprintf (D_ALWAYS, "DC_AUTHENTICATE: unable to turn on encryption, failing.\n");
				if( return_address_ss ) {
					free( return_address_ss );
					return_address_ss = NULL;
				}
				free( sess_id );
				sess_id = NULL;
				result = FALSE;
				goto finalize;
			} else {
				dprintf (D_SECURITY, "DC_AUTHENTICATE: encryption enabled with key id %s.\n", sess_id);
#ifdef SECURITY_HACK_ENABLE
				zz2printf (session->key());
#endif
			}
            // Lookup user if necessary
            if (who == NULL) {
                session->policy()->LookupString(ATTR_SEC_USER, &who);
            }
			free( sess_id );
			if (return_address_ss) {
				free( return_address_ss );
			}
		}

        if (who != NULL) {
            ((SafeSock*)stream)->setFullyQualifiedUser(who);
            ((SafeSock*)stream)->setAuthenticated(true);
			dprintf (D_SECURITY, "DC_AUTHENTICATE: authenticated UDP message is from %s.\n", who);
        }
	}


	stream->decode();

		// Determine if incoming socket is HTTP over TCP, or if it is CEDAR.
		// For better or worse, we figure this out by seeing if the socket
		// starts w/ a GET or POST.  Hopefully this does not correspond to
		// a daemoncore command int!  [not ever likely, since CEDAR ints are
		// exapanded out to 8 bytes]  Still, in a perfect world we would replace
		// with a more foolproof method.
	char tmpbuf[5];
	memset(tmpbuf,0,sizeof(tmpbuf));
	if ( is_tcp && (insock != stream) ) {
		int nro = condor_read(((Sock*)stream)->get_file_desc(),
			tmpbuf, sizeof(tmpbuf) - 1, 10, MSG_PEEK);
	}
	if ( strstr(tmpbuf,"GET") ) {
		if ( param_boolean("ENABLE_WEB_SERVER",false) ) {
			// mini-web server requires READ authorization.
			if ( Verify(READ,((Sock*)stream)->endpoint(),NULL) ) {
				is_http_get = true;
			} else {
				dprintf(D_ALWAYS,"Received HTTP GET connection from %s -- "
				             "DENIED because host not authorized for READ\n",
							 sin_to_string(((Sock*)stream)->endpoint()));
			}
		} else {
			dprintf(D_ALWAYS,"Received HTTP GET connection from %s -- "
				             "DENIED because ENABLE_WEB_SERVER=FALSE\n",
							 sin_to_string(((Sock*)stream)->endpoint()));
		}
	} else {
		if ( strstr(tmpbuf,"POST") ) {
			if ( param_boolean("ENABLE_SOAP",false) ) {
				// SOAP requires SOAP authorization.
				if ( Verify(SOAP_PERM,((Sock*)stream)->endpoint(),NULL) ) {
					is_http_post = true;
				} else {
					dprintf(D_ALWAYS,"Received HTTP POST connection from %s -- "
							"DENIED because host not authorized for SOAP\n",
							 sin_to_string(((Sock*)stream)->endpoint()));
				}
			} else {
				dprintf(D_ALWAYS,"Received HTTP POST connection from %s -- "
							 "DENIED because ENABLE_SOAP=FALSE\n",
							 sin_to_string(((Sock*)stream)->endpoint()));
			}
		}
	}
	if ( is_http_post || is_http_get )
	{
		struct soap *cursoap;

			// Socket appears to be HTTP, so deal with it.
		dprintf(D_ALWAYS, "Received HTTP %s connection from %s\n",
			is_http_get ? "GET" : "POST",
			sin_to_string(((Sock*)stream)->endpoint()) );


		cursoap = soap_copy(&soap);
		ASSERT(cursoap);

			// Mimic a gsoap soap_accept as follows:
			//   1. stash the socket descriptor in the soap object
			//   2. make socket non-blocking by setting a CEDAR timeout.
			//   3. increase size of send and receive buffers
			//   4. set SO_KEEPALIVE [done automatically by CEDAR accept()]
		cursoap->socket = ((Sock*)stream)->get_file_desc();
		cursoap->recvfd = soap.socket;
		cursoap->sendfd = soap.socket;
		if ( cursoap->recv_timeout > 0 ) {
			stream->timeout(soap.recv_timeout);
		} else {
			stream->timeout(20);
		}
		((Sock*)stream)->set_os_buffers(SOAP_BUFLEN,false);	// set read buf size
		((Sock*)stream)->set_os_buffers(SOAP_BUFLEN,true);	// set write buf size

			// Now, process the Soap RPC request and dispatch it
		dprintf(D_ALWAYS,"About to serve HTTP request...\n");
		soap_serve(cursoap);
		soap_destroy(cursoap); // clean up class instances
		soap_end(cursoap); // clean up everything and close socket
		free(cursoap);
		dprintf(D_ALWAYS, "Completed servicing HTTP request\n");

		((Sock*)stream)->_sock = INVALID_SOCKET; // so CEDAR won't close it again
		delete stream;	// clean up CEDAR socket
		CheckPrivState();	// Make sure we didn't leak our priv state
		curr_dataptr = NULL; // Clear curr_dataptr
			// Return KEEP_STEAM cuz we wanna keep our listen socket open
		return KEEP_STREAM;
	}

	if (only_allow_soap) {
		dprintf(D_ALWAYS,
			"Received CEDAR command during SOAP transaction... queueing\n");
		if ( stream != insock ) {
				// we did an accept, so we know this is TCP

			dprintf(D_DAEMONCORE,
					"stream fd being queued: %d\n",
					((Sock *) stream)->get_file_desc());
			Register_Command_Socket(stream);
		}
		return KEEP_STREAM;
	}

	// read in the command from the stream with a timeout value of 20 seconds
	old_timeout = stream->timeout(20);
	result = stream->code(req);
	// For now, lets keep the timeout, so all command handlers are called with
	// a timeout of 20 seconds on their socket.
	// stream->timeout(old_timeout);
	if(!result) {
		dprintf(D_ALWAYS,
			"DaemonCore: Can't receive command request (perhaps a timeout?)\n");
		if ( insock != stream )	{   // delete stream only if we did an accept
			delete stream;
		} else {
			stream->set_crypto_key(false, NULL);
			stream->set_MD_mode(MD_OFF, NULL);
			stream->end_of_message();
		}
		return KEEP_STREAM;
	}

	if (req == DC_AUTHENTICATE) {

		Sock* sock = (Sock*)stream;
		sock->decode();

		dprintf (D_SECURITY, "DC_AUTHENTICATE: received DC_AUTHENTICATE from %s\n", sin_to_string(sock->endpoint()));

		ClassAd auth_info;
		if( !auth_info.initFromStream(*sock)) {
			dprintf (D_ALWAYS, "ERROR: DC_AUTHENTICATE unable to "
					   "receive auth_info!\n");
			result = FALSE;
			goto finalize;
		}

		if ( is_tcp && !sock->end_of_message()) {
			dprintf (D_ALWAYS, "ERROR: DC_AUTHENTICATE is TCP, unable to "
					   "receive eom!\n");
			result = FALSE;
			goto finalize;
		}

		if (DebugFlags & D_FULLDEBUG) {
			dprintf (D_SECURITY, "DC_AUTHENTICATE: received following ClassAd:\n");
			auth_info.dPrint (D_SECURITY);
		}

		char buf[ATTRLIST_MAX_EXPRESSION];

		// look at the ad.  get the command number.
		int real_cmd = 0;
		int tmp_cmd = 0;
		auth_info.LookupInteger(ATTR_SEC_COMMAND, real_cmd);

		if (real_cmd == DC_AUTHENTICATE) {
			// we'll set tmp_cmd temporarily to
			auth_info.LookupInteger(ATTR_SEC_AUTH_COMMAND, tmp_cmd);
		} else {
			tmp_cmd = real_cmd;
		}

		// get the auth level of this command
		// locate the hash table entry
		int cmd_index = 0;

		// first compute the hash
		if ( tmp_cmd < 0 )
			cmd_index = -tmp_cmd % maxCommand;
		else
			cmd_index = tmp_cmd % maxCommand;

		int cmdFound = FALSE;
		if (comTable[cmd_index].num == tmp_cmd) {
			// hash found it first try... cool
			cmdFound = TRUE;
		} else {
			// hash did not find it, search for it
			for (j = (cmd_index + 1) % maxCommand; j != cmd_index; j = (j + 1) % maxCommand) {
				if(comTable[j].num == tmp_cmd) {
					cmdFound = TRUE;
					cmd_index = j;
					break;
				}
			}
		}

		if (!cmdFound) {
			// we have no idea what command they want to send.
			// too bad, bye bye
			result = FALSE;
			goto finalize;
		}

		bool new_session        = false;
		bool using_cookie       = false;
		bool valid_cookie		= false;

		// check if we are using a cookie
		char *incoming_cookie   = NULL;
		if( auth_info.LookupString(ATTR_SEC_COOKIE, &incoming_cookie)) {
			// compare it to the one we have internally

			valid_cookie = cookie_is_valid((unsigned char*)incoming_cookie);
			free (incoming_cookie);

			if ( valid_cookie ) {
				// we have a match... trust this command.
				using_cookie = true;
			} else {
				// bad cookie!!!
				dprintf ( D_ALWAYS, "DC_AUTHENTICATE: recieved invalid cookie!!!\n");
				result = FALSE;
				goto finalize;
			}
		}

		// check if we are restarting a cached session

		if (!using_cookie) {

			if ( sec_man->sec_lookup_feat_act(auth_info, ATTR_SEC_USE_SESSION) == SecMan::SEC_FEAT_ACT_YES) {

				KeyCacheEntry *session = NULL;

				if( ! auth_info.LookupString(ATTR_SEC_SID, &the_sid)) {
					dprintf (D_ALWAYS, "ERROR: DC_AUTHENTICATE unable to "
							   "extract auth_info.%s!\n", ATTR_SEC_SID);
					result = FALSE;
					goto finalize;
				}

				// lookup the suggested key
				if (!sec_man->session_cache->lookup(the_sid, session)) {

					// the key id they sent was not in our cache.  this is a
					// problem.

					dprintf (D_ALWAYS, "DC_AUTHENTICATE: attempt to open "
							   "invalid session %s, failing.\n", the_sid);

					char * return_addr = NULL;
					if( auth_info.LookupString(ATTR_SEC_SERVER_COMMAND_SOCK, &return_addr)) {
						sec_man->send_invalidate_packet( return_addr, the_sid );
						free (return_addr);
					}

					// close the connection.
					result = FALSE;
					goto finalize;

				} else {
					// the session->id() and the_sid strings should be identical.

					dprintf (D_SECURITY, "DC_AUTHENTICATE: resuming session id %s given to %s:\n",
								session->id(), sin_to_string(session->addr()));
				}

				if (session->key()) {
					// copy this to the HandleReq() scope
					the_key = new KeyInfo(*session->key());
				}

				if (session->policy()) {
					// copy this to the HandleReq() scope
					the_policy = new ClassAd(*session->policy());
					if (DebugFlags & D_FULLDEBUG) {
						dprintf (D_SECURITY, "DC_AUTHENTICATE: Cached Session:\n");
						the_policy->dPrint (D_SECURITY);
					}
				}

				// grab the user out of the policy.
				if (the_policy) {
					char *the_user  = NULL;
					the_policy->LookupString( ATTR_SEC_USER, &the_user);

					if (the_user) {
						// copy this to the HandleReq() scope
						strcpy (user, the_user);
						free( the_user );
						the_user = NULL;
					}
				}
				new_session = false;

			} else {
					// they did not request a cached session.  see if they
					// want to start one.  look at our security policy.
				ClassAd our_policy;
				if( ! sec_man->FillInSecurityPolicyAd(
					  PermString(comTable[cmd_index].perm), &our_policy) ) {
						// our policy is invalid even without the other
						// side getting involved.
					dprintf( D_ALWAYS, "DC_AUTHENTICATE: "
							 "Our security policy is invalid!\n" );
					result = FALSE;
					goto finalize;
				}

				if (DebugFlags & D_FULLDEBUG) {
					dprintf ( D_SECURITY, "DC_AUTHENTICATE: our_policy:\n" );
					our_policy.dPrint(D_SECURITY);
				}

				// reconcile.  if unable, close socket.
				the_policy = sec_man->ReconcileSecurityPolicyAds( auth_info,
																  our_policy );

				if (!the_policy) {
					dprintf(D_ALWAYS, "DC_AUTHENTICATE: Unable to reconcile!\n");
					result = FALSE;
					goto finalize;
				} else {
					if (DebugFlags & D_FULLDEBUG) {
						dprintf ( D_SECURITY, "DC_AUTHENTICATE: the_policy:\n" );
						the_policy->dPrint(D_SECURITY);
					}
				}

				// add our version to the policy to be sent over
				sprintf (buf, "%s=\"%s\"", ATTR_SEC_REMOTE_VERSION, CondorVersion());
				the_policy->InsertOrUpdate(buf);

				// handy policy vars
				SecMan::sec_feat_act will_authenticate      = sec_man->sec_lookup_feat_act(*the_policy, ATTR_SEC_AUTHENTICATION);

				if (sec_man->sec_lookup_feat_act(auth_info, ATTR_SEC_NEW_SESSION) == SecMan::SEC_FEAT_ACT_YES) {

					// generate a new session

					int    mypid = 0;
#ifdef WIN32
					mypid = ::GetCurrentProcessId();
#else
					mypid = ::getpid();
#endif

					// generate a unique ID.
					sprintf( buf, "%s:%i:%i:%i", my_hostname(), mypid,
							 (int)time(0), ZZZ_always_increase() );
					assert (the_sid == NULL);
					the_sid = strdup(buf);

					if (will_authenticate == SecMan::SEC_FEAT_ACT_YES) {

						char *crypto_method = NULL;
						if (!the_policy->LookupString(ATTR_SEC_CRYPTO_METHODS, &crypto_method)) {
							dprintf ( D_ALWAYS, "DC_AUTHENTICATE: tried to enable encryption but we have none!\n" );
							result = FALSE;
							goto finalize;
						}

						unsigned char* rkey = Condor_Crypt_Base::randomKey(24);
						unsigned char  rbuf[24];
						if (rkey) {
							memcpy (rbuf, rkey, 24);
							// this was malloced in randomKey
							free (rkey);
						} else {
							memset (rbuf, 0, 24);
							dprintf ( D_SECURITY, "DC_AUTHENTICATE: unable to generate key - no crypto available!\n");
							result = FALSE;
							free( crypto_method );
							crypto_method = NULL;
							goto finalize;
						}

						switch (toupper(crypto_method[0])) {
							case 'B': // blowfish
								dprintf (D_SECURITY, "DC_AUTHENTICATE: generating BLOWFISH key for session %s...\n", the_sid);
								the_key = new KeyInfo(rbuf, 24, CONDOR_BLOWFISH);
								break;
							case '3': // 3des
							case 'T': // Tripledes
								dprintf (D_SECURITY, "DC_AUTHENTICATE: generating 3DES key for session %s...\n", the_sid);
								the_key = new KeyInfo(rbuf, 24, CONDOR_3DES);
								break;
							default:
								dprintf ( D_SECURITY, "DC_AUTHENTICATE: this version doesn't support %s crypto.\n", crypto_method );
								break;
						}

						free( crypto_method );
						crypto_method = NULL;

						if (!the_key) {
							result = FALSE;
							goto finalize;
						}

#ifdef SECURITY_HACK_ENABLE
						zz2printf (the_key);
#endif
					}

					new_session = true;
				}

				// if they asked, tell them
				if (is_tcp && (sec_man->sec_lookup_feat_act(auth_info, ATTR_SEC_ENACT) == SecMan::SEC_FEAT_ACT_NO)) {
					if (DebugFlags & D_FULLDEBUG) {
						dprintf (D_SECURITY, "SECMAN: Sending following response ClassAd:\n");
						the_policy->dPrint( D_SECURITY );
					}
					sock->encode();
					if (!the_policy->put(*sock) ||
						!sock->eom()) {
						dprintf (D_ALWAYS, "SECMAN: Error sending response classad!\n");
						result = FALSE;
						goto finalize;
					}
					sock->decode();
				} else {
					dprintf( D_SECURITY, "SECMAN: Enact was '%s', not sending response.\n",
						SecMan::sec_feat_act_rev[sec_man->sec_lookup_feat_act(auth_info, ATTR_SEC_ENACT)] );
				}

			}

			if (is_tcp) {

				// do what we decided

				// handy policy vars
				SecMan::sec_feat_act will_authenticate      = sec_man->sec_lookup_feat_act(*the_policy, ATTR_SEC_AUTHENTICATION);
				SecMan::sec_feat_act will_enable_encryption = sec_man->sec_lookup_feat_act(*the_policy, ATTR_SEC_ENCRYPTION);
				SecMan::sec_feat_act will_enable_integrity  = sec_man->sec_lookup_feat_act(*the_policy, ATTR_SEC_INTEGRITY);


				// protocol fix:
				//
				// up to and including 6.6.0, will_authenticate would be set to
				// true if we are resuming a session that was authenticated.
				// this is not necessary.
				//
				// so, as of 6.6.1, if we are resuming a session (as determined
				// by the expression (!new_session), AND the other side is
				// 6.6.1 or higher, we will force will_authenticate to
				// SEC_FEAT_ACT_NO.

				if ((will_authenticate == SecMan::SEC_FEAT_ACT_YES)) {
					if ((!new_session)) {
						char * remote_version = NULL;
						the_policy->LookupString(ATTR_SEC_REMOTE_VERSION, &remote_version);
						if(remote_version) {
							// this attribute was added in 6.6.1.  it's mere
							// presence means that the remote side is 6.6.1 or
							// higher, so no need to instantiate a CondorVersionInfo.
							dprintf( D_SECURITY, "SECMAN: other side is %s, NOT reauthenticating.\n", remote_version );
							will_authenticate = SecMan::SEC_FEAT_ACT_NO;

							free (remote_version);
						} else {
							dprintf( D_SECURITY, "SECMAN: other side is pre 6.6.1, reauthenticating.\n" );
						}
					} else {
						dprintf( D_SECURITY, "SECMAN: new session, doing initial authentication.\n" );
					}
				}



				if (is_tcp && (will_authenticate == SecMan::SEC_FEAT_ACT_YES)) {

					// we are going to authenticate.  this could one of two ways.
					// the "real" way or the "quick" way which is by presenting a
					// session ID.  the fact that the private key matches on both
					// sides proves the authenticity.  if the key does not match,
					// it will be detected as long as some crypto is used.


					// we know the ..METHODS_LIST attribute exists since it was put
					// in by us.  pre 6.5.0 protocol does not put it in.
					char * auth_methods = NULL;
					the_policy->LookupString(ATTR_SEC_AUTHENTICATION_METHODS_LIST, &auth_methods);

					if (!auth_methods) {
						dprintf (D_SECURITY, "DC_AUTHENTICATE: no auth methods in response ad, failing!\n");
						result = FALSE;
						goto finalize;
					}

					if (DebugFlags & D_FULLDEBUG) {
						dprintf (D_SECURITY, "DC_AUTHENTICATE: authenticating RIGHT NOW.\n");
					}

					if (!sock->authenticate(the_key, auth_methods, &errstack)) {
						free( auth_methods );
						dprintf( D_ALWAYS,
								 "DC_AUTHENTICATE: authenticate failed: %s\n",
								 errstack.getFullText() );
						result = FALSE;
						goto finalize;
					}
					free( auth_methods );

					// check to see if the auth IP is the same
					// as the socket IP.  this cast is safe because
					// we return above if sock is not a ReliSock.
					if ( ((ReliSock*)sock)->authob ) {

						// after authenticating, update the classad to reflect
						// which method we actually used.
						char* the_method = ((ReliSock*)sock)->authob->getMethodUsed();
						sprintf(buf, "%s=\"%s\"", ATTR_SEC_AUTHENTICATION_METHODS, the_method);
						the_policy->InsertOrUpdate(buf);

						const char* sockip = sin_to_string(sock->endpoint());
						const char* authip = ((ReliSock*)sock)->authob->getRemoteAddress() ;

						result = !strncmp (sockip + 1, authip, strlen(authip) );

						if (!result && !param_boolean( "DISABLE_AUTHENTICATION_IP_CHECK", false)) {
							dprintf (D_ALWAYS, "DC_AUTHENTICATE: sock ip -> %s\n", sockip);
							dprintf (D_ALWAYS, "DC_AUTHENTICATE: auth ip -> %s\n", authip);
							dprintf (D_ALWAYS, "DC_AUTHENTICATE: ERROR: IP not in agreement!!! BAILING!\n");

							result = FALSE;
							goto finalize;

						} else {
							dprintf (D_SECURITY, "DC_AUTHENTICATE: mutual authentication to %s complete.\n", authip);
						}
					}

				} else {
					if (DebugFlags & D_FULLDEBUG) {
						dprintf (D_SECURITY, "DC_AUTHENTICATE: not authenticating.\n");
					}
				}


				if (will_enable_integrity == SecMan::SEC_FEAT_ACT_YES) {

					if (!the_key) {
						// uhm, there should be a key here!
						result = FALSE;
						goto finalize;
					}

					sock->decode();
					if (!sock->set_MD_mode(MD_ALWAYS_ON, the_key)) {
						dprintf (D_ALWAYS, "DC_AUTHENTICATE: unable to turn on message authenticator, failing.\n");
						result = FALSE;
						goto finalize;
					} else {
						dprintf (D_SECURITY, "DC_AUTHENTICATE: message authenticator enabled with key id %s.\n", the_sid);
#ifdef SECURITY_HACK_ENABLE
						zz2printf (the_key);
#endif
					}
				} else {
					sock->set_MD_mode(MD_OFF, the_key);
				}


				if (will_enable_encryption == SecMan::SEC_FEAT_ACT_YES) {

					if (!the_key) {
						// uhm, there should be a key here!
						result = FALSE;
						goto finalize;
					}

					sock->decode();
					if (!sock->set_crypto_key(true, the_key) ) {
						dprintf (D_ALWAYS, "DC_AUTHENTICATE: unable to turn on encryption, failing.\n");
						result = FALSE;
						goto finalize;
					} else {
						dprintf (D_SECURITY, "DC_AUTHENTICATE: encryption enabled for session %s\n", the_sid);
					}
				} else {
					sock->set_crypto_key(false, the_key);
				}


				if (new_session) {
					// clear the buffer
					sock->decode();
					sock->eom();

					// ready a classad to send
					ClassAd pa_ad;

					// session user
					const char *fully_qualified_user = ((ReliSock*)sock)->getFullyQualifiedUser();
					if ( fully_qualified_user ) {
						sprintf (buf, "%s=\"%s\"", ATTR_SEC_USER,
								fully_qualified_user);
						pa_ad.Insert(buf);
					}

					// session id
					sprintf (buf, "%s=\"%s\"", ATTR_SEC_SID, the_sid);
					pa_ad.Insert(buf);

					// other commands this session is good for
					sprintf (buf, "%s=\"%s\"", ATTR_SEC_VALID_COMMANDS, GetCommandsInAuthLevel(comTable[cmd_index].perm).Value());
					pa_ad.Insert(buf);

					// also put some attributes in the policy classad we are caching.
					sec_man->sec_copy_attribute( *the_policy, auth_info, ATTR_SEC_SUBSYSTEM );
					sec_man->sec_copy_attribute( *the_policy, auth_info, ATTR_SEC_SERVER_COMMAND_SOCK );
					sec_man->sec_copy_attribute( *the_policy, auth_info, ATTR_SEC_PARENT_UNIQUE_ID );
					sec_man->sec_copy_attribute( *the_policy, auth_info, ATTR_SEC_SERVER_PID );
					// it matters if the version is empty, so we must explicitly delete it
					the_policy->Delete( ATTR_SEC_REMOTE_VERSION );
					sec_man->sec_copy_attribute( *the_policy, auth_info, ATTR_SEC_REMOTE_VERSION );
					sec_man->sec_copy_attribute( *the_policy, pa_ad, ATTR_SEC_USER );
					sec_man->sec_copy_attribute( *the_policy, pa_ad, ATTR_SEC_SID );
					sec_man->sec_copy_attribute( *the_policy, pa_ad, ATTR_SEC_VALID_COMMANDS );

					if (DebugFlags & D_FULLDEBUG) {
						dprintf (D_SECURITY, "DC_AUTHENTICATE: sending session ad:\n");
						pa_ad.dPrint( D_SECURITY );
					}

					sock->encode();
					if (! pa_ad.put(*sock) ||
						! sock->eom() ) {
						dprintf (D_SECURITY, "DC_AUTHENTICATE: unable to send session %s info!\n", the_sid);
					} else {
						if (DebugFlags & D_FULLDEBUG) {
							dprintf (D_SECURITY, "DC_AUTHENTICATE: sent session %s info!\n", the_sid);
						}
					}

					// extract the session duration
					char *dur = NULL;
					the_policy->LookupString(ATTR_SEC_SESSION_DURATION, &dur);

					int expiration_time = time(0) + atoi(dur);

					// add the key to the cache
					KeyCacheEntry tmp_key(the_sid, sock->endpoint(), the_key, the_policy, expiration_time);
					sec_man->session_cache->insert(tmp_key);
					dprintf (D_SECURITY, "DC_AUTHENTICATE: added session id %s to cache for %s seconds!\n", the_sid, dur);
					if (DebugFlags & D_FULLDEBUG) {
						the_policy->dPrint(D_SECURITY);
					}

					free( dur );
					dur = NULL;
				}
			}
		}

		if (real_cmd == DC_AUTHENTICATE) {
			result = TRUE;
			goto finalize;
		}

		req = real_cmd;
		result = TRUE;

		sock->decode();
		sock->allow_one_empty_message();

		if (DebugFlags & D_FULLDEBUG) {
			dprintf (D_SECURITY, "DC_AUTHENTICATE: setting sock->decode()\n");
			dprintf (D_SECURITY, "DC_AUTHENTICATE: allowing an empty message for sock.\n");
		}

		// fill in the command info
		reqFound = TRUE;
		index = cmd_index;

		dprintf (D_SECURITY, "DC_AUTHENTICATE: Success.\n");
	} else {

		// get the handler function

		// first compute the hash
		if ( req < 0 ) {
			index = -req % maxCommand;
		} else {
			index = req % maxCommand;
		}

		reqFound = FALSE;
		if (comTable[index].num == req) {
			// hash found it first try... cool
			reqFound = TRUE;
		} else {
			// hash did not find it, search for it
			for (j = (index + 1) % maxCommand; j != index; j = (j + 1) % maxCommand) {
				if(comTable[j].num == req) {
					reqFound = TRUE;
					index = j;
					break;
				}
			}
		}


		if (reqFound) {
			// need to check our security policy to see if this is allowed.

			dprintf (D_SECURITY, "DaemonCore received UNAUTHENTICATED command %i.\n", req);

			// if the command was registered as "ALLOW", then it doesn't matter what the
			// security policy says, we just allow it.
			if (comTable[index].perm != ALLOW) {

				ClassAd our_policy;
				if( ! sec_man->FillInSecurityPolicyAd( PermString(comTable[index].perm), &our_policy) ) {
					dprintf( D_ALWAYS, "DC_AUTHENTICATE: "
							 "Our security policy is invalid!\n" );
					result = FALSE;
					goto finalize;
				}

				// well, they didn't authenticate, turn on encryption,
				// or turn on integrity.  check to see if any of those
				// were required.

				if (  (sec_man->sec_lookup_req(our_policy, ATTR_SEC_NEGOTIATION)
					   == SecMan::SEC_REQ_REQUIRED)
				   || (sec_man->sec_lookup_req(our_policy, ATTR_SEC_AUTHENTICATION)
					   == SecMan::SEC_REQ_REQUIRED)
				   || (sec_man->sec_lookup_req(our_policy, ATTR_SEC_ENCRYPTION)
					   == SecMan::SEC_REQ_REQUIRED)
				   || (sec_man->sec_lookup_req(our_policy, ATTR_SEC_INTEGRITY)
					   == SecMan::SEC_REQ_REQUIRED) ) {

					// yep, they were.  deny.

					dprintf(D_ALWAYS,
						"DaemonCore: PERMISSION DENIED for %d via %s%s%s from host %s\n",
						req,
						(is_tcp) ? "TCP" : "UDP",
						(user[0] != '\0') ? " from " : "",
						(user[0] != '\0') ? user : "",
						sin_to_string(((Sock*)stream)->endpoint()) );

					result = FALSE;
					goto finalize;
				}
			}
		}
	}


	if ( reqFound == TRUE ) {

		// Check the daemon core permission for this command handler

		// grab the user from the socket
        if (is_tcp) {
            const char *t = ((ReliSock*)stream)->getFullyQualifiedUser();
			if (t) {
				strcpy(user, t);
			}
        } else {
			// user is filled in above, but we should make it part of
			// the SafeSock too.
			((SafeSock*)stream)->setFullyQualifiedUser(user);
            ((SafeSock*)stream)->setAuthenticated(true);
		}

		if ( (perm = Verify(comTable[index].perm, ((Sock*)stream)->endpoint(), user)) != USER_AUTH_SUCCESS )
		{
			// Permission check FAILED
			reqFound = FALSE;	// so we do not call the handler function below
			// make result != to KEEP_STREAM, so we blow away this socket below
			result = 0;
			dprintf( D_ALWAYS,
                     "DaemonCore: PERMISSION DENIED to %s from host %s for command %d (%s)\n",
                     (user[0] == '\0')? "unknown user" : user, sin_to_string(((Sock*)stream)->endpoint()), req,
                     comTable[index].command_descrip );
			// if UDP, consume the rest of this message to try to stay "in-sync"
			if ( !is_tcp)
				stream->end_of_message();

		} else {
			dprintf(comTable[index].dprintf_flag,
					"DaemonCore: Command received via %s%s%s from host %s\n",
					(is_tcp) ? "TCP" : "UDP",
					(user[0] != '\0') ? " from " : "",
					(user[0] != '\0') ? user : "",
					sin_to_string(((Sock*)stream)->endpoint()) );
			dprintf(comTable[index].dprintf_flag,
                    "DaemonCore: received command %d (%s), calling handler (%s)\n",
                    req, comTable[index].command_descrip,
                    comTable[index].handler_descrip);
		}

	} else {
		dprintf(comTable[index].dprintf_flag,
				"DaemonCore: Command received via %s%s%s from host %s\n",
				(is_tcp) ? "TCP" : "UDP",
				(user[0] != '\0') ? " from " : "",
				(user[0] != '\0') ? user : "",
				sin_to_string(((Sock*)stream)->endpoint()) );
		dprintf(D_ALWAYS,
			"DaemonCore: received unregistered command request %d !\n",req);
		// make result != to KEEP_STREAM, so we blow away this socket below
		result = 0;
		// if UDP, consume the rest of this message to try to stay "in-sync"
		if ( !is_tcp)
			stream->end_of_message();
	}
/*
    // Send authorization message
    if (is_tcp) {
        stream->encode();
        if (!stream->code(perm) || !stream->end_of_message()) {
            dprintf(D_ALWAYS, "DaemonCore: Unable to send permission results\n");
        }
    }
*/
	if ( reqFound == TRUE ) {
		// call the handler function; first curr_dataptr for GetDataPtr()
		curr_dataptr = &(comTable[index].data_ptr);

		if ( comTable[index].is_cpp ) {
			// the handler is c++ and belongs to a 'Service' class
			if ( comTable[index].handlercpp )
				result = (comTable[index].service->*(comTable[index].handlercpp))(req,stream);
		} else {
			// the handler is in c (not c++), so pass a Service pointer
			if ( comTable[index].handler )
				result = (*(comTable[index].handler))(comTable[index].service,req,stream);
		}

		// clear curr_dataptr
		curr_dataptr = NULL;
	}

finalize:

	// finalize; the handler is done with the command.  the handler will return
	// with KEEP_STREAM if we should not touch the stream; otherwise, cleanup
	// the stream.  On tcp, we just delete it since the stream is the one we got
	// from accept and our listen socket is still out there.  on udp,
	// however, we cannot just delete it or we will not be "listening"
	// anymore, so we just do an eom flush all buffers, etc.
	// HACK: keep all UDP sockets as well for now.
    if (the_policy) {
        delete the_policy;
    }
    if (the_key) {
        delete the_key;
    }
    if (the_sid) {
        free(the_sid);
    }
    if (who) {
        free(who);
    }
	if ( result != KEEP_STREAM ) {
		stream->encode();	// we wanna "flush" below in the encode direction
		if ( is_tcp ) {
			stream->end_of_message();  // make certain data flushed to the wire
			if ( insock != stream )	   // delete the stream only if we did an accept; if we
				delete stream;		   //     did not do an accept, Driver() will delete the stream.
		} else {
			stream->end_of_message();

			// we need to reset the crypto keys
			stream->set_MD_mode(MD_OFF);
			stream->set_crypto_key(false, NULL);

			result = KEEP_STREAM;	// HACK: keep all UDP sockets for now.  The only ones
									// in Condor so far are Initial command socks, so keep it.
		}
	} else {
		if (!is_tcp) {
			stream->end_of_message();
			stream->set_MD_mode(MD_OFF);
			stream->set_crypto_key(false, NULL);
		}
	}

	// Now return KEEP_STREAM only if the user said to _OR_ if insock
	// is a listen socket.  Why?  we always wanna keep a listen socket.
	// Also, if we did an accept, we already deleted the stream socket above.
	if ( result == KEEP_STREAM || insock != stream )
		return KEEP_STREAM;
	else
		return TRUE;
}


int DaemonCore::HandleSigCommand(int command, Stream* stream) {
	int sig;

	assert( command == DC_RAISESIGNAL );

	// We have been sent a DC_RAISESIGNAL command

	// read the signal number from the socket
	if (!stream->code(sig))
		return FALSE;

	stream->end_of_message();

	// and call HandleSig to raise the signal
	return( HandleSig(_DC_RAISESIGNAL,sig) );
}

int DaemonCore::HandleSig(int command,int sig)
{
	int j,index;
	int sigFound;

	// find the signal entry in our table
	// first compute the hash
	if ( sig < 0 )
		index = -sig % maxSig;
	else
		index = sig % maxSig;

	sigFound = FALSE;
	if (sigTable[index].num == sig) {
		// hash found it first try... cool
		sigFound = TRUE;
	} else {
		// hash did not find it, search for it
		for (j = (index + 1) % maxSig; j != index; j = (j + 1) % maxSig)
			if(sigTable[j].num == sig) {
				sigFound = TRUE;
				index = j;
				break;
			}
	}

	if ( sigFound == FALSE ) {
		dprintf(D_ALWAYS,
			"DaemonCore: received request for unregistered Signal %d !\n",sig);
		return FALSE;
	}

	switch (command) {
		case _DC_RAISESIGNAL:
			dprintf(D_DAEMONCORE,
				"DaemonCore: received Signal %d (%s), raising event %s\n", sig,
				sigTable[index].sig_descrip, sigTable[index].handler_descrip);
			// set this signal entry to is_pending.
			// the code to actually call the handler is
			// in the Driver() method.
			sigTable[index].is_pending = TRUE;
			break;
		case _DC_BLOCKSIGNAL:
			sigTable[index].is_blocked = TRUE;
			break;
		case _DC_UNBLOCKSIGNAL:
			sigTable[index].is_blocked = FALSE;
			// now check to see if this signal we are unblocking is pending.
			// if so, set sent_signal to TRUE.  sent_signal is used by the
			// Driver() to ensure that a signal raised from inside a
			// signal handler is indeed delivered.
			if ( sigTable[index].is_pending == TRUE )
				sent_signal = TRUE;
			break;
		default:
			dprintf(D_DAEMONCORE,
				"DaemonCore: HandleSig(): unrecognized command\n");
			return FALSE;
			break;
	}	// end of switch (command)


	return TRUE;
}

int DaemonCore::Send_Signal(pid_t pid, int sig)
{
	PidEntry * pidinfo;
	int same_thread, is_local;
	char *destination;
	Stream* sock;
	int target_has_dcpm = TRUE;		// is process pid a daemon core process?

	// sanity check on the pid.  we don't want to do something silly like
	// kill pid -1 because the pid has not been initialized yet.
	int signed_pid = (int) pid;
	if ( signed_pid > -10 && signed_pid < 3 ) {
		EXCEPT("Send_Signal: sent unsafe pid (%d)",signed_pid);
	}

	// First, if not sending a signal to ourselves, lookup the PidEntry struct
	// so we can determine if our child is a daemon core process or not.
	if ( pid != mypid ) {
		if ( pidTable->lookup(pid,pidinfo) < 0 ) {
			// we did not find this pid in our hashtable
			pidinfo = NULL;
			target_has_dcpm = FALSE;
		}
		if ( pidinfo && pidinfo->sinful_string[0] == '\0' ) {
			// process pid found in our table, but does not
			// our table says it does _not_ have a command socket
			target_has_dcpm = FALSE;
		}
	}

	// handle the "special" action signals which are really just telling
	// DaemonCore to do something.
	switch (sig) {
		case SIGTERM:
			if ( pid != mypid ) {
					/*
					   If the pid you're shutting down is a DaemonCore
					   process (including ourself) Shutdown_Graceful()
					   just turns around and sends some kind of
					   SIGTERM.  This would result in an infinite
					   loop.  So, instead of using the special
					   shutdown method, we just fall through and
					   actually send a real DC SIGTERM to ourselves.
					   We want to do this here, since on UNIX there's
					   more to it than just raising the signal, we
					   also want to write to the async pipe to make
					   sure select() wakes up, etc, etc.
					   -Derek Wright <wright@cs.wisc.edu> 5/17/02
					*/
				if ( target_has_dcpm == FALSE ) { // I think Derek forgot this
					return Shutdown_Graceful(pid); // -stolley 6/18/2002
				}
			}
			break;
		case SIGKILL:
			return Shutdown_Fast(pid);
			break;
		case SIGSTOP:
			return Suspend_Process(pid);
			break;
		case SIGCONT:
			return Continue_Process(pid);
			break;
		default:
#ifndef WIN32
			// If we are on Unix, and we are not sending a 'special' signal,
			// and our child is not a daemon-core process, then just send
			// the signal as usual via kill()
			if ( target_has_dcpm == FALSE ) {
				const char* tmp = signalName(sig);
				dprintf( D_DAEMONCORE,
						 "Send_Signal(): Doing kill(%d,%d) [%s]\n",
						 pid, sig, tmp ? tmp : "Unknown" );
				priv_state priv = set_root_priv();
				int status = ::kill(pid, sig);
				set_priv(priv);
				// return 1 if kill succeeds, 0 otherwise
				return (status >= 0);
			}
#endif  // not defined Win32
			break;
	}

	// a Signal is sent via UDP if going to a different process or
	// thread on the same machine.  it is sent via TCP if going to
	// a process on a remote machine.  if the signal is being sent
	// to ourselves (i.e. this process), then just twiddle
	// the signal table and set sent_signal to TRUE.  sent_signal is used by the
	// Driver() to ensure that a signal raised from inside a signal handler is
	// indeed delivered.

#ifdef WIN32
	if ( dcmainThreadId == ::GetCurrentThreadId() )
		same_thread = TRUE;
	else
		same_thread = FALSE;
#else
	// On Unix, we only support one thread inside daemons for now...
	same_thread = TRUE;
#endif

	// handle the case of sending a signal to the same process
	if ( pid == mypid ) {
		if ( same_thread == TRUE ) {
			// send signal to ourselves, same process & thread.
			// no need to go via UDP/TCP, just call HandleSig directly.
			HandleSig(_DC_RAISESIGNAL,sig);
			sent_signal = TRUE;
#ifndef WIN32
			// On UNIX, if async_sigs_unblocked == TRUE, we are being invoked
			// from inside of a unix signal handler.  So we also need to write
			// something to the async_pipe.  It does not matter what we write,
			// we just need to write something to ensure that the
			// select() in Driver() does not block.
			if ( async_sigs_unblocked == TRUE ) {
				write(async_pipe[1],"!",1);
			}
#endif
			return TRUE;
		} else {
			// send signal to same process, different thread.
			// we will still need to go out via UDP so that our call
			// to select() returns.
			destination = InfoCommandSinfulString();
			is_local = TRUE;
		}
	}

	// handle case of sending to a child process; get info on this pid
	if ( pid != mypid ) {
		if ( target_has_dcpm == FALSE || pidinfo == NULL) {
			// this child process does not have a command socket
			dprintf(D_ALWAYS,
				"Send_Signal: ERROR Attempt to send signal %d to pid %d, but pid %d has no command socket\n",
				sig,pid,pid);
			return FALSE;
		}

		is_local = pidinfo->is_local;
		destination = pidinfo->sinful_string;
	}

	Daemon d( DT_ANY, destination );
	// now destination process is local, send via UDP; if remote, send via TCP
	if ( is_local == TRUE ) {
		sock = (Stream *)(d.startCommand(DC_RAISESIGNAL, Stream::safe_sock, 3));
	} else {
		sock = (Stream *)(d.startCommand(DC_RAISESIGNAL, Stream::reli_sock, 20));
	}

	if (!sock) {
		dprintf(D_ALWAYS,"Send_Signal: ERROR Connect to %s failed.\n",
				destination);
		return FALSE;
	}

	// send the signal out as a DC_RAISESIGNAL command
	sock->encode();
	if ( (!sock->code(sig)) ||
		 (!sock->end_of_message()) ) {
		dprintf(D_ALWAYS,
				"Send_Signal: ERROR sending signal %d to pid %d\n",sig,pid);
		delete sock;
		return FALSE;
	}

	delete sock;

	dprintf(D_DAEMONCORE,
					"Send_Signal: sent signal %d to pid %d\n",sig,pid);
	return TRUE;
}

int DaemonCore::Shutdown_Fast(pid_t pid)
{

	dprintf(D_PROCFAMILY,"called DaemonCore::Shutdown_Fast(%d)\n",
		pid);

	if ( pid == ppid )
		return FALSE;		// cannot shut down our parent

    // Clear sessions associated with the child
    clearSession(pid);

#if defined(WIN32)
	// even on a shutdown_fast, first try to send a WM_CLOSE because
	// when we call TerminateProcess, any DLL's do not get a chance to
	// free allocated memory.
	if ( Shutdown_Graceful(pid) == TRUE ) {
		// we successfully sent a WM_CLOSE.
		// sleep a quarter of a second for the process to consume the WM_CLOSE
		// before we call TerminateProcess below.
		Sleep(250);
	}
	// now call TerminateProcess as a last resort
	PidEntry *pidinfo;
	HANDLE pidHandle;
	bool must_free_handle = false;
	int ret_value;
	if (pidTable->lookup(pid, pidinfo) < 0) {
		// could not find a handle to this pid in our table.
		// try to get a handle from the NT kernel
		pidHandle = ::OpenProcess(PROCESS_TERMINATE,FALSE,pid);
		if ( pidHandle == NULL ) {
			// process must have gone away.... we hope!!!!
			return FALSE;
		}
		must_free_handle = true;
	} else {
		// found this pid on our table
		pidHandle = pidinfo->hProcess;
	}

	if( (DebugFlags & D_PROCFAMILY) && (DebugFlags & D_FULLDEBUG) ) {
			char check_name[_POSIX_PATH_MAX];
			CSysinfo sysinfo;
			sysinfo.GetProcessName(pid,check_name, _POSIX_PATH_MAX);
			dprintf(D_PROCFAMILY,
				"Shutdown_Fast(%d):calling TerminateProcess handle=%x check_name='%s'\n",
				pid,pidHandle,check_name);
	}

	if (TerminateProcess(pidHandle,0)) {
		dprintf(D_PROCFAMILY,
			"Shutdown_Fast:Successfully terminated pid %d\n", pid);
		ret_value = TRUE;
	} else {
		// TerminateProcess failed!!!??!
		// should we try anything else here?
		dprintf(D_PROCFAMILY,
			"Shutdown_Fast: Failed to TerminateProcess on pid %d\n",pid);
		ret_value = FALSE;
	}
	if ( must_free_handle ) {
		::CloseHandle( pidHandle );
	}
	return ret_value;
#else
	priv_state priv = set_root_priv();
	int status = kill(pid, SIGKILL);
	set_priv(priv);
	return (status >= 0);		// return 1 if kill succeeds, 0 otherwise
#endif
}

int DaemonCore::Shutdown_Graceful(pid_t pid)
{
	dprintf(D_PROCFAMILY,"called DaemonCore::Shutdown_Graceful(%d)\n",
		pid);

	if ( pid == ppid )
		return FALSE;		// cannot shut down our parent

    // Clear sessions associated with the child
    clearSession(pid);

#if defined(WIN32)

	// WINDOWS

	PidEntry *pidinfo;
	PidEntry tmp_pidentry;

	if ( (pidTable->lookup(pid, pidinfo) < 0) ) {

		// This pid was not created with DaemonCore Create_Process, and thus
		// we do not have a PidEntry for it.  So we set pidinfo to point to a
		// dummy PidEntry which has just enough info setup so we can send
		// a WM_CLOSE.
		pidinfo = &tmp_pidentry;
		pidinfo->pid = pid;
		pidinfo->hWnd = NULL;
		pidinfo->sinful_string[0] = '\0';
	}

	if ( pidinfo->sinful_string[0] != '\0' ) {
		// pid is a DaemonCore Process; send it a SIGTERM signal instead.
		dprintf(D_PROCFAMILY,
					"Shutdown_Graceful: Sending pid %d SIGTERM\n",pid);
		return Send_Signal(pid,SIGTERM);
	}

	// Find the Window Handle to this pid, if we don't already know it
	if ( pidinfo->hWnd == NULL ) {

		// First, save the currently winsta and desktop
		HDESK hdesk;
		HWINSTA hwinsta;
		hwinsta = GetProcessWindowStation();
		hdesk = GetThreadDesktop( GetCurrentThreadId() );

		if ( hwinsta && hdesk ) {
			// Enumerate all winsta's to find the one with the job
			// We kick this off in a separate thread, and block until
			// it returns.

			HANDLE threadHandle;
			DWORD threadID;
			threadHandle = CreateThread(NULL, 0, FindWinstaThread,
									pidinfo, 0, &threadID);
			WaitForSingleObject(threadHandle, INFINITE);

			if ( pidinfo->hWnd == NULL ) {
				// Did not find it.  This could happen because WinNT has a
				// stupid bug where a brand new winsta does not immediately
				// show up when you enumerate all the winstas.  Sometimes
				// it takes a few seconds.  So lets sleep a while and try again.
				sleep(4);
				threadHandle = CreateThread(NULL, 0, FindWinstaThread,
									pidinfo, 0, &threadID);
				WaitForSingleObject(threadHandle, INFINITE);
			}

 			// Set winsta and desktop back to the service desktop (or wherever)
			SetProcessWindowStation(hwinsta);
			SetThreadDesktop(hdesk);
			// don't leak handles!
			CloseDesktop(hdesk);
			CloseWindowStation(hwinsta);
		}
	}

	// Now, if we know the window handle, send it a WM_CLOSE message
	if ( pidinfo->hWnd ) {
		if ( pidinfo->hWnd == HWND_BROADCAST ) {
			dprintf(D_PROCFAMILY,"PostMessage to HWND_BROADCAST!\n");
			EXCEPT("About to send WM_CLOSE to HWND_BROADCAST!");
		}
		if( (DebugFlags & D_PROCFAMILY) && (DebugFlags & D_FULLDEBUG) ) {
			// A whole bunch of sanity checks
			pid_t check_pid = 0;
			GetWindowThreadProcessId(pidinfo->hWnd, &check_pid);
			char check_name[_POSIX_PATH_MAX];
			CSysinfo sysinfo;
			sysinfo.GetProcessName(check_pid,check_name, _POSIX_PATH_MAX);
			dprintf(D_PROCFAMILY,
				"Sending WM_CLOSE to pid %d: hWnd=%x check_pid=%d name='%s'\n",
				pid,pidinfo->hWnd,check_pid,check_name);
			if ( pid != check_pid ) {
				EXCEPT("In ShutdownGraceful: failed sanity check");
			}
		}
		if ( !PostMessage(pidinfo->hWnd,WM_CLOSE,0,0) ) {
			dprintf(D_PROCFAMILY,
				"Shutdown_Graceful: PostMessage FAILED, err=%d\n",
				GetLastError());
			return FALSE;
		} else {
			// Success!!  We're done.
			dprintf(D_PROCFAMILY,"Shutdown_Graceful: Success\n");
			return TRUE;
		}
	} else {
		// despite our best efforts, we cannot find the hWnd for this pid.
		dprintf(D_PROCFAMILY,"Shutdown_Graceful: Failed cuz no hWnd\n");
		return FALSE;
	}

#else

	// UNIX

		/*
		  We convert unix SIGTERM into DC SIGTERM via a signal handler
		  which calls Send_Signal.  When we want to Send_Signal() a
		  SIGTERM, we usually call Shutdown_Graceful().  But, if
		  Shutdown_Graceful() turns around and sends a unix signal to
		  ourselves, we're in an infinite loop.  So, Send_Signal()
		  checks the pid, and if it's sending a SIGTERM to itself, it
		  just does the regular stuff to raise a DC SIGTERM, instead
		  of using this special method.  However, if someone else
		  other than Send_Signal() called Shutdown_Graceful with our
		  own pid, we'd still have the infinite loop.  To be safe, we
		  check again here to catch future programmer errors...
		  -Derek Wright <wright@cs.wisc.edu> 5/17/02
		*/
	if( pid == mypid ) {
		EXCEPT( "Called Shutdown_Graceful() on yourself, "
				"which would cause an infinite loop on UNIX" );
	}

	int status;
	priv_state priv = set_root_priv();
	status = kill(pid, SIGTERM);
	set_priv(priv);
	return (status >= 0);		// return 1 if kill succeeds, 0 otherwise

#endif
}

int DaemonCore::Suspend_Thread(int tid)
{
	PidEntry *pidinfo;

	dprintf(D_DAEMONCORE,"called DaemonCore::Suspend_Thread(%d)\n",
		tid);

	// verify the tid passed in to us is valid
	if ( (pidTable->lookup(tid, pidinfo) < 0)	// is it not in our table?
#ifdef WIN32
		// is it a process (i.e. not a thread)?
		|| (pidinfo->hProcess != NULL)
		// do we not have a thread handle ?
		|| (pidinfo->hThread == NULL )
#endif
		)
	{
		dprintf(D_ALWAYS,"DaemonCore:Suspend_Thread(%d) failed, bad tid\n",
			tid);
		return FALSE;
	}

#ifndef WIN32
	// on Unix, a thread is really just a forked process
	return Suspend_Process(tid);
#else
	// on NT, suspend the thread via the handle in our table
	if ( ::SuspendThread( pidinfo->hThread ) == 0xFFFFFFFF ) {
		dprintf(D_ALWAYS,"DaemonCore:Suspend_Thread tid %d failed!\n", tid);
		return FALSE;
	}
	// if we made it here, we succeeded
	return TRUE;
#endif
}

int DaemonCore::Continue_Thread(int tid)
{
	PidEntry *pidinfo;

	dprintf(D_DAEMONCORE,"called DaemonCore::Continue_Thread(%d)\n",
		tid);

	// verify the tid passed in to us is valid
	if ( (pidTable->lookup(tid, pidinfo) < 0)	// is it not in our table?
#ifdef WIN32
		// is it a process (i.e. not a thread)?
		|| (pidinfo->hProcess != NULL)
		// do we not have a thread handle ?
		|| (pidinfo->hThread == NULL )
#endif
		)
	{
		dprintf(D_ALWAYS,"DaemonCore:Continue_Thread(%d) failed, bad tid\n",
			tid);
		return FALSE;
	}

#ifndef WIN32
	// on Unix, a thread is really just a forked process
	return Continue_Process(tid);
#else
	// on NT, continue the thread via the handle in our table.
	// keep calling it until the suspend_count hits 0
	int suspend_count;

	do {
		if ((suspend_count=::ResumeThread(pidinfo->hThread)) == 0xFFFFFFFF)
		{
			dprintf(D_ALWAYS,
				"DaemonCore:Continue_Thread tid %d failed!\n", tid);
			return FALSE;
		}
	} while (suspend_count > 1);

	// if we made it here, we succeeded
	return TRUE;
#endif
}

int DaemonCore::Suspend_Process(pid_t pid)
{
	dprintf(D_DAEMONCORE,"called DaemonCore::Suspend_Process(%d)\n",
		pid);

	if ( pid == ppid )
		return FALSE;	// cannot suspend our parent

#if defined(WIN32)

	// We need to enum all the threads in the process, and
	// then suspend all the threads.  We need to repeat this
	// process until all the threads are suspended, since a thread
	// we have not yet suspended may continue a thread we already
	// suspended.  Thus we may need to make several passes.
	// Furthermore, WIN32 maintains a suspend count for each thread.
	// We need to make certain we leave each thread's suspend count
	// incremented only by 1, otherwise we'll screw things up on resume.
	// Questions? See Todd Tannenbaum <tannenba@cs.wisc.edu>.
	int i,j,numTids,allDone,numExtraSuspends;
	ExtArray<HANDLE> hThreads;
	ExtArray<DWORD> tids;

	numTids = ntsysinfo.GetTIDs(pid,tids);

	dprintf(D_DAEMONCORE,"Suspend_Process(%d) - numTids = %d\n",
		pid, numTids);

	// if numTids is 0, this process has no threads, which likely
	// means the process does not exist (or an error in GetTIDs).
	if ( !numTids ) {
		dprintf(D_DAEMONCORE,
			"Suspend_Process failed: cannot get threads for pid %d\n",pid);
		return FALSE;
	}

	// open handles to all the threads.
	for (j=0; j < numTids; j++) {
		hThreads[j] = ntsysinfo.OpenThread(tids[j]);
		dprintf(D_DAEMONCORE,
			"Suspend_Process(%d) - thread %d  tid=%d  handle=%p\n",
			pid, j, tids[j], hThreads[j]);
	}

	// Keep calling SuspendThread until they are all suspended.
	numExtraSuspends = 0;
	do {
		allDone = TRUE;
		for (j=0; j < numTids; j++) {
			if ( hThreads[j] ) {
				dprintf(D_DAEMONCORE,
					"Suspend_Process(%d) calling SuspendThread on handle %p\n",
					pid, hThreads[j]);
				// Note: SuspendThread returns > 1 if already suspended
				if ( ::SuspendThread(hThreads[j]) == 0 ) {
					 allDone = FALSE;
				}
			} else {
				dprintf(D_DAEMONCORE,
					"Suspend_Process(%d) - NULL thread handle! (thread #%d)\n",
					pid, j);
			}
		}
		if ( allDone == FALSE ) {
			numExtraSuspends++;
		}
	} while ( allDone == FALSE );

	// Now all threads are suspended, but numExtraSuspends
	// contains the number of times we called SuspendThread
	// extraneously.  So decrement all the thread counts by this amount.
	for (i=0;i<numExtraSuspends;i++) {
		for (j=0; j < numTids; j++) {
			if ( hThreads[j] ) {
				::ResumeThread(hThreads[j]);
			}
		}
	}

	// Finally, close all the thread handles we opened.
	for (j=0; j < numTids; j++) {
		if ( hThreads[j] ) {
			ntsysinfo.CloseThread(hThreads[j]);
		}
	}

	return TRUE;
#else
	priv_state priv = set_root_priv();
	int status = kill(pid, SIGSTOP);
	set_priv(priv);
	return (status >= 0);		// return 1 if kill succeeds, 0 otherwise
#endif
}

int DaemonCore::Continue_Process(pid_t pid)
{
	dprintf(D_DAEMONCORE,"called DaemonCore::Continue_Process(%d)\n",
		pid);

#if defined(WIN32)
	// We need to get all the threads for this process, and keep calling
	// ResumeThread until at least one thread is no longer suspended.
	// However, if any one thread is not suspended, we need to leave this
	// process alone and not mess up the thread suspend counts. There is
	// no perfect way to figure this out, but usually it is less dangerous
	// to Suspend a running thread than to Resume a thread which the user
	// process explicitly suspended (more likely to mess up synchronization).
	// So, we use SuspendThread instead of ResumeThread to probe the
	// process's thread counts.
	// This is a lot of system calls, so we optimize for a case that is rather
	// common in Condor : when we call Continue_Process() on a job
	// which has not been suspended.  This optimization works as follows: we
	// try to get the primary thread from our internal DaemonCore hashtable,
	// and if this primary thread is not suspended, we return immediately.
	// Questions? See Todd Tannenbaum <tannenba@cs.wisc.edu>.
	PidEntry *pidinfo;
	HANDLE hPriThread;	// handle to the primary thread of the child
	DWORD PriTid;
	DWORD result;
	int numTids,i,j;
	int all_done = FALSE;

	if (pidTable->lookup(pid, pidinfo) < 0) {
		hPriThread = NULL;
		PriTid = 0;
	} else {
		hPriThread = pidinfo->hThread;
		PriTid = pidinfo->tid;
	}

	if ( hPriThread ) {
		result = ::SuspendThread(hPriThread);
		switch ( result ) {
		case 0xFFFFFFFF:
			// SuspendThread had an error.  Yes, that's what 0xFFFFFFFF means.
			// Seem like a stupid return code?  Call up Bill Gates.  ;^)
			dprintf(D_DAEMONCORE,
				"Continue_Process: error resuming primary thread\n");
			return FALSE;
			break;
		case 0:
			// Optimization: if SuspendThread returns 0, that means this thread
			// was not suspended.  Thus, we have no need to go any further,
			// just resume it to put the count back, and we're done.
			::ResumeThread(hPriThread);
			dprintf(D_DAEMONCORE,
							"Continue_Process: pid %d not suspended\n",pid);
			return TRUE;
			break;
		default:
			// Here we know the primary thread was already Suspended.  So,
			// we'll need to continue on enumerate all the threads in the process
			// and resume them.
			break;
		}
	}

	// If we made it here, then the primary thread was indeed suspended.
	// So, we need to enumerate all the threads in this process and call
	// ResumeThread on them until at least one thread's suspend count
	// drop all the way back down to zero.  But first probe all the thread
	// suspend counts using SuspendThread (because it is safer than Resuming)
	// and make certain there is not already an active thread.  Without doing
	// this check, we may just Resume threads that the user process has
	// explicitly Suspended (perhaps they are sleeping, waiting on an event).
	// Remember: if we found the pid in our internal hash table, we have
	// already called SuspendThread once on the primary thread.
	ExtArray<HANDLE> hThreads;
	ExtArray<DWORD> tids;

	numTids = ntsysinfo.GetTIDs(pid,tids);
	dprintf(D_DAEMONCORE,"Continue_Process: numTids = %d\n",numTids);

	// if numTids is 0, this process has no threads, which likely
	// means the process no longer exists (or an error in GetTIDs).
	if ( !numTids ) {
		dprintf(D_DAEMONCORE,
			"Continue_Process failed: cannot get threads for pid %d\n",pid);
		if ( hPriThread ) {
			::ResumeThread(hPriThread);
		}
		return FALSE;
	}

	// open handles to all the threads.
	for (j=0; j < numTids; j++) {
		hThreads[j] = ntsysinfo.OpenThread(tids[j]);
	}

	// Probe all the thread suspend counts using SuspendThread (because
	// it is safer than Resuming) and make certain there is not already an
	// active thread.
	for (j=0; j < numTids; j++) {
		if ( hThreads[j] ) {
			// Check if this is the primary thread.  If so, continue, since
			// we already called SuspendThead on it above and we do not want
			// to mess up the thread count.  Note we compare tids, not the
			// handles, since although the handles may refer to the same object
			// they may not be equal.  If we did not have the pid in our hash
			// table, PriTid is 0, so we'll do the right thing.
			if ( tids[j] == PriTid ) {
				continue;
			}
			// Note: SuspendThread returns 0 if thread was previously active
			if ( ::SuspendThread(hThreads[j]) == 0 ) {
				// This process was already active.  Resume all
				// the threads we have suspended and return.
				dprintf(D_DAEMONCORE,
						"Continue_Process: pid %d has active thread\n",pid);
				for (i=0; i<j+1; i++ ) {
					if ( hThreads[i] ) {
						::ResumeThread(hThreads[i]);
					}
					if ( tids[i] == PriTid ) {
						PriTid = 0;
					}
				}
				if ( hPriThread && PriTid ) {
					::ResumeThread(hPriThread);
				}
				all_done = TRUE;
				break;
			}
		}
	}

	// Now resume all threads until some thread becomes active.
	dprintf(D_DAEMONCORE,"Continue_Process: resuming all threads\n");
	while ( all_done == FALSE ) {
		// set all_done to TRUE in case all handles in hThreads are NULL
		all_done = TRUE;
		for (j=0; j < numTids; j++) {
			if ( hThreads[j] ) {
				all_done = FALSE;
				result = ::ResumeThread(hThreads[j]);
				dprintf(D_DAEMONCORE,
				   "Continue_Process: ResumeThread returned %d for thread %d\n",
				   result, j);
				if ( result < 2 ) {
					 all_done = TRUE;
				} else {
					if ( result == 0xFFFFFFFF ) {
						all_done = -1;
					}
				}
			}
		}
	}

	// Finally, close all the thread handles we opened.
	for (j=0; j < numTids; j++) {
		if ( hThreads[j] ) {
			ntsysinfo.CloseThread(hThreads[j]);
		}
	}

	if ( all_done == -1 ) {
		return FALSE;
	} else {
		return TRUE;
	}

#else	// of ifdef WIN32
	// Implementation for UNIX

	priv_state priv = set_root_priv();
	int status = kill(pid, SIGCONT);
	set_priv(priv);
	return (status >= 0);		// return 1 if kill succeeds, 0 otherwise
#endif
}

int DaemonCore::SetDataPtr(void *dptr)
{
	// note: curr_dataptr is updated by daemon core
	// whenever a register_* or a hanlder invocation takes place

	if ( curr_dataptr == NULL ) {
		return FALSE;
	}

	*curr_dataptr = dptr;

	return TRUE;
}

int DaemonCore::Register_DataPtr(void *dptr)
{
	// note: curr_dataptr is updated by daemon core
	// whenever a register_* or a hanlder invocation takes place

	if ( curr_regdataptr == NULL ) {
		return FALSE;
	}

	*curr_regdataptr = dptr;

	return TRUE;
}

void *DaemonCore::GetDataPtr()
{
	// note: curr_dataptr is updated by daemon core
	// whenever a register_* or a hanlder invocation takes place

	if ( curr_dataptr == NULL )
		return NULL;

	return ( *curr_dataptr );
}

#if defined(WIN32)
// WinNT Helper function: given a C runtime library file descriptor,
// set whether or not the underlying WIN32 handle should be
// inheritable or not.  If flag = TRUE, that means inheritable,
// else FALSE means not inheritable.
int DaemonCore::SetFDInheritFlag(int fh, int flag)
{
	long underlying_handle;

	underlying_handle = _get_osfhandle(fh);

	if ( underlying_handle == -1L ) {
		// this handle does not exist; return ok so this is
		// not logged as an error
		return TRUE;
	}

	// Set the inheritable flag on the underlying handle
	if (!::SetHandleInformation((HANDLE)underlying_handle,
		HANDLE_FLAG_INHERIT, flag ? HANDLE_FLAG_INHERIT : 0) ) {
			// failed to set flag
			DWORD whynot = GetLastError();

			if ( whynot == ERROR_INVALID_HANDLE ) {
				// I have no idea why _get_osfhandle() sometimes gives
				// us back an invalid handle, but apparently it does.
				// Just return ok so this is not logged as an error.
				return TRUE;
			}

			dprintf(D_ALWAYS,
				"ERROR: SetHandleInformation() failed in SetFDInheritFlag(%d,%d),"
				"err=%d\n"
				,fh,flag,whynot);

			return FALSE;
	}

	return TRUE;
}

// This is the thread function we use to call EnumWindowStations(). We
// found  that calling EnumWindowStations() from the main thread would
// later cause SetThreadDesktop() to fail, due to existing Windows or
// hooks on the "current" desktop that were owned by the main thread.
// By kicking it off in its own thread, we ensure that this doesn't happen.
DWORD WINAPI
FindWinstaThread( LPVOID pidinfo ) {

	return EnumWindowStations((WINSTAENUMPROC)DCFindWinSta, (LPARAM)pidinfo);
}

// Callback function for EnumWindowStationProc call to find hWnd for a pid
BOOL CALLBACK
DCFindWinSta(LPTSTR winsta_name, LPARAM p)
{
	BOOL ret_value;
	priv_state priv;
	BOOL check_winsta0;
	char* use_visible;
	check_winsta0 = FALSE;

	// dprintf(D_FULLDEBUG,"Opening WinSta %s\n",winsta_name);

	if ( strcmp(winsta_name,"WinSta0") == 0 ) {

		// if we're running the job in the foreground, we had better
		// look in Winsta0!
		use_visible = param("USE_VISIBLE_DESKTOP");
		if (use_visible) {
			check_winsta0 = ( use_visible[0] == 'T' ) || (use_visible[0] == 't' );
			free(use_visible);
		}

		if (! check_winsta0 ) {
			// skip the interactive Winsta to save time
			dprintf(D_PROCFAMILY,"Skipping Winsta0\n");
			return TRUE;
		}
	}

	// we used to set_user_priv() here, but we found that its not
	// needed, and worse still, would cause the startd to EXCEPT().
	HWINSTA hwinsta = OpenWindowStation(winsta_name, FALSE, MAXIMUM_ALLOWED);

	if ( hwinsta == NULL ) {
		//dprintf(D_FULLDEBUG,"Error: Failed to open WinSta %s err=%d\n",
		//	winsta_name,GetLastError());

		// return TRUE so we continue to enumerate
		return TRUE;
	}

	// Set the windowstation
	if (!SetProcessWindowStation(hwinsta)) {
			// failed; so close the handle and return TRUE to continue
			// the enumertion.
		dprintf(D_PROCFAMILY,
			"Error: Failed to SetProcessWindowStation to %s\n",winsta_name);
		CloseWindowStation(hwinsta);
		return TRUE;
	}

	// We used to set_user_priv() here, but we found it wasn't needed.
	// Worse still, it would cause the startd to EXCEPT().
	HDESK hdesk = OpenDesktop( "default", 0, FALSE, MAXIMUM_ALLOWED );

	if (hdesk == NULL) {
			// failed; so close the handle and return TRUE to continue
			// the enumertion.
		dprintf(D_PROCFAMILY,"Error: Failed to open desktop on winsta %s\n",
			winsta_name);
		CloseWindowStation(hwinsta);
		return TRUE;
	}

	// Set the desktop to be "default"
	if (!SetThreadDesktop(hdesk)) {
			// failed; so close the handle and return TRUE to continue
			// the enumertion.
		dprintf(D_PROCFAMILY,
			"Error: Failed to SetThreadDesktop desktop on winsta %s\n",
			winsta_name);
		CloseDesktop(hdesk);
		CloseWindowStation(hwinsta);
		return TRUE;
	}

	// Now check all the windows on this desktop for our user job
	EnumWindows((WNDENUMPROC)DCFindWindow, p);

	// See if we are done; if so, return FALSE so GDI will stop
	// enumerating window stations.
	DaemonCore::PidEntry *entry = (DaemonCore::PidEntry *)p;
	if ( entry->hWnd ) {
		// we're done!  we found the user job's window
		dprintf(D_PROCFAMILY,"Found job pid %d on winsta %s\n",
			entry->pid,winsta_name);
		ret_value = FALSE;
	} else {

		// Now in a post NT4 world, we have to use a different
		// method for searching for console applications (like
		// batch scripts or anything that isn't GUI). So we'll
		// do that now before declaring our search for the HWND
		// a bust.

		HWND hwnd = NULL;
		ret_value = TRUE;
		pid_t pid = 0;

		while (hwnd = FindWindowEx(HWND_MESSAGE, hwnd,
			"ConsoleWindowClass", NULL)) {

			GetWindowThreadProcessId(hwnd, &pid);

			if (pid == entry->pid) {
				entry->hWnd = hwnd;
				ret_value = FALSE; // stop enumerating...we've got it.

				dprintf(D_PROCFAMILY, "Found console window, pid=%d\n", pid);
				break;
			}
		}
	}

	CloseDesktop(hdesk);
	CloseWindowStation(hwinsta);

	return ret_value;
}

// Callback function for EnumWindow call to find hWnd for a pid
BOOL CALLBACK
DCFindWindow(HWND hWnd, LPARAM p)
{
	DaemonCore::PidEntry *entry = (DaemonCore::PidEntry *)p;
	pid_t pid = 0;
	GetWindowThreadProcessId(hWnd, &pid);
	if (pid == entry->pid) {
		entry->hWnd = hWnd;

		if( (DebugFlags & D_PROCFAMILY) && (DebugFlags & D_FULLDEBUG) ) {
			char check_name[_POSIX_PATH_MAX];
			CSysinfo sysinfo;
			sysinfo.GetProcessName(pid,check_name, _POSIX_PATH_MAX);
			dprintf(D_PROCFAMILY,
				"DCFindWindow found pid %d: hWnd=%x name='%s'\n",
				pid,hWnd,check_name);
		}


		return FALSE;
	}
	return TRUE;
}
#endif	// of ifdef WIN32


void
DaemonCore::Forked_Child_Wants_Exit_By_Exec( bool exit_by_exec )
{
	if( exit_by_exec ) {
		_condor_exit_with_exec = 1;
	}
}


#if !defined(WIN32) && !defined(DUX)
	/* On Unix, we define our own exit() call.  The reason is messy:
	 * Basically, daemonCore Create_Thread call fork() on Unix.
	 * When the forked child calls exit, however, all the class
	 * destructors are called.  However, the code was never written in
	 * a way that expects the daemons to be forked.  For instance, some
	 * global constructor in the schedd tells the gridmanager to shutdown...
	 * certainly we do not want this happening in our forked child!  Also,
	 * we've seen problems were the forked child gets stuck in libc realloc
	 * on Linux trying to free up space in the gsi libraries after being
	 * called by some global destructor.  So.... for now, if we are
	 * forked via Create_Thread, we have our child exit _without_ calling
	 * any c++ destructors.  How do we accomplish that magic feat?  By
	 * exiting via a call to exec()!  So here it is... we overload exit()
	 * inside of daemonCore -- we do it this way so we catch all calls to
	 * exit, including ones buried in dprintf etc.  Note we dont want to
	 * do this via a macro setting, because some .C files that call exit
	 * do not include condor_daemon_core.h, and we don't want to put it
	 * into condor_common.h (because we only want to overload exit for
	 * daemonCore daemons).  So doing it this way works for all cases.
	 */
extern "C" {
void exit(int status)
{

		// turns out that _exit() does *not* always flush STDOUT and
		// STDERR, that it's "implementation dependent".  so, to
		// ensure that daemoncore things that are writing to stdout
		// and exiting (like "condor_master -version" or
		// "condor_shadow -classad") will in fact produce output, we
		// need to call fflush() ourselves right here.
		// Derek Wright <wright@cs.wisc.edu> 2004-03-28
	fflush( stdout );
	fflush( stderr );

	if ( _condor_exit_with_exec == 0 ) {
		_exit(status);
	}

	char* my_argv[2];
	char* my_env[1];
	my_argv[1] = NULL;
	my_env[0] = NULL;

		// First try to just use /bin/true or /bin/false.
	if ( status == 0 ) {
		my_argv[0] = "/bin/true";
		execve("/bin/true",my_argv,my_env);
		my_argv[0] = "/usr/bin/true";
		execve("/usr/bin/true",my_argv,my_env);
	} else {
		my_argv[0] = "/bin/false";
		execve("/bin/false",my_argv,my_env);
		my_argv[0] = "/usr/bin/false";
		execve("/usr/bin/false",my_argv,my_env);
	}

		// If we made it here, we cannot use /bin/[true|false].
		// So we need to use the condor_exit_code utility, if
		// it exists.
		// TODO

		// If we made it here, we are out of options.  So we will
		// just call _exit(), and hope for the best.
	_condor_exit_with_exec = 0;
	_exit(status ? 1 : 0);
}
}
#endif


int DaemonCore::Create_Process(
			const char	*name,
			const char	*args,
			priv_state	priv,
			int			reaper_id,
			int			want_command_port,
			const char	*env,
			const char	*cwd,
			int			new_process_group,
			Stream		*sock_inherit_list[],
			int			std[],
            int         nice_inc,
			int			job_opt_mask,
			int			fd_inherit_list[]
            )
{
	int i;
	char *ptmp;
	int inheritSockFds[MAX_INHERIT_SOCKS];
	int numInheritSockFds = 0;
	extern char **environ;

	//saved errno (if any) to pass back to caller
	//Currently, only stuff that would be of interest to the user
	//is saved (so the error can be reported in the user-log).
	int return_errno = 0;
	pid_t newpid = FALSE; //return FALSE to caller, by default

	// Name buffer to really use
	const char	*namebuf = strdup( name );
	if ( NULL == namebuf ) {
		dprintf( D_ALWAYS, "strdup failed!\n" );
		return FALSE;
	}

	MyString inheritbuf;
		// note that these are on the stack; they go away nicely
		// upon return from this function.
	ReliSock rsock;
	SafeSock ssock;
	PidEntry *pidtmp;


#ifdef WIN32

		// declare these variables early so MSVS doesn't complain

		// about them being declared inside of the goto's below.
	BOOL inherit_handles = FALSE;

	char *newenv = NULL;
	MyString strArgs;
	int namelen = 0;
	bool bIs16Bit = FALSE;

#else
	int inherit_handles;
	int max_pid_retry = 0;
	static int num_pid_collisions = 0;
#endif

	dprintf(D_DAEMONCORE,"In DaemonCore::Create_Process(%s,...)\n",namebuf);

	// First do whatever error checking we can that is not platform specific

	// check reaper_id validity
	if ( (reaper_id < 1) || (reaper_id > maxReap)
		 || (reapTable[reaper_id - 1].num == 0) ) {
		dprintf(D_ALWAYS,"Create_Process: invalid reaper_id\n");
		goto wrapup;
	}

	// check name validity
	if ( !namebuf ) {
		dprintf(D_ALWAYS,"Create_Process: null name to exec\n");
		goto wrapup;
	}

	inheritbuf.sprintf("%lu ",(unsigned long)mypid);

	inheritbuf += InfoCommandSinfulString();

	if ( sock_inherit_list ) {
		inherit_handles = TRUE;
		for (i = 0 ;
			 (sock_inherit_list[i] != NULL) && (i < MAX_INHERIT_SOCKS) ;
			 i++)
		{
                // PLEASE change this to a dynamic_cast when it
                // becomes available!
//            Sock *tempSock = dynamic_cast<Sock *>( sock_inherit_list[i] );
            Sock *tempSock = ((Sock *) sock_inherit_list[i] );
            if ( tempSock ) {
                inheritSockFds[numInheritSockFds] = tempSock->get_file_desc();
                numInheritSockFds++;
                    // make certain that this socket is inheritable
                if ( !(tempSock->set_inheritable(TRUE)) ) {
					goto wrapup;
                }
            }
            else {
                dprintf ( D_ALWAYS, "Dynamic cast failure!\n" );
                EXCEPT( "dynamic_cast" );
            }

			// now place the type of socket into inheritbuf
			 switch ( sock_inherit_list[i]->type() ) {
				case Stream::reli_sock :
					inheritbuf += " 1 ";
					break;
				case Stream::safe_sock :
					inheritbuf += " 2 ";
					break;
				default:
					// we only inherit safe and reli socks at this point...
					assert(0);
					break;
			}
			// now serialize object into inheritbuf
			 ptmp = sock_inherit_list[i]->serialize();
			 inheritbuf += ptmp;
			 delete []ptmp;
		}
	}
	inheritbuf += " 0";

	// if we want a command port for this child process, create
	// an inheritable tcp and a udp socket to listen on, and place
	// the info into the inheritbuf.
	if ( want_command_port != FALSE ) {
		inherit_handles = TRUE;
		if ( want_command_port == TRUE ) {
			// choose any old port (dynamic port)
			if (!BindAnyCommandPort(&rsock, &ssock)) {
				// dprintf with error message already in BindAnyCommandPort
				goto wrapup;
			}
			if ( !rsock.listen() ) {
				dprintf( D_ALWAYS, "Create_Process:Failed to post listen "
						 "on command ReliSock\n");
				goto wrapup;
			}
		} else {
			// use well-known port specified by command_port
			int on = 1;

			/* Set options on this socket, SO_REUSEADDR, so that
			   if we are binding to a well known port, and we crash,
			   we can be restarted and still bind ok back to
			   this same port. -Todd T, 11/97
			*/
			if( (!rsock.setsockopt(SOL_SOCKET, SO_REUSEADDR,
									(char*)&on, sizeof(on)))    ||
				(!ssock.setsockopt(SOL_SOCKET, SO_REUSEADDR,
									(char*)&on, sizeof(on))) )
		    {
				dprintf(D_ALWAYS,"ERROR: setsockopt() SO_REUSEADDR failed\n");
				goto wrapup;
			}

			/* Set no delay to disable Nagle, since we buffer all our 
			   relisock output and it degrades performance of our 
			   various chatty protocols. -Todd T, 9/05
			 */
			if( !rsock.setsockopt(IPPROTO_TCP, TCP_NODELAY,
									(char*)&on, sizeof(on)) )
		    {
				dprintf(D_ALWAYS,"Warning: setsockopt() TCP_NODELAY failed\n");
			}

			if ( (!rsock.listen( want_command_port)) ||
				 (!ssock.bind( want_command_port)) ) {
				dprintf(D_ALWAYS,"Create_Process:Failed to post listen "
						"on command socket(s) (port %d)\n", want_command_port );
				goto wrapup;
			}
		}

		// now duplicate the underlying SOCKET to make it inheritable
		if ( (!(rsock.set_inheritable(TRUE))) ||
			 (!(ssock.set_inheritable(TRUE))) ) {
			dprintf(D_ALWAYS,"Create_Process:Failed to set command "
					"socks inheritable\n");
			goto wrapup;
		}

		// and now add these new command sockets to the inheritbuf
		inheritbuf += " ";
		ptmp = rsock.serialize();
		inheritbuf += ptmp;
		delete []ptmp;
		inheritbuf += " ";
		ptmp = ssock.serialize();
		inheritbuf += ptmp;
		delete []ptmp;

            // now put the actual fds into the list of fds to inherit
        inheritSockFds[numInheritSockFds++] = rsock.get_file_desc();
        inheritSockFds[numInheritSockFds++] = ssock.get_file_desc();
	}
	inheritbuf += " 0";
	
	/* this will be the pidfamily ancestor identification information */
	time_t time_of_fork;
	unsigned int mii;
	pid_t forker_pid;

	/* this stuff ends up in the child's environment to help processes
		identify children/grandchildren/great-grandchildren/etc. */
	create_id(&time_of_fork, &mii);

#ifdef WIN32
	// START A NEW PROCESS ON WIN32

	STARTUPINFO si;
	PROCESS_INFORMATION piProcess;
	// SECURITY_ATTRIBUTES sa;
	// SECURITY_DESCRIPTOR sd;


	// prepare a STARTUPINFO structure for the new process
	ZeroMemory(&si,sizeof(si));
	si.cb = sizeof(si);

	// process id for the environment ancestor history info
	forker_pid = ::GetCurrentProcessId();

	// we do _not_ want our child to inherit our file descriptors
	// unless explicitly requested.  so, set the underlying handle
	// of all files opened via the C runtime library to non-inheritable.
	for (i = 0; i < 100; i++) {
		SetFDInheritFlag(i,FALSE);
	}

	// Now make inhertiable the fds that we specified in fd_inherit_list[]
	if (fd_inherit_list) {
		for (i=0; fd_inherit_list[i] != 0; i++) {
			SetFDInheritFlag (fd_inherit_list[i], TRUE);
		}
	}

	// handle re-mapping of stdout,in,err if desired.  note we just
	// set all our file handles to non-inheritable, so for any files
	// being redirected, we need to reset to inheritable.
	if ( std ) {
		long longTemp;
		int valid = FALSE;
		if ( std[0] > -1 ) {
			SetFDInheritFlag(std[0],TRUE);	// set handle inheritable
			longTemp = _get_osfhandle(std[0]);
			if (longTemp != -1 ) {
				valid = TRUE;
				si.hStdInput = (HANDLE)longTemp;
			}
		}
		if ( std[1] > -1 ) {
			SetFDInheritFlag(std[1],TRUE);	// set handle inheritable
			longTemp = _get_osfhandle(std[1]);
			if (longTemp != -1 ) {
				valid = TRUE;
				si.hStdOutput = (HANDLE)longTemp;
			}
		}
		if ( std[2] > -1 ) {
			SetFDInheritFlag(std[2],TRUE);	// set handle inheritable
			longTemp = _get_osfhandle(std[2]);
			if (longTemp != -1 ) {
				valid = TRUE;
				si.hStdError = (HANDLE)longTemp;
			}
		}
		if ( valid ) {
			si.dwFlags |= STARTF_USESTDHANDLES;
			inherit_handles = TRUE;
		}
	}

#if 0
	// prepare the SECURITY_ATTRIBUTES structure
	ZeroMemory(&sa,sizeof(sa));
	sa.nLength = sizeof(sa);
	ZeroMemory(&sd,sizeof(sd));
	sa.lpSecurityDescriptor = &sd;
	sa.bInheritHandle = FALSE;

	// set attributes of the SECURITY_DESCRIPTOR
	if (InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) == 0) {
		dprintf(D_ALWAYS, "Create_Process: InitializeSecurityDescriptor "
				"failed, errno = %d\n",GetLastError());
		goto wrapup;
	}
#endif

	if ( new_process_group == TRUE )
		new_process_group = CREATE_NEW_PROCESS_GROUP;
	else
		new_process_group =  0;

    // Re-nice our child -- on WinNT, this means run it at IDLE process
	// priority class.

	// NOTE: Can't we have a more fine-grained approach than this?  I
	// think there are other priority classes, and we should probably
	// map them to ranges within the 0-20 Unix nice-values... --pfc

    if ( nice_inc > 0 ) {
		// or new_process_group with whatever we set above...
		new_process_group |= IDLE_PRIORITY_CLASS;
	}


	// Deal with environment.  If the user specified an environment, we
	// augment augment it with default system environment variables and
	// the CONDOR_INHERIT var.  If the user did not specify an e
	if ( env || ( HAS_DCJOBOPT_NO_ENV_INHERIT(job_opt_mask) ) ) {
		Env job_environ;
		char envbuf[_POSIX_PATH_MAX];
		const char * default_vars[] = { "SystemDrive", "SystemRoot",
			"COMPUTERNAME", "NUMBER_OF_PROCESSORS", "OS", "COMSPEC",
			"PROCESSOR_ARCHITECTURE", "PROCESSOR_IDENTIFIER",
			"PROCESSOR_LEVEL", "PROCESSOR_REVISION", "PROGRAMFILES", "WINDIR",
			"\0" };		// must end list with NULL string

			// add in what is likely the system default path.  we do this
			// here, before merging the user env, because if the user
			// specifies a path in the job ad we want top use that instead.
		envbuf[0]='\0';
		GetEnvironmentVariable("PATH",envbuf,sizeof(envbuf));
		if (envbuf[0]) {
			job_environ.Put("PATH",envbuf);
		}

		// do the same for what likely is the system default TEMP
		// directory.
		envbuf[0]='\0';
		GetEnvironmentVariable("TEMP",envbuf,sizeof(envbuf));
		if (envbuf[0]) {
			job_environ.Put("TEMP",envbuf);
		}

			// now add in env vars from user
		if ( env ) {
			job_environ.Merge(env);
		}

			// next, add in default system env variables.  we do this after
			// the user vars are in place, because we want the values for
			// these system variables to override whatever the user said.
		int i = 0;
		while ( default_vars[i][0] ) {
			envbuf[0]='\0';
			GetEnvironmentVariable(default_vars[i],envbuf,sizeof(envbuf));
			if (envbuf[0]) {
				job_environ.Put(default_vars[i],envbuf);
			}
			i++;
		}

			// now, add in the inherit buf
		job_environ.Put( EnvGetName( ENV_INHERIT ), inheritbuf.Value() );

			// and finally, get it all back as a NULL delimited string.
			// remember to deallocate this with delete [] since it will
			// be allocated on the heap with new [].
		newenv = job_environ.getNullDelimitedString();

	} else {

		newenv = NULL;
		if( !SetEnv( EnvGetName( ENV_INHERIT ), inheritbuf.Value() ) ) {
			dprintf(D_ALWAYS, "Create_Process: Failed to set %s env.\n",
					EnvGetName( ENV_INHERIT ) );
			goto wrapup;
		}
	}	

	// end of dealing with the environment....

	// Check if it's a 16-bit application
	bIs16Bit = false;
	LOADED_IMAGE loaded;
	// NOTE (not in MSDN docs): Even when this function fails it still
	// may have "failed" with LastError = "operation completed successfully"
	// and still filled in our structure.  It also might really have
	// failed.  So we init the part of the structure we care about and just
	// ignore the return value.
	loaded.fDOSImage = FALSE;
	MapAndLoad((char *)namebuf, NULL, &loaded, FALSE, TRUE);
	if (loaded.fDOSImage == TRUE)
		bIs16Bit = true;
	UnMapAndLoad(&loaded);

	// CreateProcess requires different params for 16-bit apps:
	//		NULL for the app name
	//		args begins with app name
	strArgs = args;
	namelen = strlen(namebuf);
	if (bIs16Bit)
	{
		// surround the executable name with quotes or you'll have problems
		// when the execute directory contains spaces!
		strArgs = "\"" + MyString(namebuf) + MyString("\" ");

		// make sure we're only using backslashes
		strArgs.replaceString("/", "\\", 0);

		// args already should contain the executable name as the first param
		// we have to strip it out but preserve any real args after it
		MyString strOldArgs = args;
		int iFindFirstSpace = strOldArgs.FindChar(' ');
		if (iFindFirstSpace != -1)
			strArgs += (args + iFindFirstSpace + 1);

		args = const_cast<char *>(strArgs.GetCStr());
		dprintf(D_ALWAYS, "Create_Process: 16-bit job detected, args=%s\n", args);

	} else if ( (stricmp(".bat",&(namebuf[namelen-4])) == 0) ||
			(stricmp(".cmd",&(namebuf[namelen-4])) == 0) ) {

		char systemshell[_POSIX_PATH_MAX+1];
		int firstspace;

		// deal with .cmd and .bat files, so they're exec'd properly too

		// first, skip argv[0], since that will goof up the args to the
		// batch script.
		firstspace = strArgs.FindChar(' ');
		if ( firstspace != -1 ) {
			strArgs = args+firstspace+1;
		}

		// next, stuff the extra cmd.exe args in with the arguments
		strArgs = " /Q /C \"" + MyString(namebuf) + MyString("\" ") + strArgs;
		args = strArgs.Value();

		// now find out where cmd.exe lives on this box and
		// set it to our executable
		::GetSystemDirectory(systemshell, MAX_PATH);
		strncat(systemshell, "\\cmd.exe", _POSIX_PATH_MAX);
		free((void*)namebuf);
		namebuf = strdup(systemshell);

		dprintf(D_ALWAYS, "Executable is a batch script, so executing %s %s\n",
			namebuf, args);

	}

	BOOL cp_result, gbt_result;
	DWORD binType;
	gbt_result = GetBinaryType(namebuf, &binType);

	// test if the executable is either unexecutable, or if GetBinaryType()
	// thinks its a DOS 16-bit app, but in reality the actual binary
	// image isn't (this happens when the executable is bogus or corrupt).
	if ( !gbt_result || ( binType == SCS_DOS_BINARY && !bIs16Bit) ) {

		dprintf(D_ALWAYS, "ERROR: %s is not a valid Windows executable\n",
			   	namebuf);
		cp_result = 0;

		if ( newenv ) {
			delete [] newenv;
		}
		goto wrapup;
	} else {
		dprintf(D_FULLDEBUG, "GetBinaryType() returned %d\n", binType);
	}

   	if ( priv != PRIV_USER_FINAL ) {
		cp_result = ::CreateProcess(bIs16Bit ? NULL : namebuf,(char*)args,NULL,
			NULL,inherit_handles, new_process_group,newenv,cwd,&si,&piProcess);
	} else {
		// here we want to create a process as user for PRIV_USER_FINAL

			// Get the token for the user
		HANDLE user_token = priv_state_get_handle();
		ASSERT(user_token);

			// making this a NULL string tells NT to dynamically
			// create a new Window Station for the process we are about
			// to create....
		si.lpDesktop = "";

			// Check USE_VISIBLE_DESKTOP in condor_config.  If set to TRUE,
			// then run the job on the visible desktop, otherwise create
			// a new non-visible desktop for the job.
		char *use_visible = param("USE_VISIBLE_DESKTOP");

		if (use_visible && (*use_visible=='T' || *use_visible=='t') ) {
				// user wants visible desktop.
				// place the user_token into the proper access control lists.
			int GrantDesktopAccess(HANDLE hToken);	// prototype
			if ( GrantDesktopAccess(user_token) == 0 ) {
					// Success!!  The user now has permission to use
					// the visible desktop, so change si.lpDesktop
				si.lpDesktop = "winsta0\\default";
			} else {
					// The system refuses to grant access to the visible
					// desktop.  Log a message & we'll fall back on using
					// the dynamically created non-visible desktop.
				dprintf(D_ALWAYS,
					"Create_Process: Unable to use visible desktop\n");
			}
		}
		if (use_visible) free(use_visible);

			// we need to make certain to specify CREATE_NEW_CONSOLE, because
			// our ACLs will not let us use the current console which is
			// restricted to LOCALSYSTEM.
			//
			// "Who's your Daddy ?!?!?!   JEFF B.!"

		// we set_user_priv() here because it really doesn't hurt, and more importantly,
		// if we're using an encrypted execute directory, SYSTEM can't read the user's
		// executable, so the CreateProcessAsUser() call fails. We avoid this by
		// flipping into user priv mode first, then making the call, and all is well.

		priv_state s = set_user_priv();

		cp_result = ::CreateProcessAsUser(user_token,bIs16Bit ? NULL : namebuf,
			(char *)args,NULL,NULL, inherit_handles,
			new_process_group | CREATE_NEW_CONSOLE, newenv,cwd,&si,&piProcess);

		set_priv(s);

	}

	if ( !cp_result ) {
		dprintf(D_ALWAYS,
			"Create_Process: CreateProcess failed, errno=%d\n",GetLastError());
		if ( newenv ) {
			delete [] newenv;
		}
		goto wrapup;
	}
	if ( newenv ) {
		delete [] newenv;
	}

	// save pid info out of piProcess
	newpid = piProcess.dwProcessId;

	// reset sockets that we had to inherit back to a
	// non-inheritable permission
	if ( sock_inherit_list ) {
		for (i = 0 ;
			 (sock_inherit_list[i] != NULL) && (i < MAX_INHERIT_SOCKS) ;
			 i++)
        {
			((Sock *)sock_inherit_list[i])->set_inheritable(FALSE);
		}
	}
#else
	// START A NEW PROCESS ON UNIX
	int exec_results;

		// We have to do some checks on the executable name and the
		// cwd before we fork.  We want to do these in priv state
		// specified, but in the user priv if PRIV_USER_FINAL specified.
		// Don't do anything in PRIV_UNKNOWN case.

	priv_state current_priv;
	if ( priv != PRIV_UNKNOWN ) {
		if ( priv == PRIV_USER_FINAL ) {
			current_priv = set_user_priv();
		} else {
			current_priv = set_priv( priv );
		}
	}

	// First, check to see that the specified executable exists.
	if( access(namebuf,F_OK | X_OK) < 0 ) {
		return_errno = errno;
		dprintf( D_ALWAYS, "Create_Process: "
				 "Cannot access specified executable \"%s\": "
				 "errno = %d (%s)\n", namebuf, errno, strerror(errno) );
		if ( priv != PRIV_UNKNOWN ) {
			set_priv( current_priv );
		}
		goto wrapup;
	}

	// Next, check to see that the cwd exists.
	struct stat stat_struct;
	if( cwd && (cwd[0] != '\0') ) {
		if( stat(cwd, &stat_struct) == -1 ) {
			return_errno = errno;
			dprintf( D_ALWAYS, "Create_Process: "
					 "Cannot access specified cwd \"%s\": "
					 "errno = %d (%s)\n", cwd, errno, strerror(errno) );
			if ( priv != PRIV_UNKNOWN ) {
				set_priv( current_priv );
			}
			goto wrapup;
		}
	}

		// Change back to the priv we came from:
	if ( priv != PRIV_UNKNOWN ) {
		set_priv( current_priv );
	}

		// Before we fork, we want to setup some communication with
		// our child in case something goes wrong before the exec.  We
		// don't want the child to exit if the exec fails, since we
		// can't tell from the exit code if it is from our child or if
		// the binary we exec'ed happened to use the same exit code.
		// So, we use a pipe.  The trick is that we set the
		// close-on-exec flag of the pipe, so we don't leak a file
		// descriptor to the child.  The parent reads off of the pipe.
		// If it is closed before there is any data sent, then the
		// exec succeeded.  Otherwise, it reads the errno and returns
		// that to the caller.  --Jim B. (Apr 13 2000)
	int errorpipe[2];
	if (pipe(errorpipe) < 0) {
		dprintf(D_ALWAYS,"Create_Process: pipe() failed with errno %d (%s).\n",
				errno, strerror(errno));
		goto wrapup;
	}

	// process id for the environment ancestor history info
	forker_pid = ::getpid();

	newpid = fork();
	if( newpid == 0 ) // Child Process
	{
			// make sure we're not going to try to share the lock file
			// with our parent (if it's been defined).
		if( LockFd >= 0 ) {
			close( LockFd );
			LockFd = -1;
		}

			// close the read end of our error pipe and set the
			// close-on-exec flag on the write end
		close(errorpipe[0]);
		fcntl(errorpipe[1], F_SETFD, FD_CLOEXEC);

			/********************************************************
			  Make sure we're not about to re-use a PID that
			  DaemonCore still thinks is valid.  We have this problem
			  because our reaping code on UNIX makes use of a
			  WaitpidQueue.  Whenever we get SIGCHLD, we call
			  waitpid() as many times as we can, and save each result
			  into a structure we stash in a queue.  Then, when we get
			  the chance, we service the queue and call the associated
			  reapers.  Unfortunately, this means that once we call
			  waitpid(), the OS thinks the PID is gone and can be
			  re-used.  In some pathological cases, we've had shadow
			  PIDs getting re-used, such that the schedd thought there
			  were two shadows with the same PID, and then all hell
			  breaks loose with exit status values getting crossed,
			  etc.  This is the infamous "jobs-run-twice" bug.  (GNATS
			  #PR-256).  So, if we're in the very rare, unlucky case
			  where we just fork()'ed a new PID that's sitting in the
			  WaitpidQueue that daemoncore still hasn't called the
			  reaper for and removed from the PID table, we need to
			  write out a special errno to the errorpipe and exit.
			  The parent will recognize the errno, and just re-try.
			  Luckily, we've got a free copy of the PID table sitting
			  in our address space (3 cheers for copy-on-write), so we
			  can just do the lookup directly and be done with it.
			  Derek Wright <wright@cs.wisc.edu> 2004-12-15
			********************************************************/

		pid_t pid = ::getpid();   // must use the real getpid, not DC's
		PidEntry* pidinfo = NULL;
		if( (pidTable->lookup(pid, pidinfo) >= 0) ) {
				// we've already got this pid in our table! we've got
				// to bail out immediately so our parent can retry.
			int child_errno = ERRNO_PID_COLLISION;
			write(errorpipe[1], &child_errno, sizeof(child_errno));
			exit(4);
		}
			// If we made it here, we didn't find the PID in our
			// table, so it's safe to continue and eventually do the
			// exec() in this process...

		/////////////////////////////////////////////////////////////////
		// figure out what stays and goes in the child's environment
		/////////////////////////////////////////////////////////////////
		char **unix_env;
		Env envobject;

		// We may determine to seed the child's environment with the parent's.
		if( HAS_DCJOBOPT_ENV_INHERIT(job_opt_mask) ) {
			envobject.Merge((const char**)environ);
		}

		// Put the caller's env requests into the job's environment, potentially
		// adding/overriding things in the current env if the job was allowed to
		// inherit the parent's environment.
		envobject.Merge(env);

		// if I have brought in the parent's environment, then ensure that
		// after the caller's changes have been enacted, this overrides them.
		if( HAS_DCJOBOPT_ENV_INHERIT(job_opt_mask) ) {

			// add/override the inherit variable with the correct value
			// for this process.
			envobject.Put( EnvGetName( ENV_INHERIT ), inheritbuf.Value() );

			// Make sure PURIFY can open windows for the daemons when
			// they start. This functionality appears to only exist when we've
			// decided to inherit the parent's environment. I'm not sure
			// what the ramifications are if we include it all the time so here
			// it stays for now.
        	char *display;
        	display = param ( "PURIFY_DISPLAY" );
        	if ( display ) {
            	envobject.Put( "DISPLAY", display );
            	free ( display );
            	char *purebuf;
				purebuf = (char*)malloc(sizeof(char) * 
								(strlen("-program-name=") + strlen(namebuf) + 
								1));
				if (purebuf == NULL) {
					EXCEPT("Create_Process: PUREOPTIONS is out of memory!");
				}
            	sprintf ( purebuf, "-program-name=%s", namebuf );
            	envobject.Put( "PUREOPTIONS", purebuf );
				free(purebuf);
        	}
		}

		// Now we add/override  things that must ALWAYS be in the child's 
		// environment regardless of what is already in the child's environment.

		// BEGIN pid family environment id propogation 
		// Place the pidfamily accounting entries into our 
		// environment if we can and hope any children.
		// This will help ancestors track their children a little better.
		// We should be automatically propogating the pidfamily specifying
		// env vars in the forker process as well.
		char envid[PIDENVID_ENVID_SIZE];
		PidEnvID penvid;

		pidenvid_init(&penvid);

		// if we weren't inheriting the parent's environment, then grab out
		// the parent's pidfamily history... and jam it into the child's 
		// environment
		if ( HAS_DCJOBOPT_NO_ENV_INHERIT(job_opt_mask) ) {
			int i;
			// The parent process could not have been exec'ed if there were 
			// too many ancestor markers in its environment, so this check
			// is more of an assertion.
			if (pidenvid_filter_and_insert(&penvid, environ) ==
					PIDENVID_OVERSIZED)
			{
				dprintf ( D_ALWAYS, "Create_Process: Failed to filter ancestor "
				  	"history from parent's environment because there are more "
					"than PIDENVID_MAX(%d) of them! Programmer Error.\n",
					PIDENVID_MAX );
				// before we exit, make sure our parent knows something
				// went wrong before the exec...
				write(errorpipe[1], &errno, sizeof(errno));
				exit(errno); // Yes, we really want to exit here.
			}

			// Propogate the ancestor history to the child's environment
			for (i = 0; i < PIDENVID_MAX; i++) {
				if (penvid.ancestors[i].active == TRUE) { 
					envobject.Put( penvid.ancestors[i].envid );
				} else {
					// After the first FALSE entry, there will never be
					// true entries.
					break;
				}
			}
		}

		// create the new ancestor entry for the child's environment
		if (pidenvid_format_to_envid(envid, PIDENVID_ENVID_SIZE, 
				forker_pid, pid, time_of_fork, mii) == PIDENVID_BAD_FORMAT) 
		{
			dprintf ( D_ALWAYS, "Create_Process: Failed to create envid "
				  	"\"%s\" due to bad format. !\n", envid );
				// before we exit, make sure our parent knows something
				// went wrong before the exec...
			write(errorpipe[1], &errno, sizeof(errno));
			exit(errno); // Yes, we really want to exit here.
		}

		// if the new entry fits into the penvid, then add it to the 
		// environment, else EXCEPT cause it is programmer's error 
		if (pidenvid_append(&penvid, envid) == PIDENVID_OK) {
			envobject.Put( envid );
		} else {
			dprintf ( D_ALWAYS, "Create_Process: Failed to insert envid "
				  	"\"%s\" because its insertion would mean more than "
					"PIDENVID_MAX entries in a process! Programmer "
					"Error.\n", envid );
				// before we exit, make sure our parent knows something
				// went wrong before the exec...
			write(errorpipe[1], &errno, sizeof(errno));
			exit(errno); // Yes, we really want to exit here.
		}
		// END pid family environment id propogation 

		// The child's environment:
		unix_env = envobject.getStringArray();

		/////////////////////////////////////////////////////////////////
		// figure out what stays and goes in the job's arguments
		/////////////////////////////////////////////////////////////////
		char **unix_args;
		
		// set up the args to the process
		if( (args == NULL) || (args[0] == 0) ) {
			dprintf(D_DAEMONCORE, "Create_Process: Arg: NULL\n");
			unix_args = new char*[2];
			unix_args[0] = new char[strlen(namebuf)+1];
			strcpy ( unix_args[0], namebuf );
			unix_args[1] = 0;
		}
		else {
			dprintf(D_DAEMONCORE, "Create_Process: Arg: %s\n", args);
			unix_args = ParseArgsString(args);
		}

		//create a new process group if we are supposed to
		if( new_process_group == TRUE )
		{
			// Set sid is the POSIX way of creating a new proc group
			if( setsid() == -1 )
			{
				dprintf(D_ALWAYS, "Create_Process: setsid() failed: %s\n",
						strerror(errno) );
					// before we exit, make sure our parent knows something
					// went wrong before the exec...
				write(errorpipe[1], &errno, sizeof(errno));
				exit(errno); // Yes, we really want to exit here.
			}
		}

			// if we're given a relative path (in name) AND we want to cwd
			// here, we have to prepend stuff to name make it the full path.
			// Otherwise, we change directory and execv fails.

		if( cwd && (cwd[0] != '\0') ) {

			if ( name[0] != '/' ) {   // relative path
				char currwd[_POSIX_PATH_MAX];
				if ( getcwd( currwd, _POSIX_PATH_MAX ) == NULL ) {
					dprintf ( D_ALWAYS, "Create_Process: getcwd failed\n" );
						// before we exit, make sure our parent knows something
						// went wrong before the exec...
					write(errorpipe[1], &errno, sizeof(errno));
					exit(errno);
				}

				// Allocate a new buffer to store the modified name
				const char	*origname = namebuf;
				int			namelen = strlen( namebuf ) + strlen ( currwd ) + 2;

				// Allocate the "final" buffer to use
				char *nametmp = (char *) malloc( namelen );
				if ( NULL == nametmp ) {
					dprintf( D_ALWAYS, "malloc(%d) failed!\n", namelen );
					free( (void *) origname );
					exit(ENOMEM);
				}

				// Build the new (absolute) name in nametmp2
				strcpy( nametmp, currwd );
				strcat( nametmp, "/" );
				strcat( nametmp, origname );

				// Done with the original buffer
				free( (void *) origname );

				// Ok, we're done modifying the buffer, stuff it back
				// into our const char * namebuf
				namebuf = (const char *) nametmp;

				// Finally, log it
				dprintf ( D_DAEMONCORE, "Full path exec name: %s\n", namebuf );
			}

		}

		int openfds = getdtablesize();

			// Here we have to handle re-mapping of std(in|out|err)
		if ( std ) {

			dprintf ( D_DAEMONCORE, "Re-mapping std(in|out|err) in child.\n");

			if ( std[0] > -1 ) {
				if ( ( dup2 ( std[0], 0 ) ) == -1 ) {
					dprintf( D_ALWAYS, "dup2 of std[0] failed, errno %d\n",
							 errno );
				}
				close( std[0] );  // We don't need this in the child...
			} else {
					// if we don't want to inherit it, we better close
					// what's there
				close( 0 );
			}
			if ( std[1] > -1 ) {
				if ( ( dup2 ( std[1], 1 ) ) == -1 ) {
					dprintf( D_ALWAYS, "dup2 of std[1] failed, errno %d\n",
							 errno );
				}
				close( std[1] );  // We don't need this in the child...
			} else {
				close( 1 );
			}
			if ( std[2] > -1 ) {
				if ( ( dup2 ( std[2], 2 ) ) == -1 ) {
					dprintf( D_ALWAYS, "dup2 of std[2] failed, errno %d\n",
							 errno );
				}
				close( std[2] );  // We don't need this in the child...
			} else {
				close( 2 );
			}
		} else {
			MyString msg = "Just closed standard file fd(s): ";

                // if we don't want to re-map these, close 'em.
			    // However, make sure that its not in the inherit list first
			int	num_closed = 0;
			int	closed_fds[3];
            for ( int q=0 ; (q<openfds) && (q<3) ; q++ ) {
				bool found = FALSE;
				for ( int k=0 ; k < numInheritSockFds ; k++ ) {
					if ( inheritSockFds[k] == q ) {
						found = TRUE;
						break;
					}
				}
				// Now, if we didn't find it in the inherit list, close it
                if ( ( ! found ) && ( close ( q ) != -1 ) ) {
					closed_fds[num_closed++] = q;
					msg += q;
					msg += ' ';
                }
            }
			dprintf( D_DAEMONCORE, "%s\n", msg.Value() );

			// Re-open 'em to point at /dev/null as place holders
			if ( num_closed ) {
				int	fd_null = open( NULL_FILE, 0 );
				if ( fd_null < 0 ) {
					dprintf( D_ALWAYS, "Unable to open %s: %s\n", NULL_FILE,
							 strerror(errno) );
				} else {
					int		i;
					for ( i=0;  i<num_closed;  i++ ) {
						if ( ( closed_fds[i] != fd_null ) &&
							 ( dup2( fd_null, closed_fds[i] ) < 0 ) ) {
							dprintf( D_ALWAYS,
									 "Error dup2()ing %s -> %d: %s\n",
									 NULL_FILE,
									 closed_fds[i], strerror(errno) );
						}
					}
					// Close the /dev/null descriptor _IF_ it's not stdin/out/err
					if ( fd_null > 2 ) {
						close( fd_null );
					}
				}
			}
        }

			/* here we want to put all the unix_env stuff into the
			   environment.  We also want to drop CONDOR_INHERIT in.
			   Thus, the environment should consist of
			    - The parent's environment
				- The stuff passed into Create_Process in env
				- CONDOR_INHERIT
			*/

		for ( int j=0 ; (unix_env[j] && unix_env[j][0]) ; j++ ) {
			if ( !SetEnv( unix_env[j] ) ) {
				dprintf ( D_ALWAYS, "Create_Process: Failed to put "
						  "\"%s\" into environment.\n", unix_env[j] );
					// before we exit, make sure our parent knows something
					// went wrong before the exec...
				write(errorpipe[1], &errno, sizeof(errno));
				exit(errno); // Yes, we really want to exit here.
			}
		}

			// Place inheritbuf into the environment as
			// env variable CONDOR_INHERIT
			// SetEnv makes a copy of everything.
		if( !SetEnv( EnvGetName( ENV_INHERIT ), inheritbuf.Value() ) ) {
			dprintf(D_ALWAYS, "Create_Process: Failed to set %s env.\n",
					EnvGetName( ENV_INHERIT ) );
				// before we exit, make sure our parent knows something
				// went wrong before the exec...
			write(errorpipe[1], &errno, sizeof(errno));
			exit(errno); // Yes, we really want to exit here.
		}

            /* Re-nice ourself */
        if( nice_inc > 0 ) {
			if( nice_inc > 19 ) {
				nice_inc = 19;
			}
            dprintf ( D_DAEMONCORE, "calling nice(%d)\n", nice_inc );
            nice( nice_inc );
        }

		MyString msg = "Printing fds to inherit: ";
        for ( int a=0 ; a<numInheritSockFds ; a++ ) {
			msg += inheritSockFds[a];
			msg += ' ';
        }
        dprintf( D_DAEMONCORE, "%s\n", msg.Value() );

		dprintf ( D_DAEMONCORE, "About to exec \"%s\"\n", namebuf );

			// !! !! !! !! !! !! !! !! !! !! !! !!
			// !! !! !! !! !! !! !! !! !! !! !! !!
			/* No dprintfs allowed after this! */
			// !! !! !! !! !! !! !! !! !! !! !! !!
			// !! !! !! !! !! !! !! !! !! !! !! !!


			// Now we want to close all fds that aren't in the
			// sock_inherit_list.  however, this can really screw up
			// dprintf() because if we've still got a value in
			// DebugLock and therefore in LockFd, once we close that
			// fd, we're going to get a fatal error if we try to
			// dprintf().  so, we cannot print anything to the logs
			// once we start this process!!!

			// once again, make sure that if the dprintf code opened a
			// lock file and has an fd, that we close it before we
			// exec() so we don't leak it.
#if HAVE_EXT_GCB
		/*
		  this method currently only lives in libGCB.a, so don't even
		  try to param() or call this function unless we're on a
		  platform where we're using the GCB external
		*/
        Generic_stop_logging();
#endif

		if( LockFd >= 0 ) {
			close( LockFd );
			LockFd = -1;
		}

		bool found;
		for ( int j=3 ; j < openfds ; j++ ) {
			if ( j == errorpipe[1] ) continue; // don't close our errorpipe!

			found = FALSE;
			for ( int k=0 ; k < numInheritSockFds ; k++ ) {
                if ( inheritSockFds[k] == j ) {
					found = TRUE;
					break;
				}
			}

			if (fd_inherit_list) {
				for ( int k=0 ; fd_inherit_list[k] > 0; k++ ) {
					if ( fd_inherit_list[k] == j ) {
						found = TRUE;
						break;
					}
				}
			}


			if( !found ) {
				close( j );
            }
		}

			// now head into the proper priv state...
		if ( priv != PRIV_UNKNOWN ) {
			set_priv(priv);
		}

		if ( priv != PRIV_ROOT ) {
				// Final check to make sure we're not root anymore.
			if( getuid() == 0 ) {
				int priv_errno = ERRNO_EXEC_AS_ROOT;
				write(errorpipe[1], &priv_errno, sizeof(priv_errno));
				exit(4);
			}
		}

			// switch to the cwd now that we are in user priv state
		if ( cwd && cwd[0] ) {
			if( chdir(cwd) == -1 ) {
					// before we exit, make sure our parent knows something
					// went wrong before the exec...
				write(errorpipe[1], &errno, sizeof(errno));
				exit(errno); // Let's exit.
			}
		}

			// Unblock all signals if we're starting a regular process!
		if ( !want_command_port ) {
			sigset_t emptySet;
			sigemptyset( &emptySet );
			sigprocmask( SIG_SETMASK, &emptySet, NULL );
		}

#if defined(LINUX) && defined(TDP)
		if( HAS_DCJOBOPT_SUSPEND_ON_EXEC(job_opt_mask) ) {
			if(ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {
			    write(errorpipe[1], &errno, sizeof(errno));
			    exit (errno);
			}
		}
#endif

			// and ( finally ) exec:
		if( HAS_DCJOBOPT_NO_ENV_INHERIT(job_opt_mask) ) {
			exec_results =  execve(namebuf, unix_args, unix_env);
		} else {
			exec_results =  execv(namebuf, unix_args);
		}
		if( exec_results == -1 )
		{
				// We no longer have privs to dprintf. :-(.
				// Let's exit with our errno.
				// before we exit, make sure our parent knows something
				// went wrong before the exec...
			write(errorpipe[1], &errno, sizeof(errno));
			exit(errno); // Yes, we really want to exit here.
		}
	}
	else if( newpid > 0 ) // Parent Process
	{
			// close the write end of our error pipe
		close(errorpipe[1]);
			// check our error pipe for any problems before the exec
		int child_errno = 0;
		if (read(errorpipe[0], &child_errno, sizeof(int)) == sizeof(int)) {
				// If we were able to read the errno from the
				// errorpipe before it was closed, then we know the
				// error happened before the exec.  We need to reap
				// the child and return FALSE.
			int child_status;
			waitpid(newpid, &child_status, 0);
			errno = child_errno;
			return_errno = errno;
			switch( errno ) {

			case ERRNO_EXEC_AS_ROOT:
				dprintf( D_ALWAYS, "Create_Process: child failed because "
						 "%s process was still root before exec()\n",
						 priv_to_string(priv) );
				break;

			case ERRNO_PID_COLLISION:
					/*
					  see the big comment in the top of the child code
					  above for why this can happen.  if it does, we
					  need to increment our counter, make sure we
					  haven't gone over our maximum allowed collisions
					  before we give up, and if not, recursively
					  re-try the whole call to Create_Process().
					*/
				dprintf( D_ALWAYS, "Create_Process: child failed because "
						 "PID %d is still in use by DaemonCore\n",
						 (int)newpid );
				num_pid_collisions++;
				max_pid_retry = param_integer( "MAX_PID_COLLISION_RETRY",
											   DEFAULT_MAX_PID_COLLISIONS );
				if( num_pid_collisions > max_pid_retry ) {
					dprintf( D_ALWAYS, "Create_Process: ERROR: we've had "
							 "%d consecutive pid collisions, giving up!\n",
							 num_pid_collisions );
						// if we break out of the switch(), we'll hit
						// the default failure case, goto the wrapup
						// code, and just return failure...
					break;
				}
					// if we're here, it means we had a pid collision,
					// but it's not (yet) fatal, and we should just
					// re-try the whole Create_Process().  however,
					// before we do, we need to do a little bit of the
					// default cleanup ourselves so we don't leak any
					// memory or fds...
				close(errorpipe[0]);
				free( (void *) namebuf );
					// we also need to close the command sockets we've
					// got open sitting on our stack, so that if we're
					// trying to spawn using a fixed port, we won't
					// still be holding the port open in this lower
					// stack frame...
				rsock.close();
				ssock.close();
				dprintf( D_ALWAYS, "Re-trying Create_Process() to avoid "
						 "PID re-use\n" );
				return Create_Process( name, args, priv, reaper_id,
									   want_command_port, env, cwd,
									   new_process_group,
									   sock_inherit_list, std,
									   nice_inc, job_opt_mask );
				break;

			default:
				dprintf( D_ALWAYS, "Create_Process: child failed with "
						 "errno %d (%s) before exec()\n", errno,
						 strerror(errno) );
				break;

			}
			close(errorpipe[0]);
			newpid = FALSE;
			goto wrapup;
		}
		close(errorpipe[0]);

			// Now that we've seen if exec worked, if we are trying to
			// create a paused process, we need to wait for the
			// stopped child.
		if( HAS_DCJOBOPT_SUSPEND_ON_EXEC(job_opt_mask) ) {
#if defined(LINUX) && defined(TDP)
				// NOTE: we need to be in user_priv to do this, since
				// we're going to be sending signals and such
			priv_state prev_priv;
			prev_priv = set_user_priv();
			int rval = tdp_wait_stopped_child( newpid );
			set_priv( prev_priv );
			if( rval == -1 ) {
				return_errno = errno;
				dprintf(D_ALWAYS, "Create_Process wait failed: %d (%s)\n",
					errno, strerror (errno) );
				newpid = FALSE;
				goto wrapup;
			}
#else
			dprintf(D_ALWAYS, "DCJOBOPT_SUSPEND_ON_EXEC not implemented.\n");

#endif /* LINUX && TDP */
		}
	}
	else if( newpid < 0 )// Error condition
	{
		dprintf(D_ALWAYS, "Create Process: fork() failed: %s (%d)\n",
				strerror(errno), errno );
		close(errorpipe[0]); close(errorpipe[1]);
		goto wrapup;
	}
#endif

	// Now that we have a child, store the info in our pidTable
	pidtmp = new PidEntry;
	pidtmp->pid = newpid;
	pidtmp->new_process_group = new_process_group;
	if ( want_command_port != FALSE )
		strcpy(pidtmp->sinful_string,sock_to_string(rsock._sock));
	else
		pidtmp->sinful_string[0] = '\0';
	pidtmp->is_local = TRUE;
	pidtmp->parent_is_local = TRUE;
	pidtmp->reaper_id = reaper_id;
	pidtmp->hung_tid = -1;
	pidtmp->was_not_responding = FALSE;
#ifdef WIN32
	pidtmp->hProcess = piProcess.hProcess;
	pidtmp->hThread = piProcess.hThread;
	pidtmp->tid = piProcess.dwThreadId;
	pidtmp->hWnd = 0;
	pidtmp->hPipe = 0;
	pidtmp->pipeReady = 0;
	pidtmp->deallocate = 0;
#endif 
	/* remember the family history of the new pid */
	pidenvid_init(&pidtmp->penvid);
	if (pidenvid_filter_and_insert(&pidtmp->penvid, environ) !=
		PIDENVID_OK)
	{
		EXCEPT( "Create_Process: More ancestor environment IDs found than "
				"PIDENVID_MAX which is currently %d. Programmer Error.\n", 
				PIDENVID_MAX );
	}
	if (pidenvid_append_direct(&pidtmp->penvid, 
			forker_pid, newpid, time_of_fork, mii) == PIDENVID_OVERSIZED)
	{
		EXCEPT( "Create_Process: Cannot add child pid to PidEnvID table "
				"because there aren't enough entries. PIDENVID_MAX is "
				"currently %d! Programmer Error.\n", PIDENVID_MAX );
	}

	/* add it to the pid table */
	{
	   int insert_result = pidTable->insert(newpid,pidtmp);
	   assert( insert_result == 0);
	}

	dprintf(D_DAEMONCORE,
		"Child Process: pid %lu at %s\n",
		(unsigned long)newpid,pidtmp->sinful_string);
#ifdef WIN32
	WatchPid(pidtmp);
#endif

	// Now that child exists, we (the parent) should close up our copy of
	// the childs command listen cedar sockets.  Since these are on
	// the stack (rsock and ssock), they will get closed when we return.

 wrapup:
	// Free up the name buffer
	free( (void *) namebuf );

#ifndef WIN32
		// if we're here, it means we did NOT have a pid collision, or
		// we had too many and gave up.  either way, we should clear
		// out our static counter.
	num_pid_collisions = 0;
#endif

	errno = return_errno;
	return newpid;
}

#ifdef WIN32
/* Create_Thread support */
struct thread_info {
	ThreadStartFunc start_func;
	void *arg;
	Stream *sock;
	priv_state priv;
};

unsigned
win32_thread_start_func(void *arg) {
	dprintf(D_FULLDEBUG,"In win32_thread_start_func\n");
	thread_info *tinfo = (thread_info *)arg;
	int rval;
	set_priv(tinfo->priv);	// start thread at same priv_state as parent
	rval = tinfo->start_func(tinfo->arg, tinfo->sock);
	if (tinfo->arg) free(tinfo->arg);
	if (tinfo->sock) delete tinfo->sock;
	free(tinfo);
	return rval;
}
#endif

int
DaemonCore::Create_Thread(ThreadStartFunc start_func, void *arg, Stream *sock,
						  int reaper_id)
{
	// check reaper_id validity
	if ( (reaper_id < 1) || (reaper_id > maxReap)
		 || (reapTable[reaper_id - 1].num == 0) ) {
		dprintf(D_ALWAYS,"Create_Thread: invalid reaper_id\n");
		return FALSE;
	}

	// Before we create the thread, call InfoCommandSinfulString once.
	// This makes certain that InfoCommandSinfulString has allocated its
	// buffer which will make it thread safe when called from SendSignal().
	(void)InfoCommandSinfulString();

#ifdef WIN32
	unsigned tid;
	HANDLE hThread;
	priv_state priv;
	// need to copy the sock because our caller is going to delete/close it
//	Stream *s = sock;
// #if 0
	Stream *s = NULL;
	if (sock) {
		switch(sock->type()) {
		case Stream::reli_sock:
			s = new ReliSock(*((ReliSock *)sock));
			break;
		case Stream::safe_sock:
			s = new SafeSock(*((SafeSock *)sock));
			break;
		default:
			break;
		}
	}
// #endif
	thread_info *tinfo = (thread_info *)malloc(sizeof(thread_info));
	tinfo->start_func = start_func;
	tinfo->arg = arg;
	tinfo->sock = s;
		// find out this threads priv state, so our new thread starts out
		// at the same priv state.  on Unix this is not a worry, since
		// priv_state on Unix is per process.  but on NT, it is per thread.
	priv = set_condor_priv();
	set_priv(priv);
	tinfo->priv = priv;
		// create the thread.
	hThread = (HANDLE) _beginthreadex(NULL, 1024,
				 (CRT_THREAD_HANDLER)win32_thread_start_func,
				 (void *)tinfo, 0, &tid);
	if ( hThread == NULL ) {
		EXCEPT("CreateThread failed");
	}
#else
		// we have to do the same checking for pid collision here as
		// we do in the Create_Process() case (see comments there).
	static int num_pid_collisions = 0;
	int max_pid_retry = 0;
	int errorpipe[2];
    if (pipe(errorpipe) < 0) {
        dprintf( D_ALWAYS,
				 "Create_Thread: pipe() failed with errno %d (%s)\n",
				 errno, strerror(errno) );
		return FALSE;
    }
	int tid;
	tid = fork();
	if (tid == 0) {				// new thread (i.e., child process)
		_condor_exit_with_exec = 1;
            // close the read end of our error pipe and set the
            // close-on-exec flag on the write end
        close(errorpipe[0]);
        fcntl(errorpipe[1], F_SETFD, FD_CLOEXEC);
		pid_t pid = ::getpid();
		PidEntry* pidinfo = NULL;
        if( (pidTable->lookup(pid, pidinfo) >= 0) ) {
                // we've already got this pid in our table! we've got
                // to bail out immediately so our parent can retry.
            int child_errno = ERRNO_PID_COLLISION;
            write(errorpipe[1], &child_errno, sizeof(child_errno));
			close( errorpipe[1] );
            exit(4);
        }
			// if we got this far, we know we don't need the errorpipe
			// anymore, so we can close it now...
		close( errorpipe[1] );
		exit(start_func(arg, sock));
	} else if ( tid > 0 ) {  // parent process
			// close the write end of our error pipe
        close( errorpipe[1] );
            // check our error pipe for any problems before the exec
        bool had_child_error = false;
        int child_errno = 0;
        if( read(errorpipe[0], &child_errno, sizeof(int)) == sizeof(int) ) {
			had_child_error = true;
		}
		close( errorpipe[0] );
		if( had_child_error ) {
                // If we were able to read the errno from the
                // errorpipe before it was closed, then we know the
                // error happened before the exec.  We need to reap
                // the child and return FALSE.
            int child_status;
            waitpid(tid, &child_status, 0);
			if( child_errno != ERRNO_PID_COLLISION ) {
				EXCEPT( "Impossible: Create_Thread child_errno (%d) is not "
						"ERRNO_PID_COLLISION!", child_errno );
			}
			dprintf( D_ALWAYS, "Create_Thread: child failed because "
					 "PID %d is still in use by DaemonCore\n",
					 tid );
			num_pid_collisions++;
			max_pid_retry = param_integer( "MAX_PID_COLLISION_RETRY",
										   DEFAULT_MAX_PID_COLLISIONS );
			if( num_pid_collisions > max_pid_retry ) {
				dprintf( D_ALWAYS, "Create_Thread: ERROR: we've had "
						 "%d consecutive pid collisions, giving up!\n",
						 num_pid_collisions );
				num_pid_collisions = 0;
				return FALSE;
			}
				// if we're here, it means we had a pid collision,
				// but it's not (yet) fatal, and we should just
				// re-try the whole Create_Thread().
			dprintf( D_ALWAYS, "Re-trying Create_Thread() to avoid "
					 "PID re-use\n" );
			return Create_Thread( start_func, arg, sock, reaper_id );
		}
	} else {  // fork() failure
		dprintf( D_ALWAYS, "Create_Thread: fork() failed: %s (%d)\n",
				 strerror(errno), errno );
		num_pid_collisions = 0;
		return FALSE;
	}
		// if we got here, there's no collision, so reset our counter
	num_pid_collisions = 0;
	if (arg) free(arg);			// arg should point to malloc()'ed data
#endif

	dprintf(D_DAEMONCORE,"Create_Thread: created new thread, tid=%d\n",tid);

	// store the thread info in our pidTable
	// -- this is safe on Unix since our thread is really a process but
	//    on NT we need to avoid conflicts between tids and pids -
	//	  the DaemonCore reaper code handles this on NT by checking
	//	  hProcess.  If hProcess is NULL, it is a thread, else a process.
	PidEntry *pidtmp = new PidEntry;
	pidtmp->new_process_group = FALSE;
	pidtmp->sinful_string[0] = '\0';
	pidtmp->is_local = TRUE;
	pidtmp->parent_is_local = TRUE;
	pidtmp->reaper_id = reaper_id;
	pidtmp->hung_tid = -1;
	pidtmp->was_not_responding = FALSE;
#ifdef WIN32
	// we lie here and set pidtmp->pid to equal the tid.  this allows
	// the DaemonCore WinNT pidwatcher code to remain mostly ignorant
	// that this is really a thread instead of a process.  we can get
	// away with this because currently WinNT pids and tids do not
	// conflict --- lets hope it stays that way!
	pidtmp->pid = tid;
	pidtmp->hProcess = NULL;	// setting this to NULL means this is a thread
	pidtmp->hThread = hThread;
	pidtmp->tid = tid;
	pidtmp->hWnd = 0;
	pidtmp->hPipe = 0;
	pidtmp->deallocate = 0;
#else
	pidtmp->pid = tid;
#endif
	int insert_result = pidTable->insert(tid,pidtmp);
	assert( insert_result == 0 );
#ifdef WIN32
	WatchPid(pidtmp);
#endif
	return tid;
}

int
DaemonCore::Kill_Thread(int tid)
{
	dprintf(D_DAEMONCORE,"called DaemonCore::Kill_Thread(%d)\n", tid);
#if defined(WIN32)
	/*
	  My Life of Pain:  Yuck.  This is a no-op on WinNT because
	  the TerminateThread() call is so useless --- calling it could
	  mess up the entire process, since if the thread is currently
	  executing inside of system code, system mutexes are not
	  released!!! Thus, calling NT's TerminateThread() could be
	  the last thing this process does!
	 */
	return 1;
#else
	priv_state priv = set_root_priv();
	int status = kill(tid, SIGKILL);
	set_priv(priv);
	return (status >= 0);		// return 1 if kill succeeds, 0 otherwise
#endif
}

void
DaemonCore::Inherit( void )
{
	char *inheritbuf = NULL;
	int numInheritedSocks = 0;
	char *ptmp;

    /* Here we handle inheritance of sockets, file descriptors, and/or
	   handles from our parent.  This is done via an environment variable
	   "CONDOR_INHERIT".  If this variable does not exist, it usually
	   means our parent is not a daemon core process.  CONDOR_INHERIT
	   has the following fields.  Each field seperated by a space:
		*	parent pid
		*	parent sinful-string
	    *   cedar sockets to inherit.  each will start with a
	 		"1" for relisock, a "2" for safesock, and a "0" when done.
		*	command sockets.  first the rsock, then the ssock, then a "0".
	*/
	const char *envName = EnvGetName( ENV_INHERIT );
	const char *tmp = GetEnv( envName );
	if ( tmp != NULL ) {
		inheritbuf = strdup( tmp );
		dprintf ( D_DAEMONCORE, "%s: \"%s\"\n", envName, inheritbuf );
		UnsetEnv( envName );
	} else {
		inheritbuf = strdup( "" );
		dprintf ( D_DAEMONCORE, "%s: is NULL\n", envName );
	}

	if ( (ptmp=strtok(inheritbuf," ")) != NULL ) {
		// we read out CONDOR__INHERIT ok, ptmp is now first item

		// insert ppid into table
		dprintf(D_DAEMONCORE,"Parent PID = %s\n",ptmp);
		ppid = atoi(ptmp);
		PidEntry *pidtmp = new PidEntry;
		pidtmp->pid = ppid;
		ptmp=strtok(NULL," ");
		dprintf(D_DAEMONCORE,"Parent Command Sock = %s\n",ptmp);
		strcpy(pidtmp->sinful_string,ptmp);
		pidtmp->is_local = TRUE;
		pidtmp->parent_is_local = TRUE;
		pidtmp->reaper_id = 0;
		pidtmp->hung_tid = -1;
		pidtmp->was_not_responding = FALSE;
#ifdef WIN32
		pidtmp->deallocate = 0L;
		pidtmp->hPipe = 0;

		pidtmp->hProcess = ::OpenProcess( SYNCHRONIZE |
				PROCESS_QUERY_INFORMATION, FALSE, ppid );


		// We want to avoid trying to watch the ppid if it turns out
		// that we can't open a handle to it because we have insufficient
		// permissions. In the case of dagman, it runs as a user, which
		// doesn't necessarily have the perms to open a handle to the
		// schedd process. If we fail to get the handle for some reason
		// other than ACCESS_DENIED however, we want to try to watch the
		// pid, and consequently cause the assert() to blow.

		bool watch_ppid = true;

		if ( pidtmp->hProcess == NULL ) {
			if ( GetLastError() == ERROR_ACCESS_DENIED ) {
				dprintf(D_FULLDEBUG, "OpenProcess() failed - "
						"ACCESS DENIED. We can't watch parent process.\n");
				watch_ppid = false;
			} else {
				dprintf(D_ALWAYS, "OpenProcess() failed - Error %d\n",
					   	GetLastError());
			}
		}

		pidtmp->hThread = NULL;		// do not allow child to suspend parent
		pidtmp->deallocate = 0L;
#endif
		int insert_result = pidTable->insert(ppid,pidtmp);
		assert( insert_result == 0 );
#ifdef WIN32
		if ( watch_ppid ) {
			assert(pidtmp->hProcess);
			WatchPid(pidtmp);
		}
#endif

		// inherit cedar socks
		ptmp=strtok(NULL," ");
		while ( ptmp && (*ptmp != '0') ) {
			if (numInheritedSocks >= MAX_SOCKS_INHERITED) {
				EXCEPT("MAX_SOCKS_INHERITED reached.\n");
			}
			switch ( *ptmp ) {
				case '1' :
					// inherit a relisock
					dc_rsock = new ReliSock();
					ptmp=strtok(NULL," ");
					dc_rsock->serialize(ptmp);
					dc_rsock->set_inheritable(FALSE);
					dprintf(D_DAEMONCORE,"Inherited a ReliSock\n");
					// place into array...
					inheritedSocks[numInheritedSocks++] = (Stream *)dc_rsock;
					break;
				case '2':
					dc_ssock = new SafeSock();
					ptmp=strtok(NULL," ");
					dc_ssock->serialize(ptmp);
					dc_ssock->set_inheritable(FALSE);
					dprintf(D_DAEMONCORE,"Inherited a SafeSock\n");
					// place into array...
					inheritedSocks[numInheritedSocks++] = (Stream *)dc_ssock;
					break;
				default:
					EXCEPT("Daemoncore: Can only inherit SafeSock or ReliSocks");
					break;
			} // end of switch
			ptmp=strtok(NULL," ");
		}
		inheritedSocks[numInheritedSocks] = NULL;

		// inherit our "command" cedar socks.  they are sent
		// relisock, then safesock, then a "0".
		// we then register rsock and ssock as command sockets below...
		dc_rsock = NULL;
		dc_ssock = NULL;
		ptmp=strtok(NULL," ");
		if ( ptmp && (strcmp(ptmp,"0") != 0) ) {
			dprintf(D_DAEMONCORE,"Inheriting Command Sockets\n");
			dc_rsock = new ReliSock();
			((ReliSock *)dc_rsock)->serialize(ptmp);
			dc_rsock->set_inheritable(FALSE);
		}
		ptmp=strtok(NULL," ");
		if ( ptmp && (strcmp(ptmp,"0") != 0) ) {
			dc_ssock = new SafeSock();
			dc_ssock->serialize(ptmp);
			dc_ssock->set_inheritable(FALSE);
		}

	}	// end of if we read out CONDOR_INHERIT ok

	if ( inheritbuf != NULL ) {
		free( inheritbuf );
	}
}


void
DaemonCore::InitCommandSocket( int command_port )
{
	if( command_port == 0 ) {
			// No command port wanted, just bail.
		dprintf( D_ALWAYS, "DaemonCore: No command port requested.\n" );
		return;
	}

	dprintf( D_DAEMONCORE, "Setting up command socket\n" );

		// First, try to inherit the sockets from our parent.
	Inherit();

		// If dc_rsock/dc_ssock are still NULL, we need to create our
		// own udp and tcp sockets, bind them, etc.
	if( dc_rsock == NULL && dc_ssock == NULL ) {
		dc_rsock = new ReliSock;
		dc_ssock = new SafeSock;
		if( !dc_rsock ) {
			EXCEPT( "Unable to create command Relisock" );
		}
		if( !dc_ssock ) {
			EXCEPT( "Unable to create command SafeSock" );
		}
		if( command_port == -1 ) {
				// choose any old port (dynamic port)
			if( !BindAnyCommandPort(dc_rsock, dc_ssock) ) {
				EXCEPT("BindAnyCommandPort failed");
			}
			if( !dc_rsock->listen() ) {
				EXCEPT( "Failed to post listen on command ReliSock" );
			}
		} else {
				// use well-known port specified by command_port
			int on = 1;

				// Set options on this socket, SO_REUSEADDR, so that
				// if we are binding to a well known port, and we
				// crash, we can be restarted and still bind ok back
				// to this same port. -Todd T, 11/97

			if( (!dc_rsock->setsockopt(SOL_SOCKET, SO_REUSEADDR,
									   (char*)&on, sizeof(on))) ||
				(!dc_ssock->setsockopt(SOL_SOCKET, SO_REUSEADDR,
									   (char*)&on, sizeof(on))) ) {
				EXCEPT( "setsockopt() SO_REUSEADDR failed\n" );
			}

			/* Set no delay to disable Nagle, since we buffer all our 
			   relisock output and it degrades performance of our 
			   various chatty protocols. -Todd T, 9/05
			 */
			if( !dc_rsock->setsockopt(IPPROTO_TCP, TCP_NODELAY,
									(char*)&on, sizeof(on)) )
		    {
				dprintf(D_ALWAYS,"Warning: setsockopt() TCP_NODELAY failed\n");
			}

			if( (!dc_rsock->listen(command_port)) ||
				(!dc_ssock->bind(command_port)) ) {
				EXCEPT("Failed to bind to or post listen on command socket(s)");
			}
		}
	}

		// If we are the collector, increase the socket buffer size.  This
		// helps minimize the number of updates (UDP packets) the collector
		// drops on the floor.
	if( strcmp(mySubSystem,"COLLECTOR") == 0 ) {
		int desired_size;
		char *tmp;

		if( (tmp=param("COLLECTOR_SOCKET_BUFSIZE")) ) {
			desired_size = atoi(tmp);
			free(tmp);
		} else {
				// default to 1 meg of buffers.  Gulp!
			desired_size = 1024000;
		}

			// set the UDP (ssock) read size to be large, so we do not
			// drop incoming updates.
		int final_size = dc_ssock->set_os_buffers( desired_size );

			// and also set the outgoing TCP write size to be large so the
			// collector is not blocked on the network when answering queries
		dc_rsock->set_os_buffers( desired_size, true );

		dprintf( D_FULLDEBUG,"Reset OS socket buffer size to %dk\n",
				 final_size / 1024 );
	}

#ifdef WANT_NETMAN
		// The negotiator gets a lot of UDP messages from schedds,
		// shadows, and checkpoint servers reporting network
		// usage.  We increase our UDP read buffers here so we
		// don't drop those messages.
	if( strcmp(mySubSystem,"NEGOTIATOR") == 0 ) {
		int desired_size;
		char *tmp;

		if( (tmp=param("NEGOTIATOR_SOCKET_BUFSIZE")) ) {
			desired_size = atoi( tmp );
			free( tmp );

				// set the UDP (ssock) read size to be large, so we do
				// not drop incoming updates.
			int final_size = dc_ssock->set_os_buffers( desired_size );

			dprintf( D_FULLDEBUG,"Reset OS socket buffer size to %dk\n",
					 final_size / 1024 );
		}
	}
#endif

		// now register these new command sockets.
		// Note: In other parts of the code, we assume that the
		// first command socket registered is TCP, so we must
		// register the rsock socket first.
	Register_Command_Socket( (Stream*)dc_rsock );
	Register_Command_Socket( (Stream*)dc_ssock );

	dprintf( D_ALWAYS,"DaemonCore: Command Socket at %s\n",
			 InfoCommandSinfulString() );

		// check if our command socket is on 127.0.0.1, and spit out a
		// warning if it is, since it probably means that /etc/hosts
		// is misconfigured [to preempt RUST like rust-admin #2915]

	const unsigned int my_ip = dc_rsock->get_ip_int();
	const unsigned int loopback_ip = ntohl( inet_addr( "127.0.0.1" ) );

	if( my_ip == loopback_ip ) {
		dprintf( D_ALWAYS, "WARNING: Condor is running on the loopback address (127.0.0.1)\n" );
		dprintf( D_ALWAYS, "         of this machine, and is not visible to other hosts!\n" );
		dprintf( D_ALWAYS, "         This may be due to a misconfigured /etc/hosts file.\n" );
		dprintf( D_ALWAYS, "         Please make sure your hostname is not listed on the\n" );
		dprintf( D_ALWAYS, "         same line as localhost in /etc/hosts.\n" );
	}

		// Now, drop this sinful string into a file, if
		// mySubSystem_ADDRESS_FILE is defined.
	drop_addr_file();

		// now register any DaemonCore "default" handlers

		// register the command handler to take care of signals
	daemonCore->Register_Command( DC_RAISESIGNAL, "DC_RAISESIGNAL",
				(CommandHandlercpp)&DaemonCore::HandleSigCommand,
				"HandleSigCommand()", daemonCore, IMMEDIATE_FAMILY );

		// this handler receives process exit info
	daemonCore->Register_Command( DC_PROCESSEXIT,"DC_PROCESSEXIT",
				(CommandHandlercpp)&DaemonCore::HandleProcessExitCommand,
				"HandleProcessExitCommand()", daemonCore, IMMEDIATE_FAMILY );

		// this handler receives keepalive pings from our children, so
		// we can detect if any of our kids are hung.
	daemonCore->Register_Command( DC_CHILDALIVE,"DC_CHILDALIVE",
				(CommandHandlercpp)&DaemonCore::HandleChildAliveCommand,
				"HandleChildAliveCommand", daemonCore, IMMEDIATE_FAMILY,
				D_FULLDEBUG );
}


#ifndef WIN32
int
DaemonCore::HandleDC_SIGCHLD(int sig)
{
	// This function gets called on Unix when one or more processes
	// in our pid table has terminated.
	// We need to reap the process, get the exit status,
	// and call HandleProcessExit to call a reaper.
	pid_t pid;
	int status;
	WaitpidEntry wait_entry;
	bool first_time = true;


	assert( sig == SIGCHLD );

	for(;;) {
		errno = 0;
        if( (pid = waitpid(-1,&status,WNOHANG)) <= 0 ) {
			if( errno == EINTR ) {
					// Even though we're not supposed to be getting
					// any signals inside DaemonCore methods,
					// sometimes we get EINTR here.  In this case, we
					// want to re-do the waitpid(), not break out of
					// the loop, to make sure we're not leaving any
					// zombies lying around.  -Derek Wright 2/26/99
				continue;
			}

			if( errno == ECHILD || errno == EAGAIN || errno == 0 ) {
				dprintf( D_FULLDEBUG,
						 "DaemonCore: No more children processes to reap.\n" );
			} else {
					// If it's not what we expect, we want D_ALWAYS
				dprintf( D_ALWAYS, "waitpid() returned %d, errno = %d\n",
						 pid, errno );
			}
            break; // out of the for loop and do not post DC_SERVICEWAITPIDS
        }
#if defined(LINUX) && defined(TDP)
		if( WIFSIGNALED(status) && WTERMSIG(status) == SIGTRAP ) {
				// This means the process has recieved a SIGTRAP to be
				// stopped.  Oddly, on Linux, this generates a
				// SIGCHLD.  So, we don't want to call the reaper for
				// this process, since it hasn't really exited.  So,
				// just call continue to ignore this particular pid.
			dprintf( D_FULLDEBUG, "received SIGCHLD from stopped TDP process\n");
			continue;
		}
#endif /* LINUX && TDP */

		// HandleProcessExit(pid, status);
		wait_entry.child_pid = pid;
		wait_entry.exit_status = status;
		WaitpidQueue.enqueue(wait_entry);
		if (first_time) {
			Send_Signal( mypid, DC_SERVICEWAITPIDS );
			first_time = false;
		}

	}

	return TRUE;
}
#endif // of ifndef WIN32

int
DaemonCore::HandleDC_SERVICEWAITPIDS(int sig)
{
	WaitpidEntry wait_entry;

	if ( WaitpidQueue.dequeue(wait_entry) < 0 ) {
		// queue is empty, just return
		return TRUE;
	}

	// we pulled something off the queue, handle it
	HandleProcessExit(wait_entry.child_pid, wait_entry.exit_status);

	// now check if the queue still has more entries.  if so,
	// repost the DC_SERVICEWAITPIDS signal so we'll eventually
	// come back here and service the next entry.
	if ( !WaitpidQueue.IsEmpty() ) {
		Send_Signal( mypid, DC_SERVICEWAITPIDS );
	}

	return TRUE;
}



#ifdef WIN32
// This function runs in a seperate thread and wathces over children
unsigned
pidWatcherThread( void* arg )
{
	DaemonCore::PidWatcherEntry* entry;
	int i;
	unsigned int numentries;
	bool must_send_signal = false;
	HANDLE hKids[MAXIMUM_WAIT_OBJECTS];
	int last_pidentry_exited = MAXIMUM_WAIT_OBJECTS + 5;
	unsigned int exited_pid;
	DWORD result;
	Queue<DaemonCore::WaitpidEntry> MyExitedQueue;
	DaemonCore::WaitpidEntry wait_entry;

	entry = (DaemonCore::PidWatcherEntry *) arg;

	for (;;) {

	::EnterCriticalSection(&(entry->crit_section));
	numentries = 0;
	for (i=0; i < entry->nEntries; i++ ) {
		if ( (i != last_pidentry_exited) && (entry->pidentries[i]) ) {
			if (InterlockedExchange(&(entry->pidentries[i]->deallocate),0L))
			{
				// deallocate flag was set.  delete this pidentry.
				// no need to close any process/thread handles, since we
				// know this is a pipe - deallocate is flag only set for pipes
				ASSERT( entry->pidentries[i]->hPipe );
				::CloseHandle(entry->pidentries[i]->hPipe);
				delete entry->pidentries[i];
				entry->pidentries[i] = NULL;
				continue;	// on to the next i...
			}
			hKids[numentries] = entry->pidentries[i]->hProcess;
			// if process handle is NULL, it is really a thread
			if ( hKids[numentries] == NULL ) {
				// this is a thread entry, not a process entry
				hKids[numentries] = entry->pidentries[i]->hThread;
			}
			if ( hKids[numentries] == NULL ) {
				// if it is still NULL, it is a pipe entry
				hKids[numentries] = entry->pidentries[i]->hPipe;
			}
			entry->pidentries[numentries] = entry->pidentries[i];
			numentries++;
		}
	}
	hKids[numentries] = entry->event;
	entry->nEntries = numentries;
	::LeaveCriticalSection(&(entry->crit_section));

	// if there are no more entries to watch, AND we do
	// not need to send a signal due to a previous process exit,
	// then we're done.
	if ( numentries == 0 && !must_send_signal )
		return TRUE;	// this return will kill this thread

	/*	The idea here is we call WaitForMultipleObjects in poll mode (zero timeout).
		If we timeout, then call again waiting INFINITE time for something 
		to happen.	Once we do have an event, however, we continue
		to loop, calling WaitForMultipleObjects in poll mode (timeout=0)
		and queing results until nothing is left for us to reap.  We do this
		so we reap children in some sort of deterministic order, since
		WaitForMultipleObjects does not return handles in FIFO order.  This
		is important to prevent DaemonCore from "starving" some child 
		process/thread from being reaped when many new processes are being
		continuously created.
	*/
	result = ::WaitForMultipleObjects(numentries + 1, hKids, FALSE, 0);
	if ( result == WAIT_TIMEOUT ) {
			// our poll saw nothing.  so if need to wake up the main thread
			// out of select, do so now before we block waiting for another event.
			// if must_send_signal flag is set, that means we must wake up the
			// main thread.
		
		bool notify_failed;
		while ( must_send_signal ) {
			// Eventually, we should just call SendSignal for this.
			// But for now, handle it all here.

			// In the post v6.4.x world, SafeSock and startCommand
			// are no longer thread safe, so we must grab our Big_fat lock.			
			::EnterCriticalSection(&Big_fat_mutex); // enter big fat mutex
	        SafeSock sock;
			Daemon d( DT_ANY, daemonCore->InfoCommandSinfulString() );
				// send a NOP command to wake up select()
			notify_failed =
					!sock.connect(daemonCore->InfoCommandSinfulString()) ||
					!d.sendCommand(DC_NOP, &sock, 1);
				// while we have the Big_fat_mutex, copy any exited pids
				// out of our thread local MyExitedQueue and into our main
				// thread's WaitpidQueue (of course, we must have the mutex
				// to go changing anything in WaitpidQueue).
			if ( !MyExitedQueue.IsEmpty() ) {
				daemonCore->HandleSig(_DC_RAISESIGNAL,DC_SERVICEWAITPIDS);
			}
			while (MyExitedQueue.dequeue(wait_entry)==0) {
				daemonCore->WaitpidQueue.enqueue( wait_entry );
			}
			::LeaveCriticalSection(&Big_fat_mutex); // leave big fat mutex

            if ( notify_failed )
			{
				// failed to get the notification off to the main thread.
				// we'll log a message, wait a bit, and try again
				dprintf(D_ALWAYS,
					"PidWatcher thread couldn't notify main thread "
					"(exited_pid=%d)\n", exited_pid);

				::Sleep(500);	// sleep for a half a second (500 ms)
			} else {
				must_send_signal = false;
			}
		}		
		// now just wait for something to happen instead of busy looping.
		result = ::WaitForMultipleObjects(numentries + 1, hKids, FALSE, INFINITE);
	}


	if ( result == WAIT_FAILED ) {
		EXCEPT("WaitForMultipleObjects Failed");
	}

	result = result - WAIT_OBJECT_0;

	// if result = numentries, then we are being told our entry->pidentries
	// array has been modified by another thread, and we should re-read it.
	// if result < numentries, then result signifies a child process
	// which exited.
	if ( (result < numentries) && (result >= 0) ) {

		last_pidentry_exited = result;

		// notify our main thread which process exited
		// note: if it was a thread which exited, the entry's
		// pid contains the tid.  if we are talking about a pipe,
		// set the exited_pid to be zero.
		if ( entry->pidentries[result]->hPipe ) {
			exited_pid = 0;
			if (entry->pidentries[result]->deallocate) {
				// this entry should be deallocated.  set things up so
				// it will be done at the top of the loop; no need to send
				// a signal to break out of select in the main thread, so we
				// explicitly do NOT set the must_send_signal flag here.
				last_pidentry_exited = MAXIMUM_WAIT_OBJECTS + 5;
			} else {
				// pipe is ready and has not been deallocated.
				InterlockedExchange(&(entry->pidentries[result]->pipeReady),1L);
				must_send_signal = true;
			}
		} else {
			exited_pid = entry->pidentries[result]->pid;
		}

		if ( exited_pid ) {
			// a pid exited.  add it to MyExitedQueue, which is a queue of
			// exited pids local to our thread that are waiting to be 
			// added to the main thread WaitpidQueue.
			wait_entry.child_pid = exited_pid;
			wait_entry.exit_status = 0;  // we'll get the status later
			MyExitedQueue.enqueue(wait_entry);
			must_send_signal = true;
		}
	} else {
		// no pid/thread/pipe was signaled; we were signaled because our
		// pidentries array was modified.
		// we must clear last_pidentry_exited before we loop back.
		last_pidentry_exited = MAXIMUM_WAIT_OBJECTS + 5;
	}

	}	// end of infinite for loop

}

// Add this pidentry to be watched by our watcher thread(s)
int
DaemonCore::WatchPid(PidEntry *pidentry)
{
	struct PidWatcherEntry* entry = NULL;
	int alldone = FALSE;

	// First see if we can just add this entry to an existing thread
	PidWatcherList.Rewind();
	while ( (entry=PidWatcherList.Next()) ) {

		::EnterCriticalSection(&(entry->crit_section));

		if ( entry->nEntries == 0 ) {
			// a watcher thread exits when nEntries drop to zero.
			// thus, this thread no longer exists; remove it from our list
			::DeleteCriticalSection(&(entry->crit_section));
			::CloseHandle(entry->event);
			::CloseHandle(entry->hThread);
			PidWatcherList.DeleteCurrent();
			delete entry;
			continue;	// so we dont hit the LeaveCriticalSection below
		}

		if ( entry->nEntries < ( MAXIMUM_WAIT_OBJECTS - 1 ) ) {
			// found one with space
			entry->pidentries[entry->nEntries] = pidentry;
			pidentry->watcherEvent = entry->event;
			(entry->nEntries)++;
			if ( !::SetEvent(entry->event) ) {
				EXCEPT("SetEvent failed");
			}
			alldone = TRUE;
		}

		::LeaveCriticalSection(&(entry->crit_section));

		if (alldone == TRUE )
			return TRUE;
	}

	// All watcher threads have their hands full (or there are no
	// watcher threads!).  We need to create a new watcher thread.
	entry = new PidWatcherEntry;
	::InitializeCriticalSection(&(entry->crit_section));
	entry->event = ::CreateEvent(NULL,FALSE,FALSE,NULL);	// auto-reset event
	if ( entry->event == NULL ) {
		EXCEPT("CreateEvent failed");
	}
	entry->pidentries[0] = pidentry;
	pidentry->watcherEvent = entry->event;
	entry->nEntries = 1;
	unsigned threadId;
	entry->hThread = (HANDLE) _beginthreadex(NULL, 1024,
		(CRT_THREAD_HANDLER)pidWatcherThread,
		entry, 0, &threadId );
	if ( entry->hThread == NULL ) {
		EXCEPT("CreateThread failed");
	}

	PidWatcherList.Append(entry);

	return TRUE;
}

#endif  // of WIN32


int DaemonCore::HandleProcessExitCommand(int command, Stream* stream)
{
	unsigned int pid;
	int result = TRUE;

	assert( command == DC_PROCESSEXIT );

	// We have been sent a DC_PROCESSEXIT command

	// read the pid from the socket
	if (!stream->code(pid))
		result = FALSE;

	// do the eom
	if ( !stream->end_of_message() )
		result = FALSE;

	if ( result == FALSE )
		return FALSE;

	// and call HandleProcessExit to call the reaper
	return( HandleProcessExit(pid,0) );
}

// This function gets calls with the pid of a process which just exited.
// On Unix, the exit_status is also provided; on NT, we need to fetch
// the exit status here.  Then we call any registered reaper for this process.
int DaemonCore::HandleProcessExit(pid_t pid, int exit_status)
{
	PidEntry* pidentry;
	int i;
	const char *whatexited = "pid";	// could be changed to "tid"

	// Fetch the PidEntry for this pid from our hash table.
	if ( pidTable->lookup(pid,pidentry) == -1 ) {

		if( defaultReaper!=-1 ) {
			pidentry = new PidEntry;
			ASSERT(pidentry);
			pidentry->parent_is_local = TRUE;
			pidentry->reaper_id = defaultReaper;
			pidentry->hung_tid = -1;
		} else {

			// we did not find this pid... probably popen finished.
			// log a message and return FALSE.

			dprintf(D_DAEMONCORE,
				"Unknown process exited (popen?) - pid=%d\n",pid);
			return FALSE;
		}
	}

    //Now the child is gone, clear all sessions asssociated with the child
    clearSession(pid);

	// If process is Unix, we are passed the exit status.
	// If process is NT and is remote, we are passed the exit status.
	// If process is NT and is local, we need to fetch the exit status here.
#ifdef WIN32
	pidentry->deallocate = 0L; // init deallocate on WIN32

	if ( pidentry->is_local ) {
		DWORD winexit;

		// if hProcess is not NULL, reap process exit status, else
		// reap a thread's exit code.
		if ( pidentry->hProcess ) {
			// a process exited
			if ( !::GetExitCodeProcess(pidentry->hProcess,&winexit) ) {
				dprintf(D_ALWAYS,
					"WARNING: Cannot get exit status for pid = %d\n",pid);
				return FALSE;
			}
		} else {
			// a thread created with DC Create_Thread exited
			if ( !::GetExitCodeThread(pidentry->hThread,&winexit) ) {
				dprintf(D_ALWAYS,
					"WARNING: Cannot get exit status for tid = %d\n",pid);
				return FALSE;
			}
			whatexited = "tid";
		}
		if ( winexit == STILL_ACTIVE ) {	// should never happen
			EXCEPT("DaemonCore: HandleProcessExit() and %s %d still running",
				whatexited, pid);
		}
		exit_status = winexit;
	}
#endif   // of WIN32

	// If parent process is_local, simply invoke the reaper here.
	// If remote, call the DC_INVOKEREAPER command.
	if ( pidentry->parent_is_local ) {

		// Set i to be the entry in the reaper table to use
		i = pidentry->reaper_id - 1;

		// Invoke the reaper handler if there is one registered
		if( (i >= 0) && (reapTable[i].handler || reapTable[i].handlercpp) ) {
			// Set curr_dataptr for Get/SetDataPtr()
			curr_dataptr = &(reapTable[i].data_ptr);

			// Log a message
			char *hdescrip = reapTable[i].handler_descrip;
			if ( !hdescrip )
				hdescrip = EMPTY_DESCRIP;
			dprintf(D_DAEMONCORE,
				"DaemonCore: %s %lu exited with status %d, invoking reaper "
				"%d <%s>\n",
				whatexited, (unsigned long)pid, exit_status, i+1, hdescrip);

			if ( reapTable[i].handler )
				// a C handler
				(*(reapTable[i].handler))(reapTable[i].service,pid,exit_status);
			else
			if ( reapTable[i].handlercpp )
				// a C++ handler
				(reapTable[i].service->*(reapTable[i].handlercpp))(pid,exit_status);

				// Make sure we didn't leak our priv state
			CheckPrivState();

			// Clear curr_dataptr
			curr_dataptr = NULL;
		} else {
			// no registered reaper
			dprintf(D_DAEMONCORE,
			"DaemonCore: %s %lu exited with status %d; no registered reaper\n",
				whatexited, (unsigned long)pid, exit_status);
		}
	} else {
		// TODO: the parent for this process is remote.
		// send the parent a DC_INVOKEREAPER command.
	}

	// Now remove this pid from our tables ----
		// remove from hash table
	pidTable->remove(pid);
#ifdef WIN32
		// close WIN32 handles
	::CloseHandle(pidentry->hThread);
	// must check hProcess cuz could be NULL if just a thread
	if (pidentry->hProcess) {
		::CloseHandle(pidentry->hProcess);
	}
#endif
	// cancel the hung timer if we have one
	if ( pidentry->hung_tid != -1 ) {
		Cancel_Timer(pidentry->hung_tid);
	}
	// and delete the pidentry
	delete pidentry;

	// Finally, some hard-coded logic.  If the pid that exited was our parent,
	// then shutdown gracefully.
	// TODO: should also set a timer and do a fast/hard kill later on!
	if (pid == ppid) {
		dprintf(D_ALWAYS,
				"Our Parent process (pid %lu) exited; shutting down\n",
				(unsigned long)pid);
		Send_Signal(mypid,SIGTERM);	// SIGTERM means shutdown graceful
	}

	return TRUE;
}

const char* DaemonCore::GetExceptionString(int sig)
{
	static char exception_string[80];

#ifdef WIN32
	sprintf(exception_string,"exception %s",
		ExceptionHandler::GetExceptionString(sig));
#else
	if ( sig > 64 ) {
		sig = WTERMSIG(sig);
	}
	sprintf(exception_string,"signal %d",sig);
#endif

	return exception_string;
}


int DaemonCore::HandleChildAliveCommand(int, Stream* stream)
{
	pid_t child_pid;
	unsigned int timeout_secs;
	PidEntry *pidentry;
	int ret_value;

	if (!stream->code(child_pid) ||
		!stream->code(timeout_secs) ||
		!stream->end_of_message()) {
		dprintf(D_ALWAYS,"Failed to read ChildAlive packet\n");
		return FALSE;
	}

	if ((pidTable->lookup(child_pid, pidentry) < 0)) {
		// we have no information on this pid
		dprintf(D_ALWAYS,
			"Received child alive command from unknown pid %d\n",child_pid);
		return FALSE;
	}

	if ( pidentry->hung_tid != -1 ) {
		ret_value = daemonCore->Reset_Timer( pidentry->hung_tid, timeout_secs );
		ASSERT( ret_value != -1 );
	} else {
		pidentry->hung_tid =
			Register_Timer(timeout_secs,
							(Eventcpp) &DaemonCore::HungChildTimeout,
							"DaemonCore::HungChildTimeout", this);
		ASSERT( pidentry->hung_tid != -1 );

		/* allocate a piece of memory here so 64bit architectures don't
			complain about assigning a non-pointer to a pointer type */
		/* XXX this memory is leaked if the timer is canceled before it fires.
			*/
		pid_t *child_pid_ptr = new pid_t[1];
		child_pid_ptr[0] = child_pid;
		Register_DataPtr( child_pid_ptr );
	}

	pidentry->was_not_responding = FALSE;

	dprintf(D_DAEMONCORE,
		"received childalive, pid=%d, secs=%d\n",child_pid,timeout_secs);

	return TRUE;

}

int DaemonCore::HungChildTimeout()
{
	pid_t hung_child_pid;
	pid_t *hung_child_pid_ptr;
	PidEntry *pidentry;

	/* get the pid out of the allocated memory it was placed into */
	hung_child_pid_ptr = (pid_t*)GetDataPtr();
	hung_child_pid = hung_child_pid_ptr[0];
	delete [] hung_child_pid_ptr;
	hung_child_pid_ptr = NULL;

	if ((pidTable->lookup(hung_child_pid, pidentry) < 0)) {
		// we have no information on this pid, it must have exited
		return FALSE;
	}

	// reset our tid to -1 so HandleChildAliveCommand() knows that there
	// is currently no timer set.
	pidentry->hung_tid = -1;

	// set a flag in the PidEntry so a reaper can discover it was killed
	// because it was hung.
	pidentry->was_not_responding = TRUE;

	// now we give the child one last chance to save itself.  we do this by
	// servicing any waiting commands, since there could be a child_alive
	// command sitting there in our receive buffer.  the reason we do this
	// is to handle the case where both the child _and_ the parent have been
	// hung for a period of time (e.g. perhaps the log files are on a hard
	// mounted NFS volume, and everyone was blocked until the NFS server
	// returned).  in this situation we should try to avoid killing the child.
	// so service the buffered commands and check if the was_not_responding
	// flag flips back to false.
	ServiceCommandSocket();

	// Now make certain that this pid did not exit by verifying we still
	// exist in the pid table.  We must do this because ServiceCommandSocket
	// could result in a process reaper being invoked.
	if ((pidTable->lookup(hung_child_pid, pidentry) < 0)) {
		// we have no information anymore on this pid, it must have exited
		return FALSE;
	}

	// Now see if was_not_responding flipped back to FALSE
	if ( pidentry->was_not_responding == FALSE ) {
		// the child saved itself!
		return FALSE;
	}

	dprintf(D_ALWAYS,"ERROR: Child pid %d appears hung! Killing it hard.\n",
		hung_child_pid);

	// and hardkill the bastard!
	Shutdown_Fast(hung_child_pid);

	return TRUE;
}

int DaemonCore::Was_Not_Responding(pid_t pid)
{
	PidEntry *pidentry;

	if ((pidTable->lookup(pid, pidentry) < 0)) {
		// we have no information on this pid, assume the safe
		// case.
		return FALSE;
	}

	return pidentry->was_not_responding;
}


int DaemonCore::SendAliveToParent()
{
    SafeSock sock;
	char parent_sinful_string[30];
	char *tmp;

	dprintf(D_FULLDEBUG,"DaemonCore: in SendAliveToParent()\n");

	tmp = InfoCommandSinfulString(ppid);
	if ( tmp ) {
			// copy the result from InfoCommandSinfulString to the
			// stack, because the pointer we got back is a static buffer
		strcpy(parent_sinful_string,tmp);
	} else {
		dprintf(D_FULLDEBUG,"DaemonCore: No parent_sinful_string. SendAliveToParent() failed.\n");
			// parent already gone?
		return FALSE;
	}

	dprintf(D_FULLDEBUG,"DaemonCore: attempting to connect to '%s'\n", parent_sinful_string);
	if (!sock.connect(parent_sinful_string)) {
		dprintf(D_FULLDEBUG,"DaemonCore: Could not connect to parent. SendAliveToParent() failed.\n");
		return FALSE;
	}

	Daemon d( DT_ANY, parent_sinful_string );
	if (!d.startCommand(DC_CHILDALIVE, &sock, 0)) {
		dprintf(D_FULLDEBUG,"DaemonCore: startCommand() failed. SendAliveToParent() failed.\n");
		return FALSE;
	}

	dprintf( D_DAEMONCORE, "DaemonCore: Sending alive to %s\n",
			 parent_sinful_string );

	sock.encode();
	sock.code(mypid);
	sock.code(max_hang_time);
	sock.end_of_message();

	return TRUE;
}

#ifndef WIN32
char **DaemonCore::ParseArgsString(const char *str)
{
	char separator1, separator2;
	int maxlength;
	char **argv, *arg;
	int nargs=0;

	separator1 = ' ';
	separator2 = '\t';

	/*
	maxlength is the maximum number of args and the maximum
	length of any one arg that could be found in this string.
	A little waste is insignificant here.
	*/

	maxlength = strlen(str)+1;

	argv = new char*[maxlength];

	/* While there are characters left... */
	while(*str) {
		/* Skip over any sequence of whitespace */
		while( *str == separator1 || *str == separator2 ) {
			str++;
		}

		/* If we are not at the end... */
		if(*str) {

			/* Allocate a new string */
			argv[nargs] = new char[maxlength];

			/* Copy the arg into the new string */
			arg = argv[nargs];
			while( *str && *str != separator1 && *str != separator2 ) {
				*arg++ = *str++;
			}
			*arg = 0;

			/* Move on to the next argument */
			nargs++;
		}
	}

	argv[nargs] = 0;
	return argv;
}
#endif

int
BindAnyCommandPort(ReliSock *rsock, SafeSock *ssock)
{
	for(int i = 0; i < 1000; i++) {
		if ( !rsock->bind() ) {
			dprintf(D_ALWAYS, "Failed to bind to command ReliSock\n");

#ifndef WIN32
			dprintf(D_ALWAYS, "(Make sure your IP address is correct in /etc/hosts.)\n");
#endif
#ifdef WIN32
			dprintf(D_ALWAYS, "(Your system network settings might be invalid.)\n");
#endif

			return FALSE;
		}
		// now open a SafeSock _on the same port_ choosen above
		if ( !ssock->bind(rsock->get_port()) ) {
			rsock->close();
			continue;
		}
		return TRUE;
	}
	dprintf(D_ALWAYS, "Error: BindAnyCommandPort failed!\n");
	return FALSE;
}

/**  Is_Pid_Alive() returns TRUE is pid lives, FALSE is that pid has exited.
     By Alive, (at least on UNIX), we mean either the process is still running,
     or the process is no longer running but we've called wait() so it no
     no longer exists in the kernel's process table, but we haven't called the
     application's reaper function yet
*/

int DaemonCore::Is_Pid_Alive(pid_t pid)
{
	int status = FALSE;

#ifndef WIN32

	// First, let's try and make sure that it's not already dead but
	// maybe in our Queue of pids we've called wait() on but haven't
	// reaped...

	WaitpidEntry wait_entry;
	wait_entry.child_pid = pid;

	if(WaitpidQueue.IsMember(wait_entry)) {
		status = TRUE;
		return status;
	}
	// on Unix, just try to send pid signal 0.  if sucess, pid lives.
	// first set priv_state to root, to make certain kill() does not fail
	// due to permissions.
	// News Flash!  This doesn't work in things like DAGMan, which are
	// running as USER_PRIV_FINAL.  So, we need to do more trickery.
	priv_state priv = set_root_priv();

	errno = 0;
	if ( ::kill(pid,0) == 0 ) {
		status = TRUE;
	} else {
		// Now, if errno == EPERM, that means that if we had the
		// right permissions we could kill it... and that its there!
		// and we should return TRUE.
		// If its ESRCH then the PID wasn't there, and then status
		// should be false.
		if ( errno == EPERM ) {
			dprintf(D_FULLDEBUG, "DaemonCore::IsPidAlive(): kill returned "
				"EPERM, assuming pid %d is alive.\n", pid);
			status = TRUE;
		} else {
			dprintf(D_FULLDEBUG, "DaemonCore::IsPidAlive(): kill returned "
				"errno %d, assuming pid %d is dead.\n", errno, pid);
			status = FALSE; // Just for consistancy.
		}
	}
	set_priv(priv);
#else
	// on Win32, open a handle to the pid and call GetExitStatus
	HANDLE pidHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,pid);
	if (pidHandle) {
		DWORD exitstatus;
		if ( ::GetExitCodeProcess(pidHandle,&exitstatus) ) {
			if ( exitstatus == STILL_ACTIVE )
				status = TRUE;
		}
		::CloseHandle(pidHandle);
	} else {
		dprintf(D_FULLDEBUG,"DaemonCore::IsPidAlive(): OpenProcess failed\n");
		// OpenProcess() may have failed
		// due to permissions, or because all handles to that pid are gone.
		if ( ::GetLastError() == 5 ) {
			// failure due to permissions.  this means the process object must
			// still exist, although we have no idea if the process itself still
			// does or not.  error on the safe side; return TRUE.
			status = TRUE;
		} else {
			// process object no longer exists, so process must be gone.
			status = FALSE;
		}
	}
#endif

	return status;
}


priv_state
DaemonCore::Register_Priv_State( priv_state priv )
{
	priv_state old_priv = Default_Priv_State;
	Default_Priv_State = priv;
	return old_priv;
}

bool
DaemonCore::CheckConfigSecurity( const char* config, Sock* sock )
{
	// we've got to check each textline of the string passed in by
	// config.  here we use the StringList class to split lines.

	StringList all_attrs (config, "\n");

	// start out by assuming everything is okay.  we'll check all
	// the attrs and set this flag if something is not authorized.
	bool  all_attrs_okay = true;

	char *single_attr;
	all_attrs.rewind();

	// short-circuit out of the while once any attribute is not
	// okay.  otherwise, get one value at a time
	while (all_attrs_okay && (single_attr = all_attrs.next())) {
		// check this individual attr
		if (!CheckConfigAttrSecurity(single_attr, sock)) {
			all_attrs_okay = false;
		}
	}

	return all_attrs_okay;
}



bool
DaemonCore::CheckConfigAttrSecurity( const char* attr, Sock* sock )
{
	char *name, *tmp;
	char* ip_str;
	int i;

	if( ! (name = strdup(attr)) ) {
		EXCEPT( "Out of memory!" );
	}
	tmp = strchr( name, '=' );
	if( ! tmp ) {
		tmp = strchr( name, ':' );
	}
	if( tmp ) {
			// someone's trying to set something, so we should trim
			// off the value they want to set it to and any whitespace
			// so we can just look at the attribute name.
		*tmp = ' ';
		while( isspace(*tmp) ) {
			*tmp = '\0';
			tmp--;
		}
	}

#if (DEBUG_SETTABLE_ATTR_LISTS)
		dprintf( D_ALWAYS, "CheckConfigSecurity: name is: %s\n", name );
#endif

		// Now, name should point to a NULL-terminated version of the
		// attribute name we're trying to set.  This is what we must
		// compare against our SettableAttrsLists.  We need to iterate
		// through all the possible permission levels, and for each
		// one, see if we find the given attribute in the
		// corresponding SettableAttrsList.
	for( i=0; i<LAST_PERM; i++ ) {

			// skip permission levels we know we don't want to trust
		if( i == ALLOW || i == IMMEDIATE_FAMILY ) {
			continue;
		}

		if( ! SettableAttrsLists[i] ) {
				// there's no list for this perm level, skip it.
			continue;
		}

			// if we're here, we might allow someone to set something
			// if they qualify for the perm level we're considering.
			// so, now see if the connection qualifies for this access
			// level.

		if( Verify((DCpermission)i, sock->endpoint(), sock->getFullyQualifiedUser())) {
				// now we can see if the specific attribute they're
				// trying to set is in our list.
			if( (SettableAttrsLists[i])->
				contains_anycase_withwildcard(name) ) {
					// everything's cool.  allow this.

#if (DEBUG_SETTABLE_ATTR_LISTS)
				dprintf( D_ALWAYS, "CheckConfigSecurity: "
						 "found %s at perm level %s\n", name,
						 PermString((DCpermission)i) );
#endif

				free( name );
				return true;
			}
		}
	} // end of for()

		// If we're still here, someone is trying to set something
		// they're not allowed to set.  print this out into the log so
		// folks can see that things are failing due to permissions.

		// Grab a pointer to this string, since it's a little bit
		// expensive to re-compute.
	ip_str = sock->endpoint_ip_str();
		// Upper-case-ify the string for everything we print out.
	strupr(name);

		// First, log it.
	dprintf( D_ALWAYS,
			 "WARNING: Someone at %s is trying to modify \"%s\"\n",
			 ip_str, name );
	dprintf( D_ALWAYS,
			 "WARNING: Potential security problem, request refused\n" );

	free( name );
	return false;
}


void
DaemonCore::InitSettableAttrsLists( void )
{
	int i;

		// First, clean out anything that might be there already.
	for( i=0; i<LAST_PERM; i++ ) {
		if( SettableAttrsLists[i] ) {
			delete SettableAttrsLists[i];
			SettableAttrsLists[i] = NULL;
		}
	}

		// Now, for each permission level we care about, see if
		// there's an entry in the config file.  We first check for
		// "<SUBSYS>_SETTABLE_ATTRS_<PERM-LEVEL>", if that's not
		// there, we just check for "SETTABLE_ATTRS_<PERM-LEVEL>".
	for( i=0; i<LAST_PERM; i++ ) {
			// skip permission levels we know we don't want to trust
		if( i == ALLOW || i == IMMEDIATE_FAMILY ) {
			continue;
		}
		if( InitSettableAttrsList(mySubSystem, i) ) {
				// that worked, move on to the next perm level
			continue;
		}
			// there's no subsystem-specific one, just try the generic
			// version.  if this doesn't work either, we just leave
			// this StringList NULL and will ignore cmds from it.
		InitSettableAttrsList( NULL, i );
	}

#if (DEBUG_SETTABLE_ATTR_LISTS)
		// Just for debugging, print out everything
	char* tmp;
	for( i=0; i<LAST_PERM; i++ ) {
		if( SettableAttrsLists[i] ) {
			tmp = (SettableAttrsLists[i])->print_to_string();
			dprintf( D_ALWAYS, "SettableAttrList[%s]: %s\n",
					 PermString((DCpermission)i), tmp );
			free( tmp );
		}
	}
#endif
}


bool
DaemonCore::InitSettableAttrsList( const char* subsys, int i )
{
	MyString param_name;
	char* tmp;

	if( subsys ) {
		param_name = subsys;
		param_name += "_SETTABLE_ATTRS_";
	} else {
		param_name = "SETTABLE_ATTRS_";
	}
	param_name += PermString((DCpermission)i);
	tmp = param( param_name.Value() );
	if( tmp ) {
		SettableAttrsLists[i] = new StringList;
		(SettableAttrsLists[i])->initializeFromString( tmp );
		free( tmp );
		return true;
	}
	return false;
}


KeyCache*
DaemonCore::getKeyCache() {
	return sec_man->session_cache;
}

SecMan* DaemonCore :: getSecMan()
{
    return sec_man;
}

void DaemonCore :: clearSession(pid_t pid)
{
	// this will clear any incoming sessions associated with the PID, even
	// if it isn't a daemoncore child (like the stupid old shadow) and
	// therefor has no command sock.
	if(sec_man) {
		sec_man->invalidateByParentAndPid(sec_man->my_unique_id(), pid);
	}

	// we also need to clear any outgoing sessions associated w/ this pid.
    PidEntry * pidentry = NULL;

    if ( pidTable->lookup(pid,pidentry) != -1 ) {
        if (sec_man && pidentry) {
            sec_man->invalidateHost(pidentry->sinful_string);
        }
    }
}

void DaemonCore :: invalidateSessionCache()
{
	/* for now, never invalidate the session cache */
	return;

    if (sec_man) {
        sec_man->invalidateAllCache();
    }
}


bool DaemonCore :: set_cookie( int len, unsigned char* data ) {
	if (_cookie_data) {
		  // if we have a cookie already, keep it
		  // around in case some packet that's already
		  // queued uses it.
		if ( _cookie_data_old ) {
			free(_cookie_data_old);
		}
		_cookie_data_old = _cookie_data;
		_cookie_len_old  = _cookie_len;

		// now clear the current cookie data
		_cookie_data = NULL;
		_cookie_len  = 0;
	}

	if (data) {
		_cookie_data = (unsigned char*) malloc (len);
		if (!_cookie_data) {
			// out of mem
			return false;
		}
		_cookie_len = len;
		memcpy (_cookie_data, data, len);
	}

	return true;
}

bool DaemonCore :: get_cookie( int &len, unsigned char* &data ) {
	if (data != NULL) {
		return false;
	}
	data = (unsigned char*) malloc (_cookie_len);
	if (!data) {
		// out of mem
		return false;
	}

	len = _cookie_len;
	memcpy (data, _cookie_data, _cookie_len);

	return true;
}

bool DaemonCore :: cookie_is_valid( unsigned char* data ) {

	if ( data == NULL || _cookie_data == NULL ) {
		return false;
	}

	if ( strcmp((char*)_cookie_data, (char*)data) == 0 ) {
		// we have a match... trust this command.
		return true;
	} else if ( _cookie_data_old != NULL ) {

		// maybe this packet was queued before we
		// rotated the cookie. So check it with
		// the old cookie.

		if ( strcmp((char*)_cookie_data_old, (char*)data) == 0 ) {
			return true;
		} else {

			// failure. No match.
			return false;
		}
	}
	return false; // to make MSVC++ happy
}

bool
DaemonCore::GetPeacefulShutdown() {
	return peaceful_shutdown;
}

void
DaemonCore::SetPeacefulShutdown(bool value) {
	peaceful_shutdown = value;
}

void 
DaemonCore::RegisterTimeSkipCallback(TimeSkipFunc fnc, void * data)
{
	TimeSkipWatcher * watcher = new TimeSkipWatcher;
	ASSERT(fnc);
	watcher->fn = fnc;
	watcher->data = data;
	if( ! m_TimeSkipWatchers.Append(watcher)) {
		EXCEPT("Unable to register time skip callback.  Possible out of memory condition.");	
	}
}

void 
DaemonCore::UnregisterTimeSkipCallback(TimeSkipFunc fnc, void * data)
{
	m_TimeSkipWatchers.Rewind();
	TimeSkipWatcher * p;
	while( (p = m_TimeSkipWatchers.Next()) ) {
		if(p->fn == fnc && p->data == data) {
			m_TimeSkipWatchers.DeleteCurrent();
			return;
		}
	}
	EXCEPT("Attempted to remove time skip watcher (%p, %p), but it was not registered", fnc, data);
}

void
DaemonCore::CheckForTimeSkip(time_t time_before, time_t okay_delta)
{
	if(m_TimeSkipWatchers.Number() == 0) {
		// No one cares if the clock jumped.
		return;
	}
	/*
	Okay, let's see if the time jumped in an unexpected way.
	*/
	time_t time_after = time(NULL);
	int delta = 0;
		/* Specifically doing the math in time_t space to
		try and avoid getting burned by int being unable to 
		represent a given time_t value.  This means
		different code paths depending on which variable is
		larger. */
	if((time_after + MAX_TIME_SKIP) < time_before) {
		// We've jumped backward in time.

		// If this test is ever made more aggressive, remember that
		// minor updated by ntpd might out time() sampling to
		// occasionally move back 1 second.

		delta = -(int)(time_before - time_after);
	}
	if((time_before + okay_delta*2 + MAX_TIME_SKIP) < time_after) {
		/*
		We've jumped forward in time.

			Why okay_delta*2?  Crude attempt to capture that timers
			aren't necessarily as accurate as we might hope.
		*/
		delta = time_after - time_before - okay_delta;
	}
	if(delta == 0) { 
		// No time jump.  Nothing to see here. Move along, move along.
		return;
	}
	dprintf(D_FULLDEBUG, "Time skip noticed.  The system clock jumped approximately %d seconds.\n", delta);

	// Hrm.  I guess the clock got wonky.  Warn anyone who cares.
	m_TimeSkipWatchers.Rewind();
	TimeSkipWatcher * p;
	while( (p = m_TimeSkipWatchers.Next()) ) {
		ASSERT(p->fn);
		p->fn(p->data, delta);
	}
}
