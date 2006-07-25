/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
//
// Handler NT Service Manager
//

//***************************************************************
//
// Some segments from the book "Win32 System Services: The Heart of Windows 95
// and Windows NT"
// by Marshall Brain
// Published by Prentice Hall
//
// Copyright 1995, by Prentice Hall.
//***************************************************************

#include "condor_common.h"
#include "..\condor_daemon_core.V6\condor_daemon_core.h"

#include <windows.h>
#include <stdio.h>
#include <iostream.h>
#include <stdlib.h>

// Externs
extern int dc_main(int, char**);
extern int line_where_service_stopped;

// Static variables
// The name of the service
static char *SERVICE_NAME = "Condor Master";
// Handle used to communicate status info with
// the SCM. Created by RegisterServiceCtrlHandler
static SERVICE_STATUS_HANDLE serviceStatusHandle;
// Flags holding current state of service
static BOOL pauseService = FALSE;
static BOOL runningService = FALSE;

static void ErrorHandler(char *s, DWORD err)
{
	cout << s << endl;
	cout << "Error number: " << err << endl;
	ExitProcess(err);
}


// Initializes the service by starting its thread
BOOL InitService()
{
	runningService = TRUE;
	return TRUE;
}


// Stops the service by allowing ServiceMain to
// complete
VOID StopService() 
{	
	if( daemonCore->Send_Signal(daemonCore->getpid(), SIGTERM) ) {
		runningService=FALSE;
	}
}

// This function consolidates the activities of 
// updating the service status with
// SetServiceStatus
BOOL SendStatusToSCM (DWORD dwCurrentState,
	DWORD dwWin32ExitCode, 
	DWORD dwServiceSpecificExitCode,
	DWORD dwCheckPoint,
	DWORD dwWaitHint)
{
	BOOL success;
	SERVICE_STATUS serviceStatus;

	// Fill in all of the SERVICE_STATUS fields
	serviceStatus.dwServiceType =
		SERVICE_WIN32_OWN_PROCESS;
	serviceStatus.dwCurrentState = dwCurrentState;

	// If in the process of something, then accept
	// no control events, else accept anything
	if (dwCurrentState == SERVICE_START_PENDING)
		serviceStatus.dwControlsAccepted = 0;
	else
		serviceStatus.dwControlsAccepted = 
			SERVICE_ACCEPT_STOP |
			// SERVICE_ACCEPT_PAUSE_CONTINUE |
			SERVICE_ACCEPT_SHUTDOWN;

	// if a specific exit code is defines, set up
	// the win32 exit code properly
	if (dwServiceSpecificExitCode == 0)
		serviceStatus.dwWin32ExitCode =
			dwWin32ExitCode;
	else
		serviceStatus.dwWin32ExitCode = 
			ERROR_SERVICE_SPECIFIC_ERROR;
	serviceStatus.dwServiceSpecificExitCode =
		dwServiceSpecificExitCode;

	serviceStatus.dwCheckPoint = dwCheckPoint;
	serviceStatus.dwWaitHint = dwWaitHint;

	// Pass the status record to the SCM
	success = SetServiceStatus (serviceStatusHandle,
		&serviceStatus);

	if (!success) {
		// line_where_service_stopped = __LINE__;
		StopService();
	}

	return success;
}

// Dispatches events received from the service 
// control manager
VOID ServiceCtrlHandler (DWORD controlCode) 
{
	// DWORD  currentState = 0;
	DWORD currentState = SERVICE_RUNNING;
	BOOL success;

	switch(controlCode)
	{
		// There is no START option because
		// ServiceMain gets called on a start

		// Stop the service
		case SERVICE_CONTROL_STOP:
			currentState = SERVICE_STOP_PENDING;
			// Tell the SCM what's happening
			line_where_service_stopped = __LINE__;
			success = SendStatusToSCM(
				SERVICE_STOP_PENDING,
				NO_ERROR, 0, 1, 60000);
			// Not much to do if not successful

			// Stop the service
			line_where_service_stopped = __LINE__;
			StopService();
			return;

		// Pause the service- a NOOP for now
		case SERVICE_CONTROL_PAUSE:
			line_where_service_stopped = __LINE__;
			break;

		// Resume from a pause- a NOOP for now
		case SERVICE_CONTROL_CONTINUE:
			line_where_service_stopped = __LINE__;
			break;

		// Update current status
		case SERVICE_CONTROL_INTERROGATE:
			// it will fall to bottom and send status
			line_where_service_stopped = __LINE__;
			break;

		// Do nothing in a shutdown. Could do cleanup
		// here but it must be very quick.
		case SERVICE_CONTROL_SHUTDOWN:
			daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
			line_where_service_stopped = __LINE__;
			return;
		default:
			line_where_service_stopped = __LINE__;
 			break;
	}
	//line_where_service_stopped = __LINE__;
	SendStatusToSCM(currentState, NO_ERROR,
		0, 0, 0);
}

// Handle an error from ServiceMain by cleaning up
// and telling SCM that the service didn't start.
VOID terminate(DWORD error)
{
	// Send a message to the scm to tell about
	// stopage
	if (serviceStatusHandle) {
		line_where_service_stopped = __LINE__;
		SendStatusToSCM(SERVICE_STOPPED, error,
			0, 0, 0);
	}

	// Do not need to close serviceStatusHandle
}

// ServiceMain is called when the SCM wants to
// start the service. When it returns, the service
// has stopped. It therefore waits on an event
// just before the end of the function, and
// that event gets set when it is time to stop. 
// It also returns on any error because the
// service cannot start if there is an eror.
VOID ServiceMain(DWORD argc, LPTSTR *argv) 
{
	BOOL success;

	// immediately call Registration function
	serviceStatusHandle =
		RegisterServiceCtrlHandler(
			SERVICE_NAME,
			(LPHANDLER_FUNCTION) ServiceCtrlHandler);
	if (!serviceStatusHandle)
	{
		terminate(GetLastError());
		return;
	}

#ifdef NOT_NEEDED
	// Notify SCM of progress
	line_where_service_stopped = __LINE__;
	success = SendStatusToSCM(
		SERVICE_START_PENDING,
		NO_ERROR, 0, 1, 5000);
	if (!success)
	{
		terminate(GetLastError()); 
		return;
	}
#endif

	// The service is now running. 
	// Notify SCM of progress
	line_where_service_stopped = __LINE__;
	success = SendStatusToSCM(
		SERVICE_RUNNING,
		NO_ERROR, 0, 0, 0);
	if (!success)
	{
		terminate(GetLastError()); 
		return;
	}

	FreeConsole();

	// Invoke the real daemon core main
	dc_main(argc,argv);

	terminate(0);
}

void register_service()
{
	SERVICE_TABLE_ENTRY serviceTable[] = 
	{ 
	{ SERVICE_NAME,
		(LPSERVICE_MAIN_FUNCTION) ServiceMain},
	{ NULL, NULL }
	};
	BOOL success;

	// Register with the SCM
	success = 
		StartServiceCtrlDispatcher(serviceTable);
	if (!success)
		ErrorHandler("In StartServiceCtrlDispatcher",
			GetLastError());
}
