
Executable           = x_java_RenameFile.class
Arguments           = x_java_RenameFile data backdata
Universe             = java
log = job_filexfer_sha256-remote_java.log
Notification         = NEVER
getenv               = false

should_transfer_files = YES
transfer_input_files = x_java_RenameFile.class,data,datasha256
when_to_transfer_output = ON_EXIT_OR_EVICT

output = job_filexfer_sha256-remote_java.out
error = job_filexfer_sha256-remote_java.err

Queue
