#!/bin/sh

sshd_cleanup() {
	/bin/rm -f $hostkey ${hostkey}.pub	${idkey} ${idkey}.pub sshd.out contact
}

trap sshd_cleanup 15

# note the sshd requires full path
SSHD=`condor_config_val CONDOR_SSHD`
KEYGEN=`condor_config_val CONDOR_SSH_KEYGEN`
CONDOR_CHIRP=`condor_config_val libexec`
CONDOR_CHIRP=$CONDOR_CHIRP/condor_chirp

PORT=4444

_CONDOR_REMOTE_SPOOL_DIR=$_CONDOR_REMOTE_SPOOL_DIR
_CONDOR_PROCNO=$1
_CONDOR_NPROCS=$2

# make a tmp dir to store keys, etc, that
# wont get transfered back
mkdir $_CONDOR_SCRATCH_DIR/tmp

# Create the host key. 

hostkey=$_CONDOR_SCRATCH_DIR/tmp/hostkey
/bin/rm -f $hostkey $hostkey.pub
$KEYGEN -q -f $hostkey -t rsa -N '' 

if [ $? -ne 0 ]
then
	echo ssh keygenerator $KEYGEN returned error $? exiting
	exit -1
fi

idkey=$_CONDOR_SCRATCH_DIR/tmp/$_CONDOR_PROCNO.key

# Create the identity key
$KEYGEN -q -f $idkey -t rsa -N '' 
if [ $? -ne 0 ]
then
	echo ssh keygenerator $KEYGEN returned error $? exiting
	exit -1
fi

# Send the identity keys back home
$CONDOR_CHIRP put -perm 0700 $idkey $_CONDOR_REMOTE_SPOOL_DIR/$_CONDOR_PROCNO.key
if [ $? -ne 0 ]
then
	echo error $? chirp putting identity keys back
	exit -1
fi

# ssh needs full paths to all of its arguments
# Start up sshd
done=0

while [ $done -eq 0 ]
do

# Try to launch sshd on this port
$SSHD -p$PORT -oAuthorizedKeysFile=${idkey}.pub -h$hostkey -De -f/dev/null -oStrictModes=no -oPidFile=/dev/null -oAcceptEnv=_CONDOR < /dev/null > sshd.out 2>&1 &

pid=$!

# Give sshd some time
sleep 2
if grep "^Server listening on 0.0.0.0 port" sshd.out > /dev/null 2>&1
then
	done=1
else
		# it is probably dead now
		#kill -9 $pid > /dev/null 2>&1
		PORT=`expr $PORT + 1`
fi

done

# Don't need this anymore
/bin/rm sshd.out

# create contact file
hostname=`hostname`
currentDir=`pwd`
user=`whoami`

echo "$_CONDOR_PROCNO $hostname $PORT $user $currentDir"  |
	$CONDOR_CHIRP put -mode cwa - $_CONDOR_REMOTE_SPOOL_DIR/contact 

if [ $? -ne 0 ]
then
	echo error $? chirp putting contact info back to submit machine
	exit -1
fi

# On the head node, grep for the contact file
# and the keys
if [ $_CONDOR_PROCNO -eq 0 ]
then
	done=0

	# Need to poll the contact file until all nodes have
	# reported in
	while [ $done -eq 0 ]
	do
			/bin/rm -f contact
			$CONDOR_CHIRP fetch $_CONDOR_REMOTE_SPOOL_DIR/contact $_CONDOR_SCRATCH_DIR/contact
			lines=`wc -l $_CONDOR_SCRATCH_DIR/contact | awk '{print $1}'`
			if [ $lines -eq $_CONDOR_NPROCS ]
			then
				done=1
				node=0
				while [ $node -ne $_CONDOR_NPROCS ]
				do
						$CONDOR_CHIRP fetch $_CONDOR_REMOTE_SPOOL_DIR/$node.key $_CONDOR_SCRATCH_DIR/tmp/$node.key
						node=`expr $node + 1`
				done
				chmod 0700 $_CONDOR_SCRATCH_DIR/tmp/*.key
			else
				sleep 1
			fi
	done
fi

# We'll source in this file in the MPI startup scripts,
# so we can wait and sshd_cleanup over there as needed
#wait
#sshd_cleanup
