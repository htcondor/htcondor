#!/bin/bash

IWD=${PWD}
ANNEX_LOGDIR=${IWD}/annex-logs.${ANNEX_JOBID}

# TODO Handle a PILOT_DIR that's local to the node and needs to be set up
#   from files in $PWD
PILOT_DIR=$1

# On exit, cleanup the files we've created
function cleanup() {
    if [ $? != 0 ] ; then
	echo "$(date) $(hostname) Encountered an error, aborting!"
    fi
    if [ "$LOCAL_SETUP" == 1 ] ; then
        echo "$(date) $(hostname) Cleaning up files..."
        cd ${IWD}
        rm -rf ${PILOT_DIR}
    fi
}
trap cleanup EXIT

set -o errexit

# If we can't see the PILOT_DIR, then assume it's on each node's local disk.
# Then, we need to run the job setup script now and cleanup at the end.
if [ ! -d $PILOT_DIR ] ; then
    ${IWD}/annex-job-setup.sh ${PILOT_DIR}
    LOCAL_SETUP=1
fi

cd ${PILOT_DIR}/condor-*

. condor.sh

# On Spark, the job sometimes starts before the shared FS is ready.
if ! condor_config_val FULL_HOSTNAME > /dev/null; then
    echo "$(date) $(hostname) Sleeping five seconds to let the shared FS catch up..."
    sleep 5
fi

# In case I'm part of a multi-node job, make my own node-specific local dir.
FULL_HOSTNAME=`condor_config_val FULL_HOSTNAME`
echo "$(date) $(hostname) Creating host-specific directory for HTCondor..."
cp -a local "${FULL_HOSTNAME}"
mkdir "${ANNEX_LOGDIR}/log.${FULL_HOSTNAME}"

# Detect on-node resource limits and amend the condor configuration
if [ ! -z $SLURM_CPUS_ON_NODE ] ; then
    echo "$(date) $(hostname) Limiting EP to ${SLURM_CPUS_ON_NODE} cpus as requested by SLURM"
    CONDOR_CPUS_LINE="NUM_CPUS = ${SLURM_CPUS_ON_NODE}"
fi
if [ ! -z $SLURM_MEM_PER_NODE ] ; then
    memory_limit="${SLURM_MEM_PER_NODE}"
elif [ ! -z $SLURM_MEM_PER_CPU ] ; then
    memory_limit="$(($SLURM_MEM_PER_CPU * $SLURM_CPUS_ON_NODE))"
elif [ ! -z $SLURM_MEM_PER_GPU ] ; then
    memory_limit="$(($SLURM_MEM_PER_GPU * $SLURM_GPUS_ON_NODE))"
fi
if [ ! -z $memory_limit ] ; then
    echo "$(date) $(hostname) Limiting EP to ${memory_limit}GB memory as requested by SLURM"
    CONDOR_MEMORY_LINE="MEMORY=$memory_limit"
fi
if [ ! -z $SLURM_JOB_END_TIME ] ; then
    # Give condor 5 minutes to shutdown fast before slurm yanks the cord
    shutdown_time=$(($SLURM_JOB_END_TIME - 300))
    echo "$(date) $(hostname) Configuring EP to shutdown at ${shutdown_time} as requested by SLURM"
    CONDOR_RUNTIME_LINE="MASTER.DAEMON_SHUTDOWN_FAST = time() > ${shutdown_time}
DaemonStopTime=${shutdown_time}
STARTD_ATTRS = \$(STARTD_ATTRS),DaemonStopTime"
fi

cat << EOF > ${FULL_HOSTNAME}/config.d/30-annex-node
# These are resource limits detected on an individual node of the slurm job
$CONDOR_CPUS_LINE
$CONDOR_MEMORY_LINE
$CONDOR_RUNTIME_LINE
EOF

echo "$(date) $(hostname) Starting HTCondor..."
condor_master -f
rc=$?
echo "$(date) $(hostname) HTCondor daemons exited with status $rc"

# The EXIT trap will cleanup the $PILOT_DIR if we created it
exit $rc
