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
