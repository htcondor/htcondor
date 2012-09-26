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

#ifndef SYSTRAY_MANAGER_H
#define SYSTRAY_MANAGER_H

#include "WindowsMessageReceiver.h"
#include "CondorSystrayCommon.h"
#include <string>
#include <vector>

//class CBirdWatcherDlg;
class SystrayManager;

class SystrayManager : private WindowsMessageReceiverOwner
{
public:
	SystrayManager();
	~SystrayManager();

	void init(HICON hCondorOff, HICON hIdle, HICON hClaimed, HICON hRunningJob1, HICON hRunningJob2, HICON hSuspended, HICON hPreempting);

	//Change to the win32 handle
	//CWnd notifyWnd;
	HWND notifyWnd;
private:

	struct BirdIcon
	{
		BirdIcon() { bIconActive = false; }
		NOTIFYICONDATA nid;
		TCHAR strTooltip[256];
		bool bIconActive;
		bool bRunningJob;
	};
	typedef std::vector<BirdIcon> BirdIconVector;
	BirdIconVector vecIconsForEachCpu;

	void setIcon(BirdIconVector::size_type iCpuId, HICON hToSet);
	void removeIcon(BirdIconVector::size_type iCpuId);
	
	LRESULT onReceivedWindowsMessage(WindowsMessageReceiver *pSource, UINT message, WPARAM wParam, LPARAM lParam);
	void reloadStatus();
	WindowsMessageReceiver wmr;

	int iTimerId;
	bool bUseSingleIcon;

	bool bMultipleCpusAvailable;
	bool bAllIdle;

	HMENU hPopupMenu;
	
	HICON hCondorOff;
	HICON hIdle;
	HICON hClaimed;
	HICON hRunningJob1;
	HICON hRunningJob2;
	HICON hSuspended;
	HICON hPreempting;
};

#endif
