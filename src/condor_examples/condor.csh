#Set Condor environment variable by reading 
#configuration parameters from /etc/sysconfig/condor 

set CONDOR_SYS = "/etc/sysconfig/condor"

if( -e  $CONDOR_SYS ) then
	#Parse Condor sysconfig and set variable 
	eval `grep -v '^[:blank:]*#' $CONDOR_SYS | sed 's|\([^=]*\)=\([^=]*\)|set \1 = \2|g' | sed 's|$|;|'`	
endif 

if ( ! $?CONDOR_CONFIG ) then
	echo "CONDOR_CONFIG is not set in $CONDOR_SYS"
endif

if ( ! $?CONDOR_CONFIG_VAL ) then
	echo "CONDOR_CONFIG_VAL is not set in $CONDOR_SYS"
endif

if ( $?CONDOR_CONFIG && $?CONDOR_CONFIG_VAL ) then
	setenv CONDOR_CONFIG $CONDOR_CONFIG
	set BIN = `$CONDOR_CONFIG_VAL BIN`
	set SBIN = `$CONDOR_CONFIG_VAL SBIN`
	setenv PATH ${PATH}:${BIN}:${SBIN}
endif