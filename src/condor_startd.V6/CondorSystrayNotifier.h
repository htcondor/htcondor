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