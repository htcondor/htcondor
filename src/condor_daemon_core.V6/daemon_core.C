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
// //////////////////////////////////////////////////////////////////////
//
// Implementation of DaemonCore.
//
//
// //////////////////////////////////////////////////////////////////////

#include "condor_common.h"

static const int DEFAULT_MAXCOMMANDS = 255;
static const int DEFAULT_MAXSIGNALS = 99;
static const int DEFAULT_MAXSOCKETS = 8;
static const int DEFAULT_MAXREAPS = 8;
static const int DEFAULT_PIDBUCKETS = 11;
static const char* DEFAULT_INDENT = "DaemonCore--> ";


#include "condor_daemon_core.h"
#include "condor_io.h"
#include "internet.h"
#include "condor_debug.h"
#include "get_daemon_addr.h"
#include "condor_uid.h"
#include "condor_commands.h"
#include "condor_config.h"
#ifdef WIN32
#include "exphnd.WIN32.h"
typedef unsigned (__stdcall *CRT_THREAD_HANDLER) (void *);
#endif

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
				int SocSize,int ReapSize)
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
	mypid = ::GetCurrentProcessId();
#else
	mypid = ::getpid();
#endif

	maxCommand = ComSize;
	maxSig = SigSize;
	maxSocket = SocSize;
	maxReap = ReapSize;

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

	if(maxReap == 0)
		maxReap = DEFAULT_MAXREAPS;

	reapTable = new ReapEnt[maxReap];
	if(reapTable == NULL)
	{
		EXCEPT("Out of memory!");
	}
	nReap = 0;
	memset(reapTable,'\0',maxReap*sizeof(ReapEnt));

	curr_dataptr = NULL;
	curr_regdataptr = NULL;

	send_child_alive_timer = -1;

#ifdef WIN32
	dcmainThreadId = ::GetCurrentThreadId();
#endif

#ifndef WIN32
	async_sigs_unblocked = FALSE;
#endif

	dc_rsock = NULL;
	dc_ssock = NULL;

	inheritedSocks[0] = NULL;
	inServiceCommandSocket_flag = FALSE;
	
	Default_Priv_State = PRIV_CONDOR;
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

	delete pidTable;

	t.CancelAllTimers();
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
    
    if( handler == NULL && handlercpp == NULL) {
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
    if ( (comTable[i].handler != NULL) || (comTable[i].handlercpp != NULL) ) {
		// occupied
        if(comTable[i].num == command) {
			// by the same signal
			EXCEPT("DaemonCore: Same command registered twice");
        }
		// by some other signal, so scan thru the entries to
		// find the first empty one
        for(j = (i + 1) % maxCommand; j != i; j = (j + 1) % maxCommand) {
            if ((comTable[j].handler == NULL) && 
							(comTable[j].handlercpp == NULL))
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

int DaemonCore::Register_Signal(int sig, char* sig_descrip, 
				SignalHandler handler, SignalHandlercpp handlercpp, 
				char* handler_descrip, Service* s,	DCpermission perm, 
				int is_cpp)
{
    int     i;		// hash value
    int     j;		// for linear probing
    
    
    if( handler == NULL && handlercpp == NULL) {
		dprintf(D_DAEMONCORE, "Can't register NULL signal handler\n");
		return -1;
    }

	// Semantics dictate that certain signals CANNOT be caught!
	// In addition, allow DC_SIGCHLD to be automatically replaced (for backwards
	// compatibility), so cancel any previous registration for DC_SIGCHLD.
	switch (sig) {
		case DC_SIGKILL:
		case DC_SIGSTOP:
		case DC_SIGCONT:
			EXCEPT("Trying to Register_Signal for sig %d which cannot be caught!",sig);
			break;
		case DC_SIGCHLD:
			Cancel_Signal(DC_SIGCHLD);
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
    if ( (sigTable[i].handler != NULL) || (sigTable[i].handlercpp != NULL) ) {
		// occupied...
        if(sigTable[i].num == sig) {
			// by the same signal
			EXCEPT("DaemonCore: Same signal registered twice");
        }
		// by some other signal, so scan thru the entries to
		// find the first empty one
        for(j = (i + 1) % maxSig; j != i; j = (j + 1) % maxSig) {
            if ((sigTable[j].handler == NULL) && 
							(sigTable[j].handlercpp == NULL))
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
	for ( j=0; j < nSock; j++ ) {
		if ( (*sockTable)[j].iosock == iosock ) {
			EXCEPT("DaemonCore: Same socket registered twice");
        }
	}

	// Found a blank entry at index i. Now add in the new data.
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
	if ( initial_command_sock == -1 && handler == NULL && handlercpp == NULL )
		initial_command_sock = i;

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &((*sockTable)[i].data_ptr);

	// Conditionally dump what our table looks like
	DumpSocketTable(D_FULLDEBUG | D_DAEMONCORE);

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
	dprintf(D_DAEMONCORE,"Cancel_Socket: cancelled socket %d <%s> %x\n",
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
		if ((comTable[i].handler != NULL) || 
						(comTable[i].handlercpp != NULL)) 
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
		if ((reapTable[i].handler != NULL) || 
						(reapTable[i].handlercpp != NULL)) {
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
		if ( (sigTable[i].handler != NULL) || 
						(sigTable[i].handlercpp != NULL) ) 
		{
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
			dprintf(flag, "%s%d: %s %s\n", 
					indent, i, descrip1, descrip2 );
		}
	}
	dprintf(flag, "\n");
}

int
DaemonCore::ReInit()
{
	char *addr;
	struct sockaddr_in sin;
	char *tmp;
	char buf[50];
	static int tid = -1;

	// Fetch the negotiator address for the Verify method to use
	addr = get_negotiator_addr(NULL);	// get sinful string of negotiator
	if ( addr ) {
		string_to_sin(addr,&sin);
		memcpy(&negotiator_sin_addr,&(sin.sin_addr),
						sizeof(negotiator_sin_addr));
	} else {
		// we failed to get the address of the negotiator
		memset(&negotiator_sin_addr,'\0',sizeof(negotiator_sin_addr));
	}

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

	return TRUE;
}


int
DaemonCore::Verify(DCpermission perm, const struct sockaddr_in *sin )
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
			return ipverify.Verify(perm,sin);
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
#ifndef WIN32
		// Drain our async_pipe; we must do this before we unblock unix signals.
		// Just keep reading while something is there.  async_pipe is set to 
		// non-blocking mode via fcntl, so the read below will not block.
		while( read(async_pipe[0],asyncpipe_buf,8) > 0 );
#endif

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
		temp = t.Timeout();
		if ( sent_signal == TRUE )
			temp = 0;
		timer.tv_sec = temp;
		timer.tv_usec = 0;
		if ( temp < 0 )
			ptimer = NULL;
		else
			ptimer = &timer;		// no timeout on the select() desired
		

		// Setup what socket descriptors to select on.  We recompute this
		// every time because 1) some timeout handler may have removed/added
		// sockets, and 2) it ain't that expensive....
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);
		for (i = 0; i < nSock; i++) {
			if ( (*sockTable)[i].iosock ) {	// if a valid entry....
				if ( (*sockTable)[i].is_connect_pending ) {
						// we want to be woken when a non-blocking
						// connect is ready to write.  when connect
						// is ready, select will set the writefd set
						// on success, or the exceptfd set on failure.
					FD_SET( (*sockTable)[i].sockd,&writefds);
					FD_SET( (*sockTable)[i].sockd,&exceptfds);
				} else {
						// we want to be woken when there is something
						// to read.
					FD_SET( (*sockTable)[i].sockd,&readfds);
				}
            }
		}

		
#if !defined(WIN32)
		// Add the read side of async_pipe to the list of file descriptors to
		// select on.  We write to async_pipe if a unix async signal 
		// is delivered after we unblock signals and before we block on select.

		FD_SET(async_pipe[0],&readfds);

		// Set aync_sigs_unblocked flag to true so that Send_Signal()
		// knows to put info onto the async_pipe in order to wake up select().
		// We _must_ set this flag to TRUE before we unblock async signals, and
		// set it to FALSE after we block the signals again.
		async_sigs_unblocked = TRUE;

		// Unblock all signals so that we can get them during the
		// select.
		sigprocmask( SIG_SETMASK, &emptyset, NULL );
#endif

		errno = 0;
		rv = select(FD_SETSIZE, (SELECT_FDSET_PTR) &readfds, 
								&writefds, &exceptfds, ptimer);
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
				EXCEPT("select, error # = %d", tmpErrno);
			}
		}
#else
		// Windoze
		if ( rv == SOCKET_ERROR ) {
			EXCEPT("select, error # = %d",WSAGetLastError());
		}
#endif
		
		if (rv > 0) {	// connection requested
			// scan through the socket table to find which one select() set
			for(i = 0; i < nSock; i++) {
				
				if ( (*sockTable)[i].iosock ) {	// if a valid entry...
					
					// figure out if we should call a handler.  to do this,
					// if the socket was doing a connect(), we check the
					// writefds and excepfds.  otherwise, check readfds.
					bool call_handler = false;	
					if ( (*sockTable)[i].is_connect_pending ) {					
						if ( (FD_ISSET((*sockTable)[i].sockd, &writefds)) ||
							 (FD_ISSET((*sockTable)[i].sockd, &exceptfds)) ) 
						{
							// a connection pending socket has been set.
							// only call the handler if CEDAR confirms the
							// connect algorithm has completed.
							if ( ((Sock *)(*sockTable)[i].iosock)->
											do_connect_finish() )
							{
								call_handler = true;
							}
						}
					} else {
						if (FD_ISSET((*sockTable)[i].sockd, &readfds)) 
						{
							call_handler = true;
						}
					}

					if ( call_handler ) {
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

						// Update curr_dataptr for GetDataPtr()
						curr_dataptr = &( (*sockTable)[i].data_ptr);
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
			}	// for 0 thru nSock
		}	// if rv > 0

	}	// end of infinite for loop
}


void
DaemonCore::CheckPrivState( void )
{
		// We should always be Condor, so set to it here.  If we were
		// already in Condor priv, this is just a no-op.
	priv_state old_priv = set_priv( Default_Priv_State );

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
		rv = select(FD_SETSIZE,(SELECT_FDSET_PTR) &fds, NULL, NULL, &timer);
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
	}
	
	// read in the command from the stream with a timeout value of 20 seconds
	old_timeout = stream->timeout(20);
	stream->decode();
	result = stream->code(req);
	// For now, lets keep the timeout, so all command handlers are called with
	// a timeout of 20 seconds on their socket.
	// stream->timeout(old_timeout);
	if(!result) {
		if (is_tcp) {
			dprintf(D_ALWAYS, "DaemonCore: Command received via TCP from %s\n",
					sin_to_string(((Sock*)stream)->endpoint()) );
		}
		dprintf(D_ALWAYS, 
			"DaemonCore: Can't receive command request (perhaps a timeout?)\n");
		if ( insock != stream )	{   // delete stream only if we did an accept
			delete stream;		   
		} else {
			stream->end_of_message();
		}
		return KEEP_STREAM;
	}
	
	// get the handler function

	// first compute the hash
	if ( req < 0 )
		index = -req % maxCommand;
	else
		index = req % maxCommand;

	reqFound = FALSE;
	if (comTable[index].num == req) {
		// hash found it first try... cool
		reqFound = TRUE;
	} else {
		// hash did not find it, search for it
		for (j = (index + 1) % maxCommand; j != index; j = (j + 1) % maxCommand)
			if(comTable[j].num == req) {
				reqFound = TRUE; 
				index = j;
				break;
			}
	}

	if ( reqFound == TRUE ) {
		// Check the daemon core permission for this command handler
		if ( Verify(comTable[index].perm,((Sock*)stream)->endpoint()) == FALSE )
		{
			// Permission check FAILED
			reqFound = FALSE;	// so we do not call the handler function below
			// make result != to KEEP_STREAM, so we blow away this socket below
			result = 0;
			dprintf( D_ALWAYS,
			   "DaemonCore: PERMISSION DENIED to host %s for command %d (%s)\n",
			   sin_to_string(((Sock*)stream)->endpoint()), req,
			   comTable[index].command_descrip );
			// if UDP, consume the rest of this message to try to stay "in-sync"
			if ( !is_tcp)
				stream->end_of_message();
		} else {
			dprintf(comTable[index].dprintf_flag,
					"DaemonCore: Command received via %s from %s\n",
					(is_tcp) ? "TCP" : "UDP",
					sin_to_string(((Sock*)stream)->endpoint()) );
			dprintf(comTable[index].dprintf_flag, 
				"DaemonCore: received command %d (%s), calling handler (%s)\n",
				req, comTable[index].command_descrip, 
				comTable[index].handler_descrip);
		}
	} else {
		dprintf(D_ALWAYS, "DaemonCore: Command received via %s from %s\n",
				(is_tcp) ? "TCP" : "UDP",
				sin_to_string(((Sock*)stream)->endpoint()) );
		dprintf(D_ALWAYS,
			"DaemonCore: received unregistered command request %d !\n",req);
		// make result != to KEEP_STREAM, so we blow away this socket below
		result = 0;
		// if UDP, consume the rest of this message to try to stay "in-sync"
		if ( !is_tcp)
			stream->end_of_message();
	}

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
	
	// finalize; the handler is done with the command.  the handler will return
	// with KEEP_STREAM if we should not touch the stream; otherwise, cleanup
	// the stream.  On tcp, we just delete it since the stream is the one we got
	// from accept and our listen socket is still out there.  on udp, 
	// however, we cannot just delete it or we will not be "listening" 
	// anymore, so we just do an eom flush all buffers, etc.
	// HACK: keep all UDP sockets as well for now.  
	if ( result != KEEP_STREAM ) {
		stream->encode();	// we wanna "flush" below in the encode direction 
		if ( is_tcp ) {
			stream->end_of_message();  // make certain data flushed to the wire
			if ( insock != stream )	   // delete the stream only if we did an accept; if we
				delete stream;		   //     did not do an accept, Driver() will delete the stream.
		} else {			
			stream->end_of_message(); 			
			result = KEEP_STREAM;	// HACK: keep all UDP sockets for now.  The only ones
									// in Condor so far are Initial command socks, so keep it.
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


int DaemonCore::HandleSigCommand(int command, Stream* stream)
{
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
				"DaemonCore: received Signal %d (%s), raising event\n", sig,
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
		case DC_SIGTERM:
			// if the process we are signalling is a daemon core process, 
			// then target_has_dcpm == TRUE and we will just fall thru and
			// send the DC_SIGTERM as usual.
			if ( target_has_dcpm == FALSE ) {
				return Shutdown_Graceful(pid);
			}
			break;
		case DC_SIGKILL:
			return Shutdown_Fast(pid);
			break;
		case DC_SIGSTOP:
			return Suspend_Process(pid);
			break;
		case DC_SIGCONT:
			return Continue_Process(pid);
			break;
		default:
#ifndef WIN32
			// If we are on Unix, and we are not sending a 'special' signal,
			// and our child is not a daemon-core process, then just send
			// the signal as usual via kill() after translating DC sig num
			// into the equivelent Unix signal on this platform.
			if ( target_has_dcpm == FALSE ) {
#define TRANSLATE_SIG( x ) case DC_##x: unixsig = x; unixsigname = #x; break;

				int unixsig;
				char *unixsigname;
				switch( sig ) {
					TRANSLATE_SIG( SIGHUP )
					TRANSLATE_SIG( SIGINT )
					TRANSLATE_SIG( SIGQUIT )
					TRANSLATE_SIG( SIGILL )
					TRANSLATE_SIG( SIGTRAP )
					TRANSLATE_SIG( SIGABRT )
#if defined(SIGEMT)
					TRANSLATE_SIG( SIGEMT )
#endif
					TRANSLATE_SIG( SIGFPE )
					TRANSLATE_SIG( SIGKILL )
					TRANSLATE_SIG( SIGBUS )
					TRANSLATE_SIG( SIGSEGV )
#if defined(SIGSYS)
					TRANSLATE_SIG( SIGSYS )
#endif
					TRANSLATE_SIG( SIGPIPE )
					TRANSLATE_SIG( SIGALRM )
					TRANSLATE_SIG( SIGTERM )
#if defined(SIGURG)
					TRANSLATE_SIG( SIGURG )
#endif
					TRANSLATE_SIG( SIGSTOP )
					TRANSLATE_SIG( SIGTSTP )
					TRANSLATE_SIG( SIGCONT )
					TRANSLATE_SIG( SIGCHLD )
					TRANSLATE_SIG( SIGTTIN )
					TRANSLATE_SIG( SIGTTOU )
#if defined(SIGIO)
					TRANSLATE_SIG( SIGIO )
#endif
#if defined(SIGXCPU)
					TRANSLATE_SIG( SIGXCPU )
#endif
#if defined(SIGXFSZ)
					TRANSLATE_SIG( SIGXFSZ )
#endif
#if defined(SIGVTALRM)
					TRANSLATE_SIG( SIGVTALRM )
#endif
#if defined(SIGPROF)
					TRANSLATE_SIG( SIGPROF )
#endif
#if defined(SIGWINCH)
					TRANSLATE_SIG( SIGWINCH )
#endif
#if defined(SIGINFO)
					TRANSLATE_SIG( SIGINFO )
#endif
					TRANSLATE_SIG( SIGUSR1 )
					TRANSLATE_SIG( SIGUSR2 )
					default:   	
						{
							EXCEPT("Trying to send unknown signal");
						}
				}
				dprintf(D_DAEMONCORE,
					"Send_Signal(): Doing kill(%d,%d) [sig %d=%s]\n",
					pid, unixsig,unixsig,unixsigname);
				priv_state priv = set_root_priv();
				int status = ::kill(pid, unixsig);
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

	// now destination process is local, send via UDP; if remote, send via TCP
	if ( is_local == TRUE ) {
		sock = (Stream *) new SafeSock();
		sock->timeout(3);
	} else {
		sock = (Stream *) new ReliSock();
		sock->timeout(20);
	}

	if (!((Sock *)sock)->connect(destination)) {
		dprintf(D_ALWAYS,"Send_Signal: ERROR Connect to %s failed.",
				destination);
		delete sock;
		return FALSE;
	}

	// send the signal out as a DC_RAISESIGNAL command
	sock->encode();		
	if ( (!sock->put(DC_RAISESIGNAL)) ||
		 (!sock->code(sig)) ||
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
			sysinfo.GetProcessName(pid,check_name);
			dprintf(D_PROCFAMILY,
				"Shutdown_Fast(%d):calling TerminateProcess handle=%x check_name='%s'\n",
				pid,pidHandle,check_name);
	}

	if (TerminateProcess(pidHandle, 0)) {
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
		// pid is a DaemonCore Process; send it a DC_SIGTERM signal instead.
		dprintf(D_PROCFAMILY,
					"Shutdown_Graceful: Sending pid %d DC_SIGTERM\n",pid);
		return Send_Signal(pid,DC_SIGTERM);
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
			EnumWindowStations((WINSTAENUMPROC)DCFindWinSta, (LPARAM)pidinfo);

			if ( pidinfo->hWnd == NULL ) {
				// Did not find it.  This could happen because WinNT has a 
				// stupid bug where a brand new winsta does not immediately
				// show up when you enumerate all the winstas.  Sometimes
				// it takes a few seconds.  So lets sleep a while and try again.
				sleep(4);
				EnumWindowStations((WINSTAENUMPROC)DCFindWinSta, 
								(LPARAM)pidinfo);
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
			dprintf(D_PROCFAMILY,"PostMessage to HWND_BROADCAST!");
			EXCEPT("About to send WM_CLOSE to HWND_BROADCAST!");
		}
		if( (DebugFlags & D_PROCFAMILY) && (DebugFlags & D_FULLDEBUG) ) {
			// A whole bunch of sanity checks
			pid_t check_pid = 0;
			GetWindowThreadProcessId(pidinfo->hWnd, &check_pid);
			char check_name[_POSIX_PATH_MAX];
			CSysinfo sysinfo;
			sysinfo.GetProcessName(check_pid,check_name);
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

	priv_state priv = set_root_priv();
	int status = kill(pid, SIGTERM);
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
	}

	// Keep calling SuspendThread until they are all suspended.
	numExtraSuspends = 0;
	do {
		allDone = TRUE;
		for (j=0; j < numTids; j++) {
			if ( hThreads[j] ) {
				// Note: SuspendThread returns > 1 if already suspended
				if ( ::SuspendThread(hThreads[j]) == 0 ) {
					 allDone = FALSE;
				}
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
				// Oh shit! This process was already active.  Resume all
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

// Callback function for EnumWindowStationProc call to find hWnd for a pid
BOOL CALLBACK
DCFindWinSta(LPTSTR winsta_name, LPARAM p)
{
	BOOL ret_value;
	priv_state priv;

	// dprintf(D_FULLDEBUG,"Opening WinSta %s\n",winsta_name);

	if ( strcmp(winsta_name,"WinSta0") == 0 ) {
		// skip the interactive Winsta to save time
		dprintf(D_PROCFAMILY,"Skipping Winsta0\n");
		return TRUE;
	}

	// must try to open winsta as the user
	priv = set_user_priv();
	HWINSTA hwinsta = OpenWindowStation(winsta_name, FALSE, MAXIMUM_ALLOWED);
	set_priv(priv);

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

	// must try to open desktop as the user
	set_user_priv();
	HDESK hdesk = OpenDesktop( "default", 0, FALSE, MAXIMUM_ALLOWED );
	set_priv(priv);

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
		ret_value = TRUE;
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
			sysinfo.GetProcessName(pid,check_name);
			dprintf(D_PROCFAMILY,
				"DCFindWindow found pid %d: hWnd=%x name='%s'\n",
				pid,hWnd,check_name);
		}


		return FALSE;
	}
	return TRUE;
}
#endif	// of ifdef WIN32

int DaemonCore::SetEnv( char *key, char *value)
{
	assert(key);
	assert(value);
#ifdef WIN32
	if ( !SetEnvironmentVariable(key, value) ) {
		dprintf(D_ALWAYS,
				"SetEnvironmentVariable failed, errno=%d\n",
				GetLastError());
		return FALSE;
	}
#else
	// XXX: We should actually put all of this in a hash table, so that 
	// we don't leak memory.  This is just quick and dirty to get something
	// working.
	char *buf;
	buf = new char[strlen(key) + strlen(value) + 2];
	sprintf(buf, "%s=%s", key, value);
	if( putenv(buf) != 0 )
	{
		dprintf(D_ALWAYS, "putenv failed: %s (errno=%d)\n",
				strerror(errno), errno);
		return FALSE;
	}
#endif
	return TRUE;
}

int DaemonCore::SetEnv( char *env_var ) 
{
		// this function used if you've already got a name=value type
		// of string, and want to put it into the environment.  env_var
		// must therefore contain an '='.
	if ( !env_var ) {
		dprintf (D_ALWAYS, "DaemonCore::SetEnv, env_var = NULL!\n" );
		return FALSE;
	}

		// Assume this type of string passes thru with no problem...
	if ( env_var[0] == 0 ) {
		return TRUE;
	}

	char *equalpos = NULL;

	if ( ! (equalpos = strchr( env_var, '=' )) ) {
		dprintf (D_ALWAYS, "DaemonCore::SetEnv, env_var has no '='\n" );
		dprintf (D_ALWAYS, "env_var = \"%s\"\n", env_var );
		return FALSE; 
	}

#ifdef WIN32  // will this ever be used?  Hmmm....
		// hack up string and pass to other SetEnv version.
	int namelen = (int) equalpos - (int) env_var;
	int valuelen = strlen(env_var) - namelen - 1;

	char *name = new char[namelen+1];
	char *value = new char[valuelen+1];
	strncpy ( name, env_var, namelen );
	strncpy ( value, equalpos+1, valuelen );

	int retval = SetEnv ( name, value );

	delete [] name;
	delete [] value;
	return retval;
#else
		// XXX again, this is a memory-leaking quick hack; see above.
	char *buf = new char[strlen(env_var) +1];
	strcpy ( buf, env_var );
	if( putenv(buf) != 0 ) {
		dprintf(D_ALWAYS, "putenv failed: %s (errno=%d)\n",
				strerror(errno), errno);
		return FALSE;
	}
	return TRUE;
#endif
}


int DaemonCore::Create_Process(
			char		*name,
			char		*args,
			priv_state	priv,
			int			reaper_id,
			int			want_command_port,
			char		*env,
			char		*cwd,
			int			new_process_group,
			Stream		*sock_inherit_list[],
			int			std[],
            int         nice_inc
            )
{
	int i;
	char *ptmp;
	int inheritSockFds[MAX_INHERIT_SOCKS];
	int numInheritSockFds = 0;

	char inheritbuf[_INHERITBUF_MAXSIZE];
		// note that these are on the stack; they go away nicely
		// upon return from this function.
	ReliSock rsock;
	SafeSock ssock;

	pid_t newpid;

#ifdef WIN32  // sheesh, this shouldn't be necessary
	BOOL inherit_handles = FALSE;
#else
	int inherit_handles;
#endif

	dprintf(D_DAEMONCORE,"In DaemonCore::Create_Process(%s,...)\n",name);

	// First do whatever error checking we can that is not platform specific

	// check reaper_id validity
	if ( (reaper_id < 1) || (reaper_id > maxReap) 
		 || (reapTable[reaper_id - 1].num == 0) ) {
		dprintf(D_ALWAYS,"Create_Process: invalid reaper_id\n");
		return FALSE;
	}

	// check name validity
	if ( !name ) {
		dprintf(D_ALWAYS,"Create_Process: null name to exec\n");
		return FALSE;
	}

	sprintf(inheritbuf,"%lu ",mypid);

	strcat(inheritbuf,InfoCommandSinfulString());

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
                    return FALSE;
                }
            } 
            else {
                dprintf ( D_ALWAYS, "Dynamic cast failure!\n" );
                EXCEPT( "dynamic_cast" );
            }

			// now place the type of socket into inheritbuf
			 switch ( sock_inherit_list[i]->type() ) {
				case Stream::reli_sock :
					strcat(inheritbuf," 1 ");
					break;
				case Stream::safe_sock :
					strcat(inheritbuf," 2 ");
					break;
				default:
					// we only inherit safe and reli socks at this point...
					assert(0);
					break;
			}
			// now serialize object into inheritbuf
			 ptmp = sock_inherit_list[i]->serialize();
			 strcat(inheritbuf,ptmp);
			 delete []ptmp;
		}
	}
	strcat(inheritbuf," 0");

	// if we want a command port for this child process, create
	// an inheritable tcp and a udp socket to listen on, and place 
	// the info into the inheritbuf.
	if ( want_command_port != FALSE ) {
		inherit_handles = TRUE;
		if ( want_command_port == TRUE ) {
			// choose any old port (dynamic port)
			if (!BindAnyCommandPort(&rsock, &ssock)) {
				// dprintf with error message already in BindAnyCommandPort
				return FALSE;
			}
			if ( !rsock.listen() ) {
				dprintf( D_ALWAYS, "Create_Process:Failed to post listen "
						 "on command ReliSock\n");
				return FALSE;
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
				return FALSE;
			}

			if ( (!rsock.listen( want_command_port)) ||
				 (!ssock.bind( want_command_port)) ) {
				dprintf(D_ALWAYS,"Create_Process:Failed to post listen "
						"on command socket(s)\n");
				return FALSE;
			}
		}

		// now duplicate the underlying SOCKET to make it inheritable
		if ( (!(rsock.set_inheritable(TRUE))) || 
			 (!(ssock.set_inheritable(TRUE))) ) {
			dprintf(D_ALWAYS,"Create_Process:Failed to set command "
					"socks inheritable\n");
			return FALSE;
		}

		// and now add these new command sockets to the inheritbuf
		strcat(inheritbuf," ");
		ptmp = rsock.serialize();
		strcat(inheritbuf,ptmp);
		delete []ptmp;
		strcat(inheritbuf," ");
		ptmp = ssock.serialize();
		strcat(inheritbuf,ptmp);
		delete []ptmp;		 

            // now put the actual fds into the list of fds to inherit
        inheritSockFds[numInheritSockFds++] = rsock.get_file_desc();
        inheritSockFds[numInheritSockFds++] = ssock.get_file_desc();
	}
	strcat(inheritbuf," 0");
	
#ifdef WIN32
	// START A NEW PROCESS ON WIN32

	STARTUPINFO si;
	PROCESS_INFORMATION piProcess;
	// SECURITY_ATTRIBUTES sa;
	// SECURITY_DESCRIPTOR sd;
	char *newenv = NULL;

	// prepare a STARTUPINFO structure for the new process
	ZeroMemory(&si,sizeof(si));
	si.cb = sizeof(si);

	// we do _not_ want our child to inherit our file descriptors
	// unless explicitly requested.  so, set the underlying handle
	// of all files opened via the C runtime library to non-inheritable.
	for (i = 0; i < 100; i++) {
		SetFDInheritFlag(i,FALSE);
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
		return FALSE;
	}
#endif

	if ( new_process_group == TRUE )
		new_process_group = CREATE_NEW_PROCESS_GROUP;
	else
		new_process_group =  0;	

    // Re-nice our child -- on WinNT, this means run it at IDLE process
	// priority class.
    if ( nice_inc > 0 && nice_inc < 20 ) {
		// or new_process_group with whatever we set above...
		new_process_group |= IDLE_PRIORITY_CLASS;
	}


	// Place inheritbuf into the environment as env variable CONDOR_INHERIT.
	// Add it to the user-specified environment if one specified, else add
	// it to our environment (which will then be inherited by our child).
	// Rememer to free newenv if not NULL; it was malloced.
	newenv = ParseEnvArgsString(env,inheritbuf);
	if ( !newenv ) {
		if( !SetEnv("CONDOR_INHERIT", inheritbuf) ) {
			dprintf(D_ALWAYS, "Create_Process: Failed to set "
					"CONDOR_INHERIT env.\n");
			return FALSE;
		}
	}

	BOOL cp_result;
	if ( priv != PRIV_USER_FINAL ) {
		cp_result = ::CreateProcess(name,args,NULL,NULL,inherit_handles,
			new_process_group,newenv,cwd,&si,&piProcess);
	} else {
		// here we want to create a process as user for PRIV_USER_FINAL

			// making this a NULL string tells NT to dynamically
			// create a new Window Station for the process we are about
			// to create....
		si.lpDesktop = "";

		HANDLE user_token = priv_state_get_handle();
		ASSERT(user_token);

			// we need to make certain to specify CREATE_NEW_CONSOLE, because
			// our ACLs will not let us use the current console which is
			// restricted to LOCALSYSTEM.  
			//
			// "Who's your Daddy ?!?!?!   JEFF B.!"
		cp_result = ::CreateProcessAsUser(user_token,name,args,NULL,NULL,
			inherit_handles,new_process_group | CREATE_NEW_CONSOLE, 
			newenv,cwd,&si,&piProcess);
	}

	if ( !cp_result ) {
		dprintf(D_ALWAYS,
			"Create_Process: CreateProcess failed, errno=%d\n",GetLastError());
		if ( newenv ) {
			free(newenv);
		}
		return FALSE;
	}
	if ( newenv ) {
		free(newenv);
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
	if( access(name,F_OK | X_OK) < 0 ) {
		dprintf(D_ALWAYS, 
				"Create_Process: Specified executable %s cannot be found. "
				"errno = %d (%s).\n",
				name, errno, strerror(errno));	   
		if ( priv != PRIV_UNKNOWN ) {
			set_priv( current_priv );
		}
		return FALSE;
	}

	// Next, check to see that the cwd exists.
	struct stat stat_struct;
	if( cwd && (cwd[0] != '\0') ) {
		if( stat(cwd, &stat_struct) == -1 ) {
			dprintf(D_ALWAYS, 
				"Create_Process: Specified cwd %s cannot be found.\n",
				cwd);	   
			if ( priv != PRIV_UNKNOWN ) {
				set_priv( current_priv );
			}	
			return FALSE;
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
		return FALSE;
	}

	newpid = fork();
	if( newpid == 0 ) // Child Process
	{
			// close the read end of our error pipe and set the
			// close-on-exec flag on the write end
		close(errorpipe[0]);
		fcntl(errorpipe[1], F_SETFD, FD_CLOEXEC);

			// make the args / env  into something we can use:

		char **unix_env;
		char **unix_args;

		if( (env == NULL) || (env[0] == 0) ) {
			dprintf(D_DAEMONCORE, "Create_Process: Env: NULL\n", env);
			unix_env = new char*[1];
			unix_env[0] = 0;
		}
		else {
			dprintf(D_DAEMONCORE, "Create_Process: Env: %s\n", env);
			unix_env = ParseEnvArgsString(env, true);
		}
		
		if( (args == NULL) || (args[0] == 0) ) {
			dprintf(D_DAEMONCORE, "Create_Process: Arg: NULL\n", args);
			unix_args = new char*[2];
			unix_args[0] = new char[strlen(name)+1];
			strcpy ( unix_args[0], name );
			unix_args[1] = 0;
		}
		else {
			dprintf(D_DAEMONCORE, "Create_Process: Arg: %s\n", args);
			unix_args = ParseEnvArgsString(args, false);
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

				char nametemp[_POSIX_PATH_MAX];
				strcpy( nametemp, name );
				strcpy( name, currwd );
				strcat( name, "/" );
				strcat( name, nametemp );
				
				dprintf ( D_DAEMONCORE, "Full path exec name: %s\n", name );
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
			}
			if ( std[1] > -1 ) {
				if ( ( dup2 ( std[1], 1 ) ) == -1 ) {
					dprintf( D_ALWAYS, "dup2 of std[1] failed, errno %d\n", 
							 errno );
				}
				close( std[1] );  // We don't need this in the child...
			}
			if ( std[2] > -1 ) {
				if ( ( dup2 ( std[2], 2 ) ) == -1 ) {
					dprintf( D_ALWAYS, "dup2 of std[2] failed, errno %d\n", 
							 errno );
				}
				close( std[2] );  // We don't need this in the child...
			}
		}
        else {
                // if we don't want to re-map these, close 'em.
			dprintf ( D_DAEMONCORE, "Just closed fd " );
            for ( int q=0 ; (q<openfds) && (q<3) ; q++ ) {
                if ( close ( q ) != -1 ) {
                    dprintf ( D_DAEMONCORE | D_NOHEADER, "%d ", q );
                }
            }
			dprintf ( D_DAEMONCORE | D_NOHEADER, "\n" );
        }

        dprintf ( D_DAEMONCORE, "Printing fds to inherit: " );
        for ( int a=0 ; a<numInheritSockFds ; a++ ) {
            dprintf ( D_DAEMONCORE | D_NOHEADER, 
					  "%d ", inheritSockFds[a] );
        }
        dprintf (  D_DAEMONCORE | D_NOHEADER, "\n" );

			// Now we want to close all fds that aren't in sock_inherit_list
		bool found;
		dprintf ( D_DAEMONCORE, "Just closed fd " );
		for ( int j=3 ; j < openfds ; j++ ) {
			if ( j == errorpipe[1] ) continue; // don't close our errorpipe!
			found = FALSE;
			for ( int k=0 ; k < numInheritSockFds ; k++ ) {
                if ( inheritSockFds[k] == j ) {
					found = TRUE;
					break;
				}
			}
			if ( !found ) {
				if ( close( j ) != -1 ) {
                    dprintf ( D_DAEMONCORE | D_NOHEADER, "%d ", j );
                }
            }
		}
		dprintf ( D_DAEMONCORE | D_NOHEADER, "\n" );

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
		if( !SetEnv("CONDOR_INHERIT", inheritbuf) ) {
			dprintf(D_ALWAYS, "Create_Process: Failed to set "
					"CONDOR_INHERIT env.\n");
				// before we exit, make sure our parent knows something
				// went wrong before the exec...
			write(errorpipe[1], &errno, sizeof(errno));
			exit(errno); // Yes, we really want to exit here.
		}

            /* Re-nice ourself */
        if ( nice_inc > 0 && nice_inc < 20 ) {
            dprintf ( D_FULLDEBUG, "calling nice(%d)\n", nice_inc );
            nice( nice_inc );
        }
        else if ( nice_inc >= 20 ) {
            dprintf ( D_FULLDEBUG, "calling nice(19)\n" );
            nice( 19 );
        }

#if defined( Solaris ) && defined( sun4m )
			/* The following will allow purify to pop up windows 
			   for all daemons created with Create_Process().  The
			   -program-name is so purify can find the executable.
               Note the super-secret PURIFY_DISPLAY param.
			*/

        char *display;
        display = param ( "PURIFY_DISPLAY" );
        if ( display ) {
            SetEnv( "DISPLAY", display );
            free ( display );
            char purebuf[256]; 
            sprintf ( purebuf, "-program-name=%s", name );
            SetEnv( "PUREOPTIONS", purebuf );
        }
#endif

		dprintf ( D_DAEMONCORE, "About to exec \"%s\"\n", name );

			// now head into the proper priv state...
		if ( priv != PRIV_UNKNOWN ) {
			set_priv(priv);
		}

			/* No dprintfs allowed after this! */

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

			// and ( finally ) exec:
		if( execv(name, unix_args) == -1 )
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
			dprintf(D_ALWAYS, "Create_Process: child failed with errno %d (%s)"
					"before exec().\n", errno, strerror(errno));
			close(errorpipe[0]);
			return FALSE;
		}
		close(errorpipe[0]);
		
	}
	else if( newpid < 0 )// Error condition
	{
		dprintf(D_ALWAYS, "Create Process: fork() failed: %s (%d)\n", 
				strerror(errno), errno );
		close(errorpipe[0]); close(errorpipe[1]);
		return FALSE;
	}
#endif

	// Now that we have a child, store the info in our pidTable
	PidEntry *pidtmp = new PidEntry;
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
#endif 
	assert( pidTable->insert(newpid,pidtmp) == 0 );  
	dprintf(D_DAEMONCORE,
		"Child Process: pid %lu at %s\n",newpid,pidtmp->sinful_string);
#ifdef WIN32
	WatchPid(pidtmp);
#endif

	// Now that child exists, we (the parent) should close up our copy of
	// the childs command listen cedar sockets.  Since these are on
	// the stack (rsock and ssock), they will get closed when we return.
	
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
	int tid;
	tid = fork();
	if (tid == 0) {				// new thread (i.e., child process)
		exit(start_func(arg, sock));
	} else if (tid < 0) {
		dprintf(D_ALWAYS, "Create_Thread: fork() failed: %s (%d)\n",
				strerror(errno), errno);
		return FALSE;
	}
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
#else
	pidtmp->pid = tid;
#endif 
	assert( pidTable->insert(tid,pidtmp) == 0 );  
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
	char inheritbuf[_INHERITBUF_MAXSIZE];
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
	inheritbuf[0] = '\0';
#ifdef WIN32
	if (GetEnvironmentVariable("CONDOR_INHERIT",inheritbuf,
							   _INHERITBUF_MAXSIZE) > _INHERITBUF_MAXSIZE-1) {
		EXCEPT("CONDOR_INHERIT too large");
	}
#else
	ptmp = getenv("CONDOR_INHERIT");
	if ( ptmp ) {
		if ( strlen(ptmp) > _INHERITBUF_MAXSIZE-1 ) {
			EXCEPT("CONDOR_INHERIT too large");
		}
		dprintf ( D_DAEMONCORE, "CONDOR_INHERIT: \"%s\"\n", ptmp );
		strncpy(inheritbuf,ptmp,_INHERITBUF_MAXSIZE);
	} else {
		dprintf ( D_DAEMONCORE, "CONDOR_INHERIT: is NULL\n", ptmp );
	}		
#endif

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
		pidtmp->hProcess = ::OpenProcess( SYNCHRONIZE | 
						PROCESS_QUERY_INFORMATION | STANDARD_RIGHTS_REQUIRED , 
						FALSE, ppid );
		assert(pidtmp->hProcess);
		pidtmp->hThread = NULL;		// do not allow child to suspend parent
#endif
		assert( pidTable->insert(ppid,pidtmp) == 0 );
#ifdef WIN32
		WatchPid(pidtmp);
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


	assert( sig == DC_SIGCHLD );

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
#endif // of ifndef WIN32


#ifdef WIN32
// This function runs in a seperate thread and wathces over children
unsigned
pidWatcherThread( void* arg )
{
	DaemonCore::PidWatcherEntry* entry;
	int i;
	unsigned int numentries;
	DWORD result;
	int sent_result;
	HANDLE hKids[MAXIMUM_WAIT_OBJECTS];
	int last_pidentry_exited = MAXIMUM_WAIT_OBJECTS + 5;
	unsigned int exited_pid;

	entry = (DaemonCore::PidWatcherEntry *) arg;

	for (;;) {
	
	::EnterCriticalSection(&(entry->crit_section));
	numentries = 0;
	for (i=0; i < entry->nEntries; i++ ) {
		if ( i != last_pidentry_exited ) {
			hKids[numentries] = entry->pidentries[i]->hProcess;
			// if process handle is NULL, it is really a thread
			if ( hKids[numentries] == NULL ) {
				// this is a thread entry, not a process entry
				hKids[numentries] = entry->pidentries[i]->hThread;
			}
			entry->pidentries[numentries] = entry->pidentries[i];
			numentries++;
		} 
	}
	hKids[numentries] = entry->event;
	entry->nEntries = numentries;
	::LeaveCriticalSection(&(entry->crit_section));

	// if there are no more entries to watch, we're done.
	if ( numentries == 0 )
		return TRUE;	// this return will kill this thread

	result = ::WaitForMultipleObjects(numentries + 1, hKids, FALSE, INFINITE);
	if ( result == WAIT_FAILED ) {
		EXCEPT("WaitForMultipleObjects Failed");
	}
	result = result - WAIT_OBJECT_0;

	// if result = numentries, then we are being told our entry->pidentries
	// array has been modified by another thread, and we should re-read it.
	// if result < numentries, then result signifies a child process 
	// which exited.
	if ( (result < numentries) && (result >= 0) ) {
		// notify our main thread which process exited
		// note: if it was a thread which exited, the entry's 
		// pid contains the tid
		exited_pid = entry->pidentries[result]->pid;	
		SafeSock sock;
		// Can no longer use localhost (127.0.0.1) here because we may
		// only have our command socket bound to a specific address. -Todd
		// sock.connect("127.0.0.1",daemonCore->InfoCommandPort());		
		sock.connect(daemonCore->InfoCommandSinfulString());
		sock.encode();
		sent_result = FALSE;
		while ( sent_result == FALSE ) {
			if ( !sock.snd_int(DC_PROCESSEXIT,FALSE) ||
				 !sock.code(exited_pid) ||
				 !sock.end_of_message() ) {
				// failed to get the notification off to the main thread.
				// we'll log a message, wait a bit, and try again
				dprintf(D_ALWAYS,
						"PidWatcher thread couldn't notify main thread\n");
				::Sleep(500);	// sleep for a half a second (500 ms)
			} else {
				sent_result = TRUE;
				last_pidentry_exited = result;
			}
		}
	} else {
		// no pid exited, we were signaled because our 
		// pidentries array was modified.
		// we must clear last_pidentry_exited.
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

#define EXCEPTION( x ) case EXCEPTION_##x: return 0;
int 
WIFEXITED(DWORD stat) 
{
	switch (stat) {
		EXCEPTION( ACCESS_VIOLATION )
        EXCEPTION( DATATYPE_MISALIGNMENT )        
		EXCEPTION( BREAKPOINT )
        EXCEPTION( SINGLE_STEP )        
		EXCEPTION( ARRAY_BOUNDS_EXCEEDED )
        EXCEPTION( FLT_DENORMAL_OPERAND )        
		EXCEPTION( FLT_DIVIDE_BY_ZERO )
        EXCEPTION( FLT_INEXACT_RESULT )
        EXCEPTION( FLT_INVALID_OPERATION )        
		EXCEPTION( FLT_OVERFLOW )
        EXCEPTION( FLT_STACK_CHECK )        
		EXCEPTION( FLT_UNDERFLOW )
        EXCEPTION( INT_DIVIDE_BY_ZERO )        
		EXCEPTION( INT_OVERFLOW )
        EXCEPTION( PRIV_INSTRUCTION )        
		EXCEPTION( IN_PAGE_ERROR )
        EXCEPTION( ILLEGAL_INSTRUCTION )
        EXCEPTION( NONCONTINUABLE_EXCEPTION )        
		EXCEPTION( STACK_OVERFLOW )
        EXCEPTION( INVALID_DISPOSITION )        
		EXCEPTION( GUARD_PAGE )
        EXCEPTION( INVALID_HANDLE )
	}
	return 1;
}

int 
WIFSIGNALED(DWORD stat) 
{
	if ( WIFEXITED(stat) )
		return 0;
	else
		return 1;
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
		// we did not find this pid... probably popen finished.
		// log a message and return FALSE.

		// temporary hack: if we are the startd, and we do not want DC_PM,
		// then create a temp pidentry to call reaper #1.  This hack should
		// be removed once the startd is switched over to use DC_PM.
		if ( strcmp(mySubSystem,"STARTD") == 0 ) {
			pidentry = new PidEntry;
			ASSERT(pidentry);
			pidentry->parent_is_local = TRUE;
			pidentry->reaper_id = 1;
			pidentry->hung_tid = -1;
		} else {
			dprintf(D_DAEMONCORE,
				"Unknown process exited (popen?) - pid=%d\n",pid);
			return FALSE;
		}
			
	}

	// If process is Unix, we are passed the exit status.
	// If process is NT and is remote, we are passed the exit status.
	// If process is NT and is local, we need to fetch the exit status here.
#ifdef WIN32
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
		if ( (i >= 0) && ((reapTable[i].handler != NULL) || 
			 (reapTable[i].handlercpp != NULL)) ) {
			// Set curr_dataptr for Get/SetDataPtr()
			curr_dataptr = &(reapTable[i].data_ptr);

			// Log a message
			char *hdescrip = reapTable[i].handler_descrip;
			if ( !hdescrip )
				hdescrip = EMPTY_DESCRIP;
			dprintf(D_DAEMONCORE,
				"DaemonCore: %s %lu exited with status %d, invoking reaper %d <%s>\n",
				whatexited,pid,exit_status,i+1,hdescrip);

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
				whatexited,pid,exit_status);
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
				"Our Parent process (pid %lu) exited; shutting down\n",pid);
		Send_Signal(mypid,DC_SIGTERM);	// SIGTERM means shutdown graceful
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
		Register_DataPtr( (void *)child_pid );
	}	

	pidentry->was_not_responding = FALSE;
	
	dprintf(D_DAEMONCORE,
		"received childalive, pid=%d, secs=%d\n",child_pid,timeout_secs);

	return TRUE;

}

int DaemonCore::HungChildTimeout()
{
	pid_t hung_child_pid;
	PidEntry *pidentry;

	hung_child_pid = (pid_t) GetDataPtr();

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
	char *parent_sinfull_string;
	int alive_command = DC_CHILDALIVE;

	dprintf(D_FULLDEBUG,"DaemonCore: in SendAliveToParent()\n");
	parent_sinfull_string = InfoCommandSinfulString(ppid);	
	if (!parent_sinfull_string ) {
		return FALSE;
	}
	
	if (!sock.connect(parent_sinfull_string)) {
		return FALSE;
	}
	
	dprintf( D_DAEMONCORE, "DaemonCore: Sending alive to %s\n",
			 parent_sinfull_string );

	sock.encode();
	sock.code(alive_command);
	sock.code(mypid);
	sock.code(max_hang_time);
	sock.end_of_message();

	return TRUE;
}
	
#ifdef WIN32
char *DaemonCore::ParseEnvArgsString(char *incoming, char *inheritbuf)
{
	int len;
	char *answer;

	if ( incoming == NULL )
		return NULL;

	if ( inheritbuf ) {
		len = strlen(incoming) + strlen(inheritbuf) + 20;
		answer = (char *)malloc(len);
		strcpy(answer,incoming);
		strcat(answer,"|CONDOR_INHERIT=");
		strcat(answer,inheritbuf);
	} else {
		len = strlen(incoming) + 2;
		answer = (char *)malloc(len);
		strcpy(answer,incoming);		
	}
	answer[ strlen(answer)+1 ] = '\0';

	char *temp = answer;
	while( (temp = strchr(temp, '|') ) )
	{
		*temp = '\0';
		temp++;
	}
	return answer;
}
#else
char **DaemonCore::ParseEnvArgsString(char *incomming, bool env)
{
	char seperator;
	char **argv = 0;
	char *temp = 0;
	char *cur = 0;
	int num_args = 0;
	int i = 0;
	int length = 0;

	if(env) {
		seperator = ';';
	} else {
		seperator = ' ';
	}
	// Count the number of arguments
	temp = incomming;
	do {
		num_args++;
		temp++;
	} while( ( temp = strchr(temp, seperator) ) != NULL);

	argv = new char *[num_args + 1];

	cur = incomming;
	while( (temp = strchr(cur, seperator) ) )
	{
		length = temp - cur;
		argv[i] = new char[length + 1]; 
		strncpy(argv[i], cur, length);
		argv[i][length] = 0;
		cur = temp + 1;
		i++;
	}

	argv[i] = new char[strlen(cur) + 1];
	strcpy(argv[i], cur);
	
	argv[num_args] = 0;

	return argv;
}
#endif

int
BindAnyCommandPort(ReliSock *rsock, SafeSock *ssock)
{
	for(int i = 0; i < 1000; i++) {
		if ( !rsock->bind() ) {
			dprintf(D_ALWAYS, "Failed to bind to command ReliSock\n");
			return FALSE;
		}
		// now open a SafeSock _on the same port_ choosen above
		if ( !ssock->bind(rsock->get_port()) ) {
			dprintf(D_ALWAYS, "Failed to bind SafeSock to %d\n", rsock->get_port());
			rsock->close();
			continue;
		}
		return TRUE;
	}
	return FALSE;
}

// Is_Pid_Alive() returns TRUE is pid lives, FALSE is that pid has exited.
int DaemonCore::Is_Pid_Alive(pid_t pid)
{
	int status = FALSE;

#ifndef WIN32
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
