
Executable           = x_java_RenameFile.class
Arguments           = x_java_RenameFile data backdata
Universe             = java
log = job_filexfer_md5-remote_java.log
Notification         = NEVER
getenv               = false

TransferFiles        = ALWAYS
Transfer_Input_Files = x_java_RenameFile.class,data,datamd5
when_to_transfer_output = ON_EXIT_OR_EVICT

output = job_filexfer_md5-remote_java.out
error = job_filexfer_md5-remote_java.err



Queue
