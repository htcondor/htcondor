#!/bin/sh

# condor_container_launcher.
#
# When the condor_starter runs a singularity or apptainer container, we would
# like to differentiate between failures in the runtime, especially during launch
# versus failures of the job itself.  Exit codes from the job are returned through
# the runtime, which means the runtime exit codes and the job exit codes are in the 
# same space.
#
# So, the starter will use this small script to launch jobs.  Singularity/apptainer
# will launch this script, which will touch a file in the scratch directory, then
# exec the actual job.
#
# When the container exits, the starter can stat this file.  If it exists, it means the
# container runtime successfull got the job off the ground.  We assume that 
# runtime errors after that are rare, and we don't bother to symmetrically touch
# a file after the actual job finishes.

# After touching the file, we exec(2) the job, so any job signals, exit codes, etc.
# come straight through, and this shell is out of the picture.

:> $_CONDOR_SCRATCH_DIR/.condor_container_launched

exec "$@"
