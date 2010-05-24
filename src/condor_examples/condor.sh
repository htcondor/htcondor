#Set Condor environment variable by reading 
#configuration parameters from /etc/sysconfig/condor 

CONDOR_SYS="/etc/sysconfig/condor"

if [ -f $CONDOR_SYS ]; then
	source $CONDOR_SYS
fi

if [ -z "$CONDOR_CONFIG" ] ; then
	echo "CONDOR_CONFIG is not set in $CONDOR_SYS"
fi

if [ -z "$CONDOR_CONFIG_VAL" ] ; then
	echo "CONDOR_CONFIG_VAL is not set in $CONDOR_SYS"
fi

if [ -n "$CONDOR_CONFIG_VAL" ] && [ -n "$CONDOR_CONFIG" ] ; then	
	export CONDOR_CONFIG=$CONDOR_CONFIG
	BIN=`$CONDOR_CONFIG_VAL BIN`
	SBIN=`$CONDOR_CONFIG_VAL SBIN`
	export PATH=$PATH:$BIN:$SBIN
fi