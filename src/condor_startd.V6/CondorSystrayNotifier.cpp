/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_common.h"
#include "CondorSystrayNotifier.h"
#include "CondorSystrayCommon.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CondorSystrayNotifier::CondorSystrayNotifier()
{
	iHighestCpuIdSeen = -1;
	writeNumCpusToRegistry();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::notifyCondorOff()
{
	iHighestCpuIdSeen = -1;
	writeNumCpusToRegistry();
	notifyRegistryChanged();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::notifyCondorIdle(int iCpuId)
{
	checkForSkippedCpuIds(iCpuId);
	
	writeToRegistryForCpu(iCpuId, (int) kSystrayStatusIdle);
	notifyRegistryChanged();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::notifyCondorClaimed(int iCpuId)
{
	checkForSkippedCpuIds(iCpuId);
	
	writeToRegistryForCpu(iCpuId, (int) kSystrayStatusClaimed);
	notifyRegistryChanged();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::notifyCondorJobRunning(int iCpuId)
{
	checkForSkippedCpuIds(iCpuId);
	
	writeToRegistryForCpu(iCpuId, (int) kSystrayStatusJobRunning);
	notifyRegistryChanged();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::notifyCondorJobSuspended(int iCpuId)
{
	checkForSkippedCpuIds(iCpuId);
	
	writeToRegistryForCpu(iCpuId, (int) kSystrayStatusJobSuspended);
	notifyRegistryChanged();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::notifyCondorJobPreempting(int iCpuId)
{
	checkForSkippedCpuIds(iCpuId);
	
	writeToRegistryForCpu(iCpuId, (int) kSystrayStatusJobPreempting);
	notifyRegistryChanged();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::checkForSkippedCpuIds(int iCpuId)
{
	for (int i = iHighestCpuIdSeen + 1; i < iCpuId; i++)
	{
		notifyCondorIdle(i);
	}

	if (iCpuId > iHighestCpuIdSeen)
	{
		iHighestCpuIdSeen = iCpuId;
		writeNumCpusToRegistry();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::writeNumCpusToRegistry()
{
	HKEY hNotifyHandle;
	DWORD dwCreatedOrOpened;
	int iResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\condor", 0, "REG_DWORD", 
		REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);

	DWORD dwValue = (unsigned) iHighestCpuIdSeen + 1;
	iResult = RegSetValueEx(hNotifyHandle, "systray_num_cpus", 0, REG_DWORD, (unsigned char *) &dwValue, sizeof(DWORD));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::writeToRegistryForCpu(int iCpuId, int iStatus)
{
	HKEY hNotifyHandle;
	DWORD dwCreatedOrOpened;
	RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\condor", 0, "REG_DWORD", 
		REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);

	char psBuf[256];
	sprintf(psBuf, "systray_cpu_%d_state", iCpuId);
	DWORD dwValue = iStatus;
	RegSetValueEx(hNotifyHandle, psBuf, 0, REG_DWORD, (unsigned char *) &dwValue, sizeof(DWORD));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::notifyRegistryChanged()
{
	// first open up the condor registry key
	HKEY hNotifyHandle;
	DWORD dwCreatedOrOpened;
	RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\condor", 0, "REG_DWORD", 
		REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);
	
	// now read the systray notify handle (this SHOULD be a "volatile" handle stored only in memory
	DWORD dwType;
	DWORD dwData;
	DWORD dwDataLen;
	RegQueryValueEx(hNotifyHandle, "systray_notify_handle", 0, &dwType, (unsigned char *) &dwData, &dwDataLen);

	unsigned uHandle = dwData;
	HWND hToNotify = (HWND) uHandle;
	if (IsWindow(hToNotify))
		PostMessage(hToNotify, CONDOR_SYSTRAY_NOTIFY_CHANGED, 0, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
