#/bin/bash

# This file is sourced by blahp before submitting the job to slurm
# Anything printed to stdout is included in the submit file.
# For example, to set a default walltime of 24 hours in PBS, you
# could uncomment this line:

# echo "#SBATCH --time=24:00:00"

# blahp allows arbitrary attributes to be passed to this script on a per-job
# basis.  If you add the following to your HTCondor-G submit file:

#+remote_cerequirements = NumJobs == 100 && foo = 5

# Then an environment variable, NumJobs, will be exported prior to calling this
# script and set to a value of 100.  The variable foo will be set to 5.

# You could allow users to set the walltime for the job with the following
# customization (slurm syntax given; adjust for the appropriate batch system):

# Uncomment the else block to default to 24 hours of runtime; otherwise, the queue
# default is used.
if [ -n "$Walltime" ]; then
    let Walltime=Walltime/60
    echo "#SBATCH --time=$Walltime"
# else
#     echo "#SBATCH --time=24:00:00"
fi
