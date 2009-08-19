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
			DWORD temp = GetLastError();
			WCHAR buffer[256];
			_ltow(temp, buffer, 10);
			OutputDebugString(buffer);
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

	HANDLE hBirdq_Rd;
	HANDLE hBirdq_Wr;
	HANDLE hBirdst_Rd;
	HANDLE hBirdst_Wr;

	BOOL bSuccess;

	DWORD dwRead;
	DWORD bufSize = 1024;

	SECURITY_ATTRIBUTES saAttr;

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	if(!CreatePipe(&hBirdq_Rd, &hBirdq_Wr, &saAttr, 0))
	{
		return;
	}

	if(!SetHandleInformation(hBirdq_Rd, HANDLE_FLAG_INHERIT, 0))
	{
		return;
	}

	STARTUPINFO si[2];
	PROCESS_INFORMATION pi[2];

	ZeroMemory(&si[0], sizeof(STARTUPINFO));
	ZeroMemory(&si[1], sizeof(STARTUPINFO));

	ZeroMemory(&pi[0], sizeof(PROCESS_INFORMATION));
	ZeroMemory(&pi[1], sizeof(PROCESS_INFORMATION));

	si[0].dwFlags |= STARTF_USESTDHANDLES;
	si[0].hStdOutput = hBirdq_Wr;
	si[0].cb = sizeof(si[0]);

	bSuccess = CreateProcess(L"condor_q.exe", NULL, NULL, NULL, true, CREATE_NO_WINDOW, NULL, NULL, &si[0], &pi[0]);
	if(bSuccess)
	{
		if(WaitForSingleObject(pi[0].hProcess, 2000) == WAIT_TIMEOUT)
				TerminateProcess(pi[0].hProcess, 1);

		if(CloseHandle(hBirdq_Wr))
		{
			char *psBuf = new char[bufSize];
			ZeroMemory(psBuf, bufSize);
			ReadFile(hBirdq_Rd, psBuf, bufSize, &dwRead, NULL);

			SetDlgItemTextA(birdwatcherDLG, IDC_EDIT_TOP_PANE, psBuf);
			delete [] psBuf;
			CloseHandle(hBirdq_Rd);
		}
	}

	CloseHandle(pi[0].hProcess);
	CloseHandle(pi[0].hThread);

	if(!CreatePipe(&hBirdst_Rd, &hBirdst_Wr, &saAttr, 0))
	{
		return;
	}

	if(!SetHandleInformation(hBirdst_Rd, HANDLE_FLAG_INHERIT, 0))
	{
		return;
	}

	si[1].dwFlags |= STARTF_USESTDHANDLES;
	si[1].hStdOutput = hBirdst_Wr;
	si[1].cb = sizeof(si[1]);

	bSuccess = CreateProcess(L"condor_status.exe", NULL, NULL, NULL, true, CREATE_NO_WINDOW, NULL, NULL, &si[1], &pi[1]);
	if(bSuccess)
	{
		if(WaitForSingleObject(pi[1].hProcess, 2000) == WAIT_TIMEOUT)
				TerminateProcess(pi[1].hProcess, 1);

		if(CloseHandle(hBirdst_Wr))
		{
			char *psBuf = new char[bufSize];
			ZeroMemory(psBuf, bufSize);
			ReadFile(hBirdst_Rd, psBuf, bufSize, &dwRead, NULL);

			SetDlgItemTextA(birdwatcherDLG, IDC_EDIT_BOTTOM_PANE, psBuf);
			delete [] psBuf;
			CloseHandle(hBirdst_Rd);
		}
	}

	CloseHandle(pi[1].hProcess);
	CloseHandle(pi[1].hThread);
}