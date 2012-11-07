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


// BirdWatcherDlg.cpp : implementation file
//

#include "stdafx.h"
#include "birdwatcher.h"
#include "BirdWatcherDlg.h"
#include "systrayminimize.h"

#ifdef _DEBUG
//#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
RECT rc, rcDlg, rcOwner;
/////////////////////////////////////////////////////////////////////////////
// CBirdWatcherDlg dialog

INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:

		if(!SetTimer(hwndDlg, 1000, 1000, NULL))
		{
			DWORD err = GetLastError();
			TCHAR sz[256];
			wsprintf(sz, TEXT("SetTimer failed err=%d"), err);
			OutputDebugString(sz);
		}
		
		return true;

	case WM_TIMER:
		//OutputDebugString(L"Timer\n");
		OnTimer(WM_TIMER);

		return true;
		break;
	case WM_CLOSE:
		MinimizeWndToTray(hwndDlg);

		return true;
		break;
	case WM_COMMAND:
		MinimizeWndToTray(hwndDlg);

		return true;
		break;
	default:
		return false;
	}
}

void OnTimer(UINT nIDEvent)
{
	if (!IsWindowVisible(birdwatcherDLG))
		return;

	const int bufSize = 4096;

	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	HANDLE hRd = NULL;
	HANDLE hWr = NULL;
	if(!CreatePipe(&hRd, &hWr, &saAttr, 0))
	{
		return;
	}

	if(!SetHandleInformation(hRd, HANDLE_FLAG_INHERIT, 0))
	{
		CloseHandle(hRd); hRd = NULL;
		CloseHandle(hWr); hWr = NULL;
		return;
	}

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdOutput = hWr;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	BOOL bSuccess = CreateProcess(TEXT("condor_q.exe"), 
		                          NULL, NULL, NULL, true, 
								  CREATE_NO_WINDOW, NULL, NULL, 
								  &si, &pi);
	if(bSuccess)
	{
		CloseHandle(pi.hThread); // don't need this.

		if(WaitForSingleObject(pi.hProcess, 2000) == WAIT_TIMEOUT)
			TerminateProcess(pi.hProcess, 1);

		char szBuf[bufSize+1];
		ZeroMemory(szBuf, bufSize+1);

		DWORD cbRead = 0;
		if (ReadFile(hRd, szBuf, sizeof(szBuf)-sizeof(szBuf[0]), &cbRead, NULL))
		{
			SetDlgItemTextA(birdwatcherDLG, IDC_EDIT_TOP_PANE, szBuf);
			szBuf[cbRead/sizeof(szBuf[0])] = 0;
		}


		CloseHandle(pi.hProcess);
	}
	CloseHandle(hRd); hRd = NULL;
	CloseHandle(hWr); hWr = NULL;

	ZeroMemory(&pi, sizeof(pi));

	if(!CreatePipe(&hRd, &hWr, &saAttr, 0))
	{
		return;
	}

	if(!SetHandleInformation(hRd, HANDLE_FLAG_INHERIT, 0))
	{
		CloseHandle(hRd); hRd = NULL;
		CloseHandle(hWr); hWr = NULL;
		return;
	}

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdOutput = hWr;

	bSuccess = CreateProcess(TEXT("condor_status.exe"), 
		                     NULL, NULL, NULL, true, 
							 CREATE_NO_WINDOW, NULL, NULL, 
							 &si, &pi);
	if(bSuccess)
	{
		CloseHandle(pi.hThread); // don't need this.

		if(WaitForSingleObject(pi.hProcess, 2000) == WAIT_TIMEOUT)
			TerminateProcess(pi.hProcess, 1);

		char szBuf[bufSize+1];
		ZeroMemory(szBuf, bufSize+1);

		DWORD cbRead = 0;
		if (ReadFile(hRd, szBuf, sizeof(szBuf)-sizeof(szBuf[0]), &cbRead, NULL))
		{
			SetDlgItemTextA(birdwatcherDLG, IDC_EDIT_BOTTOM_PANE, szBuf);
			szBuf[cbRead/sizeof(szBuf[0])] = 0;
		}

		CloseHandle(pi.hProcess);
	}
	CloseHandle(hRd); hRd = NULL;
	CloseHandle(hWr); hWr = NULL;
}
