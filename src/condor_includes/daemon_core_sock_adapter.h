/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

#ifndef DAEMON_CORE_SOCK_ADAPTER_H
#define DAEMON_CORE_SOCK_ADAPTER_H

/**
   This class is used as an indirect way to call daemonCore functions
   from cedar sock code.  Not all applications that use cedar
   are linked with DaemonCore (or use the DaemonCore event loop).
   In such applications, this daemonCore interface class will not
   be initialized and it is an error if these functions are ever
   used in such cases.  (They will EXCEPT.)
 */

#include "condor_daemon_core.h"

class DaemonCoreSockAdapterClass {
 public:
	typedef int (DaemonCore::*Register_Socket_fnptr)(Stream*,const char*,SocketHandlercpp,const char*,Service*,DCpermission);
	typedef int (DaemonCore::*Cancel_Socket_fnptr)( Stream *sock );
	typedef void (DaemonCore::*CallSocketHandler_fnptr)( Stream *sock, bool default_to_HandleCommand );
	typedef int (DaemonCore::*CallCommandHandler_fnptr)( int cmd, Stream *stream, bool delete_stream);
	typedef void (DaemonCore::*HandleReqAsync_fnptr)(Stream *stream);
    typedef int (DaemonCore::*Register_DataPtr_fnptr)( void *data );
    typedef void *(DaemonCore::*GetDataPtr_fnptr)();
	typedef int (DaemonCore::*Register_Timer_fnptr)(unsigned deltawhen,TimerHandlercpp handler,const char * event_descrip,Service* s);
	typedef int (DaemonCore::*Register_PeriodicTimer_fnptr)(unsigned deltawhen,unsigned period,TimerHandlercpp handler,const char * event_descrip,Service* s);
	typedef int (DaemonCore::*Cancel_Timer_fnptr)(int id);
	typedef bool (DaemonCore::*TooManyRegisteredSockets_fnptr)(int fd,MyString *msg,int num_fds);
	typedef void (DaemonCore::*incrementPendingSockets_fnptr)();
	typedef void (DaemonCore::*decrementPendingSockets_fnptr)();
	typedef const char* (DaemonCore::*publicNetworkIpAddr_fnptr)();
    typedef int (DaemonCore::*Register_Command_fnptr) (
		int             command,
		char const*     com_descrip,
		CommandHandler  handler, 
		char const*     handler_descrip,
		Service *       s,
		DCpermission    perm,
		int             dprintf_flag,
		bool            force_authentication);
	typedef void (DaemonCore::*daemonContactInfoChanged_fnptr)();


	DaemonCoreSockAdapterClass(): m_daemonCore(0) {}

	void EnableDaemonCore(
		DaemonCore *dC,
		Register_Socket_fnptr Register_Socket_fptr,
		Cancel_Socket_fnptr Cancel_Socket_fptr,
		CallSocketHandler_fnptr CallSocketHandler_fptr,
		CallCommandHandler_fnptr CallCommandHandler_fptr,
		HandleReqAsync_fnptr HandleReqAsync_fptr,
		Register_DataPtr_fnptr Register_DataPtr_fptr,
		GetDataPtr_fnptr GetDataPtrFun_fptr,
		Register_Timer_fnptr Register_Timer_fptr,
		Register_PeriodicTimer_fnptr Register_PeriodicTimer_fptr,
		Cancel_Timer_fnptr Cancel_Timer_fptr,
		TooManyRegisteredSockets_fnptr TooManyRegisteredSockets_fptr,
		incrementPendingSockets_fnptr incrementPendingSockets_fptr,
		decrementPendingSockets_fnptr decrementPendingSockets_fptr,
		publicNetworkIpAddr_fnptr publicNetworkIpAddr_fptr,
		Register_Command_fnptr Register_Command_fptr,
		daemonContactInfoChanged_fnptr daemonContactInfoChanged_fptr)
	{
		m_daemonCore = dC;
		m_Register_Socket_fnptr = Register_Socket_fptr;
		m_Cancel_Socket_fnptr = Cancel_Socket_fptr;
		m_CallSocketHandler_fnptr = CallSocketHandler_fptr;
		m_CallCommandHandler_fnptr = CallCommandHandler_fptr;
		m_HandleReqAsync_fnptr = HandleReqAsync_fptr;
		m_Register_DataPtr_fnptr = Register_DataPtr_fptr;
		m_GetDataPtr_fnptr = GetDataPtrFun_fptr;
		m_Register_Timer_fnptr = Register_Timer_fptr;
		m_Register_PeriodicTimer_fnptr = Register_PeriodicTimer_fptr;
		m_Cancel_Timer_fnptr = Cancel_Timer_fptr;
		m_TooManyRegisteredSockets_fnptr = TooManyRegisteredSockets_fptr;
		m_incrementPendingSockets_fnptr = incrementPendingSockets_fptr;
		m_decrementPendingSockets_fnptr = decrementPendingSockets_fptr;
		m_publicNetworkIpAddr_fnptr = publicNetworkIpAddr_fptr;
		m_Register_Command_fnptr = Register_Command_fptr;
		m_daemonContactInfoChanged_fnptr = daemonContactInfoChanged_fptr;
	}

		// These functions all have the same interface as the corresponding
		// daemonCore functions.

	DaemonCore *m_daemonCore;
	Register_Socket_fnptr m_Register_Socket_fnptr;
	Cancel_Socket_fnptr m_Cancel_Socket_fnptr;
	CallSocketHandler_fnptr m_CallSocketHandler_fnptr;
	CallCommandHandler_fnptr m_CallCommandHandler_fnptr;
	HandleReqAsync_fnptr m_HandleReqAsync_fnptr;
	Register_DataPtr_fnptr m_Register_DataPtr_fnptr;
	GetDataPtr_fnptr m_GetDataPtr_fnptr;
	Register_Timer_fnptr m_Register_Timer_fnptr;
	Register_PeriodicTimer_fnptr m_Register_PeriodicTimer_fnptr;
	Cancel_Timer_fnptr m_Cancel_Timer_fnptr;
	TooManyRegisteredSockets_fnptr m_TooManyRegisteredSockets_fnptr;
	incrementPendingSockets_fnptr m_incrementPendingSockets_fnptr;
	decrementPendingSockets_fnptr m_decrementPendingSockets_fnptr;
	publicNetworkIpAddr_fnptr m_publicNetworkIpAddr_fnptr;
	Register_Command_fnptr m_Register_Command_fnptr;
	daemonContactInfoChanged_fnptr m_daemonContactInfoChanged_fnptr;

    int Register_Socket (Stream*              iosock,
                         const char *         iosock_descrip,
                         SocketHandlercpp     handlercpp,
                         const char *         handler_descrip,
                         Service*             s,
                         DCpermission         perm = ALLOW)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_Socket_fnptr)(iosock,iosock_descrip,handlercpp,handler_descrip,s,perm);
	}

	int Cancel_Socket( Stream *stream )
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Cancel_Socket_fnptr)(stream);
	}

	void CallSocketHandler( Stream *stream, bool default_to_HandleCommand=false )
	{
		ASSERT(m_daemonCore);
		(m_daemonCore->*m_CallSocketHandler_fnptr)(stream,default_to_HandleCommand);
	}

	int CallCommandHandler( int cmd, Stream *stream, bool delete_stream=true )
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_CallCommandHandler_fnptr)(cmd,stream,delete_stream);
	}

	void HandleReqAsync(Stream *stream)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_HandleReqAsync_fnptr)(stream);
	}


    int Register_DataPtr( void *data )
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_DataPtr_fnptr)(data);
	}
    void *GetDataPtr()
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_GetDataPtr_fnptr)();
	}
    int Register_Timer (unsigned     deltawhen,
                        TimerHandlercpp handler,
                        const char * event_descrip, 
                        Service*     s = NULL)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_Timer_fnptr)(
			deltawhen,
			handler,
			event_descrip,
			s);
	}
    int Register_Timer (unsigned     deltawhen,
						unsigned     period,
                        TimerHandlercpp handler,
                        const char * event_descrip, 
                        Service*     s = NULL)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_PeriodicTimer_fnptr)(
			deltawhen,
			period,
			handler,
			event_descrip,
			s);
	}
    int Cancel_Timer (int id)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Cancel_Timer_fnptr)( id );
	}
	bool TooManyRegisteredSockets(int fd=-1,MyString *msg=NULL,int num_fds=1)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_TooManyRegisteredSockets_fnptr)(fd,msg,num_fds);
	}

	bool isEnabled()
	{
		return m_daemonCore != NULL;
	}

	void incrementPendingSockets() {
		ASSERT(m_daemonCore);
		(m_daemonCore->*m_incrementPendingSockets_fnptr)();
	}

	void decrementPendingSockets() {
		ASSERT(m_daemonCore);
		(m_daemonCore->*m_decrementPendingSockets_fnptr)();
	}

	const char* publicNetworkIpAddr(void) {
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_publicNetworkIpAddr_fnptr)();
	}

    int Register_Command (int             command,
                          char const*     com_descrip,
                          CommandHandler  handler, 
                          char const*     handler_descrip,
                          Service *       s                = NULL,
                          DCpermission    perm             = ALLOW,
                          int             dprintf_flag     = D_COMMAND,
						  bool            force_authentication = false)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_Command_fnptr)(command,com_descrip,handler,handler_descrip,s,perm,dprintf_flag,force_authentication);
	}

	void daemonContactInfoChanged() {
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_daemonContactInfoChanged_fnptr)();
	}
};

extern DaemonCoreSockAdapterClass daemonCoreSockAdapter;

#endif
