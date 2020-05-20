#!/bin/sh

##**************************************************************
##
## Copyright (C) 1990-2017, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************



sshd_cleanup() {
    rm -f ${hostkey}.dsa ${hostkey}.rsa ${hostkey}.dsa.pub ${hostkey}.rsa.pub ${idkey} ${idkey}.pub $_CONDOR_SCRATCH_DIR/tmp/sshd.out $_CONDOR_SCRATCH_DIR/contact
}

trap sshd_cleanup SIGTERM

# note the sshd requires full path
SSHD=`condor_config_val CONDOR_SSHD`
KEYGEN=`condor_config_val CONDOR_SSH_KEYGEN`
CONDOR_CHIRP=`condor_config_val libexec`
CONDOR_CHIRP=$CONDOR_CHIRP/condor_chirp

if [ -z "$SSHD" -o -z "$KEYGEN" ]
then
    echo CONDOR_SSHD and/or CONDOR_SSH_KEYGEN are not configured, exiting
    exit 255
fi


PORT=4444

_CONDOR_REMOTE_SPOOL_DIR=$_CONDOR_REMOTE_SPOOL_DIR
_CONDOR_PROCNO=$1
_CONDOR_NPROCS=$2

# make a tmp dir to store keys, etc, that
# wont get transfered back
mkdir $_CONDOR_SCRATCH_DIR/tmp

# Create the host keys

hostkey=$_CONDOR_SCRATCH_DIR/tmp/hostkey
for keytype in dsa rsa
do
    rm -f ${hostkey}.${keytype} ${hostkey}.${keytype}.pub
    # If FIPS enforcement enabled, do not generate DSA keys
    if [ "`cat /proc/sys/crypto/fips_enabled 2>/dev/null`" == "1" -a $keytype == "dsa" ]; then
	continue
    fi
    $KEYGEN -q -f ${hostkey}.${keytype} -t $keytype -N '' 
    _TEST=$?
    if [ $_TEST -ne 0 ]
    then
        echo ssh keygenerator $KEYGEN returned error $_TEST exiting
        exit 255
    fi
done

idkey=$_CONDOR_SCRATCH_DIR/tmp/$_CONDOR_PROCNO.key

# Create the identity key
$KEYGEN -q -f $idkey -t rsa -N '' 
_TEST=$?
if [ $_TEST -ne 0 ]
then
    echo ssh keygenerator $KEYGEN returned error $_TEST exiting
    exit 255
fi

# Send the identity keys back home
$CONDOR_CHIRP put -perm 0700 $idkey $_CONDOR_REMOTE_SPOOL_DIR/$_CONDOR_PROCNO.key
_TEST=$?
if [ $_TEST -ne 0 ]
then
    echo error $_TEST chirp putting identity keys back
    exit 255
fi

# ssh needs full paths to all of its arguments
# Start up sshd
done=0

while [ $done -eq 0 ]
do

    # Try to launch sshd on this port
    $SSHD -p$PORT -oAuthorizedKeysFile=${idkey}.pub -oHostKey=${hostkey}.dsa -oHostKey=${hostkey}.rsa -De -f/dev/null -oStrictModes=no -oPidFile=/dev/null -oAcceptEnv=_CONDOR < /dev/null > $_CONDOR_SCRATCH_DIR/tmp/sshd.out 2>&1 &

    pid=$!

    # Give sshd some time
    sleep 2
    if grep "Server listening" $_CONDOR_SCRATCH_DIR/tmp/sshd.out > /dev/null 2>&1
    then
        done=1
    else
        # it is probably dead now
        #kill -9 $pid > /dev/null 2>&1
        PORT=`expr $PORT + 1`
    fi
    
done

# Don't need this anymore
rm $_CONDOR_SCRATCH_DIR/tmp/sshd.out

# create contact file
hostname=`hostname`
currentDir=`pwd`
user=`whoami`

thisrun=`$CONDOR_CHIRP get_job_attr EnteredCurrentStatus`

echo "$_CONDOR_PROCNO $hostname $PORT $user $currentDir $thisrun"  |
        $CONDOR_CHIRP put -mode cwa - $_CONDOR_REMOTE_SPOOL_DIR/contact 

_TEST=$?
if [ $_TEST -ne 0 ]
then
    echo error $_TEST chirp putting contact info back to submit machine
    exit 255
fi

# On the head node, grep for the contact file and the keys
if [ $_CONDOR_PROCNO -eq 0 ]
then
    done=0
    count=0

    # Need to poll the contact file until all nodes have reported in
    while [ $done -eq 0 ]
    do
        rm -f contact
        $CONDOR_CHIRP fetch $_CONDOR_REMOTE_SPOOL_DIR/contact $_CONDOR_SCRATCH_DIR/contact
        
        lines=`grep -c $thisrun $_CONDOR_SCRATCH_DIR/contact`
        if [ $lines -eq $_CONDOR_NPROCS ]
        then
            done=1
            node=0

            while [ $node -ne $_CONDOR_NPROCS ]
            do
                $CONDOR_CHIRP fetch $_CONDOR_REMOTE_SPOOL_DIR/$node.key $_CONDOR_SCRATCH_DIR/tmp/$node.key
                
                # Now that we've got it, the submit side doesn't need it anymore
                $CONDOR_CHIRP remove $_CONDOR_REMOTE_SPOOL_DIR/$node.key 
                node=`expr $node + 1`
                
            done
            chmod 0700 $_CONDOR_SCRATCH_DIR/tmp/*.key

            # Erase the contact file from the spool directory, in case
            # this job is held and rescheduled  
            $CONDOR_CHIRP remove $_CONDOR_REMOTE_SPOOL_DIR/contact

        else
            # Wait a second before polling again
            sleep 1
        fi

        # Timeout after polling 1200 times (about 20 minutes)
        count=`expr $count + 1`
        if [ $count -eq 1200 ]
        then
            exit 1
        fi
        
    done
    
fi

# We'll source in this file in the MPI startup scripts,
# so we can wait and sshd_cleanup over there as needed
#wait
#sshd_cleanup
