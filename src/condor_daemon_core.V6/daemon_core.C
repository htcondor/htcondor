////////////////////////////////////////////////////////////////////////////////
//
// Implementation of DaemonCore.
//
//
////////////////////////////////////////////////////////////////////////////////

static const int DEFAULT_MAXCOMMANDS = 255;
static const int DEFAULT_MAXSIGNALS = 99;
static const int DEFAULT_MAXSOCKETS = 16;
static const char* DEFAULT_INDENT = "DaemonCore--> ";


#include "condor_common.h"

#ifndef WIN32
#include <std.h>
#if defined(Solaris251)
#include <strings.h>
#endif
#include "_condor_fix_types.h"
#include <sys/socket.h>
#include "condor_fix_signal.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif   /* ifndef WIN32 */

#include "condor_debug.h"
#include "condor_timer_manager.h"
#include "condor_daemon_core.h"
#include "condor_io.h"

extern "C" 
{
	void			dprintf(int, char*...);
	char*			sin_to_string(struct sockaddr_in*);
	void			display_from(struct sockaddr_in*);
}

static	char*  	_FileName_ = __FILE__;	// used by EXCEPT

TimerManager DaemonCore::t;

// some constants for HandleSig()
#define _DC_RAISESIGNAL 1
#define _DC_BLOCKSIGNAL 2
#define _DC_UNBLOCKSIGNAL 3

// DaemonCore constructor. 

DaemonCore::DaemonCore(int ComSize,int SigSize,int SocSize)
{

	if(ComSize < 0 || SigSize < 0 || SocSize < 0)
	{
		EXCEPT("Invalid argument(s) for DaemonCore constructor");
	}

	maxCommand = ComSize;
	maxSig = SigSize;
	maxSocket = SocSize;

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
	if(sigTable == NULL)
	{
		EXCEPT("Out of memory!");
	}
	nSock = 0;
	memset(sockTable,'\0',maxSocket*sizeof(SockEnt));

	initial_command_sock = -1;

	curr_dataptr = NULL;
	curr_regdataptr = NULL;
}

// DaemonCore destructor. Delete the all the various handler tables, plus
// delete/free any pointers in those tables.
DaemonCore::~DaemonCore()
{
	int		i;	

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

	t.CancelAllTimers();
}

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

int	DaemonCore::Register_Timer(unsigned deltawhen, Event event, char *event_descrip, 
					   Service* s, int id) {
	return( t.NewTimer(s, deltawhen, event, event_descrip, 0, id) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, unsigned period, Event event, char *event_descrip, 
					   Service* s, int id) {
	return( t.NewTimer(s, deltawhen, event, event_descrip, period, id) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, Eventcpp eventcpp, char *event_descrip, 
					   Service* s, int id) {
	return( t.NewTimer(s, deltawhen, eventcpp, event_descrip, 0, id) );
}

int	DaemonCore::Register_Timer(unsigned deltawhen, unsigned period, Eventcpp event, 
				   char *event_descrip, Service* s, int id) {
	return( t.NewTimer(s, deltawhen, event, event_descrip, period, id) );
}
	
int	DaemonCore::Cancel_Timer( int id ) {
	return( t.CancelTimer(id) );
}

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
	if ( command_descrip )
		comTable[i].command_descrip = strdup(command_descrip);
	else
		comTable[i].command_descrip = EMPTY_DESCRIP;
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

int DaemonCore::Cancel_Command( int command ) 
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
	if ( sig_descrip )
		sigTable[i].sig_descrip = strdup(sig_descrip);
	else
		sigTable[i].sig_descrip = EMPTY_DESCRIP;
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
	// stub

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
			sockTable[i].sockd = ((ReliSock *)iosock)->get_file_desc();
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
	if ( iosock_descrip )
		sockTable[i].iosock_descrip = strdup(iosock_descrip);
	else
		sockTable[i].iosock_descrip = EMPTY_DESCRIP;
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

int DaemonCore::Cancel_Socket( Stream* )
{
	// stub

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


// This function never returns. It is responsible for monitor signals and
// incoming messages or requests and invoke corresponding handlers.
void DaemonCore::Driver()
{
	int			rv;					// return value from select
	int			i;
	int			tmpErrno;
	struct timeval	timer;

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
		timer.tv_sec = t.Timeout();
		if ( sent_signal == TRUE )
			timer.tv_sec = 0;
		timer.tv_usec = 0;

		// Setup what socket descriptors to select on.  We recompute this
		// every time because 1) some timeout handler may have removed/added
		// sockets, and 2) it ain't that expensive....
		FD_ZERO(&readfds);
		for (i = 0; i < nSock; i++) {
			if ( sockTable[i].iosock )	// if a valid entry....
				FD_SET(sockTable[i].sockd,&readfds);
		}

		errno = 0;

#if defined(HPUX9)
		rv = select(FD_SETSIZE, (int *) &readfds, NULL, NULL, &timer);
#else
		rv = select(FD_SETSIZE, &readfds, NULL, NULL, &timer);
#endif
		tmpErrno = errno;

#ifndef WIN32
		// Unix
		if(rv < 0) {
			if(tmpErrno != EINTR)
			// not just interrupted by a signal...
			{
				EXCEPT("select, error # = %d", errno);
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
						// But first pdate curr_dataptr for GetDataPtr()
						curr_dataptr = &(sockTable[i].data_ptr);
						if ( sockTable[i].handler )
							// a C handler
							(*(sockTable[i].handler))(sockTable[i].service,sockTable[i].iosock);
						else
						if ( sockTable[i].handlercpp )
							// a C++ handler
							(sockTable[i].service->*(sockTable[i].handlercpp))(sockTable[i].iosock);
						else
							// no handler registered, so this is a command socket.  call
							// the DaemonCore handler which takes care of command sockets.
							HandleReq(i);
						// Clear curr_dataptr
						curr_dataptr = NULL;
					}	// if FD_ISSET
				}	// if valid entry in sockTable
			}	// for 0 thru nSock
		}	// if rv > 0

	}	// end of infinite for loop
}

void DaemonCore::HandleReq(int socki)
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
			return;
	}

	// set up a connection for a tcp socket	
	if ( is_tcp ) {
		stream = (Stream *) ((ReliSock *)insock)->accept();
		if ( !stream ) {
			dprintf(D_ALWAYS, "DaemonCore: accept() failed!");
			return;
		}

		dprintf(D_ALWAYS, "DaemonCore: Command received via TCP\n");
		display_from(stream->endpoint());
	}
	// set up a connection for a udp socket
	else {
		// on UDP, we do not have a seperate listen and accept sock.
		// our "listen sock" is also our "accept" sock, so just
		// assign stream to the insock. UDP = connectionless, get it?
		stream = insock;
		dprintf(D_ALWAYS, "DaemonCore: Command received via UDP\n");
		// in UDP we cannot display who the command is from until we read something off 
		// the socket, so we display who from after we read the command below...
	}
	
	// read in the command from the stream with a timeout value of 20 seconds
	old_timeout = stream->timeout(20);
	stream->decode();
	result = stream->code(req);
	stream->timeout(old_timeout);
	if(!result) {
		dprintf(D_ALWAYS, "DaemonCore: Can't receive command request (timeout)\n");
		if ( is_tcp )
			delete stream;
		else
			((SafeSock *)stream)->end_of_message();
		return;
	}
	
	// If UDP, display who message is from now, since we could not do it above
	if ( !is_tcp ) {		
		display_from(stream->endpoint());	
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

	if ( reqFound == TRUE )
		dprintf(D_DAEMONCORE, "DaemonCore: received command %d (%s), calling handler (%s)\n", req,
			comTable[index].command_descrip, comTable[index].handler_descrip);
	else
		dprintf(D_ALWAYS,"DaemonCore: received unregistered command request %d !\n",req);

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
	
	// finalize; the handler is done with the command.  the handler will return
	// with KEEP_STREAM if we should not touch the stream; otherwise, cleanup
	// the stream.  On tcp, we just delete it since the stream is the one we got
	// from accept and our listen socket is still out there.  on udp, however, we
	// cannot just delete it or we will not be "listening" anymore, so we just do
	// an eom to flush buffers, etc.
	if ( result != KEEP_STREAM ) {
		if ( is_tcp ) {
			delete stream;
		} else {
			((SafeSock *)stream)->end_of_message();
		}
	}

	return;
}

int DaemonCore::HandleSigCommand(int command, Stream* stream)
{
	int sig;

	// We have been sent a DC_RAISESIGNAL command

	// read the signal number from the socket
	stream->code(sig);

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

int DaemonCore::Send_Signal(int pid, int sig)
{
	// a Signal is sent via UDP if going to a different process on the same
	// machine.  it is sent via TCP if going to a process on a remote machine.
	// if the signal is being sent to ourselves (i.e. this process), then just twiddle
	// the signal table and set sent_signal to TRUE.  sent_signal is used by the
	// Driver() to ensure that a signal raised from inside a signal handler is 
	// indeed delivered.
	
	if ( pid == 0 ) {
		// send signal to ourselves
		HandleSig(DC_RAISESIGNAL,sig);
		sent_signal = TRUE;
		return TRUE;
	}

	// do not support process management yet....
	return FALSE;
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
