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