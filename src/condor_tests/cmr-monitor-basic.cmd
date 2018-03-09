executable		= x_sleep.pl
arguments		= 53
output			= cmr-monitor-basic.$(Cluster).$(Process).out
error			= cmr-monitor-basic.$(Cluster).$(Process).err
log				= cmr-monitor-basic.log
Request_SQUIDs	= 1

# See comment in CustomMachineResource.pm, in parseHistoryFile().
LeaveJobInQueue = true

queue 4
