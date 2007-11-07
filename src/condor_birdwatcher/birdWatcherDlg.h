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


#if !defined(AFX_BIRDWATCHERDLG_H__2132927B_4970_46A0_A2D8_6F50BA17E681__INCLUDED_)
#define AFX_BIRDWATCHERDLG_H__2132927B_4970_46A0_A2D8_6F50BA17E681__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// BirdWatcherDlg.h : header file
//
#include "BirdWatcher.h"
/////////////////////////////////////////////////////////////////////////////
// CBirdWatcherDlg dialog

class CBirdWatcherDlg : public CDialog
{
// Construction
public:
	CBirdWatcherDlg(CWnd* pParent = NULL);   // standard constructor

	CString zCondorDir;

// Dialog Data
	//{{AFX_DATA(CBirdWatcherDlg)
	enum { IDD = IDD_BIRDWATCHER_DIALOG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBirdWatcherDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CBirdWatcherDlg)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTimer(UINT nIDEvent);
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	virtual void OnOK();
	virtual void OnCancel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_BIRDWATCHERDLG_H__2132927B_4970_46A0_A2D8_6F50BA17E681__INCLUDED_)
