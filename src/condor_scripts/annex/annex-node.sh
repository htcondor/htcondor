#!/bin/bash

IWD=${PWD}
ANNEX_LOGDIR=${IWD}/annex-logs.${SLURM_JOBID}

# TODO Handle a PILOT_DIR that's local to the node and needs to be set up
#   from files in $PWD
PILOT_DIR=$1

# If we can't see the PILOT_DIR, then assume it's on each node's local disk.
# Then, we need to run the job setup script now and cleanup at the end.
if [ ! -d $PILOT_DIR ] ; then
    LOCAL_SETUP=1
    ${IWD}/annex-job-setup.sh ${PILOT_DIR}
fi

cd ${PILOT_DIR}/condor-*

. condor.sh

# On Spark, the job sometimes starts before the shared FS is ready.
if ! condor_config_val FULL_HOSTNAME > /dev/null; then
    echo "Sleeping five seconds on $(hostname -f) to let the shared FS catch up..."
    sleep 5
fi

# In case I'm part of a multi-node job, make my own node-specific local dir.
FULL_HOSTNAME=`condor_config_val FULL_HOSTNAME`
echo "Creating host-specific directory for HTCondor on ${FULL_HOSTNAME}..."
cp -a local "${FULL_HOSTNAME}"
mkdir "${ANNEX_LOGDIR}/log.${FULL_HOSTNAME}"

# Detect on-node resource limits and amend the condor configuration
if [ ! -z $SLURM_CPUS_ON_NODE ] ; then
    CONDOR_CPUS_LINE="NUM_CPUS = ${SLURM_CPUS_ON_NODE}"
fi
if [ ! -z $SLURM_MEM_PER_NODE ] ; then
    CONDOR_MEMORY_LINE="MEMORY=${SLURM_MEM_PER_NODE}"
elif [ ! -z $SLURM_MEM_PER_CPU ] ; then
    CONDOR_MEMORY_LINE="MEMORY=$(($SLURM_MEM_PER_CPU * $SLURM_CPUS_ON_NODE))"
elif [ ! -z $SLURM_MEM_PER_GPU ] ; then
    CONDOR_MEMORY_LINE="MEMORY=$(($SLURM_MEM_PER_GPU * $SLURM_GPUS_ON_NODE))"
fi
if [ ! -z $SLURM_JOB_END_TIME ] ; then
    # Give condor 5 minutes to shutdown fast before slurm yanks the cord
    CONDOR_RUNTIME_LINE="MASTER.DAEMON_SHUTDOWN_FAST = time() > ${SLURM_JOB_END_TIME} - 300"
fi

echo "
# These are resource limits detected on an individual node of the slurm job
$CONDOR_CPUS_LINE
$CONDOR_MEMORY_LINE
$CONDOR_RUNTIME_LINE
" >${FULL_HOSTNAME}/config.d/30-annex-node

echo "Starting HTCondor on ${FULL_HOSTNAME}..."
condor_master -f
rc=$?

if [ "$LOCAL_SETUP" == 1 ] ; then
    cd ${IWD}
    rm -rf ${PILOT_DIR}
fi

exit $rc
