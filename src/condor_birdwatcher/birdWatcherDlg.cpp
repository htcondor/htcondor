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
