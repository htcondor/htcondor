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
#include "condor_debug.h"
#include <condor_daemon_core.h>
#include "master.h"

#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

// Externs
extern int dc_main(int, char**);
extern int line_where_service_stopped;
extern Daemons daemons;

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
	if( daemonCore->Signal_Myself(SIGQUIT) ) {
		runningService=FALSE;
	}
}

#if HAVE_HIBERNATION

// Handles any power state change notifications
DWORD 
PowerEventHander(DWORD eventType, LPVOID eventData) {
	
	//
	// Events here are sometimes requests so we can return NO_ERROR 
	// to grant the request and an error code to deny the request
	// 
	DWORD       status          = NO_ERROR;
    daemon      *startd         = daemons.FindDaemon ( DT_STARTD );
	static bool auto_resumed    = false;

	switch( eventType ) {
	case PBT_APMQUERYSUSPEND:
		//
		// An process is requesting permission to suspend 
		// the machine.  It may, optionally, give us 
		// permission to query the user interactively. 
		// Here we can afforded a graceful shutdown, although
		// we may need to update the SCM on occasion: 
		// informing it that we are still doing our thing.
		//
		dprintf( D_ALWAYS, "PowerEventHander: Some driver/application "
			"is asking if we can enter hibernation\n" );
		break;
		
	case PBT_APMSUSPEND:
		//
		// The machine is about to enter a suspended state.
		//
		dprintf ( D_ALWAYS, "PowerEventHander: Machine entering "
			"hibernation\n" );
		break;
		
	case PBT_APMRESUMESUSPEND:
		//
		// The machine has resumed operation after being 
		// suspended. 
		// 
		dprintf( D_ALWAYS, "PowerEventHander: "
			"Waking machine (APM)\n" );

        /* alert the StartD of the wake event */
        if ( !auto_resumed && startd ) {
            startd->Stop();
            startd->Start();
        }

        auto_resumed = false;

		break;
		
	case PBT_APMRESUMEAUTOMATIC:
		//
		// The the machine has waken up automatically 
		// to handle an event.  If the machine detects 
		// activity it will send a PBT_APMRESUMESUSPEND 
		//
		// Should we create some activity here?  Maybe WOL 
		// packets create this type of wake event.  Further
		// testing is necessary.
		//
		dprintf( D_ALWAYS, "PowerEventHander: Waking machine to "
			"handle an event (Automatic)\n" );

        /* alert the StartD of the wake event */
        if ( startd ) {
            startd->Stop();
            startd->Start();
        }

        auto_resumed = true;

		break;
		
	case PBT_APMRESUMECRITICAL:
		//
		// The machine has resumed after a critical 
		// suspension caused by a failing battery.
		// In this case we do a "peaceful" restart 
		// since we may not have had the chance to 
		// turn off all the daemons gracefully.
		// 
		dprintf( D_ALWAYS, "PowerEventHander: Resuming machine after "
			"a critical suspension (possible battery failure?)\n" );

        /* alert the StartD of the wake event */
        if ( startd ) {
            startd->Stop();
            startd->Start();
        }

		break;
		
	default:
		break;
	}
	
	return status;
	
}

#endif /* HAVE_HIBERNATION */

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
	else {
		serviceStatus.dwControlsAccepted = 
			SERVICE_ACCEPT_STOP |
			// SERVICE_ACCEPT_PAUSE_CONTINUE |
			SERVICE_ACCEPT_SHUTDOWN;

#if HAVE_HIBERNATION
		//
		// Service will be notified when the computer's 
		// power status has changed. 
		//
		serviceStatus.dwControlsAccepted |= 
			SERVICE_ACCEPT_POWEREVENT;
#endif /* HAVE_HIBERNATION */
	
	}

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
#if HAVE_HIBERNATION

DWORD WINAPI 
ServiceCtrlHandler (
	DWORD  controlCode, // requested control code
	DWORD  eventType,   // event type
	LPVOID eventData,   // event data
	LPVOID context      // user-defined context data
	) {

#else

VOID WINAPI
ServiceCtrlHandler (
	DWORD  controlCode  // requested control code
	) {

#endif /* HAVE_HIBERNATION */
	
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
#if HAVE_HIBERNATION
			return NO_ERROR;
#else
			return;
#endif /* HAVE_HIBERNATION */

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
			daemonCore->Signal_Myself(SIGQUIT);
			line_where_service_stopped = __LINE__;
#if HAVE_HIBERNATION
			return NO_ERROR;
#else
			return;
#endif /* HAVE_HIBERNATION */

#if HAVE_HIBERNATION

		// Handle any power events
		case SERVICE_CONTROL_POWEREVENT:
			line_where_service_stopped = __LINE__;
			return PowerEventHander ( eventType, eventData );			
			break;

#endif /* HAVE_HIBERNATION */

		default:
			line_where_service_stopped = __LINE__;
 			break;
	}
	//line_where_service_stopped = __LINE__;
	SendStatusToSCM(currentState, NO_ERROR,
		0, 0, 0);
#if HAVE_HIBERNATION
	return NO_ERROR;
#endif /* HAVE_HIBERNATION */

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
#if HAVE_HIBERNATION
	serviceStatusHandle =
		RegisterServiceCtrlHandlerEx(		
		SERVICE_NAME,		
		(LPHANDLER_FUNCTION_EX) ServiceCtrlHandler,
		NULL);
#else
	serviceStatusHandle =
		RegisterServiceCtrlHandler(		
		SERVICE_NAME,		
		(LPHANDLER_FUNCTION) ServiceCtrlHandler);
#endif /* HAVE_HIBERNATION */
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

DWORD start_as_service()
{
	SERVICE_TABLE_ENTRY serviceTable[] = 
	{ 
	{ SERVICE_NAME,
		(LPSERVICE_MAIN_FUNCTION) ServiceMain},
	{ NULL, NULL }
	};

	// Register with the Windows SCM. 
    // Note: this calls ServiceMain and doesn't return 
    // until the service fails or exits.
    DWORD err = NO_ERROR;
    BOOL success = StartServiceCtrlDispatcher(serviceTable);
    if ( ! success) {
        err = GetLastError();
        // If "The service process could not connect to the service controller."
        // it's probably because the service controller didn't start us so
        // return a special error code to tell the caller it should start us as
        // a daemon instead...
        if (ERROR_FAILED_SERVICE_CONTROLLER_CONNECT == err) {
           return 0x666;
           }
        // otherwise - report the error.
		ErrorHandler("In StartServiceCtrlDispatcher", err);
    }
    return err;
}
