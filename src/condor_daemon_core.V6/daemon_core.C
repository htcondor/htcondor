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
// Implementation of DaemonCore.
//
//
////////////////////////////////////////////////////////////////////////////////

#include "condor_common.h"

static const int DEFAULT_MAXCOMMANDS = 255;
static const int DEFAULT_MAXSIGNALS = 99;
static const int DEFAULT_MAXSOCKETS = 8;
static const int DEFAULT_MAXREAPS = 8;
static const int DEFAULT_PIDBUCKETS = 8;
static const char* DEFAULT_INDENT = "DaemonCore--> ";


#include "condor_timer_manager.h"
#include "condor_daemon_core.h"
#include "condor_io.h"
#include "internet.h"
#include "condor_debug.h"
#include "get_daemon_addr.h"

#if defined(GSS_AUTHENTICATION)
#include "auth_sock.h"
#else
#define AuthSock ReliSock
#endif

static	char*  	_FileName_ = __FILE__;	// used by EXCEPT

extern "C" 
{
	char*			sin_to_string(struct sockaddr_in*);
}


TimerManager DaemonCore::t;

// Hash function for pid table.
static int compute_pid_hash(const pid_t &key, int numBuckets) 
{
	return ( key % numBuckets );
}

// DaemonCore constructor. 

DaemonCore::DaemonCore(int PidSize, int ComSize,int SigSize,int SocSize,int ReapSize)
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

	sockTable = new SockEnt[maxSocket];
	if(sockTable == NULL)
	{
		EXCEPT("Out of memory!");
	}
	nSock = 0;
	memset(sockTable,'\0',maxSocket*sizeof(SockEnt));

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

	reinit_timer_id = -1;

#ifdef WIN32
	dcmainThreadId = ::GetCurrentThreadId();
#endif

#ifndef WIN32
	async_sigs_unblocked = FALSE;
#endif
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
		for (i=0;i<maxSocket;i++) {
			free_descrip( sockTable[i].iosock_descrip );
			free_descrip( sockTable[i].handler_descrip );
		}
		delete []sockTable;
	}

	if (reapTable != NULL)
	{
		for (i=0;i<maxReap;i++) {
			free_descrip( reapTable[i].reap_descrip );
			free_descrip( reapTable[i].handler_descrip );
		}
		delete []reapTable;
	}

	t.CancelAllTimers();
}

/********************************************************
 Here are a bunch of public methods with parameter overloading.  These methods here
 just call the actual method implementation with a default parameter set.
 ********************************************************/
int	DaemonCore::Register_Command(int command, char* com_descrip, CommandHandler handler, 
								char* handler_descrip, Service* s, DCpermission perm) {
	return( Register_Command(command, com_descrip, handler, (CommandHandlercpp)NULL, handler_descrip, s, perm, FALSE) );
}

int	DaemonCore::Register_Command(int command, char *com_descrip, CommandHandlercpp handlercpp, 
								char* handler_descrip, Service* s, DCpermission perm) {
	return( Register_Command(command, com_descrip, NULL, handlercpp, handler_descrip, s, perm, TRUE) ); 
}

int	DaemonCore::Register_Signal(int sig, char* sig_descrip, SignalHandler handler, 
								char* handler_descrip, Service* s, DCpermission perm) {
	return( Register_Signal(sig, sig_descrip, handler, (SignalHandlercpp)NULL, handler_descrip, s, perm, FALSE) );
}

int	DaemonCore::Register_Signal(int sig, char *sig_descrip, SignalHandlercpp handlercpp, 
								char* handler_descrip, Service* s, DCpermission perm) {
	return( Register_Signal(sig, sig_descrip, NULL, handlercpp, handler_descrip, s, perm, TRUE) ); 
}

int	DaemonCore::Register_Socket(Stream* iosock, char* iosock_descrip, SocketHandler handler, 
								char* handler_descrip, Service* s, DCpermission perm) {
	return( Register_Socket(iosock, iosock_descrip, handler, (SocketHandlercpp)NULL, handler_descrip, s, perm, FALSE) );
}

int	DaemonCore::Register_Socket(Stream* iosock, char* iosock_descrip, SocketHandlercpp handlercpp, 
								char* handler_descrip, Service* s, DCpermission perm) {
	return( Register_Socket(iosock, iosock_descrip, NULL, handlercpp, handler_descrip, s, perm, TRUE) ); 
}

int	DaemonCore::Register_Reaper(char* reap_descrip, ReaperHandler handler, 
								char* handler_descrip, Service* s) {
	return( Register_Reaper(-1, reap_descrip, handler, (ReaperHandlercpp)NULL, handler_descrip, s, FALSE) );
}

int	DaemonCore::Register_Reaper(char* reap_descrip, ReaperHandlercpp handlercpp, 
								char* handler_descrip, Service* s) {
	return( Register_Reaper(-1, reap_descrip, NULL, handlercpp, handler_descrip, s, TRUE) ); 
}

int	DaemonCore::Reset_Reaper(int rid, char* reap_descrip, ReaperHandler handler, 
								char* handler_descrip, Service* s) {
	return( Register_Reaper(rid, reap_descrip, handler, (ReaperHandlercpp)NULL, handler_descrip, s, FALSE) );
}

int	DaemonCore::Reset_Reaper(int rid, char* reap_descrip, ReaperHandlercpp handlercpp, 
								char* handler_descrip, Service* s) {
	return( Register_Reaper(rid, reap_descrip, NULL, handlercpp, handler_descrip, s, TRUE) ); 
}

int	DaemonCore::Register_Timer(unsigned deltawhen, Event event, char *event_descrip, 
					   Service* s) {
	return( t.NewTimer(s, deltawhen, event, event_descrip, 0, -1) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, unsigned period, Event event, char *event_descrip, 
					   Service* s) {
	return( t.NewTimer(s, deltawhen, event, event_descrip, period, -1) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, Eventcpp eventcpp, char *event_descrip, 
					   Service* s) {
	return( t.NewTimer(s, deltawhen, eventcpp, event_descrip, 0, -1) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, unsigned period, Eventcpp event, 
				   char *event_descrip, Service* s ) {
	return( t.NewTimer(s, deltawhen, event, event_descrip, period, -1) );
}
	
int	DaemonCore::Cancel_Timer( int id ) {
	return( t.CancelTimer(id) );
}

int DaemonCore::Reset_Timer( int id, unsigned when, unsigned period ) {
	return( t.ResetTimer(id,when,period) );
}

/************************************************************************/


int DaemonCore::Register_Command(int command, char* command_descrip, CommandHandler handler, 
					CommandHandlercpp handlercpp, char *handler_descrip, Service* s, 
					DCpermission perm, int is_cpp)
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
            if ((comTable[j].handler == NULL) && (comTable[j].handlercpp == NULL))
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
	return( sockTable[initial_command_sock].iosock->get_port() );
}

char * DaemonCore::InfoCommandSinfulString()
{
	static char *result = NULL;

	if ( initial_command_sock == -1 ) {
		// there is no command sock!
		return NULL;
	}

	if ( result == NULL )
		result = strdup( sock_to_string( sockTable[initial_command_sock].sockd ) );
	
	return result;
}

int DaemonCore::Register_Signal(int sig, char* sig_descrip, SignalHandler handler, 
					SignalHandlercpp handlercpp, char* handler_descrip, Service* s,	
					DCpermission perm, int is_cpp)
{
    int     i;		// hash value
    int     j;		// for linear probing
    
    
    if( handler == NULL && handlercpp == NULL) {
		dprintf(D_DAEMONCORE, "Can't register NULL signal handler\n");
		return -1;
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

	// See if our hash landed on an empty bucket...  We identify an empty bucket 
	// by checking of there is a handler (or a c++ handler) defined; if there is no
	// handler, then it is an empty entry.
    if ( (sigTable[i].handler != NULL) || (sigTable[i].handlercpp != NULL) ) {
		// occupied
        if(sigTable[i].num == sig) {
			// by the same signal
			EXCEPT("DaemonCore: Same signal registered twice");
        }
		// by some other signal, so scan thru the entries to
		// find the first empty one
        for(j = (i + 1) % maxSig; j != i; j = (j + 1) % maxSig) {
            if ((sigTable[j].handler == NULL) && (sigTable[j].handlercpp == NULL))
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
	dprintf(D_DAEMONCORE,"Cancel_Signal: cancelled signal %d <%s>\n",sig,sigTable[found].sig_descrip);
	free_descrip( sigTable[found].sig_descrip );
	sigTable[found].sig_descrip = NULL;

	DumpSigTable(D_FULLDEBUG | D_DAEMONCORE);

	return TRUE;
}

int DaemonCore::Register_Socket(Stream *iosock, char* iosock_descrip, SocketHandler handler, 
					SocketHandlercpp handlercpp, char *handler_descrip, Service* s, 
					DCpermission perm, int is_cpp)
{
    int     i;	
    int     j;	
    
    // In sockTable, unlike the others handler tables, we allow for a NULL handler
	// and a NULL handlercpp - this means a command socket, so use
	// the default daemon core socket handler which strips off the command.
	// SO, a blank table entry is defined as a NULL iosock field.

	// And since FD_ISSET only allows us to probe, we do not bother using a 
	// hash table for sockets.  We simply store them in an array.  

    if ( !iosock ) {
		dprintf(D_DAEMONCORE, "Can't register NULL socket \n");
		return -1;
    }
    
    if(nSock >= maxSocket) {
		EXCEPT("# of socket handlers exceeded specified maximum");
    }

	i = nSock;

	// Make certain that entry i is empty.
	if ( sockTable[i].iosock ) {
		EXCEPT("DaemonCore: Socket table messed up");
	}

	// Verify that this socket has not already been registered
	for ( j=0; j < maxSocket; j++ ) {
		if ( sockTable[j].iosock == iosock ) {
			EXCEPT("DaemonCore: Same socket registered twice");
        }
	}

	// Found a blank entry at index i. Now add in the new data.
	sockTable[i].iosock = iosock;
	switch ( iosock->type() ) {
		case Stream::reli_sock :
			sockTable[i].sockd = ((AuthSock *)iosock)->get_file_desc();
			break;
		case Stream::safe_sock :
			sockTable[i].sockd = ((SafeSock *)iosock)->get_file_desc();
			break;
		default:
			EXCEPT("Adding CEDAR socket of unknown type\n");
			break;
	}
	sockTable[i].handler = handler;
	sockTable[i].handlercpp = handlercpp;
	sockTable[i].is_cpp = is_cpp;
	sockTable[i].perm = perm;
	sockTable[i].service = s;
	sockTable[i].data_ptr = NULL;
	free_descrip(sockTable[i].iosock_descrip);
	if ( iosock_descrip )
		sockTable[i].iosock_descrip = strdup(iosock_descrip);
	else
		sockTable[i].iosock_descrip = EMPTY_DESCRIP;
	free_descrip(sockTable[i].handler_descrip);
	if ( handler_descrip )
		sockTable[i].handler_descrip = strdup(handler_descrip);
	else
		sockTable[i].handler_descrip = EMPTY_DESCRIP;

	// Increment the counter of total number of entries
	nSock++;

	// If this is the first command sock, set initial_command_sock
	// NOTE: When we remove sockets, the intial_command_sock can change!
	if ( initial_command_sock == -1 && handler == NULL && handlercpp == NULL )
		initial_command_sock = i;

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &(sockTable[i].data_ptr);

	// Conditionally dump what our table looks like
	DumpSocketTable(D_FULLDEBUG | D_DAEMONCORE);

	return i;
}


int DaemonCore::Cancel_Socket( Stream* insock)
{
	int i,j;

	i = -1;
	for (j=0;j<nSock;j++) {
		if ( sockTable[j].iosock == insock ) {
			i = j;
			break;
		}
	}

	if ( i == -1 ) {
		dprintf(D_ALWAYS,"Cancel_Socket: called on non-registered socket!\n");
		return FALSE;
	}

	// Remove entry at index i by moving the last one in the table here.

	// Clear any data_ptr which go to this entry we just removed
	if ( curr_regdataptr == &(sockTable[i].data_ptr) )
		curr_regdataptr = NULL;
	if ( curr_dataptr == &(sockTable[i].data_ptr) )
		curr_dataptr = NULL;

	// Log a message
	dprintf(D_DAEMONCORE,"Cancel_Socket: cancelled socket %d <%s>\n",i,sockTable[i].iosock_descrip);
	
	// Remove entry, move the last one in the list into this spot
	sockTable[i].iosock = NULL;
	free_descrip( sockTable[i].iosock_descrip );
	sockTable[i].iosock_descrip = NULL;
	free_descrip( sockTable[i].handler_descrip );
	sockTable[i].handler_descrip = NULL;
	if ( i < nSock - 1 ) {	// if not the last entry in the table, move the last one here
		sockTable[i] = sockTable[nSock - 1];
		sockTable[nSock - 1].iosock = NULL;
		sockTable[nSock - 1].iosock_descrip = NULL;
		sockTable[nSock - 1].handler_descrip = NULL;
	}
	nSock--;

	DumpSocketTable(D_FULLDEBUG | D_DAEMONCORE);

	return TRUE;
}

int DaemonCore::Register_Reaper(int rid, char* reap_descrip, ReaperHandler handler, 
					ReaperHandlercpp handlercpp, char *handler_descrip, Service* s, 
					int is_cpp)
{
    int     i;	
    int     j;	
    
    // In reapTable, unlike the others handler tables, we allow for a NULL handler
	// and a NULL handlercpp - this means just reap with no handler, so use
	// the default daemon core reaper handler which reaps the exit status on unix
	// and frees the handle on Win32.

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
		// scan thru table to find a new entry. scan in such a way that we do not
		// re-use rid's until we have to.
		for(i = nReap % maxReap, j=0; j < maxReap; j++, i = (i + 1) % maxReap) {
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
		if (sockTable[i].iosock == insock) {
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
		if ((comTable[i].handler != NULL) || (comTable[i].handlercpp != NULL)) {
			descrip1 = "NULL";
			descrip2 = descrip1;
			if ( comTable[i].command_descrip )
				descrip1 = comTable[i].command_descrip;
			if ( comTable[i].handler_descrip )
				descrip2 = comTable[i].handler_descrip;
			dprintf(flag, "%s%d: %s %s\n", indent, comTable[i].num, descrip1, descrip2);
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
		if ((reapTable[i].handler != NULL) || (reapTable[i].handlercpp != NULL)) {
			descrip1 = "NULL";
			descrip2 = descrip1;
			if ( reapTable[i].reap_descrip )
				descrip1 = reapTable[i].reap_descrip;
			if ( reapTable[i].handler_descrip )
				descrip2 = reapTable[i].handler_descrip;
			dprintf(flag, "%s%d: %s %s\n", indent, reapTable[i].num, descrip1, descrip2);
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
		if ( (sigTable[i].handler != NULL) || (sigTable[i].handlercpp != NULL) ) {
			descrip1 = "NULL";
			descrip2 = descrip1;
			if ( sigTable[i].sig_descrip )
				descrip1 = sigTable[i].sig_descrip;
			if ( sigTable[i].handler_descrip )
				descrip2 = sigTable[i].handler_descrip;
			dprintf(flag, "%s%d: %s %s, Blocked:%d Pending:%d\n", indent, sigTable[i].num, 
				descrip1, descrip2, sigTable[i].is_blocked, sigTable[i].is_pending);
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
		if ( sockTable[i].iosock ) {
			descrip1 = "NULL";
			descrip2 = descrip1;
			if ( sockTable[i].iosock_descrip )
				descrip1 = sockTable[i].iosock_descrip;
			if ( sockTable[i].handler_descrip )
				descrip2 = sockTable[i].handler_descrip;
			dprintf(flag, "%s%d: %s %s\n", indent, i, descrip1, descrip2);
		}
	}
	dprintf(flag, "\n");
}

int
DaemonCore::ReInit()
{
	char *addr;
	struct sockaddr_in sin;

	// Fetch the negotiator address for the Verify method to use
	addr = get_negotiator_addr(NULL);	// get sinful string of negotiator
	if ( addr ) {
		string_to_sin(addr,&sin);
		memcpy(&negotiator_sin_addr,&(sin.sin_addr),sizeof(negotiator_sin_addr));
	} else {
		// we failed to get the address of the negotiator
		memset(&negotiator_sin_addr,'\0',sizeof(negotiator_sin_addr));
	}

	// Reset our IpVerify object
	ipverify.Init();

	// Setup a timer to call ourselves (ReInit) again in eight hours.  This
	// is a safeguard in case DNS addresses change and we are caching them.
	if ( reinit_timer_id != -1 ) {
		Cancel_Timer( reinit_timer_id );
	}
	reinit_timer_id = Register_Timer( 8 * 60 * 60, (Eventcpp)ReInit,
		"DaemonCore::ReInit", this);

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
		sigdelset(&fullset, SIGABRT);    // so assert() failures drop core right away
		sigdelset(&fullset, SIGILL);     // so we get a core right away
		sigdelset(&fullset, SIGBUS);     // so we get a core right away
	sigemptyset( &emptyset );
	char asyncpipe_buf[10];
#endif

	for(;;)
	{
		// TODO Call Reaper handlers for exited children

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
					dprintf(D_DAEMONCORE,"Calling Handler <%s> for Signal %d <%s>\n",
						sigTable[i].handler_descrip,sigTable[i].num,sigTable[i].sig_descrip);
					// call the handler
					if ( sigTable[i].is_cpp ) 
						(sigTable[i].service->*(sigTable[i].handlercpp))(sigTable[i].num);
					else
						(*sigTable[i].handler)(sigTable[i].service,sigTable[i].num);
					// Clear curr_dataptr
					curr_dataptr = NULL;
				}
			}
		}
#ifndef WIN32
		// Drain our async_pipe; we must do this before we unblock unix signals...
		// Just keep reading while something is there.  async_pipe is set to 
		// non-blocking more via fcntl, so the read below will not block.
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
		//   and service this outstanding signal and yet we do not starve commands...
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
		for (i = 0; i < nSock; i++) {
			if ( sockTable[i].iosock )	// if a valid entry....
				FD_SET(sockTable[i].sockd,&readfds);
		}

		
#if !defined(WIN32)
		// Add the read side of async_pipe to the list of file descriptors to
		// select on.  We write to async_pipe if a unix async signal is delivered
		// after we unblock signals and before we block on select.
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
		rv = select(FD_SETSIZE, (SELECT_FDSET_PTR) &readfds, NULL, NULL, ptimer);
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
				if ( sockTable[i].iosock ) {	// if a valid entry...
					if (FD_ISSET(sockTable[i].sockd, &readfds)) {
						// ok, select says this socket table entry has new data.
						// if the user provided a handler for this socket, then
						// call it now.  otherwise, call the daemoncore HandleReq()
						// handler which strips off the command request number and
						// calls any registered command handler.

						// Update curr_dataptr for GetDataPtr()
						curr_dataptr = &(sockTable[i].data_ptr);
						if ( sockTable[i].handler )
							// a C handler
							result = (*(sockTable[i].handler))(sockTable[i].service,sockTable[i].iosock);
						else
						if ( sockTable[i].handlercpp )
							// a C++ handler
							result = (sockTable[i].service->*(sockTable[i].handlercpp))(sockTable[i].iosock);
						else
							// no handler registered, so this is a command socket.  call
							// the DaemonCore handler which takes care of command sockets.
							result = HandleReq(i);
						
						// Clear curr_dataptr
						curr_dataptr = NULL;

						// Check result from socket handler, and if not KEEP_STREAM, then
						// delete the socket and the socket handler.
						if ( result != KEEP_STREAM ) {
							// delete the cedar socket
							delete sockTable[i].iosock;
							// cancel the socket handler
							Cancel_Socket( sockTable[i].iosock );
							// decrement i, since sockTable[i] may now point to a new valid socket
							i--;
						}

					}	// if FD_ISSET
				}	// if valid entry in sockTable
			}	// for 0 thru nSock
		}	// if rv > 0

	}	// end of infinite for loop
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

	
	insock = sockTable[socki].iosock;

	switch ( insock->type() ) {
		case Stream::reli_sock :
			is_tcp = TRUE;
			break;
		case Stream::safe_sock :
			is_tcp = FALSE;
			break;
		default:
			// unrecognized Stream sock
			dprintf(D_ALWAYS,"DaemonCore: HandleReq(): unrecognized Stream sock\n");
			return FALSE;
	}

	// set up a connection for a tcp socket	
	if ( is_tcp ) {

		// if the connection was received on a listen socket, do an accept.
		if ( ((AuthSock *)insock)->_state == Sock::sock_special &&
			((AuthSock *)insock)->_special_state == AuthSock::relisock_listen ) {
			stream = (Stream *) ((AuthSock *)insock)->accept();
			if ( !stream ) {
				dprintf(D_ALWAYS, "DaemonCore: accept() failed!");
				return KEEP_STREAM;		// return KEEP_STEAM cuz insock is a listen socket
			}
		} 
		// if the not a listen socket, then just assign stream to insock
		else {
			stream = insock;
		}

		dprintf( D_COMMAND, "DaemonCore: Command received via TCP from %s\n", 
				 sin_to_string(stream->endpoint()) );

	}
	// set up a connection for a udp socket
	else {
		// on UDP, we do not have a seperate listen and accept sock.
		// our "listen sock" is also our "accept" sock, so just
		// assign stream to the insock. UDP = connectionless, get it?
		stream = insock;
		// in UDP we cannot display who the command is from until we read something off 
		// the socket, so we display who from after we read the command below...
	}
	
	// read in the command from the stream with a timeout value of 20 seconds
	old_timeout = stream->timeout(20);
	stream->decode();
	result = stream->code(req);
	// For now, lets keep the timeout, so all command handlers are called with
	// a timeout of 20 seconds on their socket.
	// stream->timeout(old_timeout);
	if(!result) {
		dprintf(D_ALWAYS, "DaemonCore: Can't receive command request (perhaps a timeout?)\n");
		if ( insock != stream )	{   // delete the stream only if we did an accept
			delete stream;		   //     
		} else {
			stream->end_of_message();
		}
		return KEEP_STREAM;
	}
	
	// If UDP, display who message is from now, since we could not do it above
	if ( !is_tcp ) {		
		dprintf( D_COMMAND, "DaemonCore: Command received via UDP from %s\n", 
				 sin_to_string(stream->endpoint()) );
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
		if ( Verify(comTable[index].perm,stream->endpoint()) == FALSE ) {
			// Permission check FAILED
			reqFound = FALSE;	// so we do not call the handler function below
			result = 0;	// make result != to KEEP_STREAM, so we blow away this socket below
			dprintf(D_ALWAYS,"DaemonCore: PERMISSION DENIED to host %s for command %d (%s)\n",
				sin_to_string(stream->endpoint()),req,comTable[index].command_descrip);
			// if UDP, consume the rest of this message to try to stay "in-sync"
			if ( !is_tcp)
				stream->end_of_message();
		} else {
			dprintf(D_COMMAND, "DaemonCore: received command %d (%s), calling handler (%s)\n", req,
			comTable[index].command_descrip, comTable[index].handler_descrip);
		}
	} else {
		dprintf(D_ALWAYS,"DaemonCore: received unregistered command request %d !\n",req);
		result = 0;		// make result != to KEEP_STREAM, so we blow away this socket below
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
	// from accept and our listen socket is still out there.  on udp, however, we
	// cannot just delete it or we will not be "listening" anymore, so we just do
	// an eom flush all buffers, etc.
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

	// Now return KEEP_STREAM only if the user said to _OR_ if insock is a listen socket.
	// Why?  we always wanna keep a listen socket.  also, if we did an accept, we already
	// deleted the stream socket above.
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
		dprintf(D_ALWAYS,"DaemonCore: received request for unregistered Signal %d !\n",sig);
		return FALSE;
	}

	switch (command) {
		case _DC_RAISESIGNAL:
			dprintf(D_DAEMONCORE, "DaemonCore: received Signal %d (%s), raising event\n", sig,
				sigTable[index].sig_descrip, sigTable[index].handler_descrip);
			// set this signal entry to is_pending.  the code to actually call the handler is
			// in the Driver() method.
			sigTable[index].is_pending = TRUE;
			break;
		case _DC_BLOCKSIGNAL:
			sigTable[index].is_blocked = TRUE;
			break;
		case _DC_UNBLOCKSIGNAL:
			sigTable[index].is_blocked = FALSE;
			// now check to see if this signal we are unblocking is pending.  if so,
			// set sent_signal to TRUE.  sent_signal is used by the
			// Driver() to ensure that a signal raised from inside a signal handler is 
			// indeed delivered.
			if ( sigTable[index].is_pending == TRUE ) 
				sent_signal = TRUE;
			break;
		default:
			dprintf(D_DAEMONCORE,"DaemonCore: HandleSig(): unrecognized command\n");
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

	// a Signal is sent via UDP if going to a different process or thread on the same
	// machine.  it is sent via TCP if going to a process on a remote machine.
	// if the signal is being sent to ourselves (i.e. this process), then just twiddle
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
			// something to the async_pipe.  It does not matter what we write, we
			// just need to write something to ensure that the select() in Driver()
			// does not block.
			if ( async_sigs_unblocked == TRUE ) {
				write(async_pipe[1],"!",1);
			}
#endif
			return TRUE;
		} else {
			// send signal to same process, different thread.
			// we will still need to go out via UDP so that our call to select() returns.
			destination = InfoCommandSinfulString();
			is_local = TRUE;
		}
	}

	// handle case of sending to a child process; get info on this pid
	if ( pid != mypid ) {
		if ( pidTable->lookup(pid,pidinfo) < 0 ) {
			// invalid pid
			dprintf(D_ALWAYS,"Send_Signal: ERROR invalid pid %d\n",pid);
			return FALSE;
		}
		is_local = pidinfo->is_local;
		destination = pidinfo->sinful_string;
		if ( destination[0] == '\0' ) {
			// this child process does not have a command socket
			dprintf(D_ALWAYS,"Send_Signal: ERROR Attempt to send signal %d to pid %d, but pid %d has no command socket\n",
				sig,pid,pid);
			return FALSE;
		}
	}

	// now destination process is local, send via UDP; if remote, send via TCP
	if ( is_local == TRUE )
		sock = (Stream *) new SafeSock(destination,0,3);
	else
		sock = (Stream *) new AuthSock(destination,0,20);

	// send the signal out as a DC_RAISESIGNAL command
	sock->encode();		
	if ( (!sock->put(DC_RAISESIGNAL)) ||
		 (!sock->code(sig)) ||
		 (!sock->end_of_message()) ) {
		dprintf(D_ALWAYS,"Send_Signal: ERROR sending signal %d to pid %d\n",sig,pid);
		delete sock;
		return FALSE;
	}

	delete sock;

	dprintf(D_DAEMONCORE,"Send_Signal: sent signal %d to pid %d\n",sig,pid);
	return TRUE;
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


int DaemonCore::Create_Process(
			char		*name,
			char		*args,
			priv_state	condor_priv,
			int			reaper_id,
			int			want_command_port,
			char		*env,
			char		*cwd,
		//	unsigned int std[3],
			int			new_process_group,
			Stream		*sock_inherit_list[] )
{
	int i;
	char *ptmp;
	char inheritbuf[_INHERITBUF_MAXSIZE];
	AuthSock rsock;	// tcp command socket for new child
	SafeSock ssock;	// udp command socket for new child
	pid_t newpid;
#ifdef WIN32
	STARTUPINFO si;
	PROCESS_INFORMATION piProcess;
#endif

	dprintf(D_DAEMONCORE,"In DaemonCore::Create_Process(%s,...)\n",name);

	// First do whatever error checking we can that is not platform specific

	// check reaper_id validity
	if ( (reaper_id < 1) || (reaper_id > maxReap) || (reapTable[reaper_id - 1].num == 0) ) {
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
		for (i = 0; (sock_inherit_list[i] != NULL) && (i < MAX_INHERIT_SOCKS); i++) {
			// check that this is a valid cedar socket
			if ( !(sock_inherit_list[i]->valid()) ) {
				dprintf(D_ALWAYS,"Create_Process: invalid inherit socket list, entry=%d\n",i);
				return FALSE;
			}

			// make certain that this socket is inheritable
			if ( !( ((Sock *)sock_inherit_list[i])->set_inheritable(TRUE)) ) {
				return FALSE;
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
		if ( want_command_port == TRUE ) {
			// choose any old port (dynamic port)
			if ( !rsock.listen( 0 ) ) {
				dprintf(D_ALWAYS,"Create_Process:Failed to post listen on command AuthSock\n");
				return FALSE;
			}
			// now open a SafeSock _on the same port_ choosen above
			if ( !ssock.bind(rsock.get_port()) ) {
				dprintf(D_ALWAYS,"Create_Process:Failed to post listen on command SafeSock\n");
				return FALSE;				
			}
		} else {
			// use well-known port specified by command_port
			int on = 1;
	
			// Set options on this socket, SO_REUSEADDR, so that
			// if we are binding to a well known port, and we crash, we can be
			// restarted and still bind ok back to this same port. -Todd T, 11/97
			if( (!rsock.setsockopt(SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on))) ||
				(!ssock.setsockopt(SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on))) ) {
				dprintf(D_ALWAYS,"ERROR: setsockopt() SO_REUSEADDR failed\n");
				return FALSE;
			}

			if ( (!rsock.listen( want_command_port)) ||
				 (!ssock.bind( want_command_port)) ) {
				dprintf(D_ALWAYS,"Create_Process:Failed to post listen on command socket(s)\n");
				return FALSE;
			}
		}

		// now duplicate the underlying SOCKET to make it inheritable
		if ( (!(rsock.set_inheritable(TRUE))) || (!(ssock.set_inheritable(TRUE))) ) {
			dprintf(D_ALWAYS,"Create_Process:Failed to set command socks inheritable\n");
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
	}
	strcat(inheritbuf," 0");
	
	// Place inheritbuf into the environment as env variable CONDOR_INHERIT
#ifdef WIN32
	if ( !SetEnvironmentVariable("CONDOR_INHERIT",inheritbuf) ) {
		dprintf(D_ALWAYS,"Create_Process: SetEnvironmentVariable failed, errno=%d\n",GetLastError());
		return FALSE;
	}
#else
#endif


#ifdef WIN32
	// START A NEW PROCESS ON WIN32

	// prepare a STARTUPINFO structure for the new process
	ZeroMemory(&si,sizeof(si));
	si.cb = sizeof(si);
	

	// should be DETACHED_PROCESS (for debug, can use CREATE_NEW_CONSOLE)
	if ( new_process_group == TRUE )
		new_process_group = CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS;
	else
		new_process_group =  DETACHED_PROCESS;


	if ( !::CreateProcess(name,args,NULL,NULL,TRUE,new_process_group,
			env,cwd,&si,&piProcess) ) {
		dprintf(D_ALWAYS,
			"Create_Process: CreateProcess failed, errno=%d\n",GetLastError());
		return FALSE;
	}

	// save pid info out of piProcess 
	newpid = piProcess.dwProcessId;

	// reset sockets that we had to inherit back to a non-inheritable permission
	if ( sock_inherit_list ) {
		for (i = 0; (sock_inherit_list[i] != NULL) && (i < MAX_INHERIT_SOCKS); i++) {
			((Sock *)sock_inherit_list[i])->set_inheritable(FALSE);
		}
	}
#else
	// START A NEW PROCESS ON UNIX
#endif

	// Now that we have a child, store the info in our pidTable
	PidEntry *pidtmp = new PidEntry;
	pidtmp->pid = newpid;
	if ( want_command_port != FALSE )
		strcpy(pidtmp->sinful_string,sock_to_string(rsock._sock));
	else
		pidtmp->sinful_string[0] = '\0';
	pidtmp->is_local = TRUE;
	pidtmp->parent_is_local = TRUE;
	pidtmp->reaper_id = reaper_id;
#ifdef WIN32
	pidtmp->hProcess = piProcess.hProcess;
	pidtmp->hThread = piProcess.hThread;
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

void
DaemonCore::Inherit( AuthSock* &rsock, SafeSock* &ssock ) 
{
	char inheritbuf[_INHERITBUF_MAXSIZE];
	char *ptmp;

	// Here we handle inheritance of sockets, file descriptors, and/or handles
	// from our parent.  This is done via an environment variable "CONDOR_INHERIT".
	// If this variable does not exist, it usually means our parent is not a daemon core
	// process.  
	// CONDOR_INHERIT has the following fields.  Each field seperated by a space:
	//	*	parent pid
	//	*	parent sinful-string
	//  *   cedar sockets to inherit.  each will start with a 
	//		"1" for relisock, a "2" for safesock, and a "0" when done.
	//	*	command sockets.  first the rsock, then the ssock, then a "0".

	inheritbuf[0] = '\0';
#ifdef WIN32
	if (GetEnvironmentVariable("CONDOR_INHERIT",inheritbuf,_INHERITBUF_MAXSIZE) > _INHERITBUF_MAXSIZE-1) {
		EXCEPT("CONDOR_INHERIT too large");
	}
#else
	ptmp = getenv("CONDOR_INHERIT");
	if ( ptmp ) {
		if ( strlen(ptmp) > _INHERITBUF_MAXSIZE-1 ) {
			EXCEPT("CONDOR_INHERIT too large");
		}
		strncpy(inheritbuf,ptmp,_INHERITBUF_MAXSIZE);
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
#ifdef WIN32
		pidtmp->hProcess = ::OpenProcess( SYNCHRONIZE | PROCESS_QUERY_INFORMATION | STANDARD_RIGHTS_REQUIRED , FALSE, ppid );
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
			switch ( *ptmp ) {
				case '1' :
					// inherit a relisock
					rsock = new AuthSock();
					ptmp=strtok(NULL," ");
					rsock->serialize(ptmp);
					rsock->set_inheritable(FALSE);
					dprintf(D_DAEMONCORE,"Inherited a AuthSock\n");
					// place into array...
					break;
				case '2':
					ssock = new SafeSock();
					ptmp=strtok(NULL," ");
					ssock->serialize(ptmp);
					ssock->set_inheritable(FALSE);
					dprintf(D_DAEMONCORE,"Inherited a SafeSock\n");
					// place into array...
					break;
				default:
					EXCEPT("Daemoncore: Can only inherit SafeSock or AuthSocks");
					break;
			} // end of switch
			ptmp=strtok(NULL," ");
		}

		// inherit our "command" cedar socks.  they are sent
		// relisock, then safesock, then a "0".
		// we then register rsock and ssock as command sockets below...
		rsock = NULL;
		ssock = NULL;
		ptmp=strtok(NULL," ");
		if ( ptmp && (strcmp(ptmp,"0") != 0) ) {
			dprintf(D_DAEMONCORE,"Inheriting Command Sockets\n");
			rsock = new AuthSock();
			((AuthSock *)rsock)->serialize(ptmp);
			rsock->set_inheritable(FALSE);
		}
		ptmp=strtok(NULL," ");
		if ( ptmp && (strcmp(ptmp,"0") != 0) ) {
			ssock = new SafeSock();
			ssock->serialize(ptmp);
			ssock->set_inheritable(FALSE);
		}

	}	// end of if we read out CONDOR_INHERIT ok

}

#ifdef NOT_YET
int
DaemonCore::HandleDC_SIGCHLD(int sig)
{
	// This function gets called when one or more processes in our pid table
	// has terminated.  We need to reap the process, call any registered reapers,
	// and adjust our pid table.
}
#endif // of NOT_YET

#ifdef WIN32
// This function runs in a seperate thread and wathces over children
DWORD
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
	//		array has been modified by another thread, and we should re-read it.
	// if result < numentries, then result signifies a child process which exited.
	if ( (result < numentries) && (result >= 0) ) {
		// notify our main thread which process exited
		exited_pid = entry->pidentries[result]->pid;	// make it an unsigned int
		SafeSock sock("127.0.0.1",daemonCore->InfoCommandPort());
		sock.encode();
		sent_result = FALSE;
		while ( sent_result == FALSE ) {
			if ( !sock.snd_int(DC_PROCESSEXIT,FALSE) ||
				 !sock.code(exited_pid) ||
				 !sock.end_of_message() ) {
				// failed to get the notification off to the main thread.
				// we'll log a message, wait a bit, and try again
				dprintf(D_ALWAYS,"PidWatcher thread couldn't notify main thread\n");
				::Sleep(500);	// sleep for a half a second (500 ms)
			} else {
				sent_result = TRUE;
				last_pidentry_exited = result;
			}
		}
	} else {
		// no pid exited, we were signaled because our pidentries array was modified.
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
			continue;	// we continue here so we dont hit the LeaveCriticalSection below
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
	DWORD threadId;
	// should we be using _beginthread here instead of ::CreateThread to prevent 
	//    memory leaks from the standard C lib ?
	entry->hThread = ::CreateThread(NULL, 1024, (LPTHREAD_START_ROUTINE)pidWatcherThread,
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

	// and call HandleSig to raise the signal
	return( HandleProcessExit(pid,0) );
}

// This function gets calls with the pid of a process which just exited.
// On Unix, the exit_status is also provided; on NT, we need to fetch
// the exit status here.  Then we call any registered reaper for this process.
int DaemonCore::HandleProcessExit(pid_t pid, int exit_status)
{
	PidEntry* pidentry;
	int i;

	// Fetch the PidEntry for this pid from our hash table.
	if ( pidTable->lookup(pid,pidentry) == -1 ) {
		// we did not find this pid!1???!?
		dprintf(D_ALWAYS,"WARNING! Unknown process exited - pid=%d\n",pid);
		return FALSE;
	}

	// If process is Unix, we are passed the exit status.
	// If process is NT and is remote, we are passed the exit status.
	// If process is NT and is local, we need to fetch the exit status here.
#ifdef WIN32
	if ( pidentry->is_local ) {
		DWORD winexit;
		
		if ( !::GetExitCodeProcess(pidentry->hProcess,&winexit) ) {
			dprintf(D_ALWAYS,"WARNING: Cannot get exit status for pid = %d\n",pid);
			return FALSE;
		}
		if ( winexit == STILL_ACTIVE ) {	// should never happen
			EXCEPT("DaemonCore in HandleProcessExit() and process still running");
		}
		// TODO: deal with Exception value returns here
		exit_status = winexit;
	}
#endif   // of WIN32

	// If parent process is_local, simply invoke the reaper here.  If remote, call
	// the DC_INVOKEREAPER command.  
	if ( pidentry->parent_is_local ) {

		// Set i to be the entry in the reaper table to use
		i = pidentry->reaper_id - 1;

		// Invoke the reaper handler if there is one registered
		if ( (i >= 0) && ((reapTable[i].handler != NULL) || (reapTable[i].handlercpp != NULL)) ) {		
			// Set curr_dataptr for Get/SetDataPtr()
			curr_dataptr = &(reapTable[i].data_ptr);

			// Log a message
			char *hdescrip = reapTable[i].handler_descrip;
			if ( !hdescrip )
				hdescrip = EMPTY_DESCRIP;
			dprintf(D_DAEMONCORE,"DaemonCore: Pid %lu exited with status %d, invoking reaper %d <%s>\n",
				pid,exit_status,i+1,hdescrip);

			if ( reapTable[i].handler )
				// a C handler
				(*(reapTable[i].handler))(reapTable[i].service,pid,exit_status);
			else
			if ( reapTable[i].handlercpp )
				// a C++ handler
				(reapTable[i].service->*(reapTable[i].handlercpp))(pid,exit_status);

			// Clear curr_dataptr
			curr_dataptr = NULL;
		} else {
			// no registered reaper
			dprintf(D_DAEMONCORE,"DaemonCore: Pid %lu exited with status %d; no registered reaper\n",
				pid,exit_status);
		}
	} else {
		// TODO: the parent for this process is remote.  send the parent a DC_INVOKEREAPER command.
	}

	// Now remove this pid from our tables ----
		// remove from hash table
	pidTable->remove(pid);		
#ifdef WIN32
		// close WIN32 handles 
	::CloseHandle(pidentry->hThread);
	::CloseHandle(pidentry->hProcess);
#endif
	delete pidentry;

	// Finally, some hard-coded logic.  If the pid that exited was our parent,
	// then shutdown gracefully.
	if (pid == ppid) {
		dprintf(D_ALWAYS,"Our Parent process (pid %lu) exited; shutting down\n",pid);
		Send_Signal(mypid,DC_SIGTERM);	// SIGTERM means shutdown graceful
	}

	return TRUE;
}
