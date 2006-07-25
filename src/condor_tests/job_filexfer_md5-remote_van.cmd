
Executable           = ./job_filexfer_md5-remote.pl
Arguments           = backdata
Universe             = vanilla
log = job_filexfer_md5-remote_van.log
Notification         = NEVER
getenv               = false

TransferFiles        = ALWAYS
Transfer_Input_Files = data,datamd5
when_to_transfer_output = ON_EXIT_OR_EVICT

output = job_filexfer_md5-remote_van.out
error = job_filexfer_md5-remote_van.err



Queue
