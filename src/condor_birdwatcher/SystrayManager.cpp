#include <stdafx.h>
#include "SystrayManager.h"
#include "resource.h"
#include "systrayminimize.h"
#include "BirdWatcherDlg.h"

// #define GENERATE_BIRD_LOG 1

using namespace std;
const int CONDOR_SYSTRAY_INTERNAL_EVENTS = WM_USER + 4001;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SystrayManager::SystrayManager()
{
	bMultipleCpusAvailable = false;
	wmr.setOwner(this);
	bAllIdle = true;
	
	HKEY hNotifyHandle;
	DWORD dwCreatedOrOpened;
	RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\condor", 0, "REG_DWORD", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);
	
	DWORD dwValue = (unsigned) wmr.getHWnd();
	RegSetValueEx(hNotifyHandle, "systray_notify_handle", 0, REG_DWORD, (unsigned char *) &dwValue, sizeof(DWORD));
	
	DWORD dwType;
	DWORD dwData = 1; // assume one-bird mode
	DWORD dwDataLen = 4;
	RegQueryValueEx(hNotifyHandle, "systray_one_bird", 0, &dwType, (unsigned char *) &dwData, &dwDataLen);
	bUseSingleIcon = dwData == 1;

	char psBuf[MAX_PATH];
	dwDataLen = MAX_PATH;
	RegQueryValueEx(hNotifyHandle, "RELEASE_DIR", 0, &dwType, (unsigned char *) psBuf, &dwDataLen);
	
	vecIconsForEachCpu.resize(1);
	hPopupMenu = NULL;
	
	notifyWnd.Attach(wmr.getHWnd());
	
	pDlg = new CBirdWatcherDlg();
	pDlg->zCondorDir = psBuf;
	pDlg->Create(IDD_BIRDWATCHER_DIALOG);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SystrayManager::~SystrayManager()
{
	delete pDlg;
	for (int i = 0; i < vecIconsForEachCpu.size(); i++)
	{
		removeIcon(i);
	}
	::KillTimer(wmr.getHWnd(), iTimerId);
	notifyWnd.Detach();
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
	
	iTimerId = ::SetTimer(wmr.getHWnd(), 1000, 180, NULL);
	
	reloadStatus();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SystrayManager::removeIcon(int iCpuId)
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

void SystrayManager::setIcon(int iCpuId, HICON hToSet)
{
	if (vecIconsForEachCpu.size() <= iCpuId)
		vecIconsForEachCpu.resize(iCpuId+1);
	BirdIcon &icon = vecIconsForEachCpu[iCpuId];
	
	icon.nid.hIcon = hToSet;
	icon.nid.hWnd = wmr.getHWnd();
	strcpy(icon.nid.szTip, icon.strTooltip.c_str());
	icon.nid.uCallbackMessage = CONDOR_SYSTRAY_INTERNAL_EVENTS;
	icon.nid.uFlags = NIF_MESSAGE | NIF_ICON;
	if (icon.strTooltip.length())	// if the tooltip string isn't empty
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

void SystrayManager::onReceivedWindowsMessage(WindowsMessageReceiver *pSource, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int iTimerNum = 0;
	
	string strLogMsg;
	
	switch (message)
	{
	case (CONDOR_SYSTRAY_NOTIFY_CHANGED) :
		{
			strLogMsg = "Got notify changed msg";
			reloadStatus();
			break;
		}
	case (WM_COMMAND) :
		{
			TRACE("got command lparam %d, wparam %d\n", (int) lParam, (int) wParam);
			if (wParam == 1) // hide icon
			{
				strLogMsg = "Got hide command";
				PostQuitMessage(0);
			}
			else if (wParam == 2) // condor on
			{
				strLogMsg = "Got condor on command";
				system("condor on");
			}
			else if (wParam == 3) // condor off
			{
				strLogMsg = "Got condor off command";
				system("condor off");
			}
			else if (wParam == 5) // vacate all
			{
				strLogMsg = "Got vacate command";
				system("condor_vacate");
			}
			else if (wParam == 6)
			{
				strLogMsg = "Got single-icon toggle command";
				
				bUseSingleIcon = !bUseSingleIcon;
				
				HKEY hNotifyHandle;
				DWORD dwCreatedOrOpened;
				RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\condor", 0, "REG_DWORD", 
					REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);
				
				DWORD dwValue = bUseSingleIcon ? 1 : 0;
				RegSetValueEx(hNotifyHandle, "systray_one_bird", 0, REG_DWORD, (unsigned char *) &dwValue, sizeof(DWORD));
				
				reloadStatus();
			}
			break;
		}
	case (WM_TIMER) :
		{
			iTimerNum++;
			
			if (iTimerNum % 10 == 0)
				reloadStatus();
			
			for (int i = 0; i < vecIconsForEachCpu.size(); i++)
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
	case (CONDOR_SYSTRAY_INTERNAL_EVENTS) :
		{
			if (lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONDOWN)
			{
				if (hPopupMenu)
					DestroyMenu(hPopupMenu);
				
				strLogMsg = "Got menu popup command";
				
				
				hPopupMenu = CreatePopupMenu();
				AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 1, "Hide Birdwatcher (this icon)");
				if (bMultipleCpusAvailable)
				{
					if (!bUseSingleIcon)
						AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 6, "Show one bird only");
					else
						AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 6, "Show one bird per cpu");
				}
				AppendMenu(hPopupMenu, MF_SEPARATOR, 4, NULL);
				if (vecIconsForEachCpu.size() == 1 && vecIconsForEachCpu[0].nid.hIcon == hCondorOff)
				{
					AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 2, "Turn Condor on");
				}
				else
				{
					AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 3, "Turn Condor off");
					AppendMenu(hPopupMenu, MF_STRING | MF_ENABLED, 5, "Vacate all local jobs");
				}
				
				SetForegroundWindow(wmr.getHWnd());
				CPoint pos;
				GetCursorPos(&pos);
				if (!TrackPopupMenuEx(hPopupMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pos.x, pos.y, wmr.getHWnd(), NULL))
				{
					CHAR szBuf[256]; 
					DWORD dw = GetLastError(); 
					FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dw, 0, szBuf, 256, NULL);
					AfxMessageBox(szBuf);			
				}
			}				
			else if (lParam == WM_LBUTTONDBLCLK)
			{
				RestoreWndFromTray(pDlg->m_hWnd);
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
	RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\condor", 0, "REG_DWORD", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);
	
	DWORD dwType;
	DWORD dwData = 0;
	DWORD dwDataLen = 4;
	RegQueryValueEx(hNotifyHandle, "systray_num_cpus", 0, &dwType, (unsigned char *) &dwData, &dwDataLen);
	int iCpus = (int) dwData;
	
	if (!iCpus)
	{
		for (int i = 1; i < vecIconsForEachCpu.size(); i++)
		{	
			removeIcon(i);
		}
		
		vecIconsForEachCpu.resize(1);
		vecIconsForEachCpu[0].bRunningJob = false;
		vecIconsForEachCpu[0].strTooltip = "Birdwatcher: Condor is off";
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
	for (int i = 0; i < iCpus; i++)
	{
		char psBuf[256];
		sprintf(psBuf, "systray_cpu_%d_state", i);
		dwData = (int) kSystrayStatusIdle; // assume idle
		RegQueryValueEx(hNotifyHandle, psBuf, 0, &dwType, (unsigned char *) &dwData, &dwDataLen);
		
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
			BirdIcon &icon = vecIconsForEachCpu[i];
			if (eStatus == kSystrayStatusIdle)
			{
				icon.bRunningJob = false;
				icon.strTooltip = "Birdwatcher: This cpu is idle";
				setIcon(i, hIdle);
			}
			else if (eStatus == kSystrayStatusJobRunning)
			{
				icon.bRunningJob = true;
				icon.strTooltip = "Birdwatcher: This cpu is running a Condor job";
				setIcon(i, hRunningJob1);
			}
			else if (eStatus == kSystrayStatusJobSuspended)
			{
				icon.bRunningJob = false;
				icon.strTooltip = "Birdwatcher: The job on this cpu is suspended";
				setIcon(i, hSuspended);
			}
			else if (eStatus == kSystrayStatusJobPreempting)
			{
				icon.bRunningJob = false;
				icon.strTooltip = "Birdwatcher: The job on this cpu is preempting";
				setIcon(i, hPreempting);
			}
			else
			{
				icon.bRunningJob = false;
				icon.strTooltip = "Birdwatcher: This cpu is claimed for a Condor job";
				setIcon(i, hClaimed);
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
		for (int i = 1; i < vecIconsForEachCpu.size(); i++)
		{	
			removeIcon(i);
		}
		
		vecIconsForEachCpu.resize(1);
		
		if (bAllIdle)
		{
			vecIconsForEachCpu[0].bRunningJob = false;
			vecIconsForEachCpu[0].strTooltip = "Birdwatcher: Condor is idle";
			setIcon(0, hIdle);
		}
		else if (!bAnyRunning)
		{
			vecIconsForEachCpu[0].bRunningJob = false;
			vecIconsForEachCpu[0].strTooltip = "Birdwatcher: claimed for a Condor job...";
			setIcon(0, hClaimed);
		}
		else
		{
			vecIconsForEachCpu[0].bRunningJob = true;
			vecIconsForEachCpu[0].strTooltip = "Birdwatcher: running a Condor job";
			setIcon(0, hRunningJob1);
		}
	}
	RegCloseKey(hNotifyHandle);
}
