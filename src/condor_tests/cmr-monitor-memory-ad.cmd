executable		= cmr-monitor-memory-ad.pl
arguments		= 71
output			= cmr-monitor-memory-ad.$(Cluster).$(Process).out
error			= cmr-monitor-memory-ad.$(Cluster).$(Process).err
log				= cmr-monitor-memory-ad.log
Request_SQUIDs	= 1

# See comment in CustomMachineResource.pm, in parseHistoryFile().
LeaveJobInQueue = true

queue 8
