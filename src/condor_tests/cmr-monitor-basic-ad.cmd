executable		= cmr-monitor-basic-ad.pl
arguments		= 53
output			= cmr-monitor-basic-ad.$(Cluster).$(Process).out
error			= cmr-monitor-basic-ad.$(Cluster).$(Process).err
log				= cmr-monitor-basic-ad.log
Request_SQUIDs	= 1

# See comment in CustomMachineResource.pm, in parseHistoryFile().
LeaveJobInQueue = true

queue 8
