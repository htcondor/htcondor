#Turn on transfer-files mode and pass some input to /bin/sh, just to
#make sure remote_initialdir does not break that.

universe = vanilla
executable = /bin/sh
transfer_executable = false
remote_initialdir = /tmp
log = job_core_remote-initialdir_van.log
output = job_core_remote-initialdir_van.out
error = job_core_remote-initialdir_van.err
input = job_core_remote-initialdir_van.in
Notification = NEVER
should_transfer_files = true
when_to_transfer_output = on_exit
queue
