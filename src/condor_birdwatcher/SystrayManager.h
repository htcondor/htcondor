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
#ifndef SYSTRAY_MANAGER_H
#define SYSTRAY_MANAGER_H

#include "WindowsMessageReceiver.h"
#include "CondorSystrayCommon.h"
#include <string>
#include <vector>

class CBirdWatcherDlg;
class SystrayManager;

class SystrayManager : private WindowsMessageReceiverOwner
{
public:
	SystrayManager();
	~SystrayManager();

	void init(HICON hCondorOff, HICON hIdle, HICON hClaimed, HICON hRunningJob1, HICON hRunningJob2, HICON hSuspended, HICON hPreempting);

	CWnd notifyWnd;
private:
	CBirdWatcherDlg *pDlg;
	void setIcon(int iCpuId, HICON hToSet);
	void removeIcon(int iCpuId);
	
	void onReceivedWindowsMessage(WindowsMessageReceiver *pSource, UINT message, WPARAM wParam, LPARAM lParam);
	void reloadStatus();
	WindowsMessageReceiver wmr;

	struct BirdIcon
	{
		BirdIcon() { bIconActive = false; }
		NOTIFYICONDATA nid;
		std::string strTooltip;
		bool bIconActive;
		bool bRunningJob;
	};
	std::vector<BirdIcon> vecIconsForEachCpu;

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