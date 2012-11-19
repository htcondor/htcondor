/***************************************************************
 *
 * Copyright (C) 1990-2011 Team, Computer Sciences Department,
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

// //////////////////////////////////////////////////////////////////////
//
// Implementation of DaemonCore.
//
//
// //////////////////////////////////////////////////////////////////////

#include "condor_common.h"
#ifdef HAVE_EXT_GSOAP
#include "soap_core.h"
#endif

#include "condor_socket_types.h"

#if HAVE_CLONE
#include <sched.h>
#include <sys/syscall.h>
#endif

#if HAVE_RESOLV_H && HAVE_DECL_RES_INIT
#include <resolv.h>
#endif

static const int DEFAULT_MAXCOMMANDS = 255;
static const int DEFAULT_MAXSIGNALS = 99;
static const int DEFAULT_MAXSOCKETS = 8;
static const int DEFAULT_MAXPIPES = 8;
static const int DEFAULT_MAXREAPS = 100;
static const int DEFAULT_PIDBUCKETS = 11;
static const int DEFAULT_MAX_PID_COLLISIONS = 9;
static const char* DEFAULT_INDENT = "DaemonCore--> ";
static const int MAX_TIME_SKIP = (60*20); //20 minutes
static const int MIN_FILE_DESCRIPTOR_SAFETY_LIMIT = 20;
static const int MIN_REGISTERED_SOCKET_SAFETY_LIMIT = 15;
static const int DC_PIPE_BUF_SIZE = 65536;

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
#include "condor_getcwd.h"
#include "strupr.h"
#include "sig_name.h"
#include "env.h"
#include "condor_secman.h"
#include "condor_distribution.h"
#include "condor_environ.h"
#include "condor_version.h"
#include "setenv.h"
#include "my_popen.h"
#include "../condor_privsep/condor_privsep.h"
#ifdef WIN32
#include "exception_handling.WINDOWS.h"
#include "process_control.WINDOWS.h"
#include "executable_scripts.WINDOWS.h"
#include "access_desktop.WINDOWS.h"
#include "condor_fix_assert.h"
typedef unsigned (__stdcall *CRT_THREAD_HANDLER) (void *);
CRITICAL_SECTION Big_fat_mutex; // coarse grained mutex for debugging purposes
#endif
#include "directory.h"
#include "../condor_io/condor_rw.h"

#include "daemon_core_sock_adapter.h"
#include "HashTable.h"
#include "selector.h"
#include "proc_family_interface.h"
#include "condor_netdb.h"
#include "util_lib_proto.h"
#include "subsystem_info.h"
#include "basename.h"
#include "condor_threads.h"
#include "shared_port_endpoint.h"
#include "condor_open.h"
#include "filename_tools.h"
#include "authentication.h"
#include "condor_claimid_parser.h"
#include "condor_email.h"

#include "valgrind.h"
#include "ipv6_hostname.h"
#include "daemon_command.h"

#if defined ( HAVE_SCHED_SETAFFINITY ) && !defined ( WIN32 )
#include <sched.h>
#endif

static const char* EMPTY_DESCRIP = "<NULL>";

// special errno values that may be returned from Create_Process
const int DaemonCore::ERRNO_EXEC_AS_ROOT = 666666;
const int DaemonCore::ERRNO_PID_COLLISION = 666667;
const int DaemonCore::ERRNO_REGISTRATION_FAILED = 666668;
const int DaemonCore::ERRNO_EXIT = 666669;

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

/*
void zz2printf(int debug_levels, KeyInfo *k) {
	if (param_boolean("SEC_DEBUG_PRINT_KEYS", false)) {
		if (k) {
			char hexout[260];  // holds (at least) a 128 byte key.
			const unsigned char* dataptr = k->getKeyData();
			int   length  =  k->getKeyLength();

			for (int i = 0; (i < length) && (i < 24); i++) {
				sprintf (&hexout[i*2], "%02x", *dataptr++);
			}

			dprintf (debug_levels, "KEYPRINTF: [%i] %s\n", length, hexout);
		} else {
			dprintf (debug_levels, "KEYPRINTF: [NULL]\n");
		}
	}
}
*/

static int _condor_exit_with_exec = 0;

void **curr_dataptr;
void **curr_regdataptr;

extern void drop_addr_file( void );

TimerManager DaemonCore::t;

// Hash function for pid table.
static unsigned int compute_pid_hash(const pid_t &key)
{
	return (unsigned int)key;
}

// DaemonCore constructor.

DaemonCore::DaemonCore(int PidSize, int ComSize,int SigSize,
				int SocSize,int ReapSize,int PipeSize)
{

	if(ComSize < 0 || SigSize < 0 || SocSize < 0 || PidSize < 0)
	{
		EXCEPT("Invalid argument(s) for DaemonCore constructor");
	}

    dc_stats.Init(); // initilize statistics.
    dc_stats.SetWindowSize(20*60);

		// Provide cedar sock with pointers to various daemonCore functions
		// that cannot be directly referenced in cedar, because it
		// is sometimes used in an application that is not linked with
		// DaemonCore.
	daemonCoreSockAdapter.EnableDaemonCore(
		this,
		// Typecast Register_Socket because it is overloaded, and some (all?)
		// compilers have trouble choosing which one to use.
		(DaemonCoreSockAdapterClass::Register_Socket_fnptr)&DaemonCore::Register_Socket,
		&DaemonCore::Cancel_Socket,
		&DaemonCore::CallSocketHandler,
		&DaemonCore::CallCommandHandler,
		&DaemonCore::HandleReqAsync,
		&DaemonCore::Register_DataPtr,
		&DaemonCore::GetDataPtr,
		(DaemonCoreSockAdapterClass::Register_Timer_fnptr)&DaemonCore::Register_Timer,
		(DaemonCoreSockAdapterClass::Register_PeriodicTimer_fnptr)&DaemonCore::Register_Timer,
		&DaemonCore::Cancel_Timer,
		&DaemonCore::TooManyRegisteredSockets,
		&DaemonCore::incrementPendingSockets,
		&DaemonCore::decrementPendingSockets,
		&DaemonCore::publicNetworkIpAddr,
		&DaemonCore::Register_Command,
		&DaemonCore::daemonContactInfoChanged,
		&DaemonCore::Register_Timer_TS);

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

	// our pointer to the ProcFamilyInterface object is initially NULL. the
	// object will be created the first time Create_Process is called with
	// a non-NULL family_info argument
	//
	m_proc_family = NULL;

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
	nPendingSockets = 0;
	SockEnt blankSockEnt;
	memset(&blankSockEnt,'\0',sizeof(SockEnt));
	sockTable->fill(blankSockEnt);

	initial_command_sock = -1;
#ifdef HAVE_EXT_GSOAP
	soap_ssl_sock = -1;
#endif

	m_dirty_sinful = true;

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
	blankPipeEnt.index = -1;
	pipeTable->fill(blankPipeEnt);

	pipeHandleTable = new ExtArray<PipeHandle>(maxPipe);
	maxPipeHandleIndex = -1;
	maxPipeBuffer = 10240;

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
	m_want_send_child_alive = true;

#ifdef WIN32
	dcmainThreadId = ::GetCurrentThreadId();
#endif

#ifndef WIN32
	async_sigs_unblocked = FALSE;
#endif
	async_pipe_signal = false;

		// Note: this cannot be modified on reconfig, requires restart.
	m_wants_dc_udp = param_boolean("WANT_UDP_COMMAND_SOCKET", true);
	m_wants_dc_udp_self = m_wants_dc_udp;
#ifndef WIN32
		// In unix, the shadow never needs a UDP command socket, because
		// the schedd only sends it plain unix signals.  Since there
		// may be a lot of shadows, this is important enough to specially
		// optimize.
	if( get_mySubSystem()->isType(SUBSYSTEM_TYPE_SHADOW) ) {
		m_wants_dc_udp_self = false;
	}
#endif
	m_invalidate_sessions_via_tcp = true;
	dc_rsock = NULL;
	dc_ssock = NULL;
    m_iMaxAcceptsPerCycle = param_integer("MAX_ACCEPTS_PER_CYCLE", 8);
    if( m_iMaxAcceptsPerCycle != 1 ) {
        dprintf(D_ALWAYS,"Setting maximum accepts per cycle %d.\n", m_iMaxAcceptsPerCycle);
    }

	inheritedSocks[0] = NULL;
	inServiceCommandSocket_flag = FALSE;
	m_need_reconfig = false;
	m_delay_reconfig = false;
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

#ifdef HAVE_EXT_GSOAP
#ifdef HAVE_EXT_OPENSSL
	mapfile =  NULL;
#endif
#endif

	file_descriptor_safety_limit = 0; // 0 indicates: needs to be computed

#ifndef WIN32
	char max_fds_name[50];
	sprintf(max_fds_name,"%s_MAX_FILE_DESCRIPTORS",get_mySubSystem()->getName());
	int max_fds = param_integer(max_fds_name,0);
	if( max_fds <= 0 ) {
		max_fds = param_integer("MAX_FILE_DESCRIPTORS",0);
	}
	if( max_fds > 0 ) {
		dprintf(D_ALWAYS,"Setting maximum file descriptors to %d.\n",max_fds);

		priv_state priv = set_root_priv();
		limit(RLIMIT_NOFILE,max_fds,CONDOR_REQUIRED_LIMIT,"MAX_FILE_DESCRIPTORS");
		set_priv(priv);
	}
#endif

	soap = NULL;

	localAdFile = NULL;

	m_collector_list = NULL;
	m_wants_restart = true;
	m_in_daemon_shutdown = false;
	m_in_daemon_shutdown_fast = false;
	m_private_network_name = NULL;

#ifdef HAVE_CLONE
		// This will be initialized from the config file, so just set to
		// false here.
	m_use_clone_to_create_processes = false;
#endif

	m_fake_create_thread = false;

	m_refresh_dns_timer = -1;

	m_ccb_listeners = NULL;
	m_shared_port_endpoint = NULL;
}

// DaemonCore destructor. Delete the all the various handler tables, plus
// delete/free any pointers in those tables.
DaemonCore::~DaemonCore()
{
	int		i;

	if( m_ccb_listeners ) {
		delete m_ccb_listeners;
		m_ccb_listeners = NULL;
	}

	if( m_shared_port_endpoint ) {
		delete m_shared_port_endpoint;
		m_shared_port_endpoint = NULL;
	}

#ifndef WIN32
	close(async_pipe[1]);
	close(async_pipe[0]);
#endif

	if (comTable != NULL )
	{
		for (i=0;i<maxCommand;i++) {
			free( comTable[i].command_descrip );
			free( comTable[i].handler_descrip );
		}
		delete []comTable;
	}

	if (sigTable != NULL)
	{
		for (i=0;i<maxSig;i++) {
			free( sigTable[i].sig_descrip );
			free( sigTable[i].handler_descrip );
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
			free( (*sockTable)[i].iosock_descrip );
			free( (*sockTable)[i].handler_descrip );
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
			free( reapTable[i].reap_descrip );
			free( reapTable[i].handler_descrip );
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

	if (m_proc_family != NULL) {
		delete m_proc_family;
	}

	for( i=0; i<LAST_PERM; i++ ) {
		if( SettableAttrsLists[i] ) {
			delete SettableAttrsLists[i];
		}
	}

	if( pipeTable ) {
		delete( pipeTable );
	}

	if (pipeHandleTable) {
		delete pipeHandleTable;
	}

	t.CancelAllTimers();

	if (_cookie_data) {
		free(_cookie_data);
	}
	if (_cookie_data_old) {
		free(_cookie_data_old);
	}

#ifdef HAVE_EXT_GSOAP
	if( soap ) {
		dc_soap_free(soap);
		soap = NULL;
	}
#endif

	if(localAdFile) {
		free(localAdFile);
		localAdFile = NULL;
	}
	
	if (m_collector_list) {
		delete m_collector_list;
		m_collector_list = NULL;
	}

	if (m_private_network_name) {
		free(m_private_network_name);
		m_private_network_name = NULL;
	}
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
int	DaemonCore::Register_Command(int command, const char* com_descrip,
				CommandHandler handler, const char* handler_descrip, Service* s,
				DCpermission perm, int dprintf_flag, bool force_authentication,
				int wait_for_payload)
{
	return( Register_Command(command, com_descrip, handler,
							(CommandHandlercpp)NULL, handler_descrip, s,
							 perm, dprintf_flag, FALSE, force_authentication,
							 wait_for_payload) );
}

int	DaemonCore::Register_Command(int command, const char *com_descrip,
				CommandHandlercpp handlercpp, const char* handler_descrip,
				Service* s, DCpermission perm, int dprintf_flag,
				bool force_authentication, int wait_for_payload)
{
	return( Register_Command(command, com_descrip, NULL, handlercpp,
							 handler_descrip, s, perm, dprintf_flag, TRUE,
							 force_authentication, wait_for_payload) );
}

int	DaemonCore::Register_CommandWithPayload(int command, const char* com_descrip,
				CommandHandler handler, const char* handler_descrip, Service* s,
				DCpermission perm, int dprintf_flag, bool force_authentication,
				int wait_for_payload)
{
	return( Register_Command(command, com_descrip, handler,
							(CommandHandlercpp)NULL, handler_descrip, s,
							 perm, dprintf_flag, FALSE, force_authentication,
							 wait_for_payload) );
}

int	DaemonCore::Register_CommandWithPayload(int command, const char *com_descrip,
				CommandHandlercpp handlercpp, const char* handler_descrip,
				Service* s, DCpermission perm, int dprintf_flag,
				bool force_authentication, int wait_for_payload)
{
	return( Register_Command(command, com_descrip, NULL, handlercpp,
							 handler_descrip, s, perm, dprintf_flag, TRUE,
							 force_authentication, wait_for_payload) );
}

int	DaemonCore::Register_Signal(int sig, const char* sig_descrip,
				SignalHandler handler, const char* handler_descrip,
				Service* s)
{
	return( Register_Signal(sig, sig_descrip, handler,
							(SignalHandlercpp)NULL, handler_descrip, s,
							FALSE) );
}

int	DaemonCore::Register_Signal(int sig, const char *sig_descrip,
				SignalHandlercpp handlercpp, const char* handler_descrip,
				Service* s)
{
	return( Register_Signal(sig, sig_descrip, NULL, handlercpp,
							handler_descrip, s, TRUE) );
}

int DaemonCore::RegisteredSocketCount()
{
	return nRegisteredSocks + nPendingSockets;
}

int DaemonCore::FileDescriptorSafetyLimit()
{
	if( file_descriptor_safety_limit == 0 ) {
			// Our max is the maxiumum file descriptor that our Selector
			// class says it can handle.
		int file_descriptor_max = Selector::fd_select_size();
		// Set the danger level at 80% of the max
		file_descriptor_safety_limit = file_descriptor_max - file_descriptor_max/5;
		if( file_descriptor_safety_limit < MIN_FILE_DESCRIPTOR_SAFETY_LIMIT ) {
				// There is no point trying to live within this limit,
				// because it is too small.  It is better to try and fail
				// in this case than to trust this limit.
			file_descriptor_safety_limit = MIN_FILE_DESCRIPTOR_SAFETY_LIMIT;
		}

		int p = param_integer( "NETWORK_MAX_PENDING_CONNECTS", 0 );
		if( p!=0 ) {
			file_descriptor_safety_limit = p;
		}

		dprintf(D_FULLDEBUG,"File descriptor limits: max %d, safe %d\n",
				file_descriptor_max,
				file_descriptor_safety_limit);
	}

	return file_descriptor_safety_limit;
}

bool DaemonCore::TooManyRegisteredSockets(int fd,MyString *msg,int num_fds)
{
	int registered_socket_count = RegisteredSocketCount();
	int fds_used = registered_socket_count;
	int safety_limit = FileDescriptorSafetyLimit();

	if( safety_limit < 0 ) {
			// No limit.
		return false;
	}

		// The following heuristic is only appropriate on systems where
		// file descriptor numbers are allocated using the lowest
		// available number.
#if !defined(WIN32)
	if (fd == -1) {
		// TODO If num_fds>1, should we call open() multiple times?
		fd = safe_open_wrapper_follow( NULL_FILE, O_RDONLY );
		if ( fd >= 0 ) {
			close( fd );
		}
	}
	if( fd > fds_used ) {
			// Assume fds are allocated always lowest number first
		fds_used = fd;
	}
#endif

	if( num_fds + fds_used > file_descriptor_safety_limit ) {
		if( registered_socket_count < MIN_REGISTERED_SOCKET_SAFETY_LIMIT ) {
			// We don't have very many sockets registered, but
			// we seem to be running out of file descriptors.
			// Perhaps there is a file descriptor leak or
			// perhaps the safety limit is insanely low.
			// Either way, it is better to try and fail than
			// to risk getting into a stalemate.

			if (msg) {
					// If caller didn't ask for error messages, then don't
					// make noise in the log either, because caller is
					// just testing the waters.
				dprintf(D_NETWORK|D_FULLDEBUG,
						"Ignoring file descriptor safety limit (%d), because "
						"only %d sockets are registered (fd is %d)\n",
						file_descriptor_safety_limit,
						registered_socket_count,
						fd );
			}
			return false;
		}
		if(msg) {
			msg->formatstr( "file descriptor safety level exceeded: "
			              " limit %d, "
			              " registered socket count %d, "
			              " fd %d",
			              safety_limit, registered_socket_count, fd );
		}
		return true;
	}
	return false;
}

int	DaemonCore::Register_Socket(Stream* iosock, const char* iosock_descrip,
				SocketHandler handler, const char* handler_descrip,
				Service* s, DCpermission perm)
{
	return( Register_Socket(iosock, iosock_descrip, handler,
							(SocketHandlercpp)NULL, handler_descrip, s,
							perm, FALSE) );
}

int	DaemonCore::Register_Socket(Stream* iosock, const char* iosock_descrip,
				SocketHandlercpp handlercpp, const char* handler_descrip,
				Service* s, DCpermission perm)
{
	return( Register_Socket(iosock, iosock_descrip, NULL, handlercpp,
							handler_descrip, s, perm, TRUE) );
}

int	DaemonCore::Register_Pipe(int pipe_end, const char* pipe_descrip,
				PipeHandler handler, const char* handler_descrip,
				Service* s, HandlerType handler_type, DCpermission perm)
{
	return( Register_Pipe(pipe_end, pipe_descrip, handler,
							NULL, handler_descrip, s,
							handler_type, perm, FALSE) );
}

int	DaemonCore::Register_Pipe(int pipe_end, const char* pipe_descrip,
				PipeHandlercpp handlercpp, const char* handler_descrip,
				Service* s, HandlerType handler_type, DCpermission perm)
{
	return( Register_Pipe(pipe_end, pipe_descrip, NULL, handlercpp,
							handler_descrip, s, handler_type, perm, TRUE) );
}

int	DaemonCore::Register_Reaper(const char* reap_descrip, ReaperHandler handler,
				const char* handler_descrip, Service* s)
{
	return( Register_Reaper(-1, reap_descrip, handler,
							(ReaperHandlercpp)NULL, handler_descrip,
							s, FALSE) );
}

int	DaemonCore::Register_Reaper(const char* reap_descrip,
				ReaperHandlercpp handlercpp, const char* handler_descrip, Service* s)
{
	return( Register_Reaper(-1, reap_descrip, NULL, handlercpp,
							handler_descrip, s, TRUE) );
}

int	DaemonCore::Reset_Reaper(int rid, const char* reap_descrip,
				ReaperHandler handler, const char* handler_descrip, Service* s)
{
	return( Register_Reaper(rid, reap_descrip, handler,
							(ReaperHandlercpp)NULL, handler_descrip,
							s, FALSE) );
}

int	DaemonCore::Reset_Reaper(int rid, const char* reap_descrip,
				ReaperHandlercpp handlercpp, const char* handler_descrip, Service* s)
{
	return( Register_Reaper(rid, reap_descrip, NULL, handlercpp,
							handler_descrip, s, TRUE) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, TimerHandler handler,
				const char *event_descrip)
{
	return( t.NewTimer(deltawhen, handler, event_descrip, 0) );
}

#ifdef WIN32
int DaemonCore::Register_Timer_TS(unsigned deltawhen, TimerHandlercpp handler,
				const char *event_descrip, Service* s)
{
	EnterCriticalSection(&Big_fat_mutex);
	int status = Register_Timer(deltawhen, handler, event_descrip, s);
	Do_Wake_up_select();
	LeaveCriticalSection(&Big_fat_mutex);

	return status;
#else
int DaemonCore::Register_Timer_TS(unsigned /* deltawhen */,
				TimerHandlercpp /* handler */,
				const char * /* event_descrip */, Service * /* s */ )
{
	return 0;
#endif
}

int	DaemonCore::Register_Timer(unsigned deltawhen, TimerHandler handler,
							   Release release, const char *event_descrip)
{
	return( t.NewTimer(deltawhen, handler, release, event_descrip, 0) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, unsigned period,
				TimerHandler handler, const char *event_descrip)
{
	return( t.NewTimer(deltawhen, handler, event_descrip, period) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, TimerHandlercpp handlercpp,
				const char *event_descrip, Service* s)
{
	return( t.NewTimer(s, deltawhen, handlercpp, event_descrip, 0) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, unsigned period,
				TimerHandlercpp handler, const char *event_descrip, Service* s )
{
	return( t.NewTimer(s, deltawhen, handler, event_descrip, period) );
}

int DaemonCore::Register_Timer (const Timeslice &timeslice,TimerHandlercpp handler,const char * event_descrip,Service* s)
{
	return t.NewTimer(s, timeslice, handler, event_descrip );
}

int	DaemonCore::Cancel_Timer( int id )
{
	return( t.CancelTimer(id) );
}

int DaemonCore::Reset_Timer( int id, unsigned when, unsigned period )
{
	return( t.ResetTimer(id,when,period) );
}

int DaemonCore::Reset_Timer_Period ( int id, unsigned period )
{
	return( t.ResetTimerPeriod(id,period) );
}

int DaemonCore::ResetTimerTimeslice ( int id, Timeslice const &new_timeslice )
{
	return t.ResetTimerTimeslice(id,new_timeslice);
}

bool DaemonCore::GetTimerTimeslice( int id, Timeslice &timeslice )
{
	return t.GetTimerTimeslice( id, timeslice );
}

/************************************************************************/


int DaemonCore::Register_Command(int command, const char* command_descrip,
				CommandHandler handler, CommandHandlercpp handlercpp,
				const char *handler_descrip, Service* s, DCpermission perm,
				int dprintf_flag, int is_cpp, bool force_authentication,
				int wait_for_payload)
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
	comTable[i].force_authentication = force_authentication;
	comTable[i].service = s;
	comTable[i].data_ptr = NULL;
	comTable[i].dprintf_flag = dprintf_flag;
	comTable[i].wait_for_payload = wait_for_payload;
	free(comTable[i].command_descrip);
	if ( command_descrip )
		comTable[i].command_descrip = strdup(command_descrip);
	else
		comTable[i].command_descrip = strdup(EMPTY_DESCRIP);
	free(comTable[i].handler_descrip);
	if ( handler_descrip )
		comTable[i].handler_descrip = strdup(handler_descrip);
	else
		comTable[i].handler_descrip = strdup(EMPTY_DESCRIP);

	// Increment the counter of total number of entries
	nCommand++;

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &(comTable[i].data_ptr);

	// Conditionally dump what our table looks like
	DumpCommandTable(D_FULLDEBUG | D_DAEMONCORE);

	return(command);
}

int DaemonCore::Cancel_Command( int command )
{

	int i;
	for(i = 0; i<maxCommand; i++) {
		if( comTable[i].num == command )
		{
			comTable[i].num = 0;
			comTable[i].num = 0;
			comTable[i].handler = 0;
			comTable[i].handlercpp = 0;
			free(comTable[i].command_descrip);
			comTable[i].command_descrip = NULL;
			free(comTable[i].handler_descrip);
			comTable[i].handler_descrip = NULL;
			nCommand--;
			return TRUE;
		}
	}
	return FALSE;
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
char const * DaemonCore::InfoCommandSinfulString(int pid)
{
	// if pid is -1, we want info on our own process, else we want info
	// on a process created with Create_Process().
	if ( pid == -1 ) {
		return InfoCommandSinfulStringMyself(false);
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
		return pidinfo->sinful_string.Value();
	}
}


// NOTE: InfoCommandSinfulStringMyself always returns a pointer to a _static_ buffer!
// This means you'd better copy or strdup the result if you expect it to never
// change on you.  Plus, realize static buffers aren't exactly thread safe!
char const *
DaemonCore::InfoCommandSinfulStringMyself(bool usePrivateAddress)
{
	static char * sinful_public = NULL;
	static char * sinful_private = NULL;
	static bool initialized_sinful_private = false;

	if( m_shared_port_endpoint ) {
			// We do not advertise (or probably even have) our own network
			// port.  Instead, we advertise SharedPortServer's port along
			// with our local id so connections can be forwarded to us.
		char const *addr = m_shared_port_endpoint->GetMyRemoteAddress();
		if( !addr && usePrivateAddress ) {
				// If SharedPortServer is not running yet, and an address
				// that is local to this machine is good enough, then just
				// get enough information to connect directly without going
				// through SharedPortServer.
			addr = m_shared_port_endpoint->GetMyLocalAddress();
		}
		if( addr ) {
			return addr;
		}
	}

	if ( initial_command_sock == -1 ) {
		// there is no command sock!
		return NULL;
	}

		// If we haven't initialized our address(es), do so now.
	if (sinful_public == NULL || m_dirty_sinful) {
		free( sinful_public );
		sinful_public = NULL;

		char const *addr = ((Sock*)(*sockTable)[initial_command_sock].iosock)->get_sinful_public();
		if( !addr ) {
			EXCEPT("Failed to get public address of command socket!");
		}
		sinful_public = strdup( addr );
		m_dirty_sinful = true;
	}

	if (!initialized_sinful_private || m_dirty_sinful) {
		free( sinful_private);
		sinful_private = NULL;

		MyString private_sinful_string;
		char* tmp;
		if ((tmp = param("PRIVATE_NETWORK_INTERFACE"))) {
			int port = ((Sock*)(*sockTable)[initial_command_sock].iosock)->get_port();
			std::string private_ip;
			bool ok = network_interface_to_ip("PRIVATE_NETWORK_INTERFACE",tmp,private_ip);
			if( !ok ) {
				dprintf(D_ALWAYS,
						"Failed to determine my private IP address using PRIVATE_NETWORK_INTERFACE=%s\n",
						tmp);
			}
			else {
				private_sinful_string = generate_sinful(private_ip.c_str(), port);
				sinful_private = strdup(private_sinful_string.Value());
			}
			free(tmp);
		}

		free(m_private_network_name);
		m_private_network_name = NULL;
		if ((tmp = param("PRIVATE_NETWORK_NAME"))) {
			m_private_network_name = tmp;
		}

		initialized_sinful_private = true;
		m_dirty_sinful = true;
	}

	if( m_dirty_sinful ) { // need to rebuild full sinful string
		m_dirty_sinful = false;

		// The full sinful string is the public address plus params
		// which specify private network address and CCB contact info.

		m_sinful = Sinful(sinful_public);

			// Only publish the private name if there is a private or CCB
			// address, because otherwise, the private name doesn't matter.
		bool publish_private_name = false;

		char const *private_name = privateNetworkName();
		if( private_name ) {
			if( sinful_private && strcmp(sinful_public,sinful_private) ) {
				m_sinful.setPrivateAddr(sinful_private);
				publish_private_name = true;
			}
		}

			// if we don't hae a UDP port, advertise that fact
		char *forwarding = param("TCP_FORWARDING_HOST");
		if( forwarding ) {
			free( forwarding );
			m_sinful.setNoUDP(true);
		}
		if( !dc_ssock ) {
			m_sinful.setNoUDP(true);
		}

		if( m_ccb_listeners ) {
			MyString ccb_contact;
			m_ccb_listeners->GetCCBContactString(ccb_contact);
			if( !ccb_contact.IsEmpty() ) {
				m_sinful.setCCBContact(ccb_contact.Value());
				publish_private_name = true;
			}
		}

		if( private_name && publish_private_name ) {
			m_sinful.setPrivateNetworkName(private_name);
		}
	}

	if( usePrivateAddress ) {
		if( sinful_private ) {
			return sinful_private;
		}
		else {
			return sinful_public;
		}
	}

	return m_sinful.getSinful();
}

void
DaemonCore::daemonContactInfoChanged()
{
	m_dirty_sinful = true;

	drop_addr_file();
}

const char*
DaemonCore::publicNetworkIpAddr(void) {
	return (const char*) InfoCommandSinfulStringMyself(false);
}


const char*
DaemonCore::privateNetworkIpAddr(void) {
	return (const char*) InfoCommandSinfulStringMyself(true);
}


const char*
DaemonCore::privateNetworkName(void) {
	return (const char*)m_private_network_name;
}

// Lookup the environment id set for a particular pid, or if -1 then the
// getpid() in question.  Returns penvid or NULL of can't be found.
PidEnvID*
DaemonCore::InfoEnvironmentID(PidEnvID *penvid, int pid)
{
	if (penvid == NULL) {
		return NULL;
	}

	/* just in case... */
	pidenvid_init(penvid);

	/* handle the base case of my own pid */
	if ( pid == -1 ) {

		if (pidenvid_filter_and_insert(penvid, GetEnviron()) == 
			PIDENVID_OVERSIZED)
		{
			EXCEPT( "DaemonCore::InfoEnvironmentID: Programmer error. "
				"Tried to overstuff a PidEntryID array." );
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

int DaemonCore::Register_Signal(int sig, const char* sig_descrip, 
				SignalHandler handler, SignalHandlercpp handlercpp, 
				const char* handler_descrip, Service* s, 
				int is_cpp)
{
    int     i;		// hash value
    int     j;		// for linear probing


    if( handler == 0 && handlercpp == 0 ) {
		dprintf(D_DAEMONCORE, "Can't register NULL signal handler\n");
		return -1;
    }

    dc_stats.New("Signal", handler_descrip, AS_COUNT | IS_RCT | IF_NONZERO | IF_VERBOSEPUB);

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
	sigTable[i].service = s;
	sigTable[i].is_blocked = FALSE;
	sigTable[i].is_pending = FALSE;
	free(sigTable[i].sig_descrip);
	if ( sig_descrip )
		sigTable[i].sig_descrip = strdup(sig_descrip);
	else
		sigTable[i].sig_descrip = strdup(EMPTY_DESCRIP);
	free(sigTable[i].handler_descrip);
	if ( handler_descrip )
		sigTable[i].handler_descrip = strdup(handler_descrip);
	else
		sigTable[i].handler_descrip = strdup(EMPTY_DESCRIP);

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
	free( sigTable[found].handler_descrip );
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
	free( sigTable[found].sig_descrip );
	sigTable[found].sig_descrip = NULL;

	DumpSigTable(D_FULLDEBUG | D_DAEMONCORE);

	return TRUE;
}

int DaemonCore::Register_Socket(Stream *iosock, const char* iosock_descrip,
				SocketHandler handler, SocketHandlercpp handlercpp,
				const char *handler_descrip, Service* s, DCpermission perm,
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

	// Find empty slot, set to be i.
	for (i=0;i <= nSock; i++) {
		if ( (*sockTable)[i].iosock == NULL ) {
			break;
		}
		if ( (*sockTable)[i].remove_asap && (*sockTable)[i].servicing_tid==0 ) {
			(*sockTable)[i].iosock = NULL;
			break;
		}
	}

	// Make certain that entry i is empty.
	if ( (*sockTable)[i].iosock ) {
        dprintf ( D_ALWAYS, "Socket table fubar.  nSock = %d\n", nSock );
        DumpSocketTable( D_ALWAYS );
		EXCEPT("DaemonCore: Socket table messed up");
	}

    dc_stats.New("Socket", handler_descrip, AS_COUNT | IS_RCT | IF_NONZERO | IF_VERBOSEPUB);
    //if (iosock_descrip && iosock_descrip[0] && ! strcmp(handler_descrip, "DC Command Handler"))
    //   dc_stats.New("Command", iosock_descrip, AS_COUNT | IS_RCT | IF_NONZERO | IF_VERBOSEPUB);


	// Verify that this socket has not already been registered
	// Since we are scanning the entire table to do this (change this someday to a hash!),
	// at the same time update our nRegisteredSocks count by initializing it
	// to the number of slots (nSock) and then subtracting out the number of slots
	// not in use.
	nRegisteredSocks = nSock;
	int fd_to_register = ((Sock *)iosock)->get_file_desc();
	bool duplicate_found = false;
	for ( j=0; j < nSock; j++ )
	{		
		if ( (*sockTable)[j].iosock == iosock ) {
			duplicate_found = true;
        }

		// fd may be -1 if doing a "fake" registration: reverse_connect_pending
		// so do not require uniqueness of fd in that case
		if ( (*sockTable)[j].iosock && fd_to_register != -1 ) {
			if ( ((Sock *)(*sockTable)[j].iosock)->get_file_desc() ==
								fd_to_register ) {
				duplicate_found = true;
			}
		}

		// check if slot empty or available
		if ( ((*sockTable)[j].iosock == NULL) ||  // slot is empty
			 ((*sockTable)[j].remove_asap &&	   // slot available
			           (*sockTable)[j].servicing_tid==0 ) ) 
		{
			nRegisteredSocks--;		// decrement count of active sockets
		}
	}
	if (duplicate_found) {
		dprintf(D_ALWAYS, "DaemonCore: Attempt to register socket twice\n");
		return -2;
	} 

		// Check that we are within the file descriptor safety limit
		// We currently only do this for non-blocking connection attempts because
		// in most other places, the code does not check the return value
		// from Register_Socket().  Plus, it really does not make sense to 
		// enforce a limit for other cases --- if the socket already exists,
		// DaemonCore should be able to manage it for you.

	if( iosock->type() == Stream::reli_sock &&
	    ((ReliSock *)iosock)->is_connect_pending() )
	{
		MyString overload_msg;
		bool overload_danger =
			TooManyRegisteredSockets( ((Sock *)iosock)->get_file_desc(),
			                              &overload_msg);

		if( overload_danger )
		{
			dprintf(D_ALWAYS,
				"Aborting registration of socket %s %s: %s\n",
				iosock_descrip ? iosock_descrip : "",
				handler_descrip ? handler_descrip : ((Sock *)iosock)->get_sinful_peer(),
				overload_msg.Value() );
			return -3;
		}
	}

	// Found a blank entry at index i. Now add in the new data.
	(*sockTable)[i].servicing_tid = 0;
	(*sockTable)[i].remove_asap = false;
	(*sockTable)[i].call_handler = false;
	(*sockTable)[i].iosock = (Sock *)iosock;
	switch ( iosock->type() ) {
		case Stream::reli_sock :
			// the rest of daemon-core 
			(*sockTable)[i].is_connect_pending =
				((ReliSock *)iosock)->is_connect_pending() &&
				!((ReliSock *)iosock)->is_reverse_connect_pending();
			(*sockTable)[i].is_reverse_connect_pending =
				((ReliSock *)iosock)->is_reverse_connect_pending();
			break;
		case Stream::safe_sock :
				// SafeSock connect never blocks....
			(*sockTable)[i].is_connect_pending = false;
			(*sockTable)[i].is_reverse_connect_pending = false;
			break;
		default:
			EXCEPT("Adding CEDAR socket of unknown type");
			break;
	}
	(*sockTable)[i].handler = handler;
	(*sockTable)[i].handlercpp = handlercpp;
	(*sockTable)[i].is_cpp = is_cpp;
	(*sockTable)[i].perm = perm;
	(*sockTable)[i].service = s;
	(*sockTable)[i].data_ptr = NULL;
	free((*sockTable)[i].iosock_descrip);
	if ( iosock_descrip )
		(*sockTable)[i].iosock_descrip = strdup(iosock_descrip);
	else
		(*sockTable)[i].iosock_descrip = strdup(EMPTY_DESCRIP);
	free((*sockTable)[i].handler_descrip);
	if ( handler_descrip )
		(*sockTable)[i].handler_descrip = strdup(handler_descrip);
	else
		(*sockTable)[i].handler_descrip = strdup(EMPTY_DESCRIP);

	// Increment the counter of total number of entries if we
	// just filled our last slot.
	if ( i == nSock  ) {
		nSock++;
	}

	// If this is the first command sock, set initial_command_sock
	if ( initial_command_sock == -1 && handler == 0 && handlercpp == 0 && m_shared_port_endpoint == NULL )
		initial_command_sock = i;

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &((*sockTable)[i].data_ptr);

	// Conditionally dump what our table looks like
	DumpSocketTable(D_FULLDEBUG | D_DAEMONCORE);

	// If we are a worker thread, wake up select in the main thread
	// so the main thread re-computes the fd_sets.
	Wake_up_select();

	return i;
}

int DaemonCore::Cancel_Socket( Stream* insock)
{
	int i,j;

	if (!insock) {
		return FALSE;
	}

	i = -1;
	for (j=0;j<nSock;j++) {
		if ( (*sockTable)[j].iosock == insock ) {
			i = j;
			break;
		}
	}

	if ( i == -1 ) {
		dprintf( D_ALWAYS,"Cancel_Socket: called on non-registered socket!\n");
        if( insock ) {
            dprintf( D_ALWAYS,"Offending socket number %d to %s\n",
                     ((Sock *)insock)->get_file_desc(),
                     insock->peer_description());
        }
		DumpSocketTable( D_DAEMONCORE );
		return FALSE;
	}

	// Clear any data_ptr which go to this entry we just removed
	if ( curr_regdataptr == &( (*sockTable)[i].data_ptr) )
		curr_regdataptr = NULL;
	if ( curr_dataptr == &( (*sockTable)[i].data_ptr) )
		curr_dataptr = NULL;

	if ((*sockTable)[i].servicing_tid == 0 ||
		(*sockTable)[i].servicing_tid == CondorThreads::get_handle()->get_tid())
	{
		// Log a message
		dprintf(D_DAEMONCORE,"Cancel_Socket: cancelled socket %d <%s> %p\n",
				i,(*sockTable)[i].iosock_descrip, (*sockTable)[i].iosock );
		// Remove entry; mark it is available for next add via iosock=NULL
		(*sockTable)[i].iosock = NULL;
		free( (*sockTable)[i].iosock_descrip );
		(*sockTable)[i].iosock_descrip = NULL;
		free( (*sockTable)[i].handler_descrip );
		(*sockTable)[i].handler_descrip = NULL;
		// If we just removed the last entry in the table, we can decrement nSock
		if ( i == nSock - 1 ) {
			nSock--;            
		}
	} else {
		// Log a message
		dprintf(D_DAEMONCORE,"Cancel_Socket: deferred cancel socket %d <%s> %p\n",
				i,(*sockTable)[i].iosock_descrip, (*sockTable)[i].iosock );
		(*sockTable)[i].remove_asap = true;
	}

	nRegisteredSocks--;		// decrement count of active sockets
	
	DumpSocketTable(D_FULLDEBUG | D_DAEMONCORE);

	// If we are a worker thread, wake up select in the main thread
	// so the main thread re-computes the fd_sets.
	Wake_up_select();

	return TRUE;
}

// We no longer return "real" file descriptors from Create_Pipe. This
// is to force people to use Read_Pipe or Write_Pipe to do I/O on a pipe,
// which is necessary to encapsulate all the weird platform specifics
// (i.e. our wacky Windows pipe implementation). What we return from
// Create_Pipe is a pair of "pipe ends" that are actually just indices into
// the pipeHandleTable with the following offset added in. While the caller
// cannot do I/O directly on these handles, they *can* pass them unaltered
// to Create_Process (via the std[] parameter) and we'll do the right thing.
//    - Greg Quinn, 04/12/2006
static const int PIPE_INDEX_OFFSET = 0x10000;

int DaemonCore::pipeHandleTableInsert(PipeHandle entry)
{
	// try to find a free slot
	for (int i = 0; i <= maxPipeHandleIndex; i++) {
		if ((*pipeHandleTable)[i] == (PipeHandle)-1) {
			(*pipeHandleTable)[i] = entry;
			return i;
		}
	}

	// no vacant slots found, increment maxPipeHandleIndex and use it
	(*pipeHandleTable)[++maxPipeHandleIndex] = entry;
	return maxPipeHandleIndex;
}

void DaemonCore::pipeHandleTableRemove(int index)
{
	// invalidate this index
	(*pipeHandleTable)[index] = (PipeHandle)-1;

	// shrink down maxPipeHandleIndex, if necessary
	if (index == maxPipeHandleIndex) {
		maxPipeHandleIndex--;
	}
}

int DaemonCore::pipeHandleTableLookup(int index, PipeHandle* ph)
{
	if ((index < 0) || (index > maxPipeHandleIndex)) {
		return FALSE;
	}
	PipeHandle tmp_ph = (*pipeHandleTable)[index];
	if (tmp_ph == (PipeHandle)-1) {
		return FALSE;
	}
	if (ph != NULL) {
		*ph = tmp_ph;
	}
	return TRUE;
}

int DaemonCore::Create_Pipe( int *pipe_ends,
			     bool can_register_read,
			     bool can_register_write,
			     bool nonblocking_read,
			     bool nonblocking_write,
			     unsigned int psize)
{
	dprintf(D_DAEMONCORE,"Entering Create_Pipe()\n");
#ifdef WIN32
	static unsigned pipe_counter = 0;
	MyString pipe_name;
	pipe_name.formatstr("\\\\.\\pipe\\condor_pipe_%u_%u", GetCurrentProcessId(), pipe_counter++);
	return Create_Named_Pipe(pipe_ends,
		can_register_read,
		can_register_write,
		nonblocking_read,
		nonblocking_write,
		psize,
		pipe_name.Value());
#else // unix
	return Create_Named_Pipe(
		pipe_ends,
		can_register_read,
		can_register_write,
		nonblocking_read,
		nonblocking_write,
		psize,
		NULL);
#endif
}

// disable warning about memory leaks due to exception. all memory freed on exit anyway
MSC_DISABLE_WARNING(6211)

int DaemonCore::Create_Named_Pipe( int *pipe_ends,
			     bool can_register_read,
			     bool can_register_write,
			     bool nonblocking_read,
			     bool nonblocking_write,
			     unsigned int psize,
				 const char* pipe_name)
{
	dprintf(D_DAEMONCORE,"Entering Create_Named_Pipe()\n");

	PipeHandle read_handle, write_handle;

#ifdef WIN32
	DWORD overlapped_read_flag = 0, overlapped_write_flag = 0;
	if (can_register_read) {
		overlapped_read_flag = FILE_FLAG_OVERLAPPED;
	}
	if (can_register_write || nonblocking_write) {
		overlapped_write_flag = FILE_FLAG_OVERLAPPED;
	}

	HANDLE w =
		CreateNamedPipe(pipe_name,  // the name
				PIPE_ACCESS_OUTBOUND |      // "server" to "client" only
				overlapped_write_flag,      // overlapped mode
				0,                          // byte-mode, blocking
				PIPE_UNLIMITED_INSTANCES,                          // only one instance
				psize,                      // outgoing buffer size
				0,                          // incoming buffer size (not used)
				0,                          // default wait timeout (not used)
				NULL);                      // we mark handles inheritable in Create_Process
	if (w == INVALID_HANDLE_VALUE) {
		dprintf(D_ALWAYS, "CreateNamedPipe(%s) error: %d\n", 
			pipe_name, GetLastError());
		return FALSE;
	}
	HANDLE r =
		CreateFile(pipe_name,   // the named pipe
			   GENERIC_READ,            // desired access
			   0,                       // no sharing
			   NULL,                    // we mark handles inheritable in Create_Process
			   OPEN_EXISTING,           // existing named pipe
			   overlapped_read_flag,    // disable overlapped i/o on read end
			   NULL);                   // no template file
	if (r == INVALID_HANDLE_VALUE) {
		CloseHandle(w);
		dprintf(D_ALWAYS, "CreateFile(%s) error on named pipe: %d\n", 
			pipe_name, GetLastError());
		return FALSE;
	}
	read_handle = new ReadPipeEnd(r, overlapped_read_flag, nonblocking_read, psize);
	write_handle = new WritePipeEnd(w, overlapped_write_flag, nonblocking_write, psize);
#else
	// Unix
	if( pipe_name ) {
		EXCEPT("Create_NamedPipe() not implemented yet under unix!");
	}
		// what follows is the unix implementation of an unnamed pipe,
		// which is what we do when pipe_name == NULL

	// Shut the compiler up
	// These parameters are needed on Windows
	(void)can_register_read;
	(void)can_register_write;
	(void)psize;

	bool failed = false;
	int filedes[2];
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
	if ( failed == true ) {
		close(filedes[0]);
		filedes[0] = -1;
		close(filedes[1]);
		filedes[1] = -1;
		dprintf(D_ALWAYS,"Create_Pipe() failed to set non-blocking mode\n");
		return FALSE;
	}

	read_handle = filedes[0];
	write_handle = filedes[1];
#endif

	// add PipeHandles to pipeHandleTable
	pipe_ends[0] = pipeHandleTableInsert(read_handle) + PIPE_INDEX_OFFSET;
	pipe_ends[1] = pipeHandleTableInsert(write_handle) + PIPE_INDEX_OFFSET;

	dprintf(D_DAEMONCORE,"Create_Pipe() success read_handle=%d write_handle=%d\n",
	        pipe_ends[0],pipe_ends[1]);
	return TRUE;
}

int DaemonCore::Inherit_Pipe(int fd, bool is_write, bool can_register, bool nonblocking, int psize)
{
	PipeHandle pipe_handle;

#if defined(WIN32)
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	if (is_write) {
		pipe_handle = new WritePipeEnd(h, can_register, nonblocking, psize);
	}
	else {
		pipe_handle = new ReadPipeEnd(h, can_register, nonblocking, psize);
	}
#else
		// Shut the compiler up
		// These parameters are needed on Windows
	(void)is_write;
	(void)can_register;
	(void)nonblocking;
	(void)psize;

	pipe_handle = fd;
#endif

	return pipeHandleTableInsert(pipe_handle) + PIPE_INDEX_OFFSET;
}

int DaemonCore::Register_Pipe(int pipe_end, const char* pipe_descrip,
				PipeHandler handler, PipeHandlercpp handlercpp,
				const char *handler_descrip, Service* s,
				HandlerType handler_type, DCpermission perm,
				int is_cpp)
{
    int     i;
    int     j;

	int index = pipe_end - PIPE_INDEX_OFFSET;
	if (pipeHandleTableLookup(index) == FALSE) {
		dprintf(D_DAEMONCORE, "Register_Pipe: invalid index\n");
		return -1;
	}

	i = nPipe;

	// Make certain that entry i is empty.
	if ( (*pipeTable)[i].index != -1 ) {
        EXCEPT("Pipe table fubar!  nPipe = %d", nPipe );
	}

	// Verify that this piepfd has not already been registered
	for ( j=0; j < nPipe; j++ )
	{
		if ( (*pipeTable)[j].index == index ) {
			EXCEPT("DaemonCore: Same pipe registered twice");
        }
	}

    dc_stats.New("Pipe", handler_descrip, AS_COUNT | IS_RCT | IF_NONZERO | IF_VERBOSEPUB);

	// Found a blank entry at index i. Now add in the new data.
	(*pipeTable)[i].pentry = NULL;
	(*pipeTable)[i].call_handler = false;
	(*pipeTable)[i].in_handler = false;
	(*pipeTable)[i].index = index;
	(*pipeTable)[i].handler = handler;
	(*pipeTable)[i].handler_type = handler_type;
	(*pipeTable)[i].handlercpp = handlercpp;
	(*pipeTable)[i].is_cpp = is_cpp;
	(*pipeTable)[i].perm = perm;
	(*pipeTable)[i].service = s;
	(*pipeTable)[i].data_ptr = NULL;
	free((*pipeTable)[i].pipe_descrip);
	if ( pipe_descrip )
		(*pipeTable)[i].pipe_descrip = strdup(pipe_descrip);
	else
		(*pipeTable)[i].pipe_descrip = strdup(EMPTY_DESCRIP);
	free((*pipeTable)[i].handler_descrip);
	if ( handler_descrip )
		(*pipeTable)[i].handler_descrip = strdup(handler_descrip);
	else
		(*pipeTable)[i].handler_descrip = strdup(EMPTY_DESCRIP);

	// Increment the counter of total number of entries
	nPipe++;

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &((*pipeTable)[i].data_ptr);

#ifndef WIN32
	// On Unix, pipe fds are given to select.  So
	// if we are a worker thread, wake up select in the main thread
	// so the main thread re-computes the fd_sets.
	Wake_up_select();
#endif

#ifdef WIN32
	// On Win32, make a "pid entry" and pass it to our Pid Watcher thread.
	// This thread will then watch over the pipe handle and notify us
	// when there is something to read.
	// NOTE: WatchPid() must be called at the very end of this function.

	// tell our PipeEnd object that we're registered
	(*pipeHandleTable)[index]->set_registered();

	(*pipeTable)[i].pentry = new PidEntry;
	(*pipeTable)[i].pentry->hProcess = 0;
	(*pipeTable)[i].pentry->hThread = 0;
	(*pipeTable)[i].pentry->pipeReady = 0;
	(*pipeTable)[i].pentry->deallocate = 0;
	(*pipeTable)[i].pentry->pipeEnd = (*pipeHandleTable)[index];

	WatchPid((*pipeTable)[i].pentry);
#endif

	return pipe_end;
}


int DaemonCore::Cancel_Pipe( int pipe_end )
{
	int index = pipe_end - PIPE_INDEX_OFFSET;
	if (index < 0) {
		dprintf(D_ALWAYS, "Cancel_Pipe on invalid pipe end: %d\n", pipe_end);
		EXCEPT("Cancel_Pipe error");
	} 

	int i,j;

	i = -1;
	for (j=0;j<nPipe;j++) {
		if ( (*pipeTable)[j].index == index ) {
			i = j;
			break;
		}
	}

	if ( i == -1 ) {
		dprintf( D_ALWAYS,"Cancel_Pipe: called on non-registered pipe!\n");
		dprintf( D_ALWAYS,"Offending pipe end number %d\n", pipe_end );
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
			"Cancel_Pipe: cancelled pipe end %d <%s> (entry=%d)\n",
			pipe_end,(*pipeTable)[i].pipe_descrip, i );

	// Remove entry, move the last one in the list into this spot
	(*pipeTable)[i].index = -1;
	free( (*pipeTable)[i].pipe_descrip );
	(*pipeTable)[i].pipe_descrip = NULL;
	free( (*pipeTable)[i].handler_descrip );
	(*pipeTable)[i].handler_descrip = NULL;

#ifdef WIN32
	// we need to notify the PID-watcher thread that it should
	// no longer watch this pipe
	// note: we must acccess the deallocate flag in a thread-safe manner.
	ASSERT( (*pipeTable)[i].pentry );
	InterlockedExchange(&((*pipeTable)[i].pentry->deallocate),1L);
	if ((*pipeTable)[i].pentry->watcherEvent) {
		SetEvent((*pipeTable)[i].pentry->watcherEvent);
	}

	// call cancel on the PipeEnd, which won't return until the
	// PID-watcher is no longer using the object and it has been
	// marked as unregistered
	(*pipeTable)[i].pentry->pipeEnd->cancel();

	if ((*pipeTable)[i].in_handler) {
		// Cancel_Pipe is being called from the handler. when the
		// handler returns, the Driver needs to know whether to
		// call WatchPid on our PidEntry again. we set the pipeEnd
		// member of our PidEntry to NULL to tell it not to. the
		// Driver will deallocate the PidEntry then
		(*pipeTable)[i].pentry->pipeEnd = NULL;
	}
	else {
		// we're not in the handler so we can simply deallocate the
		// PidEntry now
		delete (*pipeTable)[i].pentry;
	}
#endif
	(*pipeTable)[i].pentry = NULL;
	if ( i < nPipe - 1 ) {
            // if not the last entry in the table, move the last one here
		(*pipeTable)[i] = (*pipeTable)[nPipe - 1];
		(*pipeTable)[nPipe - 1].index = -1;
		(*pipeTable)[nPipe - 1].pipe_descrip = NULL;
		(*pipeTable)[nPipe - 1].handler_descrip = NULL;
		(*pipeTable)[nPipe - 1].pentry = NULL;
	}
	nPipe--;

#ifndef WIN32
	// On Unix, pipe fds are passed into select.  So
	// if we are a worker thread, wake up select in the main thread
	// so the main thread re-computes the fd_sets.
	Wake_up_select();
#endif

	return TRUE;
}

#if defined(WIN32)
// If Close_Pipe is called on a Windows WritePipeEnd and there is
// an outstanding overlapped write operation, we can't immediately
// close the pipe. Instead, we call this function in a separate
// thread and close the pipe once the operation is complete
unsigned __stdcall pipe_close_thread(void *arg)
{
	WritePipeEnd* wpe = (WritePipeEnd*)arg;
	wpe->complete_async_write(false);

	dprintf(D_DAEMONCORE, "finally closing pipe %p\n", wpe);
	delete wpe;

	return 0;
}
#endif

int DaemonCore::Close_Pipe( int pipe_end )
{
	int index = pipe_end - PIPE_INDEX_OFFSET;
	if (pipeHandleTableLookup(index) == FALSE) {
		dprintf(D_ALWAYS, "Close_Pipe on invalid pipe end: %d\n", pipe_end);
		EXCEPT("Close_Pipe error");
	}

	// First, call Cancel_Pipe on this pipefd.
	int i,j;
	i = -1;
	for (j=0;j<nPipe;j++) {                                    
		if ( (*pipeTable)[j].index == index ) {
			i = j;
			break;
		}
	}
	if ( i != -1 ) {
		// We now know that this pipe end is registed.  Cancel it.
		int result = Cancel_Pipe(pipe_end);
		// ASSERT that it did not fail, because the only reason it should
		// fail is if it is not registered.  And we already checked that.
		ASSERT( result == TRUE );
	}

	// Now, close the pipe.
	int retval = TRUE;
#if defined(WIN32)
	WritePipeEnd* wpe = dynamic_cast<WritePipeEnd*>((*pipeHandleTable)[index]);
	if (wpe && wpe->needs_delayed_close()) {
		// We can't close this pipe yet, because it has an incomplete
		// overalapped write and we need to let it finish. Start a
		// thread to complete the operation then close the pipe
		CloseHandle((HANDLE)_beginthreadex(NULL, 0,
		            pipe_close_thread,
		            wpe, 0, NULL));
	}
	else {
		// no outstanding I/O - just delete the object (which
		// will close the pipe)
		delete (*pipeHandleTable)[index];
	}
#else
	int pipefd = (*pipeHandleTable)[index];
	if ( close(pipefd) < 0 ) {
		dprintf(D_ALWAYS,
			"Close_Pipe(pipefd=%d) failed, errno=%d\n",pipefd,errno);
		retval = FALSE;  // probably a bad fd
	}
#endif

	// remove from the pipe handle table
	pipeHandleTableRemove(index);

	if (retval == TRUE) {
		dprintf(D_DAEMONCORE,
				"Close_Pipe(pipe_end=%d) succeeded\n",pipe_end);
	}

	return retval;
}


int
DaemonCore::Cancel_And_Close_All_Pipes(void)
{
	// This method will cancel *and delete* all registered pipes.
	// It will return the number of pipes cancelled + closed.
	int i = 0;

	while ( nPipe > 0 ) {
		if ( (*pipeTable)[0].index != -1 ) {	// if a valid entry....
				// Note:  calling Close_Pipe will decrement
				// variable nPipe (number of registered Sockets)
				// by one.
			Close_Pipe( (*pipeTable)[0].index + PIPE_INDEX_OFFSET );
			i++;
		}
	}

	return i;
}

int
DaemonCore::Read_Pipe(int pipe_end, void* buffer, int len)
{
	if (len < 0) {
		dprintf(D_ALWAYS, "Read_Pipe: invalid len: %d\n", len);
		EXCEPT("Read_Pipe");
	}

	int index = pipe_end - PIPE_INDEX_OFFSET;
	if (pipeHandleTableLookup(index) == FALSE) {
		dprintf(D_ALWAYS, "Read_Pipe: invalid pipe_end: %d\n", pipe_end);
		EXCEPT("Read_Pipe");
	}

#if defined(WIN32)
	ReadPipeEnd* rpe = dynamic_cast<ReadPipeEnd*>((*pipeHandleTable)[index]);
	ASSERT(rpe != NULL);
	return rpe->read(buffer, len);
#else
	return read((*pipeHandleTable)[index], buffer, len);
#endif
}

int
DaemonCore::Write_Pipe(int pipe_end, const void* buffer, int len)
{
	if (len < 0) {
		dprintf(D_ALWAYS, "Write_Pipe: invalid len: %d\n", len);
		EXCEPT("Write_Pipe");
	}

	int index = pipe_end - PIPE_INDEX_OFFSET;
	if (pipeHandleTableLookup(index) == FALSE) {
		dprintf(D_ALWAYS, "Write_Pipe: invalid pipe_end: %d\n", pipe_end);
		EXCEPT("Write_Pipe: invalid pipe end");
	}

#if defined(WIN32)
	WritePipeEnd* wpe = dynamic_cast<WritePipeEnd*>((*pipeHandleTable)[index]);
	ASSERT(wpe != NULL);
	return wpe->write(buffer, len);
#else
	return write((*pipeHandleTable)[index], buffer, len);
#endif
}

#if !defined(WIN32)
int
DaemonCore::Get_Pipe_FD(int pipe_end, int* fd)
{
	int index = pipe_end - PIPE_INDEX_OFFSET;
	return pipeHandleTableLookup(index, fd);
}
#endif

int
DaemonCore::Close_FD(int fd)
{
	int retval = -1;  
	if ( fd >= PIPE_INDEX_OFFSET ) {  
		retval = ( daemonCore->Close_Pipe ( fd ) ? 0 : -1 );
	} else {
		retval = close ( fd );
	}
	return retval;
}

MyString*
DaemonCore::Read_Std_Pipe(int pid, int std_fd) {
	PidEntry *pidinfo = NULL;
	if ((pidTable->lookup(pid, pidinfo) < 0)) {
			// we have no information on this pid
			// TODO-pipe: distinguish this error somehow?
		return NULL;
	}
		// We just want to return a pointer to what we've got so
		// far. If there was no std pipe setup here, this will always
		// be NULL. However, if there was a pipe, but that's now been
		// closed, the std_pipes entry will already be cleared out, so
		// we can't rely on that.
	return pidinfo->pipe_buf[std_fd];
}


int
DaemonCore::Write_Stdin_Pipe(int pid, const void* buffer, int /* len */ ) {
	PidEntry *pidinfo = NULL;
	if ((pidTable->lookup(pid, pidinfo) < 0)) {
			// we have no information on this pid
			// TODO-pipe: set custom errno?
		return -1;
	}
	if (pidinfo->std_pipes[0] == DC_STD_FD_NOPIPE) {
			// No pipe found.
			// TODO-pipe: set custom errno?
		return -1;
	}
	pidinfo->pipe_buf[0] = new MyString;
	*pidinfo->pipe_buf[0] = (const char*)buffer;
	daemonCore->Register_Pipe(pidinfo->std_pipes[0], "DC stdin pipe", static_cast<PipeHandlercpp>(&DaemonCore::PidEntry::pipeFullWrite), "Guarantee all data written to pipe", pidinfo, HANDLE_WRITE);
	return 0;
}


bool
DaemonCore::Close_Stdin_Pipe(int pid) {
	PidEntry *pidinfo = NULL;
	int rval;

	if ((pidTable->lookup(pid, pidinfo) < 0)) {
			// we have no information on this pid
		return false;
	}
	if (pidinfo->std_pipes[0] == DC_STD_FD_NOPIPE) {
			// No pipe found.
		return false;
	}

	rval = Close_Pipe(pidinfo->std_pipes[0]);
	if (rval) {
		pidinfo->std_pipes[0] = DC_STD_FD_NOPIPE;
	}
	return (bool)rval;
}


int DaemonCore::Register_Reaper(int rid, const char* reap_descrip,
				ReaperHandler handler, ReaperHandlercpp handlercpp,
				const char *handler_descrip, Service* s, int is_cpp)
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
			dprintf(D_ALWAYS, 
				"Unable to register reaper with description: %s\n",
				reap_descrip==NULL?"[Not specified]":reap_descrip);
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
					dprintf(D_ALWAYS, 
						"Unable to register reaper with description: %s\n",
						reap_descrip==NULL?"[Not specified]":reap_descrip);
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
	free(reapTable[i].reap_descrip);
	if ( reap_descrip )
		reapTable[i].reap_descrip = strdup(reap_descrip);
	else
		reapTable[i].reap_descrip = strdup(EMPTY_DESCRIP);
	free(reapTable[i].handler_descrip);
	if ( handler_descrip )
		reapTable[i].handler_descrip = strdup(handler_descrip);
	else
		reapTable[i].handler_descrip = strdup(EMPTY_DESCRIP);

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

int DaemonCore::Cancel_Reaper( int rid )
{
	if( reapTable[rid].num == 0 ) {
		dprintf(D_ALWAYS,"Cancel_Reaper(%d) called on unregistered reaper.\n",rid);
		return FALSE;
	}

	reapTable[rid].num = 0;
	reapTable[rid].handler = NULL;
	reapTable[rid].handlercpp = NULL;
	reapTable[rid].service = NULL;
	reapTable[rid].data_ptr = NULL;

	PidEntry *pid_entry;
	pidTable->startIterations();
	while( pidTable->iterate(pid_entry) ) {
		if( pid_entry && pid_entry->reaper_id == rid ) {
			pid_entry->reaper_id = 0;
			dprintf(D_FULLDEBUG,"Cancel_Reaper(%d) found PID %d using the canceled reaper\n",
					rid, (int)pid_entry->pid);
		}
	}

	return TRUE;
}

// For debugging purposes
void DaemonCore::Dump(int flag, const char* indent)
{
	DumpCommandTable(flag, indent);
	DumpSigTable(flag, indent);
	DumpSocketTable(flag, indent);
	t.DumpTimerList(flag, indent);
}

void DaemonCore::DumpCommandTable(int flag, const char* indent)
{
	int			i;
	const char *descrip1;
	const char *descrip2;

	// we want to allow flag to be "D_FULLDEBUG | D_DAEMONCORE",
	// and only have output if _both_ are specified by the user
	// in the condor_config.  this is a little different than
	// what dprintf does by itself ( which is just
	// flag & DebugFlags > 0 ), so our own check here:
	if ( ! IsDebugCatAndVerbosity(flag) )
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

MyString DaemonCore::GetCommandsInAuthLevel(DCpermission perm,bool is_authenticated) {
	MyString res;
	int		i;
	DCpermissionHierarchy hierarchy( perm );
	DCpermission const *perms = hierarchy.getImpliedPerms();

		// iterate through a list of this perm and all perms implied by it
	for (perm = *(perms++); perm != LAST_PERM; perm = *(perms++)) {
		for (i = 0; i < maxCommand; i++) {
			if( (comTable[i].handler || comTable[i].handlercpp) &&
				(comTable[i].perm == perm) &&
				(!comTable[i].force_authentication || is_authenticated))
			{
				char const *comma = res.Length() ? "," : "";
				res.formatstr_cat( "%s%i", comma, comTable[i].num );
			}
		}
	}

	return res;
}

void DaemonCore::DumpReapTable(int flag, const char* indent)
{
	int			i;
	const char *descrip1;
	const char *descrip2;

	// we want to allow flag to be "D_FULLDEBUG | D_DAEMONCORE",
	// and only have output if _both_ are specified by the user
	// in the condor_config.  this is a little different than
	// what dprintf does by itself ( which is just
	// flag & DebugFlags > 0 ), so our own check here:
	if ( ! IsDebugCatAndVerbosity(flag) )
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
	int			i;
	const char *descrip1;
	const char *descrip2;

	// we want to allow flag to be "D_FULLDEBUG | D_DAEMONCORE",
	// and only have output if _both_ are specified by the user
	// in the condor_config.  this is a little different than
	// what dprintf does by itself ( which is just
	// flag & DebugFlags > 0 ), so our own check here:
	if ( ! IsDebugCatAndVerbosity(flag) )
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
	int			i;
	const char *descrip1;
	const char *descrip2;

	// we want to allow flag to be "D_FULLDEBUG | D_DAEMONCORE",
	// and only have output if _both_ are specified by the user
	// in the condor_config.  this is a little different than
	// what dprintf does by itself ( which is just
	// flag & DebugFlags > 0 ), so our own check here:
	if ( ! IsDebugCatAndVerbosity(flag) )
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

void
DaemonCore::refreshDNS() {
#if HAVE_RESOLV_H && HAVE_DECL_RES_INIT
		// re-initialize dns info (e.g. IP addresses of nameservers)
	res_init();
#endif

	getSecMan()->getIpVerify()->refreshDNS();
}

class DCThreadState : public Service {
 public:
	DCThreadState(int tid) 
		{m_tid=tid; m_dataptr=NULL; m_regdataptr=NULL;}
	int get_tid() { return m_tid; }
	void* *m_dataptr;
	void* *m_regdataptr;
 private:
	int m_tid;
};

void 
DaemonCore::thread_switch_callback(void* & incoming_contextVP)
{
	static int last_tid = 1;	// tid of 1 is the main thread
	DCThreadState *outgoing_context = NULL;
	DCThreadState *incoming_context = (DCThreadState *) incoming_contextVP;
	int current_tid = CondorThreads::get_tid();

		// Here we need to: (a) store state into the context of the
		// thread we are leaving, and (b) restore state from the
		// context of the thread we are starting.
	
	dprintf(D_THREADS,"DaemonCore context switch from tid %d to %d\n",
			last_tid, current_tid);

	if (!incoming_context) {
			// Must be a new thread; allocate a new context.
			// This context will be deleted by CondorThreads
			// when this thread is deallocated.
		incoming_context = new DCThreadState(current_tid);
		ASSERT(incoming_context);
		incoming_contextVP = (void *) incoming_context;
	}

		// We were passed the context of the thread being started;
		// so now lets fetch the context of the thread we were running
		// before.
		// Note in the tricky startup case where current_tid and
		// last_tid are both 1, incoming_context and outgoing_context
		// point to the same place, which is why we must first
		// allocate an incoming context above before panicing about
		// no outgoing context.  Whew.
	WorkerThreadPtr_t context = CondorThreads::get_handle(last_tid);
	if ( !context.is_null() ) {
		outgoing_context = (DCThreadState *) context->user_pointer_;
		if (!outgoing_context) {
				EXCEPT("ERROR: daemonCore - no thread context for tid %d\n",
						last_tid);
		}
	}

		// Stash our current state into the outgoing context.
	if ( outgoing_context ) {
		ASSERT(outgoing_context->get_tid() == last_tid);
		outgoing_context->m_dataptr = curr_dataptr;
		outgoing_context->m_regdataptr = curr_regdataptr;
	}

		// Restore our state from the incoming context.
	ASSERT(incoming_context->get_tid() == current_tid);
	curr_dataptr = incoming_context->m_dataptr;
	curr_regdataptr = incoming_context->m_regdataptr;

		// Record the current tid as the last tid.
	last_tid = current_tid;
}

void
DaemonCore::reconfig(void) {
	// NOTE: this function is always called on initial startup, as well
	// as at reconfig time.

	// NOTE: on reconfig, refreshDNS() will have already been called
	// by the time we get here, because it needs to be called early
	// in the process.

	// This is the compatibility layer on top of new ClassAds.
	// A few configuration parameters control its behavior.
	ClassAd::Reconfig();

    // publication and window size of daemon core stats are controlled by params
    dc_stats.Reconfig();

	m_dirty_sinful = true; // refresh our address in case config changes it

	SecMan *secman = getSecMan();
	secman->reconfig();

		// add a random offset to avoid pounding DNS
	int dns_interval = param_integer("DNS_CACHE_REFRESH",
									 8*60*60+(rand()%600), 0);
	if( dns_interval > 0 ) {
		if( m_refresh_dns_timer < 0 ) {
			m_refresh_dns_timer =
				Register_Timer( dns_interval, dns_interval,
								(TimerHandlercpp)&DaemonCore::refreshDNS,
								"DaemonCore::refreshDNS()", daemonCore );
		} else {
			Reset_Timer( m_refresh_dns_timer, dns_interval, dns_interval );
		}
	}
	else if( m_refresh_dns_timer != -1 ) {
		daemonCore->Cancel_Timer( m_refresh_dns_timer );
		m_refresh_dns_timer = -1;
	}

	// Maximum number of bytes read from a stdout/stderr pipes.
	// Default is 10k (10*1024 bytes)
	maxPipeBuffer = param_integer("PIPE_BUFFER_MAX", 10240);

    m_iMaxAcceptsPerCycle = param_integer("MAX_ACCEPTS_PER_CYCLE", 8);
    if( m_iMaxAcceptsPerCycle != 1 ) {
        dprintf(D_ALWAYS,"Setting maximum accepts per cycle %d.\n", m_iMaxAcceptsPerCycle);
    }

		// Initialize the collector list for ClassAd updates
	initCollectorList();

		// Initialize the StringLists that contain the attributes we
		// will allow people to set with condor_config_val from
		// various kinds of hosts (ADMINISTRATOR, CONFIG, WRITE, etc). 
	InitSettableAttrsLists();

#if HAVE_CLONE
	m_use_clone_to_create_processes = param_boolean("USE_CLONE_TO_CREATE_PROCESSES", true);
	if (RUNNING_ON_VALGRIND) {
		dprintf(D_ALWAYS, "Looks like we are under valgrind, forcing USE_CLONE_TO_CREATE_PROCESSES to FALSE.\n");
		m_use_clone_to_create_processes = false;
	}

		// If we are NOT the schedd, then do not use clone, as only
		// the schedd benefits from clone, and clone is more susceptable
		// to failures/bugs than fork.
	if (!get_mySubSystem()->isType(SUBSYSTEM_TYPE_SCHEDD)) {
		m_use_clone_to_create_processes = false;
	}
#endif /* HAVE CLONE */

	m_invalidate_sessions_via_tcp = param_boolean("SEC_INVALIDATE_SESSIONS_VIA_TCP", true);

#ifdef HAVE_EXT_GSOAP
	if( param_boolean("ENABLE_SOAP",false) ||
		param_boolean("ENABLE_WEB_SERVER",false) )
	{
		// tstclair: reconfigure the soap object
		if( soap ) {
			dc_soap_free(soap);
			soap = NULL;
		}

		dc_soap_init(soap);
		
	}
	else {
		// Do not have to deallocate soap if it was enabled and has
		// now been disabled.  Access to it will be disallowed, even
		// though the structure is still allocated.
	}
#endif
#ifdef HAVE_EXT_GSOAP
#ifdef HAVE_EXT_OPENSSL
	MyString subsys = MyString(get_mySubSystem()->getName());
	bool enable_soap_ssl = param_boolean("ENABLE_SOAP_SSL", false);

	if (enable_soap_ssl) {
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
#endif // HAVE_EXT_OPENSSL
#endif // HAVE_EXT_GSOAP


		// FAKE_CREATE_THREAD is an undocumented config knob which turns
		// Create_Thread() into a simple function call in the main process,
		// rather than a thread/fork.
#ifdef WIN32
		// Currently, all use of threads are deemed unsafe in Windows.
	m_fake_create_thread = param_boolean("FAKE_CREATE_THREAD",true);
#else
		// Under unix, Create_Thread() is actually a fork, so it is safe.
	m_fake_create_thread = param_boolean("FAKE_CREATE_THREAD",false);
#endif

	// Setup a timer to send child keepalives to our parent, if we have
	// a daemon core parent.
	if ( ppid && m_want_send_child_alive ) {
		MyString buf;
		buf.formatstr("%s_NOT_RESPONDING_TIMEOUT",get_mySubSystem()->getName());
		max_hang_time = param_integer(buf.Value(),-1);
		if( max_hang_time == (unsigned int)-1 ) {
			max_hang_time = param_integer("NOT_RESPONDING_TIMEOUT",0);
		}
		if ( !max_hang_time ) {
			max_hang_time = 60 * 60;	// default to 1 hour
		}
		m_child_alive_period = (max_hang_time / 3) - 30;
		if ( m_child_alive_period < 1 )
			m_child_alive_period = 1;
		if ( send_child_alive_timer == -1 ) {

				// 2008-06-18 7.0.3: commented out direct call to
				// SendAliveToParent(), because it causes deadlock
				// between the shadow and schedd if the job classad
				// that the schedd is writing over a pipe to the
				// shadow is larger than the pipe buffer size.
				// For now, register timer for 0 seconds instead
				// of calling SendAliveToParent() immediately.
				// This means we are vulnerable to a race condition,
				// in which we hang before the first CHILDALIVE.  If
				// that happens, our parent will never kill us.

			send_child_alive_timer = Register_Timer(0,
					(unsigned)m_child_alive_period,
					(TimerHandlercpp)&DaemonCore::SendAliveToParent,
					"DaemonCore::SendAliveToParent", this );

				// Send this immediately, because if we hang before
				// sending this message, our parent will not kill us.
				// (Commented out.  See reason above.)
				// SendAliveToParent();
		} else {
			Reset_Timer(send_child_alive_timer, 1, m_child_alive_period);
		}
	}

	file_descriptor_safety_limit = 0; // 0 indicates: needs to be computed

	InitSharedPort();

	bool never_use_ccb =
		get_mySubSystem()->isType(SUBSYSTEM_TYPE_GAHP) ||
		get_mySubSystem()->isType(SUBSYSTEM_TYPE_DAGMAN);

	if( !never_use_ccb ) {
		if( !m_ccb_listeners ) {
			m_ccb_listeners = new CCBListeners;
		}

		char *ccb_addresses = param("CCB_ADDRESS");
		if( m_shared_port_endpoint ) {
				// if we are using a shared port, then we don't need our
				// own ccb listener; SharedPortServer will have its own
			free( ccb_addresses );
			ccb_addresses = NULL;
		}
		m_ccb_listeners->Configure( ccb_addresses );
		free( ccb_addresses );

		const bool blocking = true;
		m_ccb_listeners->RegisterWithCCBServer(blocking);
	}

	// Cons up a thread pool.
	CondorThreads::pool_init();
	// Supply routines to call when code calls start_thread_safe() and
	// stop_thread_safe().
	_mark_thread_safe_callback(CondorThreads::start_thread_safe_block,
							   CondorThreads::stop_thread_safe_block);
	// Supply a callback to daemonCore upon thread context switch.
	CondorThreads::set_switch_callback( thread_switch_callback );

		// in case our address changed, do whatever needs to be done
	daemonContactInfoChanged();
}

void
DaemonCore::InitSharedPort(bool in_init_dc_command_socket)
{
	MyString why_not;
	bool already_open = m_shared_port_endpoint != NULL;

	if( SharedPortEndpoint::UseSharedPort(&why_not,already_open) ) {
		if( !m_shared_port_endpoint ) {
			char const *sock_name = m_daemon_sock_name.Value();
			if( !*sock_name ) sock_name = NULL;
			m_shared_port_endpoint = new SharedPortEndpoint(sock_name);
		}
		m_shared_port_endpoint->InitAndReconfig();
		if( !m_shared_port_endpoint->StartListener() ) {
			EXCEPT("Failed to start local listener (USE_SHARED_PORT=true)");
		}
	}
	else if( m_shared_port_endpoint ) {
		dprintf(D_ALWAYS,
				"Turning off shared port endpoint because %s\n",why_not.Value());
		delete m_shared_port_endpoint;
		m_shared_port_endpoint = NULL;

			// if we have no non-shared port open, we better open one now
			// or we will have cut ourselves off from the world
		if( !in_init_dc_command_socket ) {
			InitDCCommandSocket(1);
		}
	}
	else if( IsFulldebug(D_FULLDEBUG) ) {
		dprintf(D_FULLDEBUG,"Not using shared port because %s\n",why_not.Value());
	}
}

void
DaemonCore::ReloadSharedPortServerAddr()
{
	if( m_shared_port_endpoint ) {
		m_shared_port_endpoint->ReloadSharedPortServerAddr();
	}
}

int
DaemonCore::Verify(char const *command_descrip,DCpermission perm, const condor_sockaddr& addr, const char * fqu )
{
	MyString deny_reason; // always get 'deny' reason, if there is one
	MyString *allow_reason = NULL;
	MyString allow_reason_buf;
	if( IsDebugLevel( D_SECURITY ) ) {
			// only get 'allow' reason if doing verbose debugging
		allow_reason = &allow_reason_buf;
	}

	int result = getSecMan()->Verify(perm, addr, fqu, allow_reason, &deny_reason);

	MyString *reason = result ? allow_reason : &deny_reason;
	char const *result_desc = result ? "GRANTED" : "DENIED";

	if( reason ) {
		char ipstr[IP_STRING_BUF_SIZE];
		strcpy(ipstr, "(unknown)");
		addr.to_ip_string(ipstr, sizeof(ipstr));
	//sin_to_ipstring(sin,ipstr,IP_STRING_BUF_SIZE);

			// Note that although this says D_ALWAYS, when the result is
			// ALLOW, we only get here if D_SECURITY is on.
		dprintf( D_ALWAYS,
				 "PERMISSION %s to %s from host %s for %s, "
				 "access level %s: reason: %s\n",
				 result_desc,
				 (fqu && *fqu) ? fqu : "unauthenticated user",
				 ipstr,
				 command_descrip ? command_descrip : "unspecified operation",
				 PermString(perm),
				 reason->Value() );
	}

	return result;
}

bool
DaemonCore::Wake_up_select()
{
	// There is no need to wake up select if it's the main thread calling (tid==1)
	// because the main thread cannot be inside select if it's here.  There is also
	// no need if the caller is not a condor thread (get_tid() returns 0), or 
	// if the condor_threads class has never been initialized (git_tid() returns -1),
	// because in both cases, the caller is either the main thread, or some wild 
	// thread that isn't allowed to make DaemonCore calls.
	if (CondorThreads::get_tid() <= 1) {
#ifdef WIN32
		if (GetCurrentThreadId() != dcmainThreadId) {
			dprintf (D_ALWAYS, "DaemonCore::Wake_up_select called from an unknown thread. windows tid = %d", 
				GetCurrentThreadId());
		}
#endif

		return false;
	}

	return Do_Wake_up_select();
}

bool
DaemonCore::Do_Wake_up_select()
{
	// note, this code is called by threads other than the main thread,
	// and (on windows) threads other than condor_threads.  it should be
	// thread safe and it should not depend on the caller being a condor_thread.
	// it should never be called by the master thread.
	//
	bool fSuccess = true;  // return success if the pipe is not empty
	if ( ! async_pipe_signal) {
		// set the async_pipe_signal flag before we write into the pipe
		// to avoid a potential race condition. better to have the flag 
		// say the pipe is signalled and have it not be than the reverse.
		async_pipe_signal = true;
#ifdef WIN32
		if (GetCurrentThreadId() == dcmainThreadId) {
			dprintf (D_ALWAYS, "DaemonCore::Do_Wake_up_select called from main thread. this should never happen.");
			return false;
		}
		fSuccess = send(async_pipe[1].get_socket(), "!", 1, 0) > 0;
#else
		fSuccess = write(async_pipe[1],"!",1) > 0;
#endif
	}
	return fSuccess;
}

// This function never returns. It is responsible for monitor signals and
// incoming messages or requests and invoke corresponding handlers.
void DaemonCore::Driver()
{
	Selector	selector;
	int			i;
	int			tmpErrno;
	time_t		timeout;
	time_t min_deadline;

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
		sigdelset(&fullset, SIGPROF);    // so gprof works

	sigemptyset( &emptyset );
	char asyncpipe_buf[10];
#endif

	if ( param_boolean( "ENABLE_STDOUT_TESTING", false ) )
	{
		dprintf( D_ALWAYS, "Testing stdout & stderr\n" );
		{
			char	buf[1024];
			memset(buf, 0, sizeof(buf) );
			bool	do_out = true, do_err = true;
			bool	do_fd1 = true, do_fd2 = true;
			for ( i=0;  i<16*1024;  i++ )
			{
				if ( do_out && fwrite( buf, sizeof(buf), 1, stdout ) != 1 )
				{
					dprintf( D_ALWAYS, "Failed to write to stdout: %s\n",
							 strerror( errno ) );
					do_out = false;
				}
				if ( do_err && fwrite( buf, sizeof(buf), 1, stderr ) != 1 )
				{
					dprintf( D_ALWAYS, "Failed to write to stderr: %s\n",
							 strerror( errno ) );
					do_err = false;
				}
				if ( do_fd1 && write( 1, buf, sizeof(buf) ) != sizeof(buf) )
				{
					dprintf( D_ALWAYS, "Failed to write to fd 1: %s\n",
							 strerror( errno ) );
					do_fd1 = false;
				}
				if ( do_fd2 && write( 2, buf, sizeof(buf) ) != sizeof(buf) )
				{
					dprintf( D_ALWAYS, "Failed to write to fd 2: %s\n",
							 strerror( errno ) );
					do_fd2 = false;
				}
			}
		}
		dprintf( D_ALWAYS, "Done with stdout & stderr tests\n" );
	}

	double runtime = UtcTime::getTimeDouble();
	double group_runtime = runtime;
    double pump_cycle_begin_time = runtime;

	for(;;)
	{

		// call signal handlers for any pending signals
		sent_signal = FALSE;	// set to True inside Send_Signal()
			for (i=0;i<maxSig;i++) {
				if ( sigTable[i].handler || sigTable[i].handlercpp ) {
					// found a valid entry; test if we should call handler
					if ( sigTable[i].is_pending && !sigTable[i].is_blocked ) {
						// call handler, but first clear pending flag
						sigTable[i].is_pending = 0;
						// Update curr_dataptr for GetDataPtr()
						curr_dataptr = &(sigTable[i].data_ptr);
                        // update statistics
                        dc_stats.Signals += 1;

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

                        // update per-timer runtime and count statistics
                        runtime = dc_stats.AddRuntime(sigTable[i].handler_descrip, runtime);
					}
				}
			}

#ifndef WIN32
		// clear the async_pipe_signal flag before we empty to the pipe
		// that way we won't miss it if someone writes into the pipe.
		async_pipe_signal = false;
		// Drain our async_pipe; we must do this before we unblock unix signals.
		// Just keep reading while something is there.  async_pipe is set to
		// non-blocking mode via fcntl, so the read below will not block.
		while( read(async_pipe[0],asyncpipe_buf,8) > 0 ) { }
#else
		// windows version of this code is after selector.execute()
#endif

        // accumulate signal runtime (including timers) as SignalRuntime
        runtime = UtcTime::getTimeDouble();
        dc_stats.SignalRuntime += (runtime - group_runtime);
        group_runtime = runtime;

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

        int num_timers_fired = 0;
		timeout = t.Timeout(&num_timers_fired, &runtime);

        dc_stats.TimersFired += num_timers_fired;

		if ( sent_signal == TRUE ) {
			timeout = 0;
		}
		if ( timeout < 0 ) {
			timeout = TIME_T_NEVER;
		}

        // accumulate signal runtime (including timers) as SignalRuntime
        //runtime = UtcTime::getTimeDouble();
        dc_stats.TimerRuntime += (runtime - group_runtime);
        group_runtime = runtime;

		// Setup what socket descriptors to select on.  We recompute this
		// every time because 1) some timeout handler may have removed/added
		// sockets, and 2) it ain't that expensive....
		selector.reset();
		min_deadline = 0;
		for (i = 0; i < nSock; i++) {
				// if a valid entry not already being serviced, add to select
			if ( (*sockTable)[i].iosock && 
				 (*sockTable)[i].servicing_tid==0 &&
				 (*sockTable)[i].remove_asap == false ) {	
					// Setup our fdsets
				if ( (*sockTable)[i].is_reverse_connect_pending ) {
					// nothing to do; we are just allowing this socket
					// to be registered so that it behaves like a socket
					// that is doing a non-blocking connect
					// CCBClient will eventually ensure that the
					// socket's registered callback function is called
					// We want to ignore the socket's deadline (below)
					// because that is all taken care of by CCBClient.
					continue;
				}
				else if ( (*sockTable)[i].is_connect_pending ) {
						// we want to be woken when a non-blocking
						// connect is ready to write.  when connect
						// is ready, select will set the writefd set
						// on success, or the exceptfd set on failure.
					selector.add_fd( (*sockTable)[i].iosock->get_file_desc(), Selector::IO_WRITE );
					selector.add_fd( (*sockTable)[i].iosock->get_file_desc(), Selector::IO_EXCEPT );
				} else {
						// we want to be woken when there is something
						// to read.
					selector.add_fd( (*sockTable)[i].iosock->get_file_desc(), Selector::IO_READ );
				}

					// If this socket times out sooner than
					// our select timeout, adjust the select timeout.
				time_t deadline = (*sockTable)[i].iosock->get_deadline();
				if(deadline) { // If non-zero, there is a timeout.
					if(min_deadline == 0 || min_deadline > deadline) {
						min_deadline = deadline;
					}
				}
            }
		}

		if( min_deadline ) {
			int deadline_timeout = min_deadline - time(NULL) + 1;
			if(deadline_timeout < timeout) {
				if(deadline_timeout < 0) deadline_timeout = 0;
				timeout = deadline_timeout;
			}
		}

#if !defined(WIN32)
		// Add the registered pipe fds into the list of descriptors to
		// select on.
		for (i = 0; i < nPipe; i++) {
			if ( (*pipeTable)[i].index != -1 ) {	// if a valid entry....
				int pipefd = (*pipeHandleTable)[(*pipeTable)[i].index];
				switch( (*pipeTable)[i].handler_type ) {
				case HANDLE_READ:
					selector.add_fd( pipefd, Selector::IO_READ );
					break;
				case HANDLE_WRITE:
					selector.add_fd( pipefd, Selector::IO_WRITE );
					break;
				case HANDLE_READ_WRITE:
					selector.add_fd( pipefd, Selector::IO_READ );
					selector.add_fd( pipefd, Selector::IO_WRITE );
					break;
				}
			}
        }
#endif


		// Add the read side of async_pipe to the list of file descriptors to
		// select on.  We write to async_pipe if a unix async signal
		// is delivered after we unblock signals and before we block on select.
#ifdef WIN32
		if ( ! async_pipe[0].is_connected()) {
			EXCEPT("DaemonCore:: async_pipe has been unexpectedly closed!");
		} 
		selector.add_fd( async_pipe[0].get_file_desc() , Selector::IO_READ );
#else
		selector.add_fd( async_pipe[0], Selector::IO_READ );
#endif

		// Let other threads run while we are waiting on select
		CondorThreads::enable_parallel(true);

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
		//Win32 - release coarse-grained mutex
		LeaveCriticalSection(&Big_fat_mutex);
#endif

		selector.set_timeout( timeout );

		errno = 0;
		time_t time_before = time(NULL);
		time_t okay_delta = timeout;

			// Performance around select is of high importance for all
			// daemons that are single threaded (all of them). If you
			// have questions ask matt.
		if (IsDebugLevel(D_PERF_TRACE)) {
			dprintf(D_ALWAYS, "PERF: entering select\n");
		}

		selector.execute();

        // update statistics on time spent waiting in select.
        runtime = UtcTime::getTimeDouble();
        dc_stats.SelectWaittime += (runtime - group_runtime);
        //dc_stats.StatsLifetime = now - dc_stats.InitTime;

		tmpErrno = errno;

		CheckForTimeSkip(time_before, okay_delta);

#ifndef WIN32
		// Unix

		// Block all signals until next select so that we don't
		// get confused.
		sigprocmask( SIG_SETMASK, &fullset, NULL );

		// We _must_ set async_sigs_unblocked flag to TRUE
		// before we unblock async signals, and
		// set it to FALSE after we block the signals again.
		async_sigs_unblocked = FALSE;

		if ( selector.failed() ) {
			// not just interrupted by a signal...
				dprintf(D_ALWAYS,"Socket Table:\n");
        		DumpSocketTable( D_ALWAYS );
				dprintf(D_ALWAYS,"State of selector:\n");
				selector.display();
				EXCEPT("DaemonCore: select() returned an unexpected error: %d (%s)",tmpErrno,strerror(tmpErrno));
		}
#else
		// Windoze
		EnterCriticalSection(&Big_fat_mutex);
		if ( selector.select_retval() == SOCKET_ERROR ) {
			EXCEPT("select, error # = %d",WSAGetLastError());
		}

		// Drain our async_pipe (which is really a socket)
		// extra error checking because we had problems with the pipe getting stuck
		// in the signalled state in 7.5.5. 
		if (selector.has_ready() &&
			selector.fd_ready(async_pipe[0].get_file_desc(), Selector::IO_READ)) {
            dc_stats.AsyncPipe += 1;
			if ( ! async_pipe_signal) {
				dprintf(D_ALWAYS, "DaemonCore: async_pipe is signalled, but async_pipe_signal is false.\n");
			}
			async_pipe_signal = false;
			while (int cb = async_pipe[0].bytes_available_to_read()) {
				if (cb < 0) {
					dprintf(D_ALWAYS, "DaemonCore: async_pipe[0].bytes_available_to_read returned WSA Error %d\n", 
							WSAGetLastError());
					break;
				}
				char buf[16];
				if (recv(async_pipe[0].get_socket(), buf, MIN(cb, COUNTOF(buf)), 0) == SOCKET_ERROR) {
					dprintf(D_ALWAYS, "DaemonCore: recv on async_pipe[0] returned WSA Error %d\n", 
							WSAGetLastError());
					break;
				}
			}
		}
#endif

			// Performance around select is of high importance for all
			// daemons that are single threaded (all of them). If you
			// have questions ask matt.
		if (IsDebugLevel(D_PERF_TRACE)) {
			dprintf(D_ALWAYS, "PERF: leaving select\n");
			selector.display();
		}

		// For now, do not let other threads run while we are processing
		// in the main loop.
		CondorThreads::enable_parallel(false);

        runtime = group_runtime = UtcTime::getTimeDouble();

		if ( selector.has_ready() ||
			 ( selector.timed_out() && 
			   min_deadline && min_deadline < time(NULL) ) )
		{
			// Either socket activity has happened or a socket
			// operation has timed out.

			// To avoid repeated calls to time(), store it once
			// for the following loop; all uses of it below should
			// be ok if it drifts a little into the past.
			time_t now = time(NULL);

			bool recheck_status = false;
			//bool call_soap_handler = false;

			// scan through the socket table to find which ones select() set
			for(i = 0; i < nSock; i++) {
				if ( (*sockTable)[i].iosock && 
					 (*sockTable)[i].servicing_tid==0 &&
					 (*sockTable)[i].remove_asap == false ) 
				{	// if a valid entry...
					// figure out if we should call a handler.  to do this,
					// if the socket was doing a connect(), we check the
					// writefds and excepfds.  otherwise, check readfds.
					(*sockTable)[i].call_handler = false;
					time_t deadline = (*sockTable)[i].iosock->get_deadline();
					bool sock_timed_out = ( deadline && deadline < now );

					if ( (*sockTable)[i].is_reverse_connect_pending ) {
						// nothing to do
					}
					else if ( (*sockTable)[i].is_connect_pending ) {

						if ( selector.fd_ready( (*sockTable)[i].iosock->get_file_desc(),
												Selector::IO_WRITE ) ||
							 selector.fd_ready( (*sockTable)[i].iosock->get_file_desc(),
												Selector::IO_EXCEPT ) ||
							 sock_timed_out )
						{
							// A connection pending socket has been
							// set or the connection attempt has timed out.
							// Only call handler if CEDAR confirms the
							// connect algorithm has completed.

							if ( ((Sock *)(*sockTable)[i].iosock)->
							      do_connect_finish() != CEDAR_EWOULDBLOCK)
							{
								(*sockTable)[i].call_handler = true;
							}
						}
					} else {
						if ( selector.fd_ready( (*sockTable)[i].iosock->get_file_desc(),
												Selector::IO_READ ) ||
							 sock_timed_out )
						{
							(*sockTable)[i].call_handler = true;
						}
					}
				}	// end of if valid sock entry
			}	// end of for loop through all sock entries

            runtime = UtcTime::getTimeDouble();
            dc_stats.SocketRuntime += (runtime - group_runtime);
            group_runtime = runtime;

			// scan through the pipe table to find which ones select() set
			for(i = 0; i < nPipe; i++) {
				if ( (*pipeTable)[i].index != -1 ) {	// if a valid entry...
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
					int pipefd = (*pipeHandleTable)[(*pipeTable)[i].index];
					if ( selector.fd_ready( pipefd, Selector::IO_READ ) )
					{
						(*pipeTable)[i].call_handler = true;
					}
					if ( selector.fd_ready( pipefd, Selector::IO_WRITE ) )
					{
						(*pipeTable)[i].call_handler = true;
					}
#endif
				}	// end of if valid pipe entry
			}	// end of for loop through all pipe entries


			// Now loop through all pipe entries, calling handlers if required.
            runtime = UtcTime::getTimeDouble();
			for(i = 0; i < nPipe; i++) {
				if ( (*pipeTable)[i].index != -1 ) {	// if a valid entry...

					if ( (*pipeTable)[i].call_handler ) {

						(*pipeTable)[i].call_handler = false;

                        dc_stats.PipeMessages += 1;

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
							if (!saved_pentry->pipeEnd->io_ready()) {
								// hand this pipe end back to the PID-watcher thread
								WatchPid(saved_pentry);
								continue;
							}

#else
							// UNIX
							int pipefd = (*pipeHandleTable)[(*pipeTable)[i].index];
							selector.reset();
							selector.set_timeout( 0 );
							selector.add_fd( pipefd, Selector::IO_READ );
							selector.execute();
							if ( selector.timed_out() ) {
								// nothing available, try the next entry...
								continue;
							}
#endif
						}	// end of if ( recheck_status || saved_pentry )

						(*pipeTable)[i].in_handler = true;

						// log a message
						int pipe_end = (*pipeTable)[i].index + PIPE_INDEX_OFFSET;
						dprintf(D_COMMAND,"Calling pipe Handler <%s> for Pipe end=%d <%s>\n",
									(*pipeTable)[i].handler_descrip,
									pipe_end,
									(*pipeTable)[i].pipe_descrip);

						// Update curr_dataptr for GetDataPtr()
						curr_dataptr = &( (*pipeTable)[i].data_ptr);
						recheck_status = true;
						if ( (*pipeTable)[i].handler )
							// a C handler
							(*( (*pipeTable)[i].handler))( (*pipeTable)[i].service, pipe_end);
						else
						if ( (*pipeTable)[i].handlercpp )
							// a C++ handler
							((*pipeTable)[i].service->*( (*pipeTable)[i].handlercpp))(pipe_end);
						else
						{
							// no handler registered
							EXCEPT("No pipe handler callback");
						}

						dprintf(D_COMMAND,"Return from pipe Handler\n");

						(*pipeTable)[i].in_handler = false;

						// Make sure we didn't leak our priv state
						CheckPrivState();

						// Clear curr_dataptr
						curr_dataptr = NULL;

#ifdef WIN32
						// Ask a pid watcher thread to watch over this pipe
						// handle.  Note that if Cancel_Pipe() was called by the
						// handler above, pipeEnd will be NULL, so we stop
						// watching
						MSC_SUPPRESS_WARNING(6011) // can't sure sure that save_pentry is not NULL
						if ( saved_pentry->pipeEnd ) {
							WatchPid(saved_pentry);
						}
#endif

                        // update per-handler runtime statistics
                        runtime = dc_stats.AddRuntime((*pipeTable)[i].handler_descrip, runtime);

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


            runtime = UtcTime::getTimeDouble();
            dc_stats.PipeRuntime += (runtime - group_runtime);
            group_runtime = runtime;

			// Now loop through all sock entries, calling handlers if required.
			for(i = 0; i < nSock; i++) {
				if ( (*sockTable)[i].iosock ) {	// if a valid entry...

					if ( (*sockTable)[i].call_handler ) {

						(*sockTable)[i].call_handler = false;

                        dc_stats.SockMessages += 1;

						if ( recheck_status &&
							 ((*sockTable)[i].is_connect_pending == false) )
						{
							// we have already called at least one callback handler.  what
							// if this handler drained this registed pipe, so that another
							// read on the pipe could block?  to prevent this, we need
							// to check one more time to make certain the pipe is ready
							// for reading.
							selector.reset();
							selector.set_timeout( 0 );// set timeout for a poll
							selector.add_fd( (*sockTable)[i].iosock->get_file_desc(),
											 Selector::IO_READ );

							selector.execute();
							if ( selector.timed_out() ) {
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

						recheck_status = true;
						CallSocketHandler( i, true );

                        // update per-handler runtime statistics
                        runtime = dc_stats.AddRuntime((*sockTable)[i].handler_descrip, runtime);

					}	// if call_handler is True
				}	// if valid entry in sockTable
			}	// for 0 thru nSock checking if call_handler is true

            runtime = UtcTime::getTimeDouble();
            dc_stats.SocketRuntime += (runtime - group_runtime);
            group_runtime = runtime;


		}	// if rv > 0

        dc_stats.PumpCycle += (runtime - pump_cycle_begin_time);
        pump_cycle_begin_time = runtime;

	}	// end of infinite for loop
}

bool
DaemonCore::SocketIsRegistered( Stream *sock )
{
	int i = GetRegisteredSocketIndex( sock );
	return i != -1;
}

int
DaemonCore::GetRegisteredSocketIndex( Stream *sock )
{
	int i;

	for (i=0;i<nSock;i++) {
		if ( (*sockTable)[i].iosock == sock ) {
			return i;
		}
	}
	return -1;
}

void
DaemonCore::CallSocketHandler( Stream *sock, bool default_to_HandleCommand )
{
	int i = GetRegisteredSocketIndex( sock );

	if ( i == -1 ) {
		dprintf( D_ALWAYS,"CallSocketHandler: called on non-registered socket!\n");
		dprintf( D_ALWAYS,"Offending socket number %d\n", i );
		DumpSocketTable( D_DAEMONCORE );
		return;
	}

	CallSocketHandler( i, default_to_HandleCommand );
}

struct CallSocketHandler_args {
	int i;
	bool default_to_HandleCommand;
	Stream *accepted_sock;
};

void
DaemonCore::CallSocketHandler( int &i, bool default_to_HandleCommand )
{
    unsigned int iAcceptCnt = ( m_iMaxAcceptsPerCycle > 0 ) ? m_iMaxAcceptsPerCycle: -1;

    // if it is an accepting socket it will try for the connect
    // up (n) elements
    while ( iAcceptCnt )
    {
	    bool set_service_tid = false;

	    // Queue up the parameters and add to our thread pool.
	    struct CallSocketHandler_args *args;
	    args = new struct CallSocketHandler_args;

	    // If a tcp listen socket, do the accept now in the main thread
	    // so that we don't go back to the select loop with the listen
	    // socket still set.
	    args->accepted_sock = NULL;
	    Stream *insock = (*sockTable)[i].iosock;
	    ASSERT(insock);
	    if ( (*sockTable)[i].handler==NULL && (*sockTable)[i].handlercpp==NULL &&
		    default_to_HandleCommand &&
		    insock->type() == Stream::reli_sock &&
		    ((ReliSock *)insock)->_state == Sock::sock_special &&
		    ((ReliSock *)insock)->_special_state == ReliSock::relisock_listen
		    )
	    {
            // b/c we are now in a tight loop accepting, use select to check for more data and bail if none is there.
            Selector selector;
            selector.set_timeout( 0, 0 );
            selector.add_fd( (*sockTable)[i].iosock->get_file_desc(), Selector::IO_READ );
            selector.execute();

            if ( !selector.has_ready() ) {
                // avoid unnecessary blocking simply poll with timeout 0, if no data then exit
                delete args;
                return;
            }

		    args->accepted_sock = (Stream *) ((ReliSock *)insock)->accept();

		    if ( !(args->accepted_sock) ) {
		        dprintf(D_ALWAYS, "DaemonCore: accept() failed!");
		        // no need to add to work pool if we fail to accept
		        delete args;
		        return;
		    }

            iAcceptCnt --;

	    } else {
		    set_service_tid = true;
            iAcceptCnt=0;
	    }
	    args->i = i;
	    args->default_to_HandleCommand = default_to_HandleCommand;
	    int* pTid = NULL;
	    if ( set_service_tid ) {
		    // setup pointer (pTid) to pass to pool_add - thus servicing_tid will be
		    // set to the tid value BEFORE pool_add() yields.
		    pTid = &((*sockTable)[i].servicing_tid);
	    }
	    CondorThreads::pool_add(DaemonCore::CallSocketHandler_worker_demarshall,args,
								    pTid,(*sockTable)[i].handler_descrip);

    }
}

void
DaemonCore::CallSocketHandler_worker_demarshall(void *arg)
{
	struct CallSocketHandler_args *args = (struct CallSocketHandler_args *)arg;

	daemonCore->CallSocketHandler_worker( args->i, 
						args->default_to_HandleCommand,
						args->accepted_sock);

	delete args;
}

void
DaemonCore::CallSocketHandler_worker( int i, bool default_to_HandleCommand, Stream* asock )
{
	char *handlerName = NULL;
	int result=0;

		// if the user provided a handler for this socket, then
		// call it now.  otherwise, call the daemoncore
		// HandleReq() handler which strips off the command
		// request number and calls any registered command
		// handler.

		// Update curr_dataptr for GetDataPtr()
	curr_dataptr = &( (*sockTable)[i].data_ptr);

		// log a message
	if ( (*sockTable)[i].handler || (*sockTable)[i].handlercpp )
		{
			dprintf(D_DAEMONCORE,
					"Calling Handler <%s> for Socket <%s>\n",
					(*sockTable)[i].handler_descrip,
					(*sockTable)[i].iosock_descrip);
			handlerName = strdup((*sockTable)[i].handler_descrip);
			dprintf(D_COMMAND, "Calling Handler <%s> (%d)\n", handlerName,i);

		UtcTime handler_start_time;
		handler_start_time.getTime();

	if ( (*sockTable)[i].handler ) {
			// a C handler
		result = (*( (*sockTable)[i].handler))( (*sockTable)[i].service, (*sockTable)[i].iosock);
	} else if ( (*sockTable)[i].handlercpp ) {
			// a C++ handler
		result = ((*sockTable)[i].service->*( (*sockTable)[i].handlercpp))((*sockTable)[i].iosock);
		}

		UtcTime handler_stop_time;
		handler_stop_time.getTime();
		float handler_time = handler_stop_time.difference(&handler_start_time);

		dprintf(D_COMMAND, "Return from Handler <%s> %.4fs\n", handlerName, handler_time);
		free(handlerName);
	}
	else if( default_to_HandleCommand ) {
			// no handler registered, so this is a command
			// socket.  call the DaemonCore handler which
			// takes care of command sockets.
		result = HandleReq(i,asock);
	}
	else {
			// No registered callback, and we were told not to
			// call HandleCommand() by default, so just cancel
			// this socket.
		result = FALSE;
	}

		// Make sure we didn't leak our priv state
	CheckPrivState();

		// Clear curr_dataptr
	curr_dataptr = NULL;

		// Check result from socket handler, and if
		// not KEEP_STREAM, then
		// delete the socket and the socket handler.
	if ( result != KEEP_STREAM ) {
		Stream *iosock = (*sockTable)[i].iosock;
			// cancel the socket handler
		Cancel_Socket( iosock );
			// delete the cedar socket
		delete iosock;
	} else {
		// in this case, we are keeping the socket around.
		// so if this tid has it marked as being serviced,
		// reset the servicing_tid to 0 to signify we done operating
		// with the socket for the moment.
		if ( (*sockTable)[i].servicing_tid &&
			 (*sockTable)[i].servicing_tid == 
				CondorThreads::get_handle()->get_tid() ) 
		{
				(*sockTable)[i].servicing_tid = 0;
				// need to potentially add this sock to select
				daemonCore->Wake_up_select();	
		}
	}
}

bool
DaemonCore::CommandNumToTableIndex(int cmd,int *cmd_index)
{
		// first compute the hash
	if ( cmd < 0 )
		*cmd_index = -cmd % maxCommand;
	else
		*cmd_index = cmd % maxCommand;

	if (comTable[*cmd_index].num == cmd) {
			// hash found it first try... cool
		return true;
	}

		// hash did not find it, search for it
	int j;
	for (j = (*cmd_index + 1) % maxCommand; j != *cmd_index; j = (j + 1) % maxCommand) {
		if(comTable[j].num == cmd) {
			*cmd_index = j;
			return true;
		}
	}
	return false;
}

struct CallCommandHandlerInfo {
	CallCommandHandlerInfo(int req,time_t deadline,float time_spent_on_sec):
		m_req(req),
		m_deadline(deadline),
		m_time_spent_on_sec(time_spent_on_sec)
	{
		m_start_time.getTime();
	}
	int m_req;
	time_t m_deadline;
	float m_time_spent_on_sec;
	UtcTime m_start_time;
};

int
DaemonCore::HandleReqPayloadReady(Stream *stream)
{
		// The command payload has arrived or the deadline has
		// expired.
	int result = FALSE;
	CallCommandHandlerInfo *callback_info = (CallCommandHandlerInfo *)GetDataPtr();
	int req = callback_info->m_req;
	time_t orig_deadline = callback_info->m_deadline;
	float time_spent_on_sec = callback_info->m_time_spent_on_sec;
	UtcTime now;
	now.getTime();
	float time_waiting_for_payload = now.difference(callback_info->m_start_time);

	delete callback_info;

	Cancel_Socket( stream );

	int index = 0;
	bool reqFound = CommandNumToTableIndex(req,&index);

	if( !reqFound ) {
		dprintf(D_ALWAYS,
				"Command %d from %s is no longer recognized!\n",
				req,stream->peer_description());
		goto wrapup;
	}

	if( stream->deadline_expired() ) {
		dprintf(D_ALWAYS,
				"Deadline expired after %.3fs waiting for %s "
				"to send payload for command %d %s.\n",
				time_waiting_for_payload,stream->peer_description(),
				req,comTable[index].command_descrip);
		goto wrapup;
	}

	stream->set_deadline( orig_deadline );

	result = CallCommandHandler(req,stream,false,false,time_spent_on_sec,time_waiting_for_payload);

 wrapup:
	if( result != KEEP_STREAM ) {
		delete stream;
		result = KEEP_STREAM;
	}
	return result;
}

int
DaemonCore::CallCommandHandler(int req,Stream *stream,bool delete_stream,bool check_payload,float time_spent_on_sec,float time_spent_waiting_for_payload)
{
	int result = FALSE;
	int index = 0;
	bool reqFound = CommandNumToTableIndex(req,&index);
	char const *user = NULL;
	Sock *sock = (Sock *)stream;

	if ( reqFound ) {

		if( stream  && stream->type() == Stream::reli_sock && \
			comTable[index].wait_for_payload > 0 && check_payload )
		{
			if( !sock->readReady() ) {
				if( sock->deadline_expired() ) {
					dprintf(D_ALWAYS,"The payload has not arrived for command %d from %s, but the deadline has expired, so continuing to the command handler.\n",req,stream->peer_description());
				}
				else {
					time_t old_deadline = sock->get_deadline();
					sock->set_deadline_timeout(comTable[index].wait_for_payload);

					char callback_desc[50];
					snprintf(callback_desc,50,"Waiting for command %d payload",req);
					int rc = Register_Socket(
						stream,
						callback_desc,
						(SocketHandlercpp) &DaemonCore::HandleReqPayloadReady,
						"DaemonCore::HandleReqPayloadReady",
						this);
					if( rc >= 0 ) {
						CallCommandHandlerInfo *callback_info = new CallCommandHandlerInfo(req,old_deadline,time_spent_on_sec);
						Register_DataPtr((void *)callback_info);
						return KEEP_STREAM;
					}

					dprintf(D_ALWAYS,"Failed to register callback to wait for command %d payload from %s.\n",req,stream->peer_description());
					sock->set_deadline( old_deadline );
						// just call the command handler
				}
			}
		}

		MSC_SUPPRESS_WARNING(6011) // can't sure sure that sock is not NULL
		user = sock->getFullyQualifiedUser();
		if( !user ) {
			user = "";
		}
		MSC_SUPPRESS_WARNING(6011) // can't sure sure that stream is not NULL
		dprintf(D_COMMAND, "Calling HandleReq <%s> (%d) for command %d (%s) from %s %s\n",
				comTable[index].handler_descrip,
				inServiceCommandSocket_flag,
				req,
				comTable[index].command_descrip,
				user,
				stream->peer_description());

		UtcTime handler_start_time;
		handler_start_time.getTime();

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

		UtcTime handler_stop_time;
		handler_stop_time.getTime();
		float handler_time = handler_stop_time.difference(&handler_start_time);

		dprintf(D_COMMAND, "Return from HandleReq <%s> (handler: %.3fs, sec: %.3fs, payload: %.3fs)\n", comTable[index].handler_descrip, handler_time, time_spent_on_sec, time_spent_waiting_for_payload );

	}

	if ( delete_stream && result != KEEP_STREAM ) {
		delete stream;
	}

	return result;
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
		if (param_boolean_crufty("EXCEPT_ON_ERROR", false)) {
			EXCEPT( "Priv-state error found by DaemonCore" );
		}
	}
}

int DaemonCore::ServiceCommandSocket()
{
	Selector selector;
	int commands_served = 0;

	if ( inServiceCommandSocket_flag ) {
			// this function is not reentrant
			// and anyway, let's avoid potentially infinite recursion
		return 0;
	}

	// Just return if there is no command socket
	if ( initial_command_sock == -1 )
		return 0;
	if ( !( (*sockTable)[initial_command_sock].iosock) )
		return 0;

	// Setting timeout to 0 means do not block, i.e. just poll the socket
	selector.set_timeout( 0 );
	selector.add_fd( (*sockTable)[initial_command_sock].iosock->get_file_desc(),
					 Selector::IO_READ );

	inServiceCommandSocket_flag = TRUE;
	do {

		errno = 0;
		selector.execute();
#ifndef WIN32
		// Unix
		if ( selector.failed() ) {
				// not just interrupted by a signal...
				EXCEPT("select, error # = %d", errno);
		}
#else
		// Win32
		if ( selector.select_retval() == SOCKET_ERROR ) {
			EXCEPT("select, error # = %d",WSAGetLastError());
		}
#endif

		if ( selector.has_ready() ) {
			HandleReq( initial_command_sock );
			commands_served++;
				// Make sure we didn't leak our priv state
			CheckPrivState();
		}

	} while ( selector.has_ready() );	// loop until no more commands waiting on socket

	inServiceCommandSocket_flag = FALSE;
	return commands_served;
}

void
DaemonCore::HandleReqAsync(Stream *stream)
{
		// HandleReq() is now asynchronous, so just call it
	if( HandleReq(stream) != KEEP_STREAM ) {
		delete stream;
	}
}

int DaemonCore::HandleReq(int socki, Stream* asock)
{
	Stream *insock;
	
	insock = (*sockTable)[socki].iosock;

	return HandleReq(insock, asock);
}

int DaemonCore::HandleReq(Stream *insock, Stream* asock)
{
	bool is_command_sock = false;
	bool always_keep_stream = false;
	Stream *accepted_sock = NULL;

	if( asock ) {
		if( SocketIsRegistered(asock) ) {
			is_command_sock = true;
		}
	}
	else {
		ASSERT( insock );
		if( insock->type() == Stream::reli_sock && ((ReliSock *)insock)->isListenSock() ) {
			asock = ((ReliSock *)insock)->accept();
			accepted_sock = asock;

			if( !asock ) {
				dprintf(D_ALWAYS, "DaemonCore: accept() failed!\n");
					// return KEEP_STEAM cuz insock is a listen socket
				return KEEP_STREAM;
			}
			is_command_sock = false;
			always_keep_stream = true;
		}
		else {
			asock = insock;
			if( SocketIsRegistered(asock) ) {
				is_command_sock = true;
			}
			if( insock->type() == Stream::safe_sock ) {
					// currently, UDP sockets are always daemon command sockets
				always_keep_stream = true;
			}
		}
	}

	classy_counted_ptr<DaemonCommandProtocol> r = new DaemonCommandProtocol(asock,is_command_sock);

	int result = r->doProtocol();

	if( accepted_sock && result != KEEP_STREAM ) {
		delete accepted_sock;
	}

	if( always_keep_stream ) {
		return KEEP_STREAM;
	}
	return result;
}



int DaemonCore::HandleSigCommand(int command, Stream* stream) {
	int sig = 0;

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

bool DCSignalMsg::codeMsg( DCMessenger *, Sock *sock )
{
	if( !sock->code( m_signal ) ) {
		sockFailed( sock );
		return false;
	}
	return true;
}

void DCSignalMsg::reportFailure( DCMessenger * )
{
	char const *status;
	if( daemonCore->ProcessExitedButNotReaped(thePid()) ) {
		status = "exited but not reaped";
	}
	else if( daemonCore->Is_Pid_Alive(thePid()) ) {
		status = "still alive";
	}
	else {
		status = "no longer exists";
	}

	dprintf(D_ALWAYS,
			"Send_Signal: Warning: could not send signal %d (%s) to pid %d (%s)\n",
			theSignal(),signalName(),thePid(),status);
}

void DCSignalMsg::reportSuccess( DCMessenger * )
{
	dprintf(D_DAEMONCORE,
			"Send_Signal: sent signal %d (%s) to pid %d\n",
			theSignal(),signalName(),thePid());
}

char const *DCSignalMsg::signalName()
{
	switch(theSignal()) {
	case SIGUSR1:
		return "SIGUSR1";
	case SIGUSR2:
		return "SIGUSR2";
	case SIGTERM:
		return "SIGTERM";
	case SIGSTOP:
		return "SIGSTOP";
	case SIGCONT:
		return "SIGCONT";
	case SIGQUIT:
		return "SIGQUIT";
	case SIGKILL:
		return "SIGKILL";
	}

		// If it is not a system-defined signal, it must be a DC signal.

	char const *sigName = getCommandString(theSignal());
	if(!sigName) {
			// Always return something, because this goes straight to sprintf.
		return "";
	}
	return sigName;
}

bool DaemonCore::Send_Signal(pid_t pid, int sig)
{
	classy_counted_ptr<DCSignalMsg> msg = new DCSignalMsg(pid,sig);
	Send_Signal(msg, false);

	// Since we ran with nonblocking=false, the success status is now available

	return msg->deliveryStatus() == DCMsg::DELIVERY_SUCCEEDED;
}

void DaemonCore::Send_Signal_nonblocking(classy_counted_ptr<DCSignalMsg> msg) {
	Send_Signal( msg, true );

		// We need to make sure the callback hooks are called if this
		// message was handled through some means other than delivery
		// through DCMessenger.

	if( !msg->messengerDelivery() ) {
		switch( msg->deliveryStatus() ) {
		case DCMsg::DELIVERY_SUCCEEDED:
			msg->messageSent( NULL, NULL );
			break;
		case DCMsg::DELIVERY_FAILED:
		case DCMsg::DELIVERY_PENDING:
		case DCMsg::DELIVERY_CANCELED:
				// Send_Signal() typically only sets the delivery status to
				// SUCCEEDED; so if things fail, the status will remain
				// PENDING.
			msg->messageSendFailed( NULL );
			break;
		}
	}
}

void DaemonCore::Send_Signal(classy_counted_ptr<DCSignalMsg> msg, bool nonblocking)
{
	pid_t pid = msg->thePid();
	int sig = msg->theSignal();
	PidEntry * pidinfo = NULL;
	int same_thread, is_local;
	char const *destination;
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

	if( ProcessExitedButNotReaped(pid) ) {
		msg->deliveryStatus( DCMsg::DELIVERY_FAILED );
		dprintf(D_ALWAYS,"Send_Signal: attempt to send signal %d to process %d, which has exited but not yet been reaped.\n",sig,pid);
		return;
	}

	// if we're using priv sep, we may not have permission to send signals
	// to our child processes; ask the ProcD to do it for us
	//
	if (privsep_enabled() || param_boolean("GLEXEC_JOB", false)) {
		if (!target_has_dcpm && pidinfo && pidinfo->new_process_group) {
			ASSERT(m_proc_family != NULL);
			bool ok =  m_proc_family->signal_process(pid, sig);
			if (ok) {
				// set flag for success
				msg->deliveryStatus( DCMsg::DELIVERY_SUCCEEDED );
			} else {
				dprintf(D_ALWAYS,
				        "error using procd to send signal %d to pid %u\n",
				        sig,
				        pid);
			}
			return;
		}
	}

	// handle the "special" action signals which are really just telling
	// DaemonCore to do something.
	switch (sig) {
		case SIGKILL:
			if( Shutdown_Fast(pid) ) {
				msg->deliveryStatus( DCMsg::DELIVERY_SUCCEEDED );
			}
			return;
			break;
		case SIGSTOP:
			if( Suspend_Process(pid) ) {
				msg->deliveryStatus( DCMsg::DELIVERY_SUCCEEDED );
			}
			return;
			break;
		case SIGCONT:
			if( Continue_Process(pid) ) {
				msg->deliveryStatus( DCMsg::DELIVERY_SUCCEEDED );
			}
			return;
			break;
#ifdef WIN32
		case SIGTERM:
				// Under Windows, use Shutdown_Graceful to send
				// WM_CLOSE to a non DC process; otherwise use a DC
				// signal.  Under UNIX, we just use the default logic
				// below to determine whether we should send a UNIX
				// SIGTERM or a DC signal.
			if ( pid != mypid && target_has_dcpm == FALSE ) {
				if( Shutdown_Graceful(pid) ) {
					msg->deliveryStatus( DCMsg::DELIVERY_SUCCEEDED );
				}
				return;
			}
			break;
#endif
		default: {
#ifndef WIN32
			bool use_kill = false;
			if( pid == mypid ) {
					// Never never send unix signals directly to self,
					// because the signal handlers all just turn around
					// and call Send_Signal() again.
				use_kill = false;
			}
			else if( target_has_dcpm == FALSE ) {
				use_kill = true;
			}
			else if( target_has_dcpm == TRUE &&
			         (sig == SIGUSR1 || sig == SIGUSR2 || sig == SIGQUIT ||
			          sig == SIGTERM || sig == SIGHUP) )
			{
					// Try using kill(), even though this is a
					// daemon-core process we are signalling.  We do
					// this, because kill() is less prone to failure
					// than the network-message method, and it never
					// blocks.  (In the current implementation, the
					// UDP message call may block if it needs to use a
					// temporary TCP connection to establish a session
					// key.)

				use_kill = true;
			}

			if ( use_kill ) {
				const char* tmp = signalName(sig);
				dprintf( D_FULLDEBUG,
						 "Send_Signal(): Doing kill(%d,%d) [%s]\n",
						 pid, sig, tmp ? tmp : "Unknown" );
				priv_state priv = set_root_priv();
				int status = ::kill(pid, sig);
				set_priv(priv);
				// return 1 if kill succeeds, 0 otherwise
				if (status >= 0) {
					msg->deliveryStatus( DCMsg::DELIVERY_SUCCEEDED );
				}
				else if( target_has_dcpm == TRUE ) {
						// kill() failed.  Fall back on a UDP message.
					dprintf(D_ALWAYS,"Send_Signal error: kill(%d,%d) failed: errno=%d %s\n",
							pid,sig,errno,strerror(errno));
					break;
				}
				return;
			}
#endif  // not defined Win32
			break;
		}
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
				_condor_full_write(async_pipe[1],"!",1);
			}
#endif
			msg->deliveryStatus( DCMsg::DELIVERY_SUCCEEDED );
			return;
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
			return;
		}

		is_local = pidinfo->is_local;
		destination = pidinfo->sinful_string.Value();
	}

	classy_counted_ptr<Daemon> d = new Daemon( DT_ANY, destination );

	// now destination process is local, send via UDP; if remote, send via TCP
	if ( is_local == TRUE && d->hasUDPCommandPort()) {
		msg->setStreamType(Stream::safe_sock);
		if( !nonblocking ) msg->setTimeout(3);
	}
	else {
		msg->setStreamType(Stream::reli_sock);
	}
	if (pidinfo && pidinfo->child_session_id)
	{
		msg->setSecSessionId(pidinfo->child_session_id);
	}
	msg->messengerDelivery( true ); // we really are sending this message
	if( nonblocking ) {
		d->sendMsg( msg.get() );
	}
	else {
		d->sendBlockingMsg( msg.get() );
	}
}

int DaemonCore::Shutdown_Fast(pid_t pid, bool want_core )
{
	(void) want_core;		// For windoze

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

	if( IsDebugVerbose(D_PROCFAMILY) ) {
			char check_name[MAX_PATH];
			CSysinfo sysinfo;
			sysinfo.GetProcessName(pid,check_name, sizeof(check_name));
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
	int status = kill(pid, want_core ? SIGABRT : SIGKILL );
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

	// send a DC TERM signal if the target is daemon core
	//
	PidEntry* pidinfo;
	if ((pidTable->lookup(pid, pidinfo) != -1) &&
	    (pidinfo->sinful_string[0] != '\0'))
	{
		dprintf(D_PROCFAMILY,
		        "Shutdown_Graceful: Sending pid %d SIGTERM\n",
		        pid);
		return Send_Signal(pid, SIGTERM);
	}

	// otherwise, invoke the condor_softkill program, which
	// will attempt to find a top-level window owned by the
	// target process and post a WM_CLOSE to it
	//
	ArgList args;
	char* softkill_binary = param("WINDOWS_SOFTKILL");
	if (softkill_binary == NULL) {
		dprintf(D_ALWAYS, "cannot send softkill since WINDOWS_SOFTKILL is undefined\n");
		return FALSE;
	}
	args.AppendArg(softkill_binary);
	free(softkill_binary);
	args.AppendArg(pid);
	char* softkill_log = param("WINDOWS_SOFTKILL_LOG");
	if (softkill_log) {
		args.AppendArg(softkill_log);
		free(softkill_log);
	}
	int ret = my_system(args);
	dprintf((ret == 0) ? D_FULLDEBUG : D_ALWAYS,
	        "return value from my_system for softkill: %d\n",
	        ret);
	return (ret == 0);

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
	return windows_suspend(pid);
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
	return windows_continue(pid);
#else
	priv_state priv = set_root_priv();
	int status = kill(pid, SIGCONT);
	set_priv(priv);
	return (status >= 0);		// return 1 if kill succeeds, 0 otherwise
#endif
}

int DaemonCore::SetDataPtr(void *dptr)
{
	// note: curr_dataptr is updated by daemon core
	// whenever a register_* or a handler invocation takes place

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

#endif	// of ifdef WIN32


void
DaemonCore::Forked_Child_Wants_Exit_By_Exec( bool exit_by_exec )
{
	if( exit_by_exec ) {
		_condor_exit_with_exec = 1;
	}
}

#if !defined(WIN32)
class CreateProcessForkit;
CreateProcessForkit *g_create_process_forkit = NULL;

void
enterCreateProcessChild(CreateProcessForkit *forkit) {
	ASSERT( g_create_process_forkit == NULL );
	g_create_process_forkit = forkit;
}

void
exitCreateProcessChild() {
	g_create_process_forkit = NULL;
}

void
writeExecError(CreateProcessForkit *forkit,int exec_errno);

#endif

#if !defined(WIN32)
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

#if HAVE_GNU_LD
void __real_exit(int status);
void __wrap_exit(int status)
{
	if ( _condor_exit_with_exec == 0 && g_create_process_forkit == NULL ) {
			// The advantage of calling the real exit() rather than
			// _exit() is that things like gprof and google-perftools
			// can write a final profile dump.
		__real_exit(status);
	}

#else
void exit(int status)
{
#endif

		// turns out that _exit() does *not* always flush STDOUT and
		// STDERR, that it's "implementation dependent".  so, to
		// ensure that daemoncore things that are writing to stdout
		// and exiting (like "condor_master -version" or
		// "condor_shadow -classad") will in fact produce output, we
		// need to call fflush() ourselves right here.
		// Derek Wright <wright@cs.wisc.edu> 2004-03-28
	fflush( stdout );
	fflush( stderr );

	if( g_create_process_forkit ) {
			// We are inside fork() or clone() and we need to tell our
			// parent process that something has gone horribly wrong.
		writeExecError(g_create_process_forkit,DaemonCore::ERRNO_EXIT);
	}

	if ( _condor_exit_with_exec == 0 ) {
		_exit(status);
	}

	const char* my_argv[2];
	const char* my_env[1];
	my_argv[1] = NULL;
	my_env[0] = NULL;

		// First try to just use /bin/true or /bin/false.
	if ( status == 0 ) {
		my_argv[0] = "/bin/true";
		execve( "/bin/true",
				const_cast<char *const*>(my_argv),
				const_cast<char *const*>(my_env)  );
		my_argv[0] = "/usr/bin/true";
		execve( "/usr/bin/true",
				const_cast<char *const*>(my_argv),
				const_cast<char *const*>(my_env)  );
	} else {
		my_argv[0] = "/bin/false";
		execve( "/bin/false",
				const_cast<char *const*>(my_argv),
				const_cast<char *const*>(my_env)  );
		my_argv[0] = "/usr/bin/false";
		execve( "/usr/bin/false",
				const_cast<char *const*>(my_argv),
				const_cast<char *const*>(my_env)  );
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

// helper function for registering a family with our ProcFamily
// logic. the first 3 arguments are mandatory for registration.
// the next three are optional and specify what tracking methods
// should be used for the new process family. if group is non-NULL
// then we will ask the ProcD to track by supplementary group
// ID - the ID that the ProcD chooses for this purpose is returned
// in the location pointed to by the argument. the last argument
// optionally specifies a proxy to give to the ProcD so that
// it can use glexec to signal the processes in this family
//
bool
DaemonCore::Register_Family(pid_t       child_pid,
                            pid_t       parent_pid,
                            int         max_snapshot_interval,
                            PidEnvID*   penvid,
                            const char* login,
                            gid_t*      group,
			    const char* cgroup,
                            const char* glexec_proxy)
{
    double begintime = UtcTime::getTimeDouble();
   	double runtime = begintime;

	bool success = false;
	bool family_registered = false;
	if (!m_proc_family->register_subfamily(child_pid,
	                                       parent_pid,
	                                       max_snapshot_interval))
	{
		dprintf(D_ALWAYS,
		        "Create_Process: error registering family for pid %u\n",
		        child_pid);
		goto REGISTER_FAMILY_DONE;
	}
	family_registered = true;
    runtime = dc_stats.AddRuntimeSample("DCRregister_subfamily", IF_VERBOSEPUB, runtime);
	if (penvid != NULL) {
		if (!m_proc_family->track_family_via_environment(child_pid, *penvid)) {
			dprintf(D_ALWAYS,
			        "Create_Process: error tracking family "
			            "with root %u via environment\n",
					child_pid);
			goto REGISTER_FAMILY_DONE;
		}
       runtime = dc_stats.AddRuntimeSample("DCRtrack_family_via_env", IF_VERBOSEPUB, runtime);
	}
	if (login != NULL) {
		if (!m_proc_family->track_family_via_login(child_pid, login)) {
			dprintf(D_ALWAYS,
			        "Create_Process: error tracking family "
			            "with root %u via login (name: %s)\n",
			        child_pid,
			        login);
			goto REGISTER_FAMILY_DONE;
		}
       runtime = dc_stats.AddRuntimeSample("DCRtrack_family_via_login", IF_VERBOSEPUB, runtime);
	}
	if (group != NULL) {
#if defined(LINUX)
		*group = 0;
		if (!m_proc_family->
			track_family_via_allocated_supplementary_group(child_pid, *group))
		{
			dprintf(D_ALWAYS,
			        "Create_Process: error tracking family "
			            "with root %u via group ID\n",
			        child_pid);
			goto REGISTER_FAMILY_DONE;
		}
		ASSERT( *group != 0 ); // tracking gid should never be group 0
#else
		EXCEPT("Internal error: "
		           "group-based tracking unsupported on this platform");
#endif
	}
	if (cgroup != NULL) {
#if defined(HAVE_EXT_LIBCGROUP)
		if (!m_proc_family->track_family_via_cgroup(child_pid, cgroup))
		{
			dprintf(D_ALWAYS,
				"Create_Process: error tracking family "
				    "with root %u via cgroup %s\n",
				child_pid, cgroup);
			goto REGISTER_FAMILY_DONE;
		}
#else
		EXCEPT("Internal error: "
			    "cgroup-based tracking unsupported in this condor build");
#endif
	}
	if (glexec_proxy != NULL) {
		if (!m_proc_family->use_glexec_for_family(child_pid,
		                                          glexec_proxy))
		{
			dprintf(D_ALWAYS,
			        "Create_Process: error using GLExec for "
				    "family with root %u\n",
			        child_pid);
			goto REGISTER_FAMILY_DONE;
		}
       runtime = dc_stats.AddRuntimeSample("DCRuse_glexec_for_family", IF_VERBOSEPUB, runtime);
	}
	success = true;
REGISTER_FAMILY_DONE:
	if (family_registered && !success) {
		if (!m_proc_family->unregister_family(child_pid)) {
			dprintf(D_ALWAYS,
			        "Create_Process: error unregistering family "
			            "with root %u\n",
			        child_pid);
		}
        runtime = dc_stats.AddRuntimeSample("DCRunregister_family", IF_VERBOSEPUB, runtime);
	}

    runtime = dc_stats.AddRuntimeSample("DCRegister_Family", IF_VERBOSEPUB, begintime);

	return success;
}

#ifndef WIN32

/*************************************************************
 * CreateProcessForkit is a helper class for Create_Process. *
 * It does the fork and exec, or something equivalent, such  *
 * as a fast clone() and exec under Linux.                   *
 *************************************************************/

class CreateProcessForkit {
public:
	CreateProcessForkit(
		const int (&the_errorpipe)[2],
		const ArgList &the_args,
		int the_job_opt_mask,
		const Env *the_env,
		const MyString &the_inheritbuf,
		const MyString &the_privateinheritbuf,
		pid_t the_forker_pid,
		time_t the_time_of_fork,
		unsigned int the_mii,
		const FamilyInfo *the_family_info,
		const char *the_cwd,
		const char *the_executable,
		const char *the_executable_fullpath,
		const int *the_std,
		int the_numInheritFds,
		const int (&the_inheritFds)[MAX_INHERIT_FDS],
		int the_nice_inc,
		const priv_state &the_priv,
		int the_want_command_port,
		const sigset_t *the_sigmask,
		size_t *core_hard_limit,
		long    as_hard_limit,
		int		*affinity_mask,
		FilesystemRemap *fs_remap
	): m_errorpipe(the_errorpipe), m_args(the_args),
	   m_job_opt_mask(the_job_opt_mask), m_env(the_env),
	   m_inheritbuf(the_inheritbuf),
	   m_privateinheritbuf(the_privateinheritbuf),
	   m_forker_pid(the_forker_pid),
	   m_time_of_fork(the_time_of_fork), m_mii(the_mii),
	   m_family_info(the_family_info), m_cwd(the_cwd),
	   m_executable(the_executable),
	   m_executable_fullpath(the_executable_fullpath), m_std(the_std),
	   m_numInheritFds(the_numInheritFds),
	   m_inheritFds(the_inheritFds), m_nice_inc(the_nice_inc),
	   m_priv(the_priv), m_want_command_port(the_want_command_port),
	   m_sigmask(the_sigmask), m_unix_args(0), m_unix_env(0),
	   m_core_hard_limit(core_hard_limit),
	   m_as_hard_limit(as_hard_limit),
	   m_affinity_mask(affinity_mask),
 	   m_fs_remap(fs_remap),
	   m_wrote_tracking_gid(false),
	   m_no_dprintf_allowed(false)
	{
	}

	~CreateProcessForkit() {
			// We have to delete these here, in the case where child and parent share
			// memory.
		deleteStringArray(m_unix_args);
		deleteStringArray(m_unix_env);
	}

	pid_t fork_exec();
	void exec();
	static int clone_fn( void *arg );

	pid_t clone_safe_getpid();
	pid_t clone_safe_getppid();

	void writeTrackingGid(gid_t tracking_gid);
	void writeExecError(int exec_errno);

private:
		// Data passed to us from the parent:
		// The following are mostly const, just to avoid accidentally
		// making changes to the parent data from the child.  The parent
		// should expect anything that is not const to potentially be
		// modified.
	const int (&m_errorpipe)[2];
	const ArgList &m_args;
	const int m_job_opt_mask;
	const Env *m_env;
	const MyString &m_inheritbuf;
	const MyString &m_privateinheritbuf;
	const pid_t m_forker_pid;
	const time_t m_time_of_fork;
	const unsigned int m_mii;
	const FamilyInfo *m_family_info;
	const char *m_cwd;
	const char *m_executable;
	const char *m_executable_fullpath;
	const int *m_std;
	const int m_numInheritFds;
	const int (&m_inheritFds)[MAX_INHERIT_FDS];
	int m_nice_inc;
	const priv_state &m_priv;
	const int m_want_command_port;
	const sigset_t *m_sigmask; // preprocessor macros prevent us from
	                           // using the name "sigmask" here

		// Data generated internally in the child that must be
		// cleaned up in the destructor of this class in the
		// case where parent and child share the same address space.
	char **m_unix_args;
	char **m_unix_env;
	size_t *m_core_hard_limit;
	long m_as_hard_limit;
	const int    *m_affinity_mask;
	Env m_envobject;
    FilesystemRemap *m_fs_remap;
	bool m_wrote_tracking_gid;
	bool m_no_dprintf_allowed;
	priv_state m_priv_state;
};

enum {
        STACK_GROWS_UP,
        STACK_GROWS_DOWN
};

#if HAVE_CLONE
static int stack_direction(volatile int *ptr=NULL) {
    volatile int location;
    if(!ptr) return stack_direction(&location);
    if (ptr < &location) {
        return STACK_GROWS_UP;
    }

    return STACK_GROWS_DOWN;
}
#endif

pid_t CreateProcessForkit::clone_safe_getpid() {
#if HAVE_CLONE
		// In some broken threading implementations (e.g. PPC SUSE 9 tls),
		// getpid() in the child branch of clone(CLONE_VM) returns
		// the pid of the parent process (presumably due to internal
		// caching in libc).  Therefore, use the syscall to get
		// the answer directly.
	return syscall(SYS_getpid);
#else
	return ::getpid();
#endif
}
pid_t CreateProcessForkit::clone_safe_getppid() {
#if HAVE_CLONE
		// See above comment for clone_safe_getpid() for explanation of
		// why we need to do this.
	return syscall(SYS_getppid);
#else
	return ::getppid();
#endif
}

pid_t CreateProcessForkit::fork_exec() {
	pid_t newpid;

#if HAVE_CLONE
		// Why use clone() instead of fork?  In current versions of
		// Linux, fork() is slower for processes with lots of memory
		// (e.g. a big schedd), because all the page tables have to be
		// copied for the new process.  In a future version of Linux,
		// there will supposedly be a fix for this (making the page
		// tables themselves copy-on-write), but this does not exist
		// in Linux 2.6 as of today.  We could use vfork(), but the
		// limitations of what can be safely done from within the
		// child of vfork() are too restrictive.  So instead, we use
		// clone() with the CLONE_VM option, which creates a child
		// that runs in the same address space as the parent,
		// eliminating the extra overhead of spawning children from
		// large processes.

	if( daemonCore->UseCloneToCreateProcesses() ) {
		dprintf(D_FULLDEBUG,"Create_Process: using fast clone() "
		                    "to create child process.\n");

			// The stack size must be big enough for everything that
			// happens in CreateProcessForkit::clone_fn().  In some
			// environments, some extra steps may need to be taken to
			// make a stack on the heap (to mark it as executable), so
			// we just do it using the parent's stack space and we use
			// CLONE_VFORK to ensure the child is done with the stack
			// before the parent throws it away.
		const int stack_size = 16384;
		char child_stack[stack_size];

			// Beginning of stack is at end on all processors that run
			// Linux, except for HP PA.  Here we just detect at run-time
			// which way it goes.
		char *child_stack_ptr = child_stack;
		if( stack_direction() == STACK_GROWS_DOWN ) {
			child_stack_ptr += stack_size;
		}

			// save some state in dprintf
		dprintf_before_shared_mem_clone();

			// reason for flags passed to clone:
			// CLONE_VM    - child shares same address space (so no time
			//               wasted copying page tables)
			// CLONE_VFORK - parent is suspended until child calls exec/exit
			//               (so we do not throw away child's stack etc.)
			// SIGCHLD     - we want this signal when child dies, as opposed
			//               to some other non-standard signal

		enterCreateProcessChild(this);

		newpid = clone(
			CreateProcessForkit::clone_fn,
			child_stack_ptr,
			(CLONE_VM|CLONE_VFORK|SIGCHLD),
			this );

		exitCreateProcessChild();

			// Since we used the CLONE_VFORK flag, the child has exited
			// or called exec by now.

			// restore state
		dprintf_after_shared_mem_clone();

		return newpid;
	}
#endif /* HAVE_CLONE */

	newpid = fork();
	if( newpid == 0 ) {
			// in child
		enterCreateProcessChild(this);
		exec(); // never returns
	}

	return newpid;
}

int CreateProcessForkit::clone_fn( void *arg ) {
		// We are now in the child branch of the clone() system call.
		// Our parent is suspended and we are in the same address space.
		// Now it is time to exec().
	((CreateProcessForkit *)arg)->exec();
	return 0;
}

void CreateProcessForkit::writeTrackingGid(gid_t tracking_gid)
{
	m_wrote_tracking_gid = true;
	int rc = full_write(m_errorpipe[1], &tracking_gid, sizeof(tracking_gid));
	if( rc != sizeof(tracking_gid) ) {
			// the writing of the tracking gid _must_ succeed or we must
			// abort before calling exec()
		if( !m_no_dprintf_allowed ) {
			dprintf(D_ALWAYS,"Create_Process: Failed to write tracking gid: rc=%d, errno=%d\n",rc,errno);
		}
		_exit(4);
	}
}

void CreateProcessForkit::writeExecError(int child_errno)
{
	if( !m_wrote_tracking_gid ) {
			// Tracking gid must come before errno on the pipe,
			// so write a bogus gid now.  The value doesn't
			// matter, because we are reporting failure to
			// call exec().
		writeTrackingGid(0);
	}
	int rc = full_write(m_errorpipe[1], &child_errno, sizeof(child_errno));
	if( rc != sizeof(child_errno) ) {
		if( !m_no_dprintf_allowed ) {
			dprintf(D_ALWAYS,"Create_Process: Failed to write error to error pipe: rc=%d, errno=%d\n",rc,errno);
		}
	}
}

void writeExecError(CreateProcessForkit *forkit,int exec_errno)
{
	forkit->writeExecError(exec_errno);
}

void CreateProcessForkit::exec() {
	gid_t  tracking_gid = 0;

		// Keep in mind that there are two cases:
		//   1. We got here by forking, (cannot modify parent's memory)
		//   2. We got here by cloning, (can modify parent's memory)
		// So don't screw up the parent's memory and don't allocate any
		// memory assuming it will be freed on exec() or _exit().  All objects
		// that allocate memory MUST BE in data structures that are cleaned
		// up by the parent (e.g. this instance of CreateProcessForkit).
		// We do have our own copy of the file descriptors and signal masks.

		// All critical errors in this function should write out the error
		// code to the error pipe and then should call _exit().  Since
		// some of the functions called below may result in a call to
		// exit() (e.g. dprintf could EXCEPT), we use daemonCore's
		// exit() wrapper to catch these cases and do the right thing.
		// That is, this function must be wrapped by calls to
		// enterCreateProcessChild() and exitCreateProcessChild().

		// make sure we're not going to try to share the lock file
		// with our parent (if it's been defined).
	dprintf_init_fork_child();

		// close the read end of our error pipe and set the
		// close-on-exec flag on the write end
	close(m_errorpipe[0]);
	fcntl(m_errorpipe[1], F_SETFD, FD_CLOEXEC);

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

	pid_t pid = clone_safe_getpid();
	pid_t ppid = clone_safe_getppid();
	DaemonCore::PidEntry* pidinfo = NULL;
	if( (daemonCore->pidTable->lookup(pid, pidinfo) >= 0) ) {
			// we've already got this pid in our table! we've got
			// to bail out immediately so our parent can retry.
		writeExecError(DaemonCore::ERRNO_PID_COLLISION);
		_exit(4);
	}
		// If we made it here, we didn't find the PID in our
		// table, so it's safe to continue and eventually do the
		// exec() in this process...

		/////////////////////////////////////////////////////////////////
		// figure out what stays and goes in the child's environment
		/////////////////////////////////////////////////////////////////

		// We may determine to seed the child's environment with the parent's.
	if( HAS_DCJOBOPT_ENV_INHERIT(m_job_opt_mask) ) {
		m_envobject.Import();
	}

		// Put the caller's env requests into the job's environment, potentially
		// adding/overriding things in the current env if the job was allowed to
		// inherit the parent's environment.
	if(m_env) {
		m_envobject.MergeFrom(*m_env);
	}

		// if I have brought in the parent's environment, then ensure that
		// after the caller's changes have been enacted, this overrides them.
	if( HAS_DCJOBOPT_ENV_INHERIT(m_job_opt_mask) && HAS_DCJOBOPT_CONDOR_ENV_INHERIT(m_job_opt_mask) ) {

			// add/override the inherit variable with the correct value
			// for this process.
		m_envobject.SetEnv( EnvGetName( ENV_INHERIT ), m_inheritbuf.Value() );

		if( !m_privateinheritbuf.IsEmpty() ) {
			m_envobject.SetEnv( EnvGetName( ENV_PRIVATE ), m_privateinheritbuf.Value() );
		}
			// Make sure PURIFY can open windows for the daemons when
			// they start. This functionality appears to only exist when we've
			// decided to inherit the parent's environment. I'm not sure
			// what the ramifications are if we include it all the time so here
			// it stays for now.
		char *display;
		display = param ( "PURIFY_DISPLAY" );
		if ( display ) {
			m_envobject.SetEnv( "DISPLAY", display );
			free ( display );
			char *purebuf;
			purebuf = (char*)malloc(sizeof(char) * 
									(strlen("-program-name=") + strlen(m_executable) + 
									 1));
			if (purebuf == NULL) {
				EXCEPT("Create_Process: PUREOPTIONS is out of memory!");
			}
			sprintf ( purebuf, "-program-name=%s", m_executable );
			m_envobject.SetEnv( "PUREOPTIONS", purebuf );
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

		// PidEnvID does no dynamic allocation, so it is being declared here
		// rather than in the CreateProcessForkit object, because we do not
		// have to worry about freeing anything up later etc.
	PidEnvID penvid;
	pidenvid_init(&penvid);

		// if we weren't inheriting the parent's environment, then grab out
		// the parent's pidfamily history... and jam it into the child's 
		// environment
	if ( HAS_DCJOBOPT_NO_ENV_INHERIT(m_job_opt_mask) ) {
		int i;
			// The parent process could not have been exec'ed if there were 
			// too many ancestor markers in its environment, so this check
			// is more of an assertion.
		if (pidenvid_filter_and_insert(&penvid, GetEnviron()) ==
			PIDENVID_OVERSIZED)
			{
				dprintf ( D_ALWAYS, "Create_Process: Failed to filter ancestor "
						  "history from parent's environment because there are more "
						  "than PIDENVID_MAX(%d) of them! Programmer Error.\n",
						  PIDENVID_MAX );
					// before we exit, make sure our parent knows something
					// went wrong before the exec...
				writeExecError(errno);
				_exit(errno);
			}

			// Propogate the ancestor history to the child's environment
		for (i = 0; i < PIDENVID_MAX; i++) {
			if (penvid.ancestors[i].active == TRUE) { 
				m_envobject.SetEnv( penvid.ancestors[i].envid );
			} else {
					// After the first FALSE entry, there will never be
					// true entries.
				break;
			}
		}
	}

		// create the new ancestor entry for the child's environment
	if (pidenvid_format_to_envid(envid, PIDENVID_ENVID_SIZE, 
								 m_forker_pid, pid, m_time_of_fork, m_mii) == PIDENVID_BAD_FORMAT) 
		{
			dprintf ( D_ALWAYS, "Create_Process: Failed to create envid "
					  "\"%s\" due to bad format. !\n", envid );
				// before we exit, make sure our parent knows something
				// went wrong before the exec...
			writeExecError(errno);
			_exit(errno);
		}

		// if the new entry fits into the penvid, then add it to the 
		// environment, else EXCEPT cause it is programmer's error 
	if (pidenvid_append(&penvid, envid) == PIDENVID_OK) {
		m_envobject.SetEnv( envid );
	} else {
		dprintf ( D_ALWAYS, "Create_Process: Failed to insert envid "
				  "\"%s\" because its insertion would mean more than "
				  "PIDENVID_MAX entries in a process! Programmer "
				  "Error.\n", envid );
			// before we exit, make sure our parent knows something
			// went wrong before the exec...
		writeExecError(errno);
		_exit(errno);
	}
		// END pid family environment id propogation 

		// The child's environment:
	m_unix_env = m_envobject.getStringArray();


		/////////////////////////////////////////////////////////////////
		// figure out what stays and goes in the job's arguments
		/////////////////////////////////////////////////////////////////

	if( m_args.Count() == 0 ) {
		dprintf(D_DAEMONCORE, "Create_Process: Arg: NULL\n");
		ArgList tmpargs;
		tmpargs.AppendArg(m_executable);
		m_unix_args = tmpargs.GetStringArray();
	}
	else {
		if(IsDebugLevel(D_DAEMONCORE)) {
			MyString arg_string;
			m_args.GetArgsStringForDisplay(&arg_string);
			dprintf(D_DAEMONCORE, "Create_Process: Arg: %s\n", arg_string.Value());
		}
		m_unix_args = m_args.GetStringArray();
	}


		// check to see if this is a subfamily
	if( ( m_family_info != NULL ) ) {

			//create a new process group if we are supposed to
		if(param_boolean( "USE_PROCESS_GROUPS", true )) {

				// Set sid is the POSIX way of creating a new proc group
			if( setsid() == -1 )
				{
					dprintf(D_ALWAYS, "Create_Process: setsid() failed: %s\n",
							strerror(errno) );
						// before we exit, make sure our parent knows something
						// went wrong before the exec...
					writeExecError(errno);
					_exit(errno);
				}
		}

		// regardless of whether or not we made an "official" unix
		// process group above, ALWAYS contact the procd to register
		// ourselves
		//
		ASSERT(daemonCore->m_proc_family != NULL);
		if (daemonCore->m_proc_family->register_from_child()) {

			// NOTE: only on Linux do we pass the structure with the
			// environment tracking info in it, since Linux is currently
			// the only platform for which ProcAPI can read process'
			// environments. this is important because on MacOS, sending
			// this structure makes the write to the ProcD's named pipe exceed
			// PIPE_BUF, which means the write wouldn't be atomic (which
			// means data from multiple ProcD clients could be interleaved,
			// causing major badness)
			//
#if defined(LINUX)
			PidEnvID* penvid_ptr = &penvid;
#else
			PidEnvID* penvid_ptr = NULL;
#endif

			// yet another linux-only tracking method: supplementary GID
			//
			gid_t* tracking_gid_ptr = NULL;
#if defined(LINUX)
			if( m_family_info->group_ptr ) {
				tracking_gid_ptr = &tracking_gid;
			}
#endif

			bool ok =
				daemonCore->Register_Family(pid,
				                            ppid,
				                            m_family_info->max_snapshot_interval,
				                            penvid_ptr,
				                            m_family_info->login,
				                            tracking_gid_ptr,
							    m_family_info->cgroup,
				                            m_family_info->glexec_proxy);
			if (!ok) {
				errno = DaemonCore::ERRNO_REGISTRATION_FAILED;
				writeExecError(errno);
				_exit(4);
			}

			if (tracking_gid_ptr != NULL) {
				ASSERT( *tracking_gid_ptr != 0 ); // tracking group should never be group 0
				set_user_tracking_gid(*tracking_gid_ptr);
			}
		}
	}

		// This _must_ be called before calling exec().
	writeTrackingGid(tracking_gid);

		// Create new filesystem namespace if wanted

	int openfds = getdtablesize();

		// Here we have to handle re-mapping of std(in|out|err)
	if ( m_std ) {

		dprintf ( D_DAEMONCORE, "Re-mapping std(in|out|err) in child.\n");

		for (int i = 0; i < 3; i++) {
			if ( m_std[i] > -1 ) {
				int fd = m_std[i];
				if (fd >= PIPE_INDEX_OFFSET) {
						// this is a DaemonCore pipe we'd like to pass down.
						// replace the std array entry, which is an index into
						// the pipeHandleTable, with the actual file descriptor
					int index = fd - PIPE_INDEX_OFFSET;
					fd = (*daemonCore->pipeHandleTable)[index];
				}
				if ( ( dup2 ( fd, i ) ) == -1 ) {
					dprintf( D_ALWAYS,
							 "dup2 of m_std[%d] failed: %s (%d)\n",
							 i,
							 strerror(errno),
							 errno );
				}
			} else {
					// if we don't want to inherit it, we better close
					// what's there
				close( i );
			}
		}

	} else {
		MyString msg = "Just closed standard file fd(s): ";

			// If we don't want to re-map these, close 'em.

			// NRL 2006-08-10: Why do we do this?  See the comment
			// in daemon_core_main.C before the block of code that
			// closes std in/out/err in main() -- currently, about
			// line 1570.

			// However, make sure that its not in the inherit list first
		int	num_closed = 0;
		int	closed_fds[3];
		for ( int q=0 ; (q<openfds) && (q<3) ; q++ ) {
			bool found = FALSE;
			for ( int k=0 ; k < m_numInheritFds ; k++ ) {
				if ( m_inheritFds[k] == q ) {
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
			int	fd_null = safe_open_wrapper_follow( NULL_FILE, O_RDWR );
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

	// Now remount filesystems with fs_bind option, to give this
	// process per-process tree mount table

	// This requires rootly power
	if (m_fs_remap) {
		int ret = 0;
		if (can_switch_ids()) {
			m_priv_state = set_priv_no_memory_changes(PRIV_ROOT);
#ifdef HAVE_UNSHARE
			int rc = ::unshare(CLONE_NEWNS|CLONE_FS);
			if (rc) {
				dprintf(D_ALWAYS, "Failed to unshare the mount namespace\n");
				ret = write(m_errorpipe[1], &errno, sizeof(errno));
				if (ret < 1) {
					_exit(errno);
				} else {
					_exit(errno);
				}
			}
#else
			dprintf(D_ALWAYS, "Can not remount filesystems because this system does not have unshare(2)\n");
			errno = ENOSYS;
			ret = write(m_errorpipe[1], &errno, sizeof(errno));
			if (ret < 1) {
				_exit(errno);
			} else {
				_exit(errno);
			}
#endif
		} else {
			dprintf(D_ALWAYS, "Not remapping FS as requested, due to lack of privileges.\n");
			m_fs_remap = NULL;
		}
	}

	if (m_fs_remap && m_fs_remap->PerformMappings()) {
		int ret = write(m_errorpipe[1], &errno, sizeof(errno));
		if (ret < 1) {
			_exit(errno);
		} else {
			_exit(errno);
		}
	}

	// And back to normal userness
	if (m_fs_remap) {
		set_priv_no_memory_changes( m_priv_state );
	}


		/* Re-nice ourself */
	if( m_nice_inc > 0 ) {
		if( m_nice_inc > 19 ) {
			m_nice_inc = 19;
		}
		dprintf ( D_DAEMONCORE, "calling nice(%d)\n", m_nice_inc );
		errno = 0;
		int newnice = nice( m_nice_inc );
		/* The standards say that newnice should be m_nice_inc on success and
		 * -1 on failure.  However, implementations vary.  For example, old
		 * Linux glibcs return 0 on success. Checking errno should be safer
		 * (and indeed is recommended by the Linux man page.
		 */
		if(errno != 0) {
			dprintf(D_ALWAYS, "Warning: When attempting to exec a new process, failed to nice(%d): return code: %d, errno: %d %s\n", m_nice_inc, newnice, errno, strerror(errno));
		}
	}

#ifdef HAVE_SCHED_SETAFFINITY
	// Set the processor affinity
	if (m_affinity_mask) {
		cpu_set_t mask;
		CPU_ZERO(&mask);
		for (int i = 1; i < m_affinity_mask[0]; i++) {
			CPU_SET(m_affinity_mask[i], &mask);
		}
		dprintf(D_FULLDEBUG, "Calling sched_setaffinity\n");
		// first argument of pid 0 means self.
#ifdef HAVE_SCHED_SETAFFINITY_2ARG
			// this is the old (rhel3 vintage) interface
		int result = sched_setaffinity(0, &mask);
#else
		int result = sched_setaffinity(0, sizeof(mask), &mask);
#endif
		if (result != 0) {
			dprintf(D_ALWAYS, "Error calling sched_setaffinity: %d\n", errno);
		}
	}
#endif

	if( IsDebugLevel( D_DAEMONCORE ) ) {
			// This MyString is scoped to free itself before the call to
			// exec().  Otherwise, it would be a leak.
		MyString msg = "Printing fds to inherit: ";
		for ( int a=0 ; a<m_numInheritFds ; a++ ) {
			msg += m_inheritFds[a];
			msg += ' ';
		}
		dprintf( D_DAEMONCORE, "%s\n", msg.Value() );
	}

	// Set up the hard limit core size this process should get.
	// Of course, if I'm asking for a limit larger than this process'
	// current hard limit, it will be limited to the current hard limit.
	// This is done here because limit() may do dprintfs and this happens
	// before we lose our rootly powers, if any.
	if (m_core_hard_limit != NULL) {
		limit(RLIMIT_CORE, *m_core_hard_limit, CONDOR_HARD_LIMIT, "max core size");
	}

	if (m_as_hard_limit != 0L) {
		limit(RLIMIT_AS, m_as_hard_limit, CONDOR_HARD_LIMIT, "max virtual adddress space");
	}


	dprintf ( D_DAEMONCORE, "About to exec \"%s\"\n", m_executable_fullpath );

		// !! !! !! !! !! !! !! !! !! !! !! !!
		// !! !! !! !! !! !! !! !! !! !! !! !!
		/* No dprintfs allowed after this! */
		// !! !! !! !! !! !! !! !! !! !! !! !!
		// !! !! !! !! !! !! !! !! !! !! !! !!

	m_no_dprintf_allowed = true;

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
	dprintf_wrapup_fork_child();

	bool found;
	for ( int j=3 ; j < openfds ; j++ ) {
		if ( j == m_errorpipe[1] ) continue; // don't close our errorpipe!

		found = FALSE;
		for ( int k=0 ; k < m_numInheritFds ; k++ ) {
			if ( m_inheritFds[k] == j ) {
				found = TRUE;
				break;
			}
		}

		if( !found ) {
			close( j );
		}
	}

		// now head into the proper priv state...
	if ( m_priv != PRIV_UNKNOWN ) {
			// This is tricky in the case where we share memory with our
			// parent.  It is fine for us to switch privs (parent and child
			// are independent in this respect), but set_priv() does some
			// internal bookkeeping that we don't want, because the parent
			// will see these changes and will get confused.
		set_priv_no_memory_changes( m_priv );

			// the user tracking group ID may also have been set above. unset
			// it here, to restore the parent's state, if we happen to be
			// sharing memory
		unset_user_tracking_gid();

			// From here on, the priv-switching code doesn't know our
			// true priv state (i.e. the priv state we just switched to),
			// because we told it not to make any persistent changes to
			// memory.  That's ok, because we are about to exec ourselves
			// into oblivion anyway.
	}

	if ( m_priv != PRIV_ROOT ) {
			// Final check to make sure we're not root anymore.
		if( getuid() == 0 ) {
			writeExecError(DaemonCore::ERRNO_EXEC_AS_ROOT);
			_exit(4);
		}
	}

		// switch to the cwd now that we are in user priv state
	if ( m_cwd && m_cwd[0] ) {
		if( chdir(m_cwd) == -1 ) {
				// before we exit, make sure our parent knows something
				// went wrong before the exec...
			writeExecError(errno);
			_exit(errno);
		}
	}

		// if we're starting a non-DC process, modify the signal mask.
		// by default (i.e. if sigmask is NULL), we unblock everything
	if ( !m_want_command_port ) {
		const sigset_t* new_mask = m_sigmask;
		sigset_t empty_mask;
		if (new_mask == NULL) {
			sigemptyset(&empty_mask);
			new_mask = &empty_mask;
		}
		if (sigprocmask(SIG_SETMASK, new_mask, NULL) == -1) {
			writeExecError(errno);
			_exit(errno);
		}
	}

#if defined(LINUX) && defined(TDP)
	if( HAS_DCJOBOPT_SUSPEND_ON_EXEC(m_job_opt_mask) ) {
		if(ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {
			writeExecError(errno);
			_exit (errno);
		}
	}
#endif


	pidenvid_optimize_final_env(m_unix_env);

		// and ( finally ) exec:
	int exec_results;
	exec_results =  execve(m_executable_fullpath, m_unix_args, m_unix_env);

	if( exec_results == -1 ) {
			// We no longer have privs to dprintf. :-(.
			// Let's exit with our errno.
			// before we exit, make sure our parent knows something
			// went wrong before the exec...
		writeExecError(errno);
		_exit(errno);
	}
}
#endif

MSC_DISABLE_WARNING(6262) // function uses 62916 bytes of stack
int DaemonCore::Create_Process(
			const char    *executable,
			ArgList const &args,
			priv_state    priv,
			int           reaper_id,
			int           want_command_port,
			Env const     *env,
			const char    *cwd,
			FamilyInfo    *family_info,
			Stream        *sock_inherit_list[],
			int           std[],
			int           fd_inherit_list[],
			int           nice_inc,
			sigset_t      *sigmask,
			int           job_opt_mask,
			size_t        *core_hard_limit,
			int			  *affinity_mask,
			char const    *daemon_sock,
			MyString      *err_return_msg,
			FilesystemRemap *remap,
			long		  as_hard_limit
            )
{
	int i, j;
	char *ptmp;
	int inheritFds[MAX_INHERIT_FDS];
	int numInheritFds = 0;
	MyString executable_buf;
	priv_state current_priv = PRIV_UNKNOWN;

	// Remap our executable and CWD if necessary.
	std::string alt_executable_fullpath, alt_cwd;

	// For automagic DC std pipes.
	int dc_pipe_fds[3][2] = {{-1, -1}, {-1, -1}, {-1, -1}};

	//saved errno (if any) to pass back to caller
	//Currently, only stuff that would be of interest to the user
	//is saved (so the error can be reported in the user-log).
	int return_errno = 0;
	pid_t newpid = FALSE; //return FALSE to caller, by default

	MyString inheritbuf;
		// note that these are on the stack; they go away nicely
		// upon return from this function.
	ReliSock rsock;
	SharedPortEndpoint shared_port_endpoint( daemon_sock );
	SafeSock ssock;
	PidEntry *pidtmp;

	/* this will be the pidfamily ancestor identification information */
	time_t time_of_fork;
	unsigned int mii;
	pid_t forker_pid;
	//Security session ID and key for daemon core processes.  Not used on Solaris.
	std::string session_id;
	MyString privateinheritbuf;

#ifdef WIN32

		// declare these variables early so MSVS doesn't complain
		// about them being declared inside of the goto's below.
	DWORD create_process_flags = 0;
	BOOL inherit_handles = FALSE;
	char *newenv = NULL;
	MyString strArgs;
	MyString args_errors;
	int namelen = 0;
	bool bIs16Bit = FALSE;
	int first_arg_to_copy = 0;
	bool args_success = false;
	const char *extension = NULL;
	bool allow_scripts = true;
	bool batch_file = false;
	bool binary_executable = false;
	CHAR interpreter[MAX_PATH+1];
	MyString description;
	BOOL ok;
	MyString executable_with_exe;	// buffer for executable w/ .exe appended

#else
	int inherit_handles;
	int max_pid_retry = 0;
	static int num_pid_collisions = 0;
	int errorpipe[2];
	MyString executable_fullpath_buf;
	char const *executable_fullpath = executable;
#endif

	bool want_udp = !HAS_DCJOBOPT_NO_UDP(job_opt_mask) && m_wants_dc_udp;

	double runtime = UtcTime::getTimeDouble();
    double create_process_begin_time = runtime;
    double delta_runtime = 0;
	dprintf(D_DAEMONCORE,"In DaemonCore::Create_Process(%s,...)\n",executable ? executable : "NULL");

	// First do whatever error checking we can that is not platform specific

	// check reaper_id validity.  note: reaper id of 0 means no reaper wanted.
	if ( (reaper_id < 0) || (reaper_id > maxReap) ||
		 ((reaper_id > 0) && (reapTable[reaper_id - 1].num == 0)) ) {
		dprintf(D_ALWAYS,"Create_Process: invalid reaper_id\n");
		goto wrapup;
	}

	// check name validity
	if ( !executable ) {
		dprintf(D_ALWAYS,"Create_Process: null name to exec\n");
		goto wrapup;
	}

	inheritbuf.formatstr("%lu ",(unsigned long)mypid);

		// true = Give me a real local address, circumventing
		//  CCB's trickery if present.  As this address is
		//  intended for my own children on the same machine,
		//  this should be safe.
	{
		MyString mysin = InfoCommandSinfulStringMyself(true);
		ASSERT(mysin.Length() > 0); // Empty entry means unparsable string.
		inheritbuf += mysin;
	}

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
                inheritFds[numInheritFds] = tempSock->get_file_desc();
                numInheritFds++;
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
	// the info into the inheritbuf.  If we want a shared command port,
	// do that instead.  Currently, we only want that if we are not
	// given a pre-assigned port and if we have permission to write
	// to the daemon socket directory.
	if ( want_command_port != FALSE && want_command_port <= 1 &&
		 !HAS_DCJOBOPT_NEVER_USE_SHARED_PORT(job_opt_mask) &&
		 SharedPortEndpoint::UseSharedPort() )
	{
		shared_port_endpoint.InitAndReconfig();
		if( !shared_port_endpoint.CreateListener() ) {
			goto wrapup;
		}

		if( !shared_port_endpoint.ChownSocket(priv) ) {
				// The child process will probably not be able to remove the
				// named socket.  That's ok.  We'll try to clean up for
				// it.
		}

		dprintf(D_FULLDEBUG|D_NETWORK,"Created shared port endpoint for child process (%s)\n",shared_port_endpoint.GetSharedPortID());
		inheritbuf += " SharedPort:";
		int fd = -1;
		shared_port_endpoint.serialize(inheritbuf,fd);
        inheritFds[numInheritFds++] = fd;
		inherit_handles = true;
	}
	else if ( want_command_port != FALSE ) {
		inherit_handles = TRUE;
		SafeSock* ssock_ptr = want_udp ? &ssock : NULL;
		if (!InitCommandSockets(want_command_port, &rsock, ssock_ptr, false)) {
				// error messages already printed by InitCommandSockets()
			goto wrapup;
		}

		// now duplicate the underlying SOCKET to make it inheritable
		if ( (!rsock.set_inheritable(TRUE)) ||
			 (m_wants_dc_udp && !ssock.set_inheritable(TRUE)) ) {
			dprintf(D_ALWAYS,"Create_Process:Failed to set command "
					"socks inheritable\n");
			goto wrapup;
		}

		// and now add these new command sockets to the inheritbuf
		inheritbuf += " ";
		ptmp = rsock.serialize();
		inheritbuf += ptmp;
		delete []ptmp;
		if (want_udp) {
			inheritbuf += " ";
			ptmp = ssock.serialize();
			inheritbuf += ptmp;
			delete []ptmp;
		}

            // now put the actual fds into the list of fds to inherit
        inheritFds[numInheritFds++] = rsock.get_file_desc();
		if (want_udp) {
			inheritFds[numInheritFds++] = ssock.get_file_desc();
		}
	}
	inheritbuf += " 0";
	/*
	Due to the belief that on Solaris there are no private environments, the passing of
	security keys through the environment is a bad idea.  Thus we do not attempt this on
	Solaris.
	*/
#ifndef Solaris
	/*
	Currently there is no way to detect if we are starting a daemon core process
	or not.  The closest thing is so far all daemons the master starts will receive
	a command port, whereas non-daemon core processes would not know what to do with
	one.  Since these security sessions are for communication between daemons, this
	is the best that can currently be done to make sure non-daemon core processes
	don't get a session key.
	*/
	if(want_command_port != FALSE)
	{
		char* c_session_id = Condor_Crypt_Base::randomHexKey();
		char* c_session_key = Condor_Crypt_Base::randomHexKey();

		session_id.assign(c_session_id);
		std::string session_key(c_session_key);

		free(c_session_id);
		free(c_session_key);

			// An apparent compiler optimization bug in gcc 3.2.3 is
			// causing session_id.c_str() to return a string that is
			// not null terminated.  It works the first couple times
			// it is called and then writes an uninitialized value from
			// the stack in place of the null.  Therefore, in an attempt
			// to work around this problem, I am calling c_str() once
			// and storing the result for subsequent use.
		char const *session_id_c_str = session_id.c_str();
		char const *session_key_c_str = session_key.c_str();

		bool rc = getSecMan()->CreateNonNegotiatedSecuritySession(
			DAEMON,
			session_id_c_str,
			session_key_c_str,
			NULL,
			CONDOR_CHILD_FQU,
			NULL,
			0);

		if(!rc)
		{
			dprintf(D_ALWAYS, "ERROR: Create_Process failed to create security session for child daemon.\n");
			goto wrapup;
		}
		privateinheritbuf += " SessionKey:";

		MyString session_info;
		rc = getSecMan()->ExportSecSessionInfo(session_id_c_str, session_info);
		if(!rc)
		{
			dprintf(D_ALWAYS, "ERROR: Create_Process failed to export security session for child daemon.\n");
			goto wrapup;
		}
		ClaimIdParser claimId(session_id_c_str, session_info.Value(), session_key_c_str);
		privateinheritbuf += claimId.claimId();
	}
#endif
	// now process fd_inherit_list, which allows the caller the specify
	// arbitrary file descriptors to be passed through to the child process
	// (currently only implemented on UNIX)
	if (fd_inherit_list != NULL) {
#if defined(WIN32)
		EXCEPT("Create_Process: fd_inherit_list specified, "
		           "but not implemented on Windows: programmer error");
#else
		for (int* fd_ptr = fd_inherit_list; *fd_ptr != 0; fd_ptr++) {
			inheritFds[numInheritFds++] = *fd_ptr;
			if (numInheritFds > MAX_INHERIT_FDS) {
				EXCEPT("Create_Process: MAX_INHERIT_FDS (%d) reached",
				       MAX_INHERIT_FDS);
			}
		}
#endif
	}

	/* this stuff ends up in the child's environment to help processes
		identify children/grandchildren/great-grandchildren/etc. */
	create_id(&time_of_fork, &mii);

	// if this is the first time Create_Process is being called with a
	// non-NULL family_info argument, create the ProcFamilyInterface object
	// that we'll use to interact with the procd for controlling process
	// families
	//
	if ((family_info != NULL) && (m_proc_family == NULL)) {
		m_proc_family = ProcFamilyInterface::create(get_mySubSystem()->getName());
		ASSERT(m_proc_family);
	}

		// Before we get into the platform-specific stuff, see if any
		// of the std fds are requesting a DC-managed pipe.  If so, we
		// want to create those pipes now so they can be inherited.
	for (i=0; i<=2; i++) {
		if (std && std[i] == DC_STD_FD_PIPE) {
			if (i == 0) {
				if (!Create_Pipe(dc_pipe_fds[i], false, false, false, true)) {
					dprintf(D_ALWAYS|D_FAILURE, "ERROR: Create_Process: "
							"Can't create DC pipe for stdin.\n");
					goto wrapup;
				}
					// We want to have the child inherit the read end.
				std[i] = dc_pipe_fds[i][0];
			}
			else {
				if (!Create_Pipe(dc_pipe_fds[i], true, false, true)) {
					dprintf(D_ALWAYS|D_FAILURE, "ERROR: Create_Process: "
							"Can't create DC pipe for %s.\n",
							i == 1 ? "stdout" : "stderr");
					goto wrapup;
				}
					// We want to have the child inherit the write end.
				std[i] = dc_pipe_fds[i][1];
			}
		}
	}


#ifdef WIN32
	// START A NEW PROCESS ON WIN32

	STARTUPINFO si;
	PROCESS_INFORMATION piProcess;

	// prepare a STARTUPINFO structure for the new process
	ZeroMemory(&si,sizeof(si));
	si.cb = sizeof(si);

	// process id for the environment ancestor history info
	forker_pid = ::GetCurrentProcessId();

	// we do _not_ want our child to inherit our file descriptors
	// unless explicitly requested.  so, set the underlying handle
	// of all files opened via the C runtime library to non-inheritable.
	//
	// TODO: find a way to do this properly for all types of handles
	// and for the total number of possible handles (and all types: 
	// files, pipes, etc.)... and if it's even required (see if _fopen 
	// sets inherit option... actually inherit option comes from 
	// CreateProcess, we should be enumerating all open handles... 
	// see sysinternals handles app for insights).

    /* TJ: 2011-10-17 this causes the c-runtime to throw an exception!
	for (i = 0; i < 100; i++) {
		SetFDInheritFlag(i,FALSE);
	}
    */

	// handle re-mapping of stdout,in,err if desired.  note we just
	// set all our file handles to non-inheritable, so for any files
	// being redirected, we need to reset to inheritable.
	if ( std ) {
		int valid = FALSE;
		HANDLE *std_handles[3] = {&si.hStdInput, &si.hStdOutput, &si.hStdError};
		for (int ii = 0; ii < 3; ii++) {
			if ( std[ii] > -1 ) {
				if (std[ii] >= PIPE_INDEX_OFFSET) {
					// we are handing down a DaemonCore pipe
					int index = std[ii] - PIPE_INDEX_OFFSET;
					*std_handles[ii] = (*pipeHandleTable)[index]->get_handle();
					SetHandleInformation(*std_handles[ii], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
					valid = TRUE;
				}
				else {
					// we are handing down a C library FD
					SetFDInheritFlag(std[ii],TRUE);	// set handle inheritable
					long longTemp = _get_osfhandle(std[ii]);
					if (longTemp != -1 ) {
						valid = TRUE;
						*std_handles[ii] = (HANDLE)longTemp;
					}
				}
			}
		}
		if ( valid ) {
			si.dwFlags |= STARTF_USESTDHANDLES;
			inherit_handles = TRUE;
		}
	}

	if (family_info != NULL) {
		create_process_flags |= CREATE_NEW_PROCESS_GROUP;
	}

    // Re-nice our child -- on WinNT, this means run it at IDLE process
	// priority class.

	// NOTE: Can't we have a more fine-grained approach than this?  I
	// think there are other priority classes, and we should probably
	// map them to ranges within the 0-20 Unix nice-values... --pfc

    if ( nice_inc > 0 ) {
		// or new_process_group with whatever we set above...
		create_process_flags |= IDLE_PRIORITY_CLASS;
	}


	// Deal with environment.

	// compiler complains about job_environ's initialization being skipped
	// by "goto wrapup", so we start a new block here
	{
		Env job_environ;

			// if not using inheritance, start with default values for
			// PATH and TEMP, othwise just start with our environment
		if ( HAS_DCJOBOPT_NO_ENV_INHERIT(job_opt_mask) ) {

				// add in what is likely the system default path.  we do this
				// here, before merging the user env, because if the user
				// specifies a path in the job ad we want top use that instead.
			MyString path;
			GetEnv("PATH",path);
			if (path.Length()) {
				job_environ.SetEnv("PATH",path.Value());
			}

			// do the same for what likely is the system default TEMP
			// directory.
			MyString temp_path;
			GetEnv("TEMP",temp_path);
			if (temp_path.Length()) {
				job_environ.SetEnv("TEMP",temp_path.Value());
			}
		}
		else {
			char* my_env = GetEnvironmentStrings();
			if (my_env == NULL) {
				dprintf(D_ALWAYS,
				        "GetEnvironmentStrings error: %u\n",
				        GetLastError());
			}
			else {
				job_environ.MergeFrom(my_env);
				if (FreeEnvironmentStrings(my_env) == FALSE) {
					dprintf(D_ALWAYS,
					        "FreeEnvironmentStrings error: %u\n",
					        GetLastError());
				}
			}
		}

			// now add in env vars from user
		if(env) {
			job_environ.MergeFrom(*env);
		}

			// next, add in default system env variables.  we do this after
			// the user vars are in place, because we want the values for
			// these system variables to override whatever the user said.
		const char * default_vars[] = { "SystemDrive", "SystemRoot",
			"COMPUTERNAME", "NUMBER_OF_PROCESSORS", "OS", "COMSPEC",
			"PROCESSOR_ARCHITECTURE", "PROCESSOR_IDENTIFIER",
			"PROCESSOR_LEVEL", "PROCESSOR_REVISION", "PROGRAMFILES", "WINDIR",
			"\0" };		// must end list with NULL string
		int ixvar = 0;
		while ( default_vars[ixvar][0] ) {
			MyString envbuf;
			GetEnv(default_vars[ixvar],envbuf);
			if (envbuf.Length()) {
				job_environ.SetEnv(default_vars[ixvar],envbuf.Value());
			}
			ixvar++;
		}

			// now, add in the inherit buf
		job_environ.SetEnv( EnvGetName( ENV_INHERIT ), inheritbuf.Value() );

		if( !privateinheritbuf.IsEmpty() )
			job_environ.SetEnv( EnvGetName( ENV_PRIVATE ), privateinheritbuf.Value() );

			// and finally, get it all back as a NULL delimited string.
			// remember to deallocate this with delete [] since it will
			// be allocated on the heap with new [].
		newenv = job_environ.getWindowsEnvironmentString();
	}
	// end of dealing with the environment....

	// Check if it's a 16-bit application
	bIs16Bit = false;
	LOADED_IMAGE loaded;
	BOOL map_and_load_result;
	// NOTE (not in MSDN docs): Even when this function fails it still
	// may have "failed" with LastError = "operation completed successfully"
	// and still filled in our structure.  It also might really have
	// failed.  So we init the part of the structure we care about and just
	// ignore the return value for purposes of setting bIs16Bit - we still
	// must honor the return value for purposes of calling UnMapAndLoad() or
	// else risk an access violation upon unmapping.
	loaded.fDOSImage = FALSE;
	map_and_load_result = MapAndLoad((char *)executable, NULL, &loaded, FALSE, TRUE);
	if (loaded.fDOSImage == TRUE)
		bIs16Bit = true;
	if (map_and_load_result)
		UnMapAndLoad(&loaded);

	// Define a some short-hand variables for use bellow
	namelen				= strlen(executable);
	extension			= namelen > 3 ? &(executable[namelen-4]) : NULL;
	batch_file			= ( extension && 
							( MATCH == strcasecmp ( ".bat", extension ) || 
							  MATCH == strcasecmp ( ".cmd", extension ) ) ),
	allow_scripts		= param_boolean ( 
							"ALLOW_SCRIPTS_TO_RUN_AS_EXECUTABLES", true ),
	binary_executable	= ( extension && 
							( MATCH == strcasecmp ( ".exe", extension ) || 
							  MATCH == strcasecmp ( ".com", extension ) ) );

	dprintf (
		D_FULLDEBUG,
		"Create_Process(): executable: '%s'\n",
		executable );
	
	if ( bIs16Bit ) {

		/** CreateProcess() requires different params for 16-bit apps:
			1) NULL for the app name
			2) args begins with app name
			*/

		/** surround the executable name with quotes or you'll 
			have problems when the execute directory contains 
			spaces! */
		strArgs.formatstr ( 
			"\"%s\"",
			executable );
		
		/* make sure we're only using backslashes */
		strArgs.replaceString (
			"/", 
			"\\", 
			0 );

		first_arg_to_copy = 1;
		args_success = args.GetArgsStringWin32 ( 
			&strArgs,
			first_arg_to_copy,
			&args_errors );

		dprintf ( 
			D_ALWAYS, 
			"Executable is 16-bit, "
			"args=%s\n", 
			args );

	} else if ( batch_file ) {

		char systemshell[MAX_PATH+1];

		/** find out where cmd.exe lives on this box and
			set it to our executable */
		UINT length = GetSystemDirectory ( systemshell, MAX_PATH );
		strncat ( systemshell, "\\cmd.exe", MAX_PATH - length - 1 );
		
		/** next, stuff the extra cmd.exe args in with 
			the arguments */
		strArgs.formatstr ( 
			"\"%s\" /Q /C \"%s\"",
			systemshell,
			executable );

		/** store the cmd.exe as the executable */
		executable_buf	= systemshell;
		executable		= executable_buf.Value();

		/** skip argv[0], since it only contains junk and will goof
			up the args to the batch file. */
		first_arg_to_copy = 1;
		args_success = args.GetArgsStringWin32 (
			&strArgs,
			first_arg_to_copy,
			&args_errors);

		dprintf ( 
			D_ALWAYS, 
			"Executable is a batch file, "
			"running: %s\n",
			strArgs.Value () );

	} else if ( allow_scripts && !binary_executable ) {

		/** since we do not actually know how long the extension of
			the file is, we'll need to hunt down the '.' in the path
			*/
		extension = strrchr ( condor_basename(executable), '.' );

		if ( !extension ) {

			dprintf ( 
				D_ALWAYS, 
				"Create_Process(): Failed to extract "
				"the extension from file %s.\n", executable );

			/** don't fail here, since we want executables to run
				as usual.  That is, some condor jobs submit 
				executables that do not have the '.exe' extension,
				but are, nonetheless, executable binaries.  For
				instance, a submit script may contain:
				
				executable = executable$(OPSYS) 
				
				As such, we assume the file is an executable. */
			binary_executable = true;

		} else {

			/** try and find the executable associated with this 
				file extension */
			ok = GetExecutableAndArgumentTemplateByExtention ( 
				extension, 
				interpreter );

			if ( !ok ) {

				dprintf ( 
					D_ALWAYS, 
					"Create_Process(): Failed to find an "
					"executable for extension *%s\n",
					extension );

				/** don't fail here either, for the same reasons we 
					outline above, save a small modification to the 
					executable's name: executable.$(OPSYS).
					As above, we assume the file is an executable. */
				binary_executable = true;

			} else {

				/** add the script to the command-line. The 
					executable is actually the script. */
				strArgs.formatstr (
					"\"%s\" \"%s\"",
					interpreter, 
					executable );
				
				/** change executable to be the interpreter 
					associated with the file type. */
				executable_buf	= interpreter;
				executable		= executable_buf.Value ();

				/** skip argv[0], since it only contains junk and
					will goof up the args to the script. */
				first_arg_to_copy = 1;
				args_success = args.GetArgsStringWin32 (
					&strArgs,
					first_arg_to_copy,
					&args_errors );
				
				dprintf (
					D_FULLDEBUG,
					"Executable is a *%s script, "
					"running: %s\n",
					extension,
					strArgs.Value () );

			}

		}

	} 
	
	/** either we were given an binary executable directly, or one of
		the	above checks determined that the given executable must be
		either a binary executable or garbage. Either way, we treat it
		as a binary and hope for the best. */
	if ( binary_executable && !bIs16Bit ) {

		/** append the arguments given in the submit file. */
		first_arg_to_copy = 0;
		args_success = args.GetArgsStringWin32 (
			&strArgs,
			first_arg_to_copy,
			&args_errors );

	}

	if(!args_success) {
		dprintf(D_ALWAYS, "ERROR: failed to produce Win32 argument string from CreateProcess: %s\n",args_errors.Value());
		goto wrapup;
	}

	BOOL cp_result, gbt_result;
	DWORD binType;
	gbt_result = GetBinaryType(executable, &binType);

	// if GetBinaryType() failed,
	// try an alternate exec pathname (aka perhaps append .exe etc) and 
	// try again. if there is an alternate exec pathname, stash the name
	// in a C++ MyString buffer (so it is deallocated automagically) and
	// change executable to point into that buffer.
	if ( !gbt_result ) {
		char *alt_name = alternate_exec_pathname( executable );
		if ( alt_name ) {
			executable_with_exe = alt_name;
			executable = executable_with_exe.Value();
			free(alt_name);
				// try GetBinaryType again...
			gbt_result = GetBinaryType(executable, &binType);
		}
	}

	// test if the executable is either unexecutable, or if GetBinaryType()
	// thinks its a DOS 16-bit app, but in reality the actual binary
	// image isn't (this happens when the executable is bogus or corrupt).
	if ( !gbt_result || ( binType == SCS_DOS_BINARY && !bIs16Bit) ) {

		dprintf(D_ALWAYS, "ERROR: %s is not a valid Windows executable\n",
			   	executable);
		cp_result = 0;

		goto wrapup;
	} else {
		dprintf(D_FULLDEBUG, "GetBinaryType() returned %d\n", binType);
	}

	// if we want to create a process family for this new process, we
	// will create the process suspended, so we can register it with the
	// procd
	//
	if (family_info != NULL) {
		create_process_flags |= CREATE_SUSPENDED;
	}

	// if we are creating a process with PRIV_USER_FINAL,
	// we need to add flag CREATE_NEW_CONSOLE to be certain the user
	// job starts in a new console windows.  This allows Condor to 
	// find the window when we want to send it a WM_CLOSE
	//
	if ( priv == PRIV_USER_FINAL ) {
		create_process_flags |= CREATE_NEW_CONSOLE;
		si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOWNOACTIVATE;
	}

	const char *cwdBackup;
	if (cwd && (cwd[0] == '\0')) {
		cwdBackup = NULL;
	} else {
		cwdBackup = cwd;
	}
		
    runtime = dc_stats.AddRuntimeSample("DCCreate_Process000", IF_VERBOSEPUB, runtime);

   	if ( priv != PRIV_USER_FINAL || !can_switch_ids() ) {
		cp_result = ::CreateProcess(bIs16Bit ? NULL : executable,(char*)strArgs.Value(),NULL,
			NULL,inherit_handles, create_process_flags,newenv,cwdBackup,&si,&piProcess);

        runtime = dc_stats.AddRuntimeSample("DCCreateProcessW32", IF_VERBOSEPUB, runtime);

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
		if (param_boolean_crufty("USE_VISIBLE_DESKTOP", false)) {
				// user wants visible desktop.
				// place the user_token into the proper access control lists.
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

		cp_result = ::CreateProcessAsUser(user_token,bIs16Bit ? NULL : executable,
			(char *)strArgs.Value(),NULL,NULL, inherit_handles,
			create_process_flags, newenv,cwdBackup,&si,&piProcess);

        runtime = dc_stats.AddRuntimeSample("DCCreateProcessAsUser", IF_VERBOSEPUB, runtime);

		set_priv(s);
	}

	if ( !cp_result ) {
		dprintf(D_ALWAYS,
			"Create_Process: CreateProcess failed, errno=%d\n",GetLastError());
		goto wrapup;
	}

	// save pid info out of piProcess
	//
	newpid = piProcess.dwProcessId;
	
#ifdef HAVE_SCHED_SETAFFINITY
	/* if we have an affinity array mask then: */
	if ( affinity_mask ) {
		
		/* build the Win32 affinity mask ... */
		DWORD_PTR mask = 0;
		for ( int i = 1; i < affinity_mask[0]; ++i ) {
			mask |= ( 1 << affinity_mask[i] );
		}

		dprintf ( 
			D_FULLDEBUG, 
			"Setting process affinity\n" );
		
		/* ... set the process affinity. */
		BOOL ok = SetProcessAffinityMask ( 
			piProcess.hProcess, 
			mask );
		
		if ( !ok ) {

			dprintf ( D_ALWAYS, 
				"Failed to set process affinity. "
				"(last-error=%d)\n", 
				GetLastError () );

			/* This is NOT a fatal error, so
			   continue on... */

		}

	}
#endif

	// if requested, register a process family with the procd and unsuspend
	// the process
	//
	if (family_info != NULL) {
		ASSERT(m_proc_family != NULL);
		bool okrf = Register_Family(newpid,
		                          getpid(),
		                          family_info->max_snapshot_interval,
		                          NULL,
		                          family_info->login,
		                          NULL,
		                          family_info->cgroup,
		                          family_info->glexec_proxy);
		if (!okrf) {
			EXCEPT("error registering process family with procd");
		}
		if (ResumeThread(piProcess.hThread) == (DWORD)-1) {
			EXCEPT("error resuming newly created process: %u",
			       GetLastError());
		}
	}

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
	// Squash compiler warning about inherit_handles being set but not used on Linux
	if (inherit_handles) {}

	// START A NEW PROCESS ON UNIX

		// We have to do some checks on the executable name and the
		// cwd before we fork.  We want to do these in priv state
		// specified, but in the user priv if PRIV_USER_FINAL specified
		// or in the condor priv if PRIV_CONDOR_FINAL is specified.
		// Don't do anything in PRIV_UNKNOWN case.

	if ( priv != PRIV_UNKNOWN ) {
		if ( priv == PRIV_USER_FINAL ) {
			current_priv = set_user_priv();
		} else if ( priv == PRIV_CONDOR_FINAL ) {
			current_priv = set_condor_priv();
		} else {
			current_priv = set_priv( priv );
		}
	}

	// First, check to see that the specified executable exists.
	if( access(executable,F_OK | X_OK) < 0 ) {
		return_errno = errno;
		dprintf( D_ALWAYS, "Create_Process: "
				 "Cannot access specified executable \"%s\": "
				 "errno = %d (%s)\n", executable, errno, strerror(errno) );
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
            if (NULL != err_return_msg) {
                err_return_msg->formatstr("Cannot access specified iwd \"%s\"", cwd);
            }
			dprintf( D_ALWAYS, "Create_Process: "
					 "Cannot access specified iwd \"%s\": "
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

		// if we're given a relative path (in name) AND we want to cwd
		// here, we have to prepend stuff to name make it the full path.
		// Otherwise, we change directory and execv fails.

	if( cwd && (cwd[0] != '\0') ) {

		if ( executable[0] != '/' ) {   // relative path
			MyString currwd;
			if ( !condor_getcwd( currwd ) ) {
				dprintf ( D_ALWAYS, "Create_Process: getcwd failed\n" );
				goto wrapup;
			}

			executable_fullpath_buf.formatstr("%s/%s", currwd.Value(), executable);
			executable_fullpath = executable_fullpath_buf.Value();

				// Finally, log it
			dprintf ( D_DAEMONCORE, "Full path exec name: %s\n", executable_fullpath );
		}

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
	if (pipe(errorpipe) < 0) {
		dprintf(D_ALWAYS,"Create_Process: pipe() failed with errno %d (%s).\n",
				errno, strerror(errno));
		goto wrapup;
	}

	// process id for the environment ancestor history info
	forker_pid = ::getpid();

	// if the caller passed in a signal mask to apply to the child, we
	// block these signals before we fork. then in the parent, we change
	// the mask back to what it was immediately following the fork. in
	// the child, we apply the given mask again (using SIG_SETMASK rather
	// than SIG_BLOCK). the reason we take this extra step here is to
	// avoid the possibility of a signal in the passed-in mask being received
	// by the child before it has a chance to call sigprocmask
	//
	sigset_t saved_mask;
	if (sigmask != NULL) {

		// can't set the signal mask for daemon core processes
		//
		ASSERT(!want_command_port);

		if (sigprocmask(SIG_BLOCK, sigmask, &saved_mask) == -1) {
			dprintf(D_ALWAYS,
			        "Create_Process: sigprocmask error: %s (%d)\n",
			        strerror(errno),
			        errno);
			goto wrapup;
		}
	}

	if (remap) {
		alt_executable_fullpath = remap->RemapFile(executable_fullpath);
		alt_cwd = remap->RemapDir(cwd);
		if (alt_executable_fullpath.compare(executable_fullpath))
			dprintf(D_ALWAYS, "Remapped file: %s\n", alt_executable_fullpath.c_str());
		if (alt_cwd.compare(cwd))
			dprintf(D_ALWAYS, "Remapped cwd: %s\n", alt_cwd.c_str());
	}

	{
			// Create a "forkit" object to hold all the state that we need in the child.
			// In some cases, the "fork" will actually be a clone() operation, which
			// is why we have to package all the state into something we can pass to
			// another function, rather than just doing it all inline here.
		CreateProcessForkit forkit(
			errorpipe,
			args,
			job_opt_mask,
			env,
			inheritbuf,
			privateinheritbuf,
			forker_pid,
			time_of_fork,
			mii,
			family_info,
			alt_cwd.length() ? alt_cwd.c_str() : cwd,
			executable,
			alt_executable_fullpath.length() ? alt_executable_fullpath.c_str() : executable_fullpath,
			std,
			numInheritFds,
			inheritFds,
			nice_inc,
			priv,
			want_command_port,
			sigmask,
			core_hard_limit,
			as_hard_limit,
			affinity_mask,
			remap);

		newpid = forkit.fork_exec();
	}

	if( newpid > 0 ) // fork succeeded
	{
		// if we were told to set the child's signal mask, we ANDed
		// those signals into our own mask right before the fork (see
		// the comment right before the fork). here we set our signal
		// mask back to what it was
		//
		if (sigmask != NULL) {
			if (sigprocmask(SIG_SETMASK, &saved_mask, NULL) == -1) {
				EXCEPT("Create_Process: sigprocmask error: %s (%d)\n",
				       strerror(errno),
				       errno);
			}
		}

			// close the write end of our error pipe
		close(errorpipe[1]);

			// read the tracking gid from the child.
		gid_t child_tracking_gid = 0;
		int tracking_gid_rc = full_read(errorpipe[0], &child_tracking_gid, sizeof(gid_t));
		if( tracking_gid_rc != sizeof(gid_t)) {
				// This should only happen if our child process died
				// before writing to the pipe.  So it cannot have
				// called exec(), because it always writes to the pipe
				// before calling exec.
			dprintf(D_ALWAYS,"Error: Create_Process(%s): failed to read child tracking gid: rc=%d, gid=%d, errno=%d %s.\n",
					executable,tracking_gid_rc,child_tracking_gid,errno,strerror(errno));

			int child_status;
			waitpid(newpid, &child_status, 0);
			close(errorpipe[0]);
			newpid = FALSE;
			goto wrapup;
		}

			// check our error pipe for any problems before the exec
		int child_errno = 0;
		if (full_read(errorpipe[0], &child_errno, sizeof(int)) == sizeof(int)) {
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
				dprintf( D_ALWAYS, "Create_Process(%s): child "
						 "failed because %s process was still root "
						 "before exec()\n",
						 executable,
						 priv_to_string(priv) );
				break;

			case ERRNO_REGISTRATION_FAILED:
				dprintf( D_ALWAYS, "Create_Process(%s): child "
						 "failed because it failed to register itself "
						 "with the ProcD\n",
						 executable );
				break;

			case ERRNO_EXIT:
				dprintf( D_ALWAYS, "Create_Process(%s): child "
						 "failed because it called exit(%d).\n",
						 executable,
						 child_status );
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
				dprintf( D_ALWAYS, "Create_Process(%s): child "
						 "failed because PID %d is still in use by "
						 "DaemonCore\n",
						 executable,
						 (int)newpid );
				num_pid_collisions++;
				max_pid_retry = param_integer( "MAX_PID_COLLISION_RETRY",
											   DEFAULT_MAX_PID_COLLISIONS );
				if( num_pid_collisions > max_pid_retry ) {
					dprintf( D_ALWAYS, "Create_Process: ERROR: we've had "
							 "%d consecutive pid collisions, giving up! "
							 "(%d PIDs being tracked internally.)\n",
							 num_pid_collisions, pidTable->getNumElements() );
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
					// we also need to close the command sockets we've
					// got open sitting on our stack, so that if we're
					// trying to spawn using a fixed port, we won't
					// still be holding the port open in this lower
					// stack frame...
				rsock.close();
				ssock.close();
				dprintf( D_ALWAYS, "Re-trying Create_Process() to avoid "
						 "PID re-use\n" );
				return Create_Process( executable, args, priv, reaper_id,
				                       want_command_port, env, cwd,
				                       family_info,
				                       sock_inherit_list, std, fd_inherit_list,
				                       nice_inc, sigmask, job_opt_mask );
				break;

			default:
				dprintf( D_ALWAYS, "Create_Process(%s): child "
						 "failed with errno %d (%s) before exec()\n",
						 executable,
						 errno,
						 strerror(errno) );
				break;

			}
			close(errorpipe[0]);
			newpid = FALSE;
			goto wrapup;
		}
		close(errorpipe[0]);

#if defined(LINUX)
		if( family_info && family_info->group_ptr ) {
				// pass the tracking gid back to our caller
				// (Currently, we only get here in the starter.)
				// By the time we get here, we know exec succeeded,
				// so the tracking gid should never be group 0.
			ASSERT( child_tracking_gid != 0 );
			*(family_info->group_ptr) = child_tracking_gid;
		}
#endif

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
				dprintf(D_ALWAYS, 
					"Create_Process wait for '%s' failed: %d (%s)\n",
					executable,
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
		dprintf(D_ALWAYS, "Create Process: fork() for '%s' "
				"failed: %s (%d)\n",
				executable,
				strerror(errno), errno );
		close(errorpipe[0]); close(errorpipe[1]);
		newpid = FALSE;
		goto wrapup;
	}
#endif

	// Now that we have a child, store the info in our pidTable
	pidtmp = new PidEntry;
	pidtmp->pid = newpid;
	pidtmp->new_process_group = (family_info != NULL);

	{
		char const *shared_port_addr = shared_port_endpoint.GetMyRemoteAddress();
		if( !shared_port_addr ) {
			shared_port_addr = shared_port_endpoint.GetMyLocalAddress();
		}

		if( shared_port_addr ) {
			pidtmp->sinful_string = shared_port_addr;
				// we will clean up the socket if the child doesn't
			pidtmp->shared_port_fname = shared_port_endpoint.GetSocketFileName();
		}
		else if ( want_command_port != FALSE ) {
			Sinful sinful(sock_to_string(rsock._sock));
			if( !want_udp ) {
				sinful.setNoUDP(true);
			}
			pidtmp->sinful_string = sinful.getSinful();
		}
	}

	pidtmp->is_local = TRUE;
	pidtmp->parent_is_local = TRUE;
	pidtmp->reaper_id = reaper_id;
	pidtmp->hung_tid = -1;
	pidtmp->was_not_responding = FALSE;
	if(!session_id.empty())
	{
		pidtmp->child_session_id = strdup(session_id.c_str());
	}
#ifdef WIN32
	pidtmp->hProcess = piProcess.hProcess;
	pidtmp->hThread = piProcess.hThread;
	pidtmp->pipeEnd = NULL;
	pidtmp->tid = piProcess.dwThreadId;
	pidtmp->hWnd = 0;
	pidtmp->pipeReady = 0;
	pidtmp->deallocate = 0;
#endif 

		// Leave named socket in place so child can continue to use it.
	shared_port_endpoint.Detach();

		// Now, handle the DC-managed std pipes, if any.
	for (i=0; i<=2; i++) {
		if (dc_pipe_fds[i][0] != -1) {
				// We made a DC pipe, so close the end we don't need,
				// and stash the end we care about in the PidEntry.
			if (i == 0) {
					// For stdin, we close our copy of the read end
					// and stash the write end.
				Close_Pipe(dc_pipe_fds[i][0]);
				pidtmp->std_pipes[i] = dc_pipe_fds[i][1];
			}
			else {
					// std(out|err) is reversed: we close our copy of
					// the write end and stash the read end.
				Close_Pipe(dc_pipe_fds[i][1]);
				pidtmp->std_pipes[i] = dc_pipe_fds[i][0];
				const char* pipe_desc;
				const char* pipe_handler_desc;
				if (i == 1) {
					pipe_desc = "DC stdout pipe";
					pipe_handler_desc = "DC stdout pipe handler";
				}
				else {
					pipe_desc = "DC stderr pipe";
					pipe_handler_desc = "DC stderr pipe handler";
				}
				Register_Pipe(dc_pipe_fds[i][0], pipe_desc,
					  static_cast<PipeHandlercpp>(&DaemonCore::PidEntry::pipeHandler),
					  pipe_handler_desc, pidtmp);
			}
				// Either way, we stashed/closed as needed, so clear
				// out the records in dc_pipe_fds so we don't try to
				// clean these up again during wrapup.
			dc_pipe_fds[i][0] = dc_pipe_fds[i][1] = DC_STD_FD_NOPIPE;
		}
	}

	/* remember the family history of the new pid */
	pidenvid_init(&pidtmp->penvid);
	if (pidenvid_filter_and_insert(&pidtmp->penvid, GetEnviron()) !=
		PIDENVID_OK)
	{
		EXCEPT( "Create_Process: More ancestor environment IDs found than "
				"PIDENVID_MAX which is currently %d. Programmer Error.",
				PIDENVID_MAX );
	}
	if (pidenvid_append_direct(&pidtmp->penvid, 
			forker_pid, newpid, time_of_fork, mii) == PIDENVID_OVERSIZED)
	{
		EXCEPT( "Create_Process: Cannot add child pid to PidEnvID table "
				"because there aren't enough entries. PIDENVID_MAX is "
				"currently %d! Programmer Error.", PIDENVID_MAX );
	}

	/* add it to the pid table */
	{
	   int insert_result = pidTable->insert(newpid,pidtmp);
	   assert( insert_result == 0);
	}

#if !defined(WIN32)
	// here, we do any parent-side work needed to register the new process
	// with our ProcFamily logic
	//
	if ((family_info != NULL) && !m_proc_family->register_from_child()) {
		Register_Family(newpid,
		                getpid(),
		                family_info->max_snapshot_interval,
		                &pidtmp->penvid,
		                family_info->login,
		                NULL,
				family_info->cgroup,
		                family_info->glexec_proxy);
	}
#endif

    runtime = UtcTime::getTimeDouble();
    delta_runtime = (runtime - create_process_begin_time);
	dprintf(D_DAEMONCORE,
		"Child Process: pid %lu at %s (%.2f sec)\n",
		(unsigned long)newpid, 
        pidtmp->sinful_string.Value(), 
        delta_runtime);
#ifdef WIN32
	WatchPid(pidtmp);
#endif

	// Now that child exists, we (the parent) should close up our copy of
	// the childs command listen cedar sockets.  Since these are on
	// the stack (rsock and ssock), they will get closed when we return.

 wrapup:

#ifndef WIN32
		// if we're here, it means we did NOT have a pid collision, or
		// we had too many and gave up.  either way, we should clear
		// out our static counter.
	num_pid_collisions = 0;
#else
	if(newenv) {
		delete [] newenv;
	}
#endif

		// If we created any pipes for this process and didn't yet
		// close them or stash them in the PidEntry, close them now.
	for (i=0; i<=2; i++) {
		for (j=0; j<=1; j++) {
			if (dc_pipe_fds[i][j] != -1) {
				Close_Pipe(dc_pipe_fds[i][j]);
			}
		}
	}

    runtime = dc_stats.AddRuntimeSample("DCCreate_Process001", IF_VERBOSEPUB, runtime);
    runtime = dc_stats.AddRuntimeSample("DCCreate_ProcessTot", IF_VERBOSEPUB, create_process_begin_time);
    if ((runtime - create_process_begin_time) > delta_runtime + 0.5) {
	   dprintf(D_DAEMONCORE,
           "Warning: cleanup from Create_Process took %.3f sec, Create_Process took %.3f sec overall\n",
           (runtime - create_process_begin_time) - delta_runtime,
           (runtime - create_process_begin_time));
    }

	errno = return_errno;
	return newpid;
}
MSC_RESTORE_WARNING(6262) // warn when function uses > 16k stack

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

class FakeCreateThreadReaperCaller: public Service {
public:
	FakeCreateThreadReaperCaller(int exit_status,int reaper_id);

	void CallReaper();

	int FakeThreadID() { return m_tid; }

private:
	int m_tid; // timer id
	int m_exit_status;
	int m_reaper_id;
};

FakeCreateThreadReaperCaller::FakeCreateThreadReaperCaller(int exit_status,int reaper_id):
	m_exit_status(exit_status), m_reaper_id(reaper_id)
{
		// We cannot call the reaper right away, because the caller of
		// Create_Thread doesn't yet know the "thread id".  Therefore,
		// register a timer that will call the reaper.
	m_tid = daemonCore->Register_Timer(
		0,
		(TimerHandlercpp)&FakeCreateThreadReaperCaller::CallReaper,
		"FakeCreateThreadReaperCaller::CallReaper()",
		this );

	ASSERT( m_tid >= 0 );
}

void
FakeCreateThreadReaperCaller::CallReaper() {
	daemonCore->CallReaper( m_reaper_id, "fake thread", m_tid, m_exit_status );
	delete this;
}

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

	if( DoFakeCreateThread() ) {
			// Rather than creating a thread (or fork), we have been
			// configured to just call the function directly in the
			// current process, and then register a timer to call the
			// reaper.

		// need to copy the sock because our caller is going to delete/close it
		Stream *s = sock ? sock->CloneStream() : (Stream *)NULL;

		priv_state saved_priv = get_priv();
		int exit_status = start_func(arg,s);

		if (s) delete s;
#ifndef WIN32
			// In unix, we need to make exit_status like wait waitpid() returns
		exit_status = exit_status<<8;
#endif

		priv_state new_priv = get_priv();
		if( saved_priv != new_priv ) {
			char const *reaper = reapTable[reaper_id-1].handler_descrip;
			dprintf(D_ALWAYS,
					"Create_Thread: UNEXPECTED: priv state changed "
					"during worker function: %d %d (%s)\n",
					(int)saved_priv, (int)new_priv,
					reaper ? reaper : "no reaper" );
			set_priv(saved_priv);
		}

		FakeCreateThreadReaperCaller *reaper_caller =
			new FakeCreateThreadReaperCaller( exit_status, reaper_id );

		return reaper_caller->FakeThreadID();
	}

	// Before we create the thread, call InfoCommandSinfulString once.
	// This makes certain that InfoCommandSinfulString has allocated its
	// buffer which will make it thread safe when called from SendSignal().
	(void)InfoCommandSinfulString();

#ifdef WIN32
	unsigned tid = 0;
	HANDLE hThread = NULL;
	priv_state priv;
	// need to copy the sock because our caller is going to delete/close it
	Stream *s = sock ? sock->CloneStream() : (Stream *)NULL;

	thread_info *tinfo = (thread_info *)malloc(sizeof(thread_info));
	ASSERT( tinfo );
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

		dprintf_init_fork_child();

		pid_t pid = ::getpid();
		PidEntry* pidinfo = NULL;
        if( (pidTable->lookup(pid, pidinfo) >= 0) ) {
                // we've already got this pid in our table! we've got
                // to bail out immediately so our parent can retry.
            int child_errno = ERRNO_PID_COLLISION;
            int ret = write(errorpipe[1], &child_errno, sizeof(child_errno));
			close( errorpipe[1] );
			if (ret < 1) {
				exit(4);
			} else {
				exit(4);
			}
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
						 "%d consecutive pid collisions, giving up! "
						 "(%d PIDs being tracked internally.)\n",
						 num_pid_collisions, pidTable->getNumElements() );
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
        close( errorpipe[0] );
        close( errorpipe[1] );
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
	pidtmp->pipeEnd = NULL;
	pidtmp->tid = tid;
	pidtmp->hWnd = 0;
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

int
DaemonCore::Get_Family_Usage(pid_t pid, ProcFamilyUsage& usage, bool full)
{
	ASSERT(m_proc_family != NULL);
	return m_proc_family->get_usage(pid, usage, full);
}

int
DaemonCore::Suspend_Family(pid_t pid)
{
	ASSERT(m_proc_family != NULL);
	return m_proc_family->suspend_family(pid);
}

int
DaemonCore::Continue_Family(pid_t pid)
{
	ASSERT(m_proc_family != NULL);
	return m_proc_family->continue_family(pid);
}

int
DaemonCore::Kill_Family(pid_t pid)
{
	ASSERT(m_proc_family != NULL);
	return m_proc_family->kill_family(pid);
}

int
DaemonCore::Signal_Process(pid_t pid, int sig)
{
	ASSERT(m_proc_family != NULL);
	dprintf(D_ALWAYS, "sending signal %d to process with pid %u\n",sig,pid);
	return m_proc_family->signal_process(pid,sig);
}

void
DaemonCore::Proc_Family_Init()
{
	if (m_proc_family == NULL) {
		m_proc_family = ProcFamilyInterface::create(get_mySubSystem()->getName());
		ASSERT(m_proc_family);
	}
}

void
DaemonCore::Proc_Family_Cleanup()
{
	if (m_proc_family) {
		delete m_proc_family;
		m_proc_family = NULL;
	}
}

void
DaemonCore::Inherit( void )
{
	char *inheritbuf = NULL;
	int numInheritedSocks = 0;
	char *ptmp;
	static bool already_inherited = false;

	if( already_inherited ) {
		return;
	}
	already_inherited = true;

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

	StringList inherit_list(inheritbuf," ");
	if ( inheritbuf != NULL ) {
		free( inheritbuf );
		inheritbuf = NULL;
	}
	inherit_list.rewind();
	if ( (ptmp=inherit_list.next()) != NULL && *ptmp ) {
		// we read out CONDOR__INHERIT ok, ptmp is now first item

		// insert ppid into table
		dprintf(D_DAEMONCORE,"Parent PID = %s\n",ptmp);
		ppid = atoi(ptmp);
		PidEntry *pidtmp = new PidEntry;
		pidtmp->pid = ppid;
		ptmp=inherit_list.next();
		dprintf(D_DAEMONCORE,"Parent Command Sock = %s\n",ptmp);
		pidtmp->sinful_string = ptmp;
		pidtmp->is_local = TRUE;
		pidtmp->parent_is_local = TRUE;
		pidtmp->reaper_id = 0;
		pidtmp->hung_tid = -1;
		pidtmp->was_not_responding = FALSE;
#ifdef WIN32
		pidtmp->deallocate = 0L;

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
		pidtmp->pipeEnd = NULL;
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
		ptmp=inherit_list.next();
		while ( ptmp && (*ptmp != '0') ) {
			if (numInheritedSocks >= MAX_SOCKS_INHERITED) {
				EXCEPT("MAX_SOCKS_INHERITED reached.");
			}
			switch ( *ptmp ) {
				case '1' :
					// inherit a relisock
					dc_rsock = new ReliSock();
					ptmp=inherit_list.next();
					dc_rsock->serialize(ptmp);
					dc_rsock->set_inheritable(FALSE);
					dprintf(D_DAEMONCORE,"Inherited a ReliSock\n");
					// place into array...
					inheritedSocks[numInheritedSocks++] = (Stream *)dc_rsock;
					break;
				case '2':
					dc_ssock = new SafeSock();
					ptmp=inherit_list.next();
					dc_ssock->serialize(ptmp);
					dc_ssock->set_inheritable(FALSE);
					dprintf(D_DAEMONCORE,"Inherited a SafeSock\n");
					// place into array...
					inheritedSocks[numInheritedSocks++] = (Stream *)dc_ssock;
					break;
				default:
					EXCEPT("Daemoncore: Can only inherit SafeSock or ReliSocks, not %c (%d)", *ptmp, (int)*ptmp);
					break;
			} // end of switch
			ptmp=inherit_list.next();
		}
		inheritedSocks[numInheritedSocks] = NULL;

		// inherit our "command" cedar socks.  they are sent
		// relisock, then safesock, then a "0".
		// we then register rsock and ssock as command sockets below...
		dc_rsock = NULL;
		dc_ssock = NULL;
		ptmp=inherit_list.next();
		if( ptmp && strncmp(ptmp,"SharedPort:",11)==0 ) {
			ptmp += 11;
			delete m_shared_port_endpoint;
			m_shared_port_endpoint = new SharedPortEndpoint();
			dprintf(D_DAEMONCORE, "Inheriting a shared port pipe.\n");
			m_shared_port_endpoint->deserialize(ptmp);
			ptmp=inherit_list.next();
		}
		if ( ptmp && (strcmp(ptmp,"0") != 0) ) {
			dprintf(D_DAEMONCORE,"Inheriting Command Sockets\n");
			dc_rsock = new ReliSock();
			((ReliSock *)dc_rsock)->serialize(ptmp);
			dc_rsock->set_inheritable(FALSE);
			ptmp=inherit_list.next();
		}
		if ( ptmp && (strcmp(ptmp,"0") != 0) ) {
			if( !m_wants_dc_udp_self ) {
					// we don't want a UDP command socket, but our parent
					// made one for us, because it didn't know any better
				Sock::close_serialized_socket(ptmp);
				dprintf(D_DAEMONCORE,"Removing inherited UDP command socket.\n");
			}
			else {
				dc_ssock = new SafeSock();
				dc_ssock->serialize(ptmp);
				dc_ssock->set_inheritable(FALSE);
			}

			ptmp=inherit_list.next();
		}
	}	// end of if we read out CONDOR_INHERIT ok
	/*
	This environment variable is never set on Solaris so
	don't check for it.  Since environments are apparently
	public on Solaris, we can't use it to securely pass
	information around.
	*/
#ifndef Solaris
	const char *privEnvName = EnvGetName( ENV_PRIVATE );
	const char *privTmp = GetEnv( privEnvName );
	if ( privTmp != NULL ) {
		dprintf ( D_DAEMONCORE, "Processing %s from parent\n", privEnvName );
	}
	if(!privTmp)
	{
		return;
		}

	StringList private_list(privTmp, " ");
	UnsetEnv( privEnvName );

	private_list.rewind();
	while((ptmp = private_list.next()) != NULL)
	{
		if( ptmp && strncmp(ptmp,"SessionKey:",11)==0 ) {
			dprintf(D_DAEMONCORE, "Removing session key.\n");
			ClaimIdParser claimid(ptmp+11);
			bool rc = getSecMan()->CreateNonNegotiatedSecuritySession(
				DAEMON,
				claimid.secSessionId(),
				claimid.secSessionKey(),
				claimid.secSessionInfo(),
				CONDOR_PARENT_FQU,
				NULL,
				0);
			if(!rc)
			{
				dprintf(D_ALWAYS, "Error: Failed to recreate security session in child daemon.\n");
			}
			IpVerify* ipv = getSecMan()->getIpVerify();
			MyString id;
			id.formatstr("%s", CONDOR_PARENT_FQU);
			ipv->PunchHole(DAEMON, id);
		}
	}
#endif
}

void
DaemonCore::SetDaemonSockName( char const *sock_name )
{
	m_daemon_sock_name = sock_name;
}

void
DaemonCore::InitDCCommandSocket( int command_port )
{
	if( command_port == 0 ) {
			// No command port wanted, just bail.
		dprintf( D_ALWAYS, "DaemonCore: No command port requested.\n" );
		return;
	}

	dprintf( D_DAEMONCORE, "Setting up command socket\n" );

		// First, try to inherit the sockets from our parent.
	Inherit();

		// If we are using a shared listener port, set that up.
	InitSharedPort(true);

		// If dc_rsock/dc_ssock are still NULL, we need to create our
		// own udp and tcp sockets, bind them, etc.
	if( !m_shared_port_endpoint && (dc_rsock == NULL || (m_wants_dc_udp_self && dc_ssock == NULL)) ) {
		if( !dc_rsock ) {
			dc_rsock = new ReliSock;
		}
		if( !dc_rsock ) {
			EXCEPT( "Unable to create command Relisock" );
		}
		if (m_wants_dc_udp_self) {
			if( !dc_ssock ) {
				dc_ssock = new SafeSock;
			}
			if( !dc_ssock ) {
				EXCEPT( "Unable to create command SafeSock" );
			}
		}
		else {
			ASSERT(dc_ssock == NULL);
		}
			// Final bool indicates any error should be considered fatal.
		InitCommandSockets(command_port, dc_rsock, dc_ssock, true);
	}

		// If we are the collector, increase the socket buffer size.  This
		// helps minimize the number of updates (UDP packets) the collector
		// drops on the floor.
	if( get_mySubSystem()->isType(SUBSYSTEM_TYPE_COLLECTOR) ) {
		int desired_size;

			// Dynamically construct the log message.
		MyString msg;

		if (dc_ssock) {
				// set the UDP (ssock) read size to be large, so we do
				// not drop incoming updates.
			desired_size = param_integer("COLLECTOR_SOCKET_BUFSIZE",
										 10000 * 1024, 1024);
			int final_udp = dc_ssock->set_os_buffers(desired_size);
			msg += (int)(final_udp / 1024);
			msg += "k (UDP), ";
		}

			// and also set the outgoing TCP write size to be large so the
			// collector is not blocked on the network when answering queries
		if( dc_rsock ) {
			desired_size = param_integer("COLLECTOR_TCP_SOCKET_BUFSIZE",
										 128 * 1024, 1024 );
			int final_tcp = dc_rsock->set_os_buffers( desired_size, true );

			msg += (int)(final_tcp / 1024);
			msg += "k (TCP)";
		}
		if( !msg.IsEmpty() ) {
			dprintf(D_FULLDEBUG,
					"Reset OS socket buffer size to %s\n", msg.Value());
		}
	}

		// now register these new command sockets.
		// Note: In other parts of the code, we assume that the
		// first command socket registered is TCP, so we must
		// register the rsock socket first.
	if( dc_rsock ) {
		Register_Command_Socket( (Stream*)dc_rsock );
	}
	if (dc_ssock) {
		Register_Command_Socket( (Stream*)dc_ssock );
	}
	char const *addr = publicNetworkIpAddr();
	if( addr ) {
		dprintf( D_ALWAYS,"DaemonCore: command socket at %s\n", addr );
	}
	char const *priv_addr = privateNetworkIpAddr();
	if( priv_addr ) {
		dprintf( D_ALWAYS,"DaemonCore: private command socket at %s\n", priv_addr );
	}

	if( dc_rsock && m_shared_port_endpoint ) {
			// SOAP-enabled daemons may have both a shared port and
			// a fixed TCP port for receiving SOAP commands
		dprintf( D_ALWAYS,"DaemonCore: non-shared command socket at %s\n",
				 dc_rsock->get_sinful() );
	}

	if (!dc_ssock) {
		dprintf( D_FULLDEBUG, "DaemonCore: UDP Command socket not created.\n");
	}

		// check if our command socket is on 127.0.0.1, and spit out a
		// warning if it is, since it probably means that /etc/hosts
		// is misconfigured [to preempt RUST like rust-admin #2915]

	if( dc_rsock ) {
		if ( dc_rsock->my_addr().is_loopback() ) {
			dprintf( D_ALWAYS, "WARNING: Condor is running on the loopback address (127.0.0.1)\n" );
			dprintf( D_ALWAYS, "         of this machine, and is not visible to other hosts!\n" );
		}
	}

		// Now, drop this sinful string into a file, if
		// mySubSystem_ADDRESS_FILE is defined.
	drop_addr_file();

		// now register any DaemonCore "default" handlers

	static int already_registered = false;
	if( !already_registered ) {
		already_registered = true;

			// register the command handler to take care of signals
		daemonCore->Register_Command( DC_RAISESIGNAL, "DC_RAISESIGNAL",
			(CommandHandlercpp)&DaemonCore::HandleSigCommand,
			"HandleSigCommand()", daemonCore, DAEMON );

			// this handler receives keepalive pings from our children, so
			// we can detect if any of our kids are hung.
		daemonCore->Register_Command( DC_CHILDALIVE,"DC_CHILDALIVE",
			(CommandHandlercpp)&DaemonCore::HandleChildAliveCommand,
			"HandleChildAliveCommand", daemonCore, DAEMON,
			D_FULLDEBUG );
	}
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
DaemonCore::HandleDC_SERVICEWAITPIDS(int)
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
				// deallocate flag was set.  call set_unregistered on the
				// PipeEnd and then remove this pentry
				// from our watch list.  Cancel_Pipe will take care of
				// the rest
				entry->pidentries[i]->pipeEnd->set_unregistered();
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
				
				// This is a pipe. We use overlapped I/O to similate the
				// semantics of select. This is all handled by the
				// PipeEnd classes. (see pipe.WIN32.[Ch])
				hKids[numentries] = entry->pidentries[i]->pipeEnd->pre_wait();
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
	result = WAIT_TIMEOUT;
	// if we are no longer watching anything (but we still have a signal
	// to send), simulate the wait function returning WAIT_TIMEOUT
	if (numentries) {
		result = ::WaitForMultipleObjects(numentries + 1, hKids, FALSE, 0);
	}
	if ( result == WAIT_TIMEOUT ) {
			// our poll saw nothing.  so if need to wake up the main thread
			// out of select, do so now before we block waiting for another event.
			// if must_send_signal flag is set, that means we must wake up the
			// main thread.
		
		bool notify_failed;
		while ( must_send_signal ) {
			// Eventually, we should just call SendSignal for this.
			// But for now, handle it all here.

			::EnterCriticalSection(&Big_fat_mutex); // enter big fat mutex
#if 0  // this code replaced by call to Do_Wake_up_select() below
				// send a NOP command to wake up select()
			Daemon d( DT_ANY, daemonCore->privateNetworkIpAddr() );
	        SafeSock ssock;
			ReliSock rsock;
			Sock &sock = (d.hasUDPCommandPort() && daemonCore->dc_ssock) ?
				*(Sock *)&ssock : *(Sock *)&rsock;
				// Use raw command protocol to avoid blocking on ourself.
			notify_failed = 
				!d.connectSock(&sock,1) ||
				!d.startCommand(DC_NOP, &sock, 1, NULL, "DC_NOP", true) ||
				!sock.end_of_message();
#else
//#pragma REMIND("TJ: remove this dead code.")
#endif
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

			// now wakeup the main thread so it notices our changes.
			// we use Do_Wake_up rather than Wake_up because we know
			// we aren't the main thread, but we don't know if condor_threads
			// was ever initialized. the latter function will not wake in that case.
			notify_failed = ! daemonCore->Do_Wake_up_select();

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
		if (numentries == 0) {
			// now there's nothing left to do if we are no longer
			// watching anything
			return TRUE;
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
		if ( entry->pidentries[result]->pipeEnd ) {
			exited_pid = 0;
			if (entry->pidentries[result]->deallocate) {
				// this entry should be deallocated.  set things up so
				// it will be done at the top of the loop; no need to send
				// a signal to break out of select in the main thread, so we
				// explicitly do NOT set the must_send_signal flag here.
				last_pidentry_exited = MAXIMUM_WAIT_OBJECTS + 5;
			} else {
				// pipe is ready and has not been deallocated.
				if (entry->pidentries[result]->pipeEnd->post_wait()) {
					// the handler is ready to be fired
					InterlockedExchange(&(entry->pidentries[result]->pipeReady),1L);
					must_send_signal = true;
				}
				else {
					// not ready yet...
					last_pidentry_exited = MAXIMUM_WAIT_OBJECTS + 5;
				}
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

		// we will no longer be watching this PidEntry, so detach
		// it from our watcherEvent
		entry->pidentries[result]->watcherEvent = NULL;

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

	// If this PidEntry actually represents a pipe, we tell its
	// PipeEnd object that it will now be managed by a PID-watcher
	if (pidentry->pipeEnd) {
		pidentry->pipeEnd->set_watched();
	}

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
	unsigned threadId = 0;
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


void
DaemonCore::CallReaper(int reaper_id, char const *whatexited, pid_t pid, int exit_status)
{
	ReapEnt *reaper = NULL;

	if( reaper_id > 0 ) {
		reaper = &(reapTable[reaper_id-1]);
	}
	if( !reaper || !(reaper->handler || reaper->handlercpp) ) {
			// no registered reaper
			dprintf(D_DAEMONCORE,
			"DaemonCore: %s %lu exited with status %d; no registered reaper\n",
				whatexited, (unsigned long)pid, exit_status);
		return;
	}

		// Set curr_dataptr for Get/SetDataPtr()
	curr_dataptr = &(reaper->data_ptr);

		// Log a message
	const char *hdescrip = reaper->handler_descrip;
	if ( !hdescrip ) {
		hdescrip = EMPTY_DESCRIP;
	}
	dprintf(D_COMMAND,
		"DaemonCore: %s %lu exited with status %d, invoking reaper "
		"%d <%s>\n",
		whatexited, (unsigned long)pid, exit_status, reaper_id, hdescrip);

	if ( reaper->handler ) {
		// a C handler
		(*(reaper->handler))(reaper->service,pid,exit_status);
	}
	else if ( reaper->handlercpp ) {
		// a C++ handler
		(reaper->service->*(reaper->handlercpp))(pid,exit_status);
	}

	dprintf(D_COMMAND,
			"DaemonCore: return from reaper for pid %lu\n", (unsigned long)pid);

		// Make sure we didn't leak our priv state
	CheckPrivState();

	// Clear curr_dataptr
	curr_dataptr = NULL;
}

// This function gets calls with the pid of a process which just exited.
// On Unix, the exit_status is also provided; on NT, we need to fetch
// the exit status here.  Then we call any registered reaper for this process.
int DaemonCore::HandleProcessExit(pid_t pid, int exit_status)
{
	PidEntry* pidentry;
	const char *whatexited = "pid";	// could be changed to "tid"
	int i;

	// Fetch the PidEntry for this pid from our hash table.
	if ( pidTable->lookup(pid,pidentry) == -1 ) {

		if( defaultReaper!=-1 ) {
			pidentry = new PidEntry;
			ASSERT(pidentry);
			pidentry->parent_is_local = TRUE;
			pidentry->reaper_id = defaultReaper;
			pidentry->hung_tid = -1;
			pidentry->new_process_group = FALSE;
		} else {

			// we did not find this pid... probably popen finished.
			// log a message and return FALSE.

			dprintf(D_DAEMONCORE,
				"Unknown process exited (popen?) - pid=%d\n",pid);
			return FALSE;
		}
	}

	// If this process has DC-managed pipes attached to stdout or
	// stderr and those are still open, read them one last time.
	for (i=1; i<=2; i++) {
		if (pidentry->std_pipes[i] != DC_STD_FD_NOPIPE) {
			pidentry->pipeHandler(pidentry->std_pipes[i]);
			Close_Pipe(pidentry->std_pipes[i]);
			pidentry->std_pipes[i] = DC_STD_FD_NOPIPE;
		}
	}

	// If stdin had a pipe and that's still open, close it, too.
	if (pidentry->std_pipes[0] != DC_STD_FD_NOPIPE) {
		Close_Pipe(pidentry->std_pipes[0]);
		pidentry->std_pipes[0] = DC_STD_FD_NOPIPE;
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
		CallReaper( pidentry->reaper_id, whatexited, pid, exit_status );
	} else {
		// TODO: the parent for this process is remote.
		// send the parent a DC_INVOKEREAPER command.
	}

	// now that we've invoked the reaper, check if we've registered a family
	// with the procd for this pid; if we have, unregister it now
	//
	if (pidentry->new_process_group == TRUE) {
		ASSERT(m_proc_family != NULL);
		if (!m_proc_family->unregister_family(pid)) {
			dprintf(D_ALWAYS,
			        "error unregistering pid %u with the procd\n",
			        pid);
		}
	}
	//Delete the session information.
	if(pidentry->child_session_id)
		getSecMan()->session_cache->remove(pidentry->child_session_id);
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
#ifdef HAVE_STRSIGNAL
	sprintf(exception_string,"signal %d (%s)",sig,strsignal(sig));
#else
	sprintf(exception_string,"signal %d",sig);
#endif
#endif

	return exception_string;
}


int DaemonCore::HandleChildAliveCommand(int, Stream* stream)
{
	pid_t child_pid = 0;
	unsigned int timeout_secs = 0;
	PidEntry *pidentry;
	int ret_value;
	double dprintf_lock_delay = 0.0;

	if (!stream->code(child_pid) ||
		!stream->code(timeout_secs)) {
		dprintf(D_ALWAYS,"Failed to read ChildAlive packet (1)\n");
		return FALSE;
	}

		// There is an optional additional dprintf_lock_delay in the
		// message.  It is optional so that external programs can send
		// simple alive messages using condor_squawk.
	if( stream->peek_end_of_message() ) {
		if( !stream->end_of_message() ) {
			dprintf(D_ALWAYS,"Failed to read ChildAlive packet (2)\n");
			return FALSE;
		}
	}
	else if( !stream->code(dprintf_lock_delay) ||
			 !stream->end_of_message())
	{
		dprintf(D_ALWAYS,"Failed to read ChildAlive packet (3)\n");
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
							(TimerHandlercpp) &DaemonCore::HungChildTimeout,
							"DaemonCore::HungChildTimeout", this);
		ASSERT( pidentry->hung_tid != -1 );

		Register_DataPtr( &pidentry->pid);
	}

	pidentry->was_not_responding = FALSE;

	dprintf(D_DAEMONCORE,
			"received childalive, pid=%d, secs=%d, dprintf_lock_delay=%f\n",child_pid,timeout_secs,dprintf_lock_delay);

		/* The Lock Convoy of Shadowy Doom.  Once upon a time, there
		 * was a submit machine that melted down for hours with high
		 * load and no way for the admins to bring it back to normal
		 * without rebooting it.  There were 30k-40k shadows running.
		 * During that time, some of the shadows waited for hours to
		 * get a lock to the shadow log, while others experienced much
		 * less wait time, indicating a high degree of unfairness.
		 * Further analysis revealed that the system was likely
		 * spending most of its time context switching whenever the
		 * lock was freed and was rarely actually getting any real
		 * work done.  This is known as the lock convoy problem.
		 *
		 * To address this, we made unix daemons append without
		 * locking (using O_APPEND).  A lock is still used for
		 * rotation, but this should not lead to the lock convoy
		 * problem unless rotation is happening in a matter of
		 * seconds.  Just in case the shadowy convoy ever returns,
		 * we want to leave some better clues behind.
		 */

	if( dprintf_lock_delay > 0.01 ) {
		dprintf(D_ALWAYS,"WARNING: child process %d reports that it has spent %.1f%% of its time waiting for a lock to its log file.  This could indicate a scalability limit that could cause system stability problems.\n",child_pid,dprintf_lock_delay*100);
	}
	if( dprintf_lock_delay > 0.1 ) {
			// things are looking serious, so let's send mail
		static time_t last_email = 0;
		if( last_email == 0 || time(NULL)-last_email > 60 ) {
			last_email = time(NULL);

			std::string subject;
			formatstr(subject,"Condor process reports long locking delays!");

			FILE *mailer = email_admin_open(subject.c_str());
			if( mailer ) {
				fprintf(mailer,
						"\n\nThe %s's child process with pid %d has spent %.1f%% of its time waiting\n"
						"for a lock to its log file.  This could indicate a scalability limit\n"
						"that could cause system stability problems.\n",
						get_mySubSystem()->getName(),
						child_pid,
						dprintf_lock_delay*100);
				email_close( mailer );
			}
		}
	}

	return TRUE;

}

int DaemonCore::HungChildTimeout()
{
	pid_t hung_child_pid;
	pid_t *hung_child_pid_ptr;
	PidEntry *pidentry;
	bool first_time = true;

	/* get the pid out of the allocated memory it was placed into */
	hung_child_pid_ptr = (pid_t*)GetDataPtr();
	hung_child_pid = *hung_child_pid_ptr;

	if ((pidTable->lookup(hung_child_pid, pidentry) < 0)) {
		// we have no information on this pid, it must have exited
		return FALSE;
	}

	// reset our tid to -1 so HandleChildAliveCommand() knows that there
	// is currently no timer set.
	pidentry->hung_tid = -1;

	if( ProcessExitedButNotReaped( hung_child_pid ) ) {
			// This process has exited, but we have not gotten around to
			// reaping it yet.  Do nothing.
		dprintf(D_FULLDEBUG,"Canceling hung child timer for pid %d, because it has exited but has not been reaped yet.\n",hung_child_pid);
		return FALSE;
	}

	// set a flag in the PidEntry so a reaper can discover it was killed
	// because it was hung.
	if( pidentry->was_not_responding ) {
		first_time = false;
	}
	else {
	pidentry->was_not_responding = TRUE;
	}

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
	bool want_core = param_boolean( "NOT_RESPONDING_WANT_CORE", false );
#ifndef WIN32
	if( want_core ) {
		// On multiple occassions, I have observed the child process
		// get hung while writing its core file.  If we never follow
		// up with a hard-kill, this can result in the service going
		// down for days, which is terrible.  Therefore, set a timer
		// to call us again and follow up with a hard-kill.
		if( !first_time ) {
			dprintf(D_ALWAYS,
					"Child pid %d is still hung!  Perhaps it hung while generating a core file.  Killing it harder.\n",hung_child_pid);
			want_core = false;
		}
		else {
			const int want_core_timeout = 600;
			pidentry->hung_tid =
				Register_Timer(want_core_timeout,
							   (TimerHandlercpp) &DaemonCore::HungChildTimeout,
							   "DaemonCore::HungChildTimeout", this);
			ASSERT( pidentry->hung_tid != -1 );
			Register_DataPtr( &pidentry->pid );
		}
	}
#endif
	Shutdown_Fast(hung_child_pid, want_core );

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
	MyString parent_sinful_string_buf;
	char const *parent_sinful_string;
	char const *tmp;
	int ret_val;
	static bool first_time = true;
	int number_of_tries = 3;

	dprintf(D_FULLDEBUG,"DaemonCore: in SendAliveToParent()\n");

	if ( !ppid ) {
		// no daemon core parent, nothing to send
		return FALSE;
	}

		/* Don't have the CGAHP and/or DAGMAN, which are launched as the user,
		   attempt to send keep alives to daemon. Permissions are not likely to
		   allow user proccesses to send signals to Condor daemons. 
		   Note that we shouldn't have to check for DAGMan here as no
		   daemon core info should have been inherited down to DAGMan, 
		   but it doesn't hurt to be sure here since we already need
		   to check for the CGAHP. */
	if (get_mySubSystem()->isType(SUBSYSTEM_TYPE_GAHP) ||
	  	get_mySubSystem()->isType(SUBSYSTEM_TYPE_DAGMAN))
	{
		return FALSE;
	}

		/* Before we possibly block trying to send this alive message to our 
		   parent, lets see if this parent pid (ppid) exists on this system.
		   This protects, for instance, against us acting a bogus CONDOR_INHERIT
		   environment variable that perhaps just got inherited down through
		   the ages.
		*/
	if ( !Is_Pid_Alive(ppid) ) {
		dprintf(D_FULLDEBUG,
			"DaemonCore: in SendAliveToParent() - ppid %ul disappeared!\n",
			ppid);
		return FALSE;
	}

	tmp = InfoCommandSinfulString(ppid);
	if ( tmp ) {
			// copy the result from InfoCommandSinfulString,
			// because the pointer we got back is a static buffer
		parent_sinful_string_buf = tmp;
		parent_sinful_string = parent_sinful_string_buf.Value();
	} else {
		dprintf(D_FULLDEBUG,"DaemonCore: No parent_sinful_string. "
			"SendAliveToParent() failed.\n");
			// parent already gone?
		return FALSE;
	}

		// If we are using glexec, then keepalives from the starter
		// to the startd will likely fail unless the user really went out
		// of their way to set things up so the starter and startd can authenticate
		// over the network.  So in the event that glexec
		// is being used, clear our first time flag so we do not
		// EXCEPT on failure and so we only try once.
	if ( get_mySubSystem()->isType( SUBSYSTEM_TYPE_STARTER) && 
		 param_boolean("GLEXEC_STARTER",false) )
	{
		first_time = false;
	}

	double dprintf_lock_delay = dprintf_get_lock_delay();
	dprintf_reset_lock_delay();

	bool blocking = first_time;
	classy_counted_ptr<Daemon> d = new Daemon(DT_ANY,parent_sinful_string);
	classy_counted_ptr<ChildAliveMsg> msg = new ChildAliveMsg(mypid,max_hang_time,number_of_tries,dprintf_lock_delay,blocking);

	int timeout = m_child_alive_period / number_of_tries;
	if( timeout < 60 ) {
		timeout = 60;
		}
	msg->setDeadlineTimeout( timeout );
	msg->setTimeout( timeout );

	if( blocking || !d->hasUDPCommandPort() || !m_wants_dc_udp ) {
		msg->setStreamType( Stream::reli_sock );
			}
	else {
		msg->setStreamType( Stream::safe_sock );
		}

	if( blocking ) {
		d->sendBlockingMsg( msg.get() );
		ret_val = msg->deliveryStatus() == DCMsg::DELIVERY_SUCCEEDED;
			}
	else {
		d->sendMsg( msg.get() );
		ret_val = TRUE;
		}

	if ( first_time ) {
		first_time = false;
		if ( ret_val == FALSE ) {
			EXCEPT("FAILED TO SEND INITIAL KEEP ALIVE TO OUR PARENT %s",
				parent_sinful_string);
		}
	}

	if (ret_val == FALSE) {
		dprintf(D_ALWAYS,"DaemonCore: Leaving SendAliveToParent() - "
			"FAILED sending to %s\n",
			parent_sinful_string);
	} else if( msg->deliveryStatus() == DCMsg::DELIVERY_SUCCEEDED ) {
		dprintf(D_FULLDEBUG,"DaemonCore: Leaving SendAliveToParent() - success\n");
	} else {
		dprintf(D_FULLDEBUG,"DaemonCore: Leaving SendAliveToParent() - pending\n");
	}

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
		/* bind(FALSE,...) means this is an incoming connection */
		if ( !rsock->bind(FALSE) ) {
			dprintf(D_ALWAYS, "Failed to bind to command ReliSock\n");

#ifndef WIN32
			dprintf(D_ALWAYS, "(Make sure your IP address is correct in /etc/hosts.)\n");
#endif
#ifdef WIN32
			dprintf(D_ALWAYS, "(Your system network settings might be invalid.)\n");
#endif

			return FALSE;
		}
		// Now open a SafeSock _on the same port_ chosen above,
		// assuming the caller wants a SafeSock (UDP) at all.
		// bind(FALSE,...) means this is an incoming connection.
		if (ssock && !ssock->bind(FALSE, rsock->get_port())) {
			rsock->close();
			continue;
		}
		return TRUE;
	}
	dprintf(D_ALWAYS, "Error: BindAnyCommandPort failed!\n");
	return FALSE;
}

bool
InitCommandSockets(int port, ReliSock *rsock, SafeSock *ssock, bool fatal)
{
		/*
		  DaemonCore::Create_Process has a rather stupid handling of
		  the want_command_port argument.  If it's set to FALSE (0),
		  it means no port.  If it's -1, or 1, it means "we want one,
		  and we don't care about the port".  If it's > 1, it means
		  "we want one on this specific port".  However, we can assume
		  this function is never called if port is 0, so we just have
		  to see if the port is <= 1 (which handles both -1 or 1) to
		  grab any old port, and if it's bigger than 1, we do
		  everything for a specifically requested port. *sigh*
		  Derek Wright 2007-08-09
		*/
	ASSERT(port != 0);
	if (port <= 1) {
			// Choose any old port (dynamic port)
		if( !BindAnyCommandPort(rsock, ssock) ) {
			if (fatal) {
				EXCEPT("BindAnyCommandPort() failed");
			}
			else {
				dprintf(D_ALWAYS | D_FAILURE, "BindAnyCommandPort() failed\n");
				return false;
			}

		}
		if( !rsock->listen() ) {
			if (fatal) {
				EXCEPT( "Failed to post listen on command ReliSock" );
			}
			else {
				dprintf(D_ALWAYS | D_FAILURE,
						"Failed to post listen on command ReliSock\n");
				return false;
			}
		}
	}
	else {
			// Use the well-known port specified in the arguments.
		int on = 1;
		int so_option = SO_REUSEADDR;

#if defined ( WIN32 )
		/** To better match the *nix semantics of SO_REUSEADDR we
			enable MSs SO_EXCLUSIVEADDRUSE option, which prohibits
			multiple process/identities/etc. from binding to the
			same address (which is just crazy behaviour!). 

			For further details refer to ticket #288. */
		so_option = SO_EXCLUSIVEADDRUSE;
#endif

			// Set options on this socket, SO_REUSEADDR, so that
			// if we are binding to a well known port, and we
			// crash, we can be restarted and still bind ok back
			// to this same port. -Todd T, 11/97
		if( !rsock->setsockopt(SOL_SOCKET, so_option,
							   (char*)&on, sizeof(on)) ) {
			if (fatal) {
				EXCEPT("setsockopt() SO_REUSEADDR failed on TCP command port");
			}
			else {
				dprintf(D_ALWAYS | D_FAILURE,
						"setsockopt() SO_REUSEADDR failed on TCP command port\n");
				return false;
			}
		}
		if( ssock &&
			!ssock->setsockopt(SOL_SOCKET, so_option,
							   (char*)&on, sizeof(on)) ) {
			if (fatal) {
				EXCEPT("setsockopt() SO_REUSEADDR failed on UDP command port");
			}
			else {
				dprintf(D_ALWAYS | D_FAILURE,
						"setsockopt() SO_REUSEADDR failed on UDP command port\n");
				return false;
			}
		}

			/* Set no delay to disable Nagle, since we buffer all our 
			   relisock output and it degrades performance of our 
			   various chatty protocols. -Todd T, 9/05
			*/
		if( !rsock->setsockopt(IPPROTO_TCP, TCP_NODELAY,
							   (char*)&on, sizeof(on)) ) {
			dprintf(D_ALWAYS, "Warning: setsockopt() TCP_NODELAY failed\n");
		}

		if (!rsock->listen(port)) {
			if (fatal) {
				EXCEPT("Failed to listen(%d) on TCP command socket.", port);
			}
			else {
				dprintf(D_ALWAYS | D_FAILURE,
						"Failed to listen(%d) on TCP command socket.\n", port);
				return false;
			}
		}
			/* bind(FALSE,...) means this is an incoming connection */
		if (ssock && !ssock->bind(FALSE, port)) {
			if (fatal) {
				EXCEPT("Failed to bind(%d) on UDP command socket.", port);
			}
			else {
				dprintf(D_ALWAYS | D_FAILURE,
						"Failed to bind(%d) on UDP command socket.\n", port);
				return false;
			}
		}
	}
	return true;
}


bool DaemonCore::ProcessExitedButNotReaped(pid_t pid)
{

#ifndef WIN32
	WaitpidEntry wait_entry;
	wait_entry.child_pid = pid;
	wait_entry.exit_status = 0; // ignored in WaitpidEntry::operator==, avoid uninit warning

	if(WaitpidQueue.IsMember(wait_entry)) {
		return true;
	}
#endif

	return false;
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

	if( ProcessExitedButNotReaped(pid) ) {
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
DaemonCore::CheckConfigAttrSecurity( const char* name, Sock* sock )
{
	const char* ip_str;
	int i;

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
		if( i == ALLOW ) {
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

		MyString command_desc;
		command_desc.formatstr("remote config %s",name);

		if( Verify(command_desc.Value(),(DCpermission)i, sock->peer_addr(), sock->getFullyQualifiedUser())) {
				// now we can see if the specific attribute they're
				// trying to set is in our list.
			if( (SettableAttrsLists[i])->
				contains_anycase_withwildcard(name) ) {
					// everything's cool.  allow this.

#if (DEBUG_SETTABLE_ATTR_LISTS)
				dprintf( D_ALWAYS, "CheckConfigSecurity: "
						 "found %s at access level %s\n", name,
						 PermString((DCpermission)i) );
#endif

				return true;
			}
		}
	} // end of for()

		// If we're still here, someone is trying to set something
		// they're not allowed to set.  print this out into the log so
		// folks can see that things are failing due to permissions.

		// Grab a pointer to this string, since it's a little bit
		// expensive to re-compute.
	ip_str = sock->peer_ip_str();

		// First, log it.
	dprintf( D_ALWAYS,
			 "WARNING: Someone at %s is trying to modify \"%s\"\n",
			 ip_str, name );
	dprintf( D_ALWAYS,
			 "WARNING: Potential security problem, request refused\n" );

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
		if( i == ALLOW ) {
			continue;
		}
		if( InitSettableAttrsList(get_mySubSystem()->getName(), i) ) {
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
					 PermString((DCpermission)i), tmp?tmp:"" );
			free( tmp );
		}
	}
#endif
}


bool
DaemonCore::InitSettableAttrsList( const char* /* subsys */, int i )
{
	MyString param_name;
	char* tmp;

/* XXX Comment this out and let subsys.SETTABLE_ATTRS_* work instead */
/*	if( subsys ) {*/
/*		param_name = subsys;*/
/*		param_name += "_SETTABLE_ATTRS_";*/
/*	} else {*/
		param_name = "SETTABLE_ATTRS_";
/*	}*/
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
            sec_man->invalidateHost(pidentry->sinful_string.Value());
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


bool DaemonCore :: set_cookie( int len, const unsigned char* data ) {
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

bool DaemonCore :: cookie_is_valid( const unsigned char* data ) {

	if ( data == NULL || _cookie_data == NULL ) {
		return false;
	}

	if ( strcmp((const char*)_cookie_data, (const char*)data) == 0 ) {
		// we have a match... trust this command.
		return true;
	} else if ( _cookie_data_old != NULL ) {

		// maybe this packet was queued before we
		// rotated the cookie. So check it with
		// the old cookie.

		if ( strcmp((const char*)_cookie_data_old, (const char*)data) == 0 ) {
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


void
DaemonCore::UpdateLocalAd(ClassAd *daemonAd,char const *fname) 
{
    FILE    *AD_FILE;

	if( !fname ) {
		char    localAd_path[100];
		sprintf( localAd_path, "%s_DAEMON_AD_FILE", get_mySubSystem()->getName() );

			// localAdFile is saved here so that daemon_core_main can clean
			// it up on exit.
		if( localAdFile ) {
			free( localAdFile );
		}
		localAdFile = param( localAd_path );
		fname = localAdFile;
	}

    if( fname ) {
		MyString newLocalAdFile;
		newLocalAdFile.formatstr("%s.new",fname);
        if( (AD_FILE = safe_fopen_wrapper_follow(newLocalAdFile.Value(), "w")) ) {
            daemonAd->fPrint(AD_FILE);
            fclose( AD_FILE );
			if( rotate_file(newLocalAdFile.Value(),fname)!=0 ) {
					// Under windows, rotate_file() sometimes failes with
					// system error 5 (access denied).  This is believed
					// to be expected in the case where some other process
					// has the target file open for reading, so only
					// report this as a WARNING under windows.
#ifdef WIN32
				dprintf( D_ALWAYS,
						 "DaemonCore: WARNING: failed to rotate %s to %s\n",
						 newLocalAdFile.Value(),
						 fname);
#else
				dprintf( D_ALWAYS,
						 "DaemonCore: ERROR: failed to rotate %s to %s\n",
						 newLocalAdFile.Value(),
						 fname);
#endif
			}
        } else {
            dprintf( D_ALWAYS,
                     "DaemonCore: ERROR: Can't open daemon address file %s\n",
                     newLocalAdFile.Value() );
        }
    }
}


void
DaemonCore::publish(ClassAd *ad) {
	const char* tmp;

		// Every ClassAd needs the common attributes directly from the
		// config file:
	config_fill_ad(ad);

		// Include our local current time.
	ad->Assign(ATTR_MY_CURRENT_TIME, (int)time(NULL));

		// Every daemon wants ATTR_MACHINE to be the full hostname:
	ad->Assign(ATTR_MACHINE, get_local_fqdn().Value());

		// Publish our network identification attributes:
	tmp = privateNetworkName();
	if (tmp) {
			// The private network name is published in the contact
			// string, so we don't really need to advertise it in
			// a separate attribute.  However, it may be useful for
			// other purposes.
		ad->Assign(ATTR_PRIVATE_NETWORK_NAME, tmp);
	}

	tmp = publicNetworkIpAddr();
	if( tmp ) {
		ad->Assign(ATTR_MY_ADDRESS, tmp);
	}
}


void
DaemonCore::initCollectorList() {
	if (m_collector_list) {
		delete m_collector_list;
	}
	m_collector_list = CollectorList::create();
}


CollectorList*
DaemonCore::getCollectorList() {
	return m_collector_list;
}


int
DaemonCore::sendUpdates( int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblock )
{
	ASSERT(ad1);
	ASSERT(m_collector_list);

		// Now's our chance to evaluate the DAEMON_SHUTDOWN expressions.
	if (!m_in_daemon_shutdown_fast &&
		evalExpr(ad1, "DAEMON_SHUTDOWN_FAST", ATTR_DAEMON_SHUTDOWN_FAST,
				 "starting fast shutdown"))	{
			// Daemon wants to quickly shut itself down and not restart.
		m_wants_restart = false;
		m_in_daemon_shutdown_fast = true;
		daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
	}
	else if (!m_in_daemon_shutdown &&
			 evalExpr(ad1, "DAEMON_SHUTDOWN", ATTR_DAEMON_SHUTDOWN,
					  "starting graceful shutdown")) {
		m_wants_restart = false;
		m_in_daemon_shutdown = true;
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}

		// Even if we just decided to shut ourselves down, we should
		// still send the updates originally requested by the caller.
	return m_collector_list->sendUpdates(cmd, ad1, ad2, nonblock);
}


bool
DaemonCore::wantsRestart()
{
	return m_wants_restart;
}


bool
DaemonCore::evalExpr( ClassAd* ad, const char* param_name,
					  const char* attr_name, const char* message )
{
	bool value = false;
	char* expr = param(param_name);
	if (!expr) {
		expr = param(attr_name);
	}
	if (expr) {
		if (!ad->AssignExpr(attr_name, expr)) {
			dprintf( D_ALWAYS|D_FAILURE,
					 "ERROR: Failed to parse %s expression \"%s\"\n",
					 attr_name, expr );
			free(expr);
			return false;
		}
		int result = 0;
		if (ad->EvalBool(attr_name, NULL, result) && result) {
			dprintf( D_ALWAYS,
					 "The %s expression \"%s\" evaluated to TRUE: %s\n",
					 attr_name, expr, message );
			value = true;
		}
		free(expr);
	}
	return value;
}

DaemonCore::PidEntry::PidEntry() : pid(0),
	new_process_group(0),
	is_local(0),
	parent_is_local(0),
	reaper_id(0),
	hung_tid(0),
	was_not_responding(0),
	stdin_offset(0),
	child_session_id(NULL)
{
	for (int i=0;i<3;++i) {
		pipe_buf[i] = NULL;
		std_pipes[i] = DC_STD_FD_NOPIPE;
	}
	penvid.num = PIDENVID_MAX;
	for (int i = 0;i<PIDENVID_MAX; ++i) {
		penvid.ancestors[i].active=0;
		for (unsigned int j=0;j<PIDENVID_ENVID_SIZE;++j)
			penvid.ancestors[i].envid[j]='\0';
	}
}

DaemonCore::PidEntry::~PidEntry() {
	int i;
	for (i=0; i<=2; i++) {
		if (pipe_buf[i]) {
			delete pipe_buf[i];
		}
	}
		// Close and cancel handlers for any pipes we created for this pid.
	for (i=0; i<=2; i++) {
		if (std_pipes[i] != DC_STD_FD_NOPIPE) {
			daemonCore->Close_Pipe(std_pipes[i]);
		}
	}

	if( !shared_port_fname.IsEmpty() ) {
			// Clean up the named socket for this process if the child
			// didn't already do so.
#ifndef WIN32
		SharedPortEndpoint::RemoveSocket( shared_port_fname.Value() );
#endif
	}
	if(child_session_id)
		free(child_session_id);
}


MSC_DISABLE_WARNING(6262) // function uses 65572 bytes of stack
int
DaemonCore::PidEntry::pipeHandler(int pipe_fd) {
    char buf[DC_PIPE_BUF_SIZE + 1];
    int bytes, max_read_bytes, max_buffer;
	int pipe_index = 0;
	MyString* cur_buf = NULL;
	const char* pipe_desc=NULL;
	if (std_pipes[1] == pipe_fd) {
		pipe_index = 1;
		pipe_desc = "stdout";
	}
	else if (std_pipes[2] == pipe_fd) {
		pipe_index = 2;
		pipe_desc = "stderr";
	}
	else {
		EXCEPT("IMPOSSIBLE: in pipeHandler() for pid %d with unknown fd %d",
			   (int)pid, pipe_fd);
	}

	if (pipe_buf[pipe_index] == NULL) {
			// Make a MyString buffer to hold the data.
		pipe_buf[pipe_index] = new MyString;
	}
	cur_buf = pipe_buf[pipe_index];

	// Read until we consume all the data (or loop too many times...)
	max_buffer = daemonCore->Get_Max_Pipe_Buffer();

	max_read_bytes = max_buffer - cur_buf->Length();
	if (max_read_bytes > DC_PIPE_BUF_SIZE) {
		max_read_bytes = DC_PIPE_BUF_SIZE;
	}

	bytes = daemonCore->Read_Pipe(pipe_fd, buf, max_read_bytes);
	if (bytes > 0) {
		// Actually read some data, so append it to our MyString.
		// First, null-terminate the buffer so that formatstr_cat()
		// doesn't go berserk. This is always safe since buf was
		// created on the stack with 1 extra byte, just in case.
		buf[bytes] = '\0';
		*cur_buf += buf;

		if (cur_buf->Length() >= max_buffer) {
			dprintf(D_DAEMONCORE, "DC %s pipe closed for "
					"pid %d because max bytes (%d)"
					"read\n", pipe_desc, (int)pid,
					max_buffer);
			daemonCore->Close_Pipe(pipe_fd);
			std_pipes[pipe_index] = DC_STD_FD_NOPIPE;
		}
	}
	else if ((bytes < 0) && ((EWOULDBLOCK != errno) && (EAGAIN != errno))) {
		// Negative is an error; If not EWOULDBLOCK or EAGAIN then:
		// Something bad	
		dprintf(D_ALWAYS|D_FAILURE, "DC pipeHandler: "
				"read %s failed for pid %d: '%s' (errno: %d)\n",
				pipe_desc, (int)pid, strerror(errno), errno);
		return FALSE;
	}
	return TRUE;
}
MSC_RESTORE_WARNING(6262) // warn when function uses > 16k stack

int
DaemonCore::PidEntry::pipeFullWrite(int fd)
{
	int bytes_written = 0;
	int total_len = 0;

	if (pipe_buf[0] != NULL)
	{
		const void* data_left = (const void*)(((const char*) pipe_buf[0]->Value()) + stdin_offset);
		total_len = pipe_buf[0]->Length();
		bytes_written = daemonCore->Write_Pipe(fd, data_left, total_len - stdin_offset);
		dprintf(D_DAEMONCORE, "DaemonCore::PidEntry::pipeFullWrite: Total bytes to write = %d, bytes written this pass = %d\n", total_len, bytes_written);
	}

	if (0 <= bytes_written)
	{
		stdin_offset = stdin_offset + bytes_written;
		if ((stdin_offset == total_len) || (pipe_buf[0] == NULL))
		{
			dprintf(D_DAEMONCORE, "DaemonCore::PidEntry::pipeFullWrite: Closing Stdin Pipe\n");
			// All data has been written to the pipe
			daemonCore->Close_Stdin_Pipe(pid);
		}
	}
	else if (errno != EINTR && errno != EAGAIN)
	{
		// Problem writting to the pipe and it's not an acceptable
		// failure case, so close the pipe
		dprintf(D_ALWAYS, "DaemonCore::PidEntry::pipeFullWrite: Unable to write to fd %d (errno = %d).  Aborting write attempts.\n", fd, errno);
		daemonCore->Close_Stdin_Pipe(pid);
	}
	else
	{
		dprintf(D_DAEMONCORE|D_FULLDEBUG, "DaemonCore::PidEntry::pipeFullWrite: Failed to write to fd %d (errno = %d).  Will try again.\n", fd, errno);
	}
	return 0;
}

void DaemonCore::send_invalidate_session ( const char* sinful, const char* sessid ) {
	if ( !sinful ) {
		dprintf (D_SECURITY, "DC_AUTHENTICATE: couldn't invalidate session %s... don't know who it is from!\n", sessid);
		return;
	}

	classy_counted_ptr<Daemon> daemon = new Daemon(DT_ANY,sinful,NULL);

	classy_counted_ptr<DCStringMsg> msg = new DCStringMsg(
		DC_INVALIDATE_KEY,
		sessid );

	msg->setSuccessDebugLevel(D_SECURITY);
	msg->setRawProtocol(true);

	if( m_invalidate_sessions_via_tcp ) {
		msg->setStreamType(Stream::reli_sock);
	}
	else {
		msg->setStreamType(Stream::safe_sock);
	}

	daemon->sendMsg( msg.get() );
}
