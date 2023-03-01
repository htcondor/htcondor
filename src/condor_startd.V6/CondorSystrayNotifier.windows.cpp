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

#include "condor_common.h"
#include "CondorSystrayNotifier.windows.h"
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
	int iResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\condor", 0, NULL,
		REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);

	if (  iResult != ERROR_SUCCESS ) {
		return; 
	}

	DWORD dwValue = (unsigned) iHighestCpuIdSeen + 1;
	iResult = RegSetValueEx(hNotifyHandle, "systray_num_cpus", 0, REG_DWORD, (unsigned char *) &dwValue, sizeof(DWORD));
	RegCloseKey(hNotifyHandle);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::writeToRegistryForCpu(int iCpuId, int iStatus)
{
	HKEY hNotifyHandle;
	DWORD dwCreatedOrOpened, result;
	result = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\condor", 0, (LPSTR)"REG_DWORD", 
		REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);

	if ( result != ERROR_SUCCESS ) {
		return;
	}

	char psBuf[256];
	snprintf(psBuf, 256, "systray_cpu_%d_state", iCpuId);
	DWORD dwValue = iStatus;
	RegSetValueEx(hNotifyHandle, psBuf, 0, REG_DWORD, (unsigned char *) &dwValue, sizeof(DWORD));
	RegCloseKey(hNotifyHandle);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CondorSystrayNotifier::notifyRegistryChanged()
{
	// first open up the condor registry key
	HKEY hNotifyHandle;
	DWORD dwCreatedOrOpened;
	DWORD result;
	result = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\condor", 0, NULL,
		REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hNotifyHandle, &dwCreatedOrOpened);

	if ( result != ERROR_SUCCESS ) {
		return;
	}

	// now read the systray notify handle (this SHOULD be a "volatile" handle stored only in memory
	DWORD dwType = 0;
	ULONGLONG qwData = 0;
	DWORD dwDataLen = sizeof(qwData);
	
	
	RegQueryValueEx(hNotifyHandle, "systray_notify_handle", 0, &dwType, (unsigned char *) &qwData, &dwDataLen);

	HWND hToNotify = (HWND)(ULONG_PTR)qwData;
	if (IsWindow(hToNotify))
		PostMessage(hToNotify, CONDOR_SYSTRAY_NOTIFY_CHANGED, 0, 0);

	RegCloseKey(hNotifyHandle);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
