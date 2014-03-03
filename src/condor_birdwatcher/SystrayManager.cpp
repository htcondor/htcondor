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

#include "stdafx.h"
#include <windows.h>
#include "birdwatcher.h"
#include "systrayminimize.h"
#include "BirdWatcherDlg.h"

// #define GENERATE_BIRD_LOG 1

using namespace std;
const int CONDOR_SYSTRAY_INTERNAL_EVENTS = WM_USER + 4001;
HWND birdwatcherDLG;
HWND parentHwnd;
TCHAR *zCondorDir;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SystrayManager::SystrayManager()
{
	bMultipleCpusAvailable = false;
	wmr.setOwner(this);
	bAllIdle = true;
	
	HKEY hNotifyHandle;
	DWORD dwCreatedOrOpened;
	RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\condor"), 0, NULL, 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);
	
	DWORD dwValue = (unsigned) wmr.getHWnd();
	RegSetValueEx(hNotifyHandle, TEXT("systray_notify_handle"), 0, REG_DWORD, (unsigned char *) &dwValue, sizeof(DWORD));
	
	DWORD dwType;
	DWORD dwData = 1; // assume one-bird mode
	DWORD dwDataLen = 4;
	RegQueryValueEx(hNotifyHandle, TEXT("systray_one_bird"), 0, &dwType, (unsigned char *) &dwData, &dwDataLen);
	bUseSingleIcon = dwData == 1;

	TCHAR *psBuf = new TCHAR[MAX_PATH];
	dwDataLen = MAX_PATH * sizeof(TCHAR);
	RegQueryValueEx(hNotifyHandle, TEXT("RELEASE_DIR"), 0, &dwType, (unsigned char *) psBuf, &dwDataLen);
	
	vecIconsForEachCpu.resize(1);
	hPopupMenu = NULL;
	
	//Just directly get the handle.
	notifyWnd = wmr.getHWnd();
	
	birdwatcherDLG = CreateDialog(hInst, MAKEINTRESOURCE(IDD_BIRDWATCHER_DIALOG), notifyWnd, DialogProc);
	if(!birdwatcherDLG)
	{
		DWORD err = GetLastError();
		TCHAR sz[256];
		wsprintf(sz, TEXT("CreateDialog failed err=%d"), err);
		OutputDebugString(sz);
	}
	zCondorDir = psBuf;
	//pDlg->Create(IDD_BIRDWATCHER_DIALOG);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SystrayManager::~SystrayManager()
{
	for (BirdIconVector::size_type i = 0; i < vecIconsForEachCpu.size(); i++)
	{
		removeIcon(i);
	}
	KillTimer(wmr.getHWnd(), iTimerId);
	//notifyWnd.Detach();
	//I assume this is just to destroy the handle.
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SystrayManager::init(HICON hCondorOff, HICON hIdle, HICON hClaimed, HICON hRunningJob1, HICON hRunningJob2, HICON hSuspended, HICON hPreempting)
{
	this->hCondorOff = hCondorOff;
	this->hIdle = hIdle;
	this->hClaimed = hClaimed;
	this->hRunningJob1 = hRunningJob1;
	this->hRunningJob2 = hRunningJob2;
	this->hSuspended = hSuspended;
	this->hPreempting = hPreempting;
	
	iTimerId = SetTimer(wmr.getHWnd(), 1000, 180, NULL);
	
	reloadStatus();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SystrayManager::removeIcon(BirdIconVector::size_type iCpuId)
{
	if (iCpuId >= vecIconsForEachCpu.size())
		return;
	
	BirdIcon &icon = vecIconsForEachCpu[iCpuId];
	if (icon.bIconActive)
	{
		Shell_NotifyIcon(NIM_DELETE, &(icon.nid));
		icon.bIconActive = false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SystrayManager::setIcon(BirdIconVector::size_type iCpuId, HICON hToSet)
{
	if (vecIconsForEachCpu.size() <= iCpuId)
		vecIconsForEachCpu.resize(iCpuId+1);
	BirdIcon &icon = vecIconsForEachCpu[iCpuId];
	
	icon.nid.hIcon = hToSet;
	icon.nid.hWnd = wmr.getHWnd();
	lstrcpyn(icon.nid.szTip, icon.strTooltip, _countof(icon.nid.szTip));
	icon.nid.uCallbackMessage = CONDOR_SYSTRAY_INTERNAL_EVENTS;
	icon.nid.uFlags = NIF_MESSAGE | NIF_ICON;
	if (lstrlen(icon.strTooltip))	// if the tooltip string isn't empty
		icon.nid.uFlags |= NIF_TIP;  // enable the tooltip
	icon.nid.cbSize = sizeof(icon.nid);
	
	if (!icon.bIconActive) // first call, so need to create the icon
	{
		icon.bIconActive = true;
		icon.nid.uID = iCpuId;
		Shell_NotifyIcon(NIM_ADD, &(icon.nid));
	}
	else
	{
		Shell_NotifyIcon(NIM_MODIFY, &(icon.nid));
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT SystrayManager::onReceivedWindowsMessage(WindowsMessageReceiver *pSource, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int iTimerNum = 0;
	LRESULT lRet = 0;
	
	string strLogMsg;
	switch (message)
	{
	case CONDOR_SYSTRAY_NOTIFY_CHANGED:
			strLogMsg = "Got notify changed msg";
			reloadStatus();
			break;
	case WM_COMMAND :
		{
			//Debug output, uncomment and fix if you need it
			//TRACE("got command lparam %d, wparam %d\n", (int) lParam, (int) wParam);
			if (wParam == 1) // hide icon
			{
				strLogMsg = "Got hide command";
				PostQuitMessage(0);
			}
			else if (wParam == 2) // condor on
			{
				strLogMsg = "Got condor on command";
				int err = system("condor on");
				if (err == -1) lRet = (LRESULT)GetLastError();
			}
			else if (wParam == 3) // condor off
			{
				strLogMsg = "Got condor off command";
				int err = system("condor off");
				if (err == -1) lRet = (LRESULT)GetLastError();
			}
			else if (wParam == 5) // vacate all
			{
				strLogMsg = "Got vacate command";
				int err = system("condor_vacate");
				if (err == -1) lRet = (LRESULT)GetLastError();
			}
			else if (wParam == 6)
			{
				strLogMsg = "Got single-icon toggle command";
				
				bUseSingleIcon = !bUseSingleIcon;
				
				HKEY hNotifyHandle;
				DWORD dwCreatedOrOpened;
				RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\condor"), 0, NULL, 
					REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);
				
				DWORD dwValue = bUseSingleIcon ? 1 : 0;
				RegSetValueEx(hNotifyHandle, TEXT("systray_one_bird"), 0, REG_DWORD, (BYTE*)&dwValue, sizeof(DWORD));
				
				reloadStatus();
			}
			break;
		}
	case WM_TIMER:
		{
			iTimerNum++;
			
			if (iTimerNum % 10 == 0)
				reloadStatus();
			
			for (BirdIconVector::size_type i = 0; i < vecIconsForEachCpu.size(); i++)
			{
				BirdIcon &icon = vecIconsForEachCpu[i];
				if (icon.bIconActive && icon.bRunningJob)
				{
					HICON hToSet = hRunningJob1;
					if ((iTimerNum + i)%2 == 0) // every other bird flaps in sync
					{
						hToSet = hRunningJob2;
					}
					
					setIcon(i, hToSet);
				}
			}
			
			break;
		}
	case CONDOR_SYSTRAY_INTERNAL_EVENTS:
		{
			if (lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONDOWN)
			{
				if (hPopupMenu)
					DestroyMenu(hPopupMenu);
				
				strLogMsg = "Got menu popup command";
				
				
				hPopupMenu = CreatePopupMenu();
				AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 1, TEXT("Hide Birdwatcher (this icon)"));
				if (bMultipleCpusAvailable)
				{
					if (!bUseSingleIcon)
						AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 6, TEXT("Show one bird only"));
					else
						AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 6, TEXT("Show one bird per cpu"));
				}
				AppendMenu(hPopupMenu, MF_SEPARATOR, 4, NULL);
				if (vecIconsForEachCpu.size() == 1 && vecIconsForEachCpu[0].nid.hIcon == hCondorOff)
				{
					AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 2, TEXT("Turn Condor on"));
				}
				else
				{
					AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 3, TEXT("Turn Condor off"));
					AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 5, TEXT("Vacate all local jobs"));
				}
				
				SetForegroundWindow(wmr.getHWnd());

				POINT pos;
				GetCursorPos(&pos);
				int idCmd = (int)TrackPopupMenuEx(hPopupMenu, 
					                              TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD, 
												  pos.x, pos.y, wmr.getHWnd(), NULL);
				if (idCmd)
				{
					DWORD err = SendMessage(wmr.getHWnd(), WM_COMMAND, idCmd, 0);
					if (err)
					{
						TCHAR sz[256];
						FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0, sz, _countof(sz), NULL);
						sz[_countof(sz)-1] = 0;
						OutputDebugString(sz);
						MessageBox(parentHwnd, sz, TEXT("Command Failed"), 0);
					}
				}
			}				
			else if (lParam == WM_LBUTTONDBLCLK)
			{
				RestoreWndFromTray(birdwatcherDLG);
			}
			break;
		}
	}
	
#ifdef GENERATE_BIRD_LOG
	if (strLogMsg != "")
	{
		strLogMsg += "\r\n";
		CFile outfile;
		outfile.Open(".\\birdlog.txt", CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite);
		outfile.SeekToEnd();
		outfile.Write(strLogMsg.c_str(), strLogMsg.length());
		outfile.Close();
	}
#endif

	return lRet;
}

std::string makeString(int iVal)
{
	char psBuf[32];
	itoa(iVal, psBuf, 10);
	return string(psBuf);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SystrayManager::reloadStatus()
{
	HKEY hNotifyHandle;
	DWORD dwCreatedOrOpened;
	RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\condor"), 0, NULL, 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);
	
	DWORD dwType;
	DWORD dwData = 0;
	DWORD dwDataLen = sizeof(dwData);
	RegQueryValueEx(hNotifyHandle, TEXT("systray_num_cpus"), 0, &dwType, (BYTE *)&dwData, &dwDataLen);
	int iCpus = (int) dwData;
	
	if (!iCpus)
	{
		for (BirdIconVector::size_type i = 1; i < vecIconsForEachCpu.size(); i++)
		{	
			removeIcon(i);
		}
		
		vecIconsForEachCpu.resize(1);
		vecIconsForEachCpu[0].bRunningJob = false;
		lstrcpyn(vecIconsForEachCpu[0].strTooltip, TEXT("Birdwatcher: Condor is off"), _countof(vecIconsForEachCpu[0].strTooltip));
		setIcon(0, hCondorOff);
		
#ifdef GENERATE_BIRD_LOG
		string strLogMsg = "Reloading - CONDOR IS OFF\r\n";
		CFile outfile;
		outfile.Open(".\\birdlog.txt", CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite);
		outfile.SeekToEnd();
		outfile.Write(strLogMsg.c_str(), strLogMsg.length());
		outfile.Close();
#endif

		RegCloseKey(hNotifyHandle);
		return;
	}
	
	bMultipleCpusAvailable = iCpus > 1;
	if (iCpus > 10)
	{
		bUseSingleIcon = true;
		iCpus = 10;
	}
	
	if (!bUseSingleIcon && iCpus != vecIconsForEachCpu.size())
		vecIconsForEachCpu.resize(iCpus);
	
	bAllIdle = true;
	bool bAnyRunning = false;
	
	string strLogMsg = "Reloading " + makeString(iCpus) + " cpus: ";
	for (int ii = 0; ii < iCpus; ++ii)
	{
		TCHAR szKey[50];
		wsprintf(szKey, TEXT("systray_cpu_%d_state"), ii);
		dwData = (int) kSystrayStatusIdle; // assume idle
		dwDataLen = sizeof(dwData);
		RegQueryValueEx(hNotifyHandle, szKey, 0, &dwType, (BYTE *) &dwData, &dwDataLen);
		
		ECpuStatus eStatus = (ECpuStatus) ((int) dwData);
		
		strLogMsg += makeString((int) eStatus) + " ";
		
		if (bUseSingleIcon && bMultipleCpusAvailable)
		{
			if (eStatus != kSystrayStatusIdle)
				bAllIdle = false;
			if (eStatus == kSystrayStatusJobRunning)
				bAnyRunning = true;
		}
		else
		{
			BirdIcon &icon = vecIconsForEachCpu[ii];
			if (eStatus == kSystrayStatusIdle)
			{
				icon.bRunningJob = false;
				lstrcpyn(icon.strTooltip, TEXT("Birdwatcher: This cpu is idle"), _countof(icon.strTooltip));
				setIcon(ii, hIdle);
			}
			else if (eStatus == kSystrayStatusJobRunning)
			{
				icon.bRunningJob = true;
				lstrcpyn(icon.strTooltip, TEXT("Birdwatcher: This cpu is running a Condor job"), _countof(icon.strTooltip));
				setIcon(ii, hRunningJob1);
			}
			else if (eStatus == kSystrayStatusJobSuspended)
			{
				icon.bRunningJob = false;
				lstrcpyn(icon.strTooltip, TEXT("Birdwatcher: The job on this cpu is suspended"), _countof(icon.strTooltip));
				setIcon(ii, hSuspended);
			}
			else if (eStatus == kSystrayStatusJobPreempting)
			{
				icon.bRunningJob = false;
				lstrcpyn(icon.strTooltip, TEXT("Birdwatcher: The job on this cpu is preempting"), _countof(icon.strTooltip));
				setIcon(ii, hPreempting);
			}
			else
			{
				icon.bRunningJob = false;
				lstrcpyn(icon.strTooltip, TEXT("Birdwatcher: This cpu is claimed for a Condor job"), _countof(icon.strTooltip));
				setIcon(ii, hClaimed);
			}
		}
	}
	
#ifdef GENERATE_BIRD_LOG
	strLogMsg += "\r\n";
	CFile outfile;
	outfile.Open(".\\birdlog.txt", CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite);
	outfile.SeekToEnd();
	outfile.Write(strLogMsg.c_str(), strLogMsg.length());
	outfile.Close();
#endif
	
	if (bUseSingleIcon && bMultipleCpusAvailable)
	{
		for (BirdIconVector::size_type i = 1; i < vecIconsForEachCpu.size(); i++)
		{	
			removeIcon(i);
		}
		
		vecIconsForEachCpu.resize(1);
		
		if (bAllIdle)
		{
			vecIconsForEachCpu[0].bRunningJob = false;
			lstrcpyn(vecIconsForEachCpu[0].strTooltip, TEXT("Birdwatcher: Condor is idle"), _countof(vecIconsForEachCpu[0].strTooltip));
			setIcon(0, hIdle);
		}
		else if (!bAnyRunning)
		{
			vecIconsForEachCpu[0].bRunningJob = false;
			lstrcpyn(vecIconsForEachCpu[0].strTooltip,  TEXT("Birdwatcher: claimed for a Condor job..."), _countof(vecIconsForEachCpu[0].strTooltip));
			setIcon(0, hClaimed);
		}
		else
		{
			vecIconsForEachCpu[0].bRunningJob = true;
			lstrcpyn(vecIconsForEachCpu[0].strTooltip, TEXT("Birdwatcher: running a Condor job"), _countof(vecIconsForEachCpu[0].strTooltip));
			setIcon(0, hRunningJob1);
		}
	}
	RegCloseKey(hNotifyHandle);
}
