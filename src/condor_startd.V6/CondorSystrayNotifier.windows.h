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

#ifndef CONDOR_SYSTRAY_NOTIFIER
#define CONDOR_SYSTRAY_NOTIFIER

class CondorSystrayNotifier
{
public:
	CondorSystrayNotifier();

	void notifyCondorOff();
	
	void notifyCondorIdle(int iCpuId);
	void notifyCondorClaimed(int iCpuId);
	void notifyCondorJobRunning(int iCpuId); 
	void notifyCondorJobSuspended(int iCpuId); 
	void notifyCondorJobPreempting(int iCpuId); 
	
private:
	int iHighestCpuIdSeen;
	void checkForSkippedCpuIds(int iCpuId);
	void writeNumCpusToRegistry();
	void writeToRegistryForCpu(int iCpuId, int iStatus);
	void notifyRegistryChanged();
};	

#endif
