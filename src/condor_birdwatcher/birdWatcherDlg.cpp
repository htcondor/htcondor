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

// BirdWatcherDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Birdwatcher.h"
#include "BirdWatcherDlg.h"
#include "systrayminimize.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBirdWatcherDlg dialog


CBirdWatcherDlg::CBirdWatcherDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CBirdWatcherDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CBirdWatcherDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CBirdWatcherDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBirdWatcherDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CBirdWatcherDlg, CDialog)
	//{{AFX_MSG_MAP(CBirdWatcherDlg)
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBirdWatcherDlg message handlers

void CBirdWatcherDlg::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);
	
	
}

void CBirdWatcherDlg::OnTimer(UINT nIDEvent) 
{
	if (!IsWindowVisible())
		return;

	CFile infile;
	if (infile.Open(zCondorDir + "birdq.tmp", CFile::modeRead))
	{
		int iLen = infile.GetLength();
		char *psBuf = new char[iLen+1];
		infile.Read(psBuf, iLen);
		psBuf[iLen] = 0;
		
		SetDlgItemText(IDC_EDIT_TOP_PANE, psBuf);
		delete [] psBuf;
		infile.Close();
	}
	if (infile.Open(zCondorDir + "birdstatus.tmp", CFile::modeRead))
	{
		int iLen = infile.GetLength();
		char *psBuf = new char[iLen+1];
		infile.Read(psBuf, iLen);
		psBuf[iLen] = 0;
		
		SetDlgItemText(IDC_EDIT_BOTTOM_PANE, psBuf);
		delete [] psBuf;
		infile.Close();
	}

	CString zExec;
	zExec = zCondorDir + "bin\\condor_q.exe > " + zCondorDir + "birdq.tmp";
	WinExec(zExec, SW_HIDE);
	zExec = zCondorDir + "bin\\condor_status.exe > " + zCondorDir + "birdstatus.tmp";
	WinExec(zExec, SW_HIDE);

	CDialog::OnTimer(nIDEvent);
}

BOOL CBirdWatcherDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	SetIcon(LoadIcon(NULL, MAKEINTRESOURCE(IDR_MAINFRAME)), TRUE);
	
	char cLast = zCondorDir[zCondorDir.GetLength()-1];
	if (cLast != '\\' && cLast != '/')
		zCondorDir += '\\';
		
	SetTimer(1000, 1000, NULL);	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CBirdWatcherDlg::OnClose() 
{
	MinimizeWndToTray(m_hWnd);
}

void CBirdWatcherDlg::OnOK() 
{
	MinimizeWndToTray(m_hWnd);
}

void CBirdWatcherDlg::OnCancel() 
{
	MinimizeWndToTray(m_hWnd);
}
