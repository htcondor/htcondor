
Executable           = ./job_filexfer_md5-remote.pl
Arguments           = backdata
Universe             = vanilla
log = job_filexfer_md5-remote_van.log
Notification         = NEVER
getenv               = false

should_transfer_files = YES
transfer_input_files = data,datamd5
when_to_transfer_output = ON_EXIT_OR_EVICT

output = job_filexfer_md5-remote_van.out
error = job_filexfer_md5-remote_van.err

# On at least one of our cross-test machines, when the free disk space
# is spread across 15 slots, the free space per slot is insufficient
# to run this job. Since few, if any jobs use this much disk, match
# on the total free space.
requirements = TARGET.TotalDisk > DiskUsage

Queue
