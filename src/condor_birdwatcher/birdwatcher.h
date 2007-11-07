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


// Birdwatcher.h : main header file for the BIRDWATCHER application
//

#if !defined(AFX_BIRDWATCHER_H__48E19F6C_ED69_4718_8BBA_D281C8649091__INCLUDED_)
#define AFX_BIRDWATCHER_H__48E19F6C_ED69_4718_8BBA_D281C8649091__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols
#include "SystrayManager.h"

class SystrayManager;

/////////////////////////////////////////////////////////////////////////////
// CBirdwatcherApp:
// See Birdwatcher.cpp for the implementation of this class
//

class CBirdwatcherApp : public CWinApp
{
public:
	CBirdwatcherApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBirdwatcherApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation
	SystrayManager sysman;
	CWnd wnd;

	//{{AFX_MSG(CBirdwatcherApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_BIRDWATCHER_H__48E19F6C_ED69_4718_8BBA_D281C8649091__INCLUDED_)
