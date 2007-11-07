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


// Birdwatcher.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "Birdwatcher.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBirdwatcherApp

BEGIN_MESSAGE_MAP(CBirdwatcherApp, CWinApp)
	//{{AFX_MSG_MAP(CBirdwatcherApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBirdwatcherApp construction

CBirdwatcherApp::CBirdwatcherApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CBirdwatcherApp object

CBirdwatcherApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CBirdwatcherApp initialization

BOOL CBirdwatcherApp::InitInstance()
{
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	HICON hFlying2 = LoadIcon(IDI_CONDOR_FLYING2);
	HICON hClaimed = LoadIcon(IDI_CONDOR_CLAIMED);

	sysman.init(LoadIcon(IDI_CONDOR_OFF),
				LoadIcon(IDI_CONDOR_IDLE),
				hClaimed,
				LoadIcon(IDI_CONDOR_FLYING1),
				hFlying2,
				hFlying2,
				LoadIcon(IDI_PREEMPTING)
				);
	m_pMainWnd = &sysman.notifyWnd;

	return TRUE;
}
