#!/bin/sh

prog=`basename $0`

usage() {
cat <<EOF >&2
$prog: create stork release tarball

usage: $prog  <workspace_directory>
	where <workspace_directory> is an _absolute path_, and has a ./src subdir

EOF
}

if [ -z "$1" ]; then
	usage
	exit 1
fi

if [ ! -d $1/src ]; then
	echo $1 is not a build directory >&2
	usage
	exit 1
fi

set -v
set -x
set -e

umask 002

start_dir=`pwd`
target_dir=/tmp/stork1
cleanup() {
	rm -rf $target_dir
}
trap cleanup 0 1 2 3 15

CP="cp -p"
LN="ln"

cleanup
mkdir $target_dir
mkdir $target_dir/bin
mkdir $target_dir/sbin
mkdir $target_dir/sbin/DaP_Catalog
mkdir $target_dir/etc
mkdir $target_dir/etc/examples
mkdir $target_dir/etc/examples/stork_submit_samples/
mkdir $target_dir/doc

$CP $1/src/stork/documentation.txt $target_dir/doc
$CP $1/src/stork/README $target_dir/doc
$CP $1/src/stork/README $target_dir
$CP $1/src/stork/Release-Notes $target_dir/doc

$CP /p/condor/workspaces/kosart/stork_samples/* $target_dir/etc/examples/stork_submit_samples/

$CP $1/src/condor_examples/condor_config.generic $target_dir/etc/examples/condor_config.generic

# Condor master
$CP $1/src/condor_master.V6/condor_master $target_dir/sbin

# Other condor tools
$CP $1/src/condor_tools/condor_config_val $target_dir/bin
$CP $1/src/condor_tools/condor $target_dir/sbin/condor_off
$CP $1/src/condor_tools/condor_version $target_dir/bin/condor_version
$LN $target_dir/bin/condor_version $target_dir/bin/stork_version

# Credd executables
$CP $1/src/condor_credd/condor_credd $target_dir/sbin
$CP $1/src/condor_credd/condor_store_cred $target_dir/bin
$CP $1/src/condor_credd/condor_rm_cred $target_dir/bin
$CP $1/src/condor_credd/condor_list_cred $target_dir/bin

# Stork executables
$CP $1/src/stork/stork_server $target_dir/sbin
#$CP $1/src/stork/stork_client_agent $target_dir/sbin
$CP $1/src/stork/stork_submit $target_dir/bin
$CP $1/src/stork/stork_rm $target_dir/bin
$CP $1/src/stork/stork_status $target_dir/bin
$CP $1/src/stork/stork_q $target_dir/bin

# Stork DAP catalog
$CP $1/src/stork/DaP.* $target_dir/sbin/DaP_Catalog
pushd $target_dir/sbin/DaP_Catalog >> /dev/null

rm -f DaP.transfer.*gsiftp*
rm -f DaP.transfer.file-ftp
rm -f DaP.transfer.ftp-file
# Ugly, but temporary.  Transfer programs we don't want to release yet.  This s
# script would run quicker if they are not copied over in the first place.
rm -f DaP.reserve.*
rm -f DaP.release.*
rm -f DaP.transfer.*ibp*
rm -f DaP.transfer.*nest*
rm -f DaP.transfer.*diskrouter*
rm -f DaP.transfer.*fnalsrm*
rm -f DaP.transfer.*lbnlsrm*

#GsiFTP
ln -s DaP.transfer.globus-url-copy  DaP.transfer.file-gsiftp
ln -s DaP.transfer.globus-url-copy  DaP.transfer.file-ftp
ln -s DaP.transfer.globus-url-copy  DaP.transfer.file-http
ln -s DaP.transfer.globus-url-copy  DaP.transfer.gsiftp-gsiftp
ln -s DaP.transfer.globus-url-copy  DaP.transfer.gsiftp-file
ln -s DaP.transfer.globus-url-copy  DaP.transfer.ftp-file
ln -s DaP.transfer.globus-url-copy  DaP.transfer.http-file

#DiskRouter
#ln -s DaP.transfer.diskrouter  DaP.transfer.diskrouter-diskrouter

#SRB
ln -s DaP.transfer.srb  DaP.transfer.file-srb
ln -s DaP.transfer.srb  DaP.transfer.srb-file

#FNALSRM
#ln -s DaP.transfer.fnalsrm  DaP.transfer.file-fnalsrm
#ln -s DaP.transfer.fnalsrm  DaP.transfer.fnalsrm-file

#LBNLSRM
#ln -s DaP.transfer.lbnlsrm  DaP.transfer.file-lbnlsrm
#ln -s DaP.transfer.lbnlsrm  DaP.transfer.lbnlsrm-file

# Castor SRM
ln -s DaP.transfer.castor_srm DaP.transfer.file-csrm
ln -s DaP.transfer.castor_srm DaP.transfer.csrm-csrm
ln -s DaP.transfer.castor_srm DaP.transfer.csrm-file

# dcache SRM
ln -s DaP.transfer.dcache_srm DaP.transfer.file-srm
ln -s DaP.transfer.dcache_srm DaP.transfer.srm-file
ln -s DaP.transfer.dcache_srm DaP.transfer.srm-srm

#NeST
#ln -s DaP.transfer.nest  DaP.transfer.file-nest
#ln -s DaP.transfer.nest  DaP.transfer.nest-nest
#ln -s DaP.transfer.nest  DaP.transfer.nest-file
##ln -s DaP.reserve.nest  DaP.reserve.nest
##ln -s DaP.release.nest  DaP.release.nest

#FILE
#ln -s DaP.transfer.file-file  DaP.transfer.file-file

#IBP
#ln -s DaP.transfer.ibp  DaP.transfer.file-ibp
#ln -s DaP.transfer.ibp  DaP.transfer.ibp-ibp
#ln -s DaP.transfer.ibp  DaP.transfer.ibp-file
##ln -s DaP.reserve.ibp  DaP.reserve.ibp

#UniTree
ln -s DaP.transfer.unitree  DaP.transfer.file-unitree
ln -s DaP.transfer.unitree  DaP.transfer.unitree-file

popd >> /dev/null

# Create stork (classad) config file
cat >> $target_dir/etc/examples/stork.config <<\EOF

/*
 * Stork sample configuration file
 * Please edit the values below based on your path
 * */
[ //do not delete this line

    max_num_jobs = 5; 
    max_retry = 5;
    maxdelay_inminutes = 5;

    // dap_catalog should be <Condor>/sbin/DaP_Catalog";
    dap_catalog = "@DAP_CATALOG@";

    // Use this to set LD_LIBRARY_PATH for executing all porotocol-specific requests
    // (e.g. globus-url-copy needs libraries in $GLOBUS_LOCATION/lib), etc
    ld_library_path = "/opt/vdt/globus/lib";

    // globus_bin_dir should be $GLOBUS_LOCATION/bin
    globus_bin_dir = "/opt/vdt/globus/bin";

    //srb_util_dir = "/home/condor/srb-2.0.0/utilities/bin";
    srb_util_dir = "/unsup/srb-2.0.0/utilities/bin";
    dcache_srm_bin_dir = "/opt/d-cache-client/srm/bin";
    msscmd_bin_dir = "/p/condor/workspaces/kosart/stork_experiments/sdss-srb-unitree-pipeline";
] //do not delete this line

EOF


#Create Stork (Condor) config file
cat >> $target_dir/etc/examples/condor_config.stork <<\EOF

STORK = $(SBIN)/stork_server
STORK_ADDRESS_FILE = $(LOG)/.stork_address
STORK_QUEUE_FILE = $(LOG)/Stork.queue
STORK_USER_LOG = $(LOG)/Stork
STORK_PORT = 34048

# Pass args to Stork here
STORK_ARGS = -p $(STORK_PORT) -f -Config $(RELEASE_DIR)/etc/stork.config \
    -Serverlog $(STORK_USER_LOG)

DAEMON_LIST = STORK, $(DAEMON_LIST)


# This is the myproxy-get-delegation executable
# Fill in the actual value
MYPROXY_GET_DELEGATION = $(SBIN)/myproxy-get-delegation-wrapper.sh

# This is stork DaemonCore (Condor) log
STORK_LOG = $(LOG)/StorkLog
STORK_DEBUG = D_FULLDEBUG
#STORK_DEBUG = D_FULLDEBUG D_SECURITY D_COMMAND D_IO
#MAX_STORK_LOG = 4000000

EOF



#Create CredD config file
cat >> $target_dir/etc/examples/condor_config.credd <<\EOF

CREDD = $(SBIN)/condor_credd
DAEMON_LIST = CREDD, $(DAEMON_LIST)
CREDD_ARGS = -f
CREDD_ADDRESS_FILE = $(LOG)/.credd_address

# Pass args to CredD here
# CREDD_ARGS = -p $(CREDD_PORT)

CREDD_LOG = $(LOG)/CredLog
CREDD_DEBUG = D_FULLDEBUG
#CREDD_DEBUG = D_FULLDEBUG D_SECURITY D_COMMAND D_IO
#MAX_CREDD_LOG = 4000000

CRED_STORE_DIR = $(LOCAL_DIR)/cred_dir

# Verbose tool debugging
TOOL_DEBUG = D_FULLDEBUG D_SECURITY D_COMMAND D_IO

# General security settings
SEC_DEFAULT_AUTHENTICATION = REQUIRED
SEC_DEFAULT_AUTHENTICATION_METHODS = FS
SEC_CLIENT_AUTHENTICATION_METHODS = FS

EOF


# Create myproxy-get-delegation-wrapper
cat >> $target_dir/sbin/myproxy-get-delegation-wrapper.sh <<\EOF
#!/bin/sh

# ----- !!!! EDIT GLOBUS_LOCATION !!!! ------------
if [ ! -d "$GLOBUS_LOCATION" ]; then
    GLOBUS_LOCATION=/opt/globus
fi
export GLOBUS_LOCATION

LD_LIBRARY_PATH=$GLOBUS_LOCATION/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

exec $GLOBUS_LOCATION/bin/myproxy-get-delegation $@
 
EOF
chmod a+rx $target_dir/sbin/myproxy-get-delegation-wrapper.sh

cd $target_dir
chmod -R  a+rX *

#tar cvf --owner=root release.tar bin/ sbin/ etc/ doc/
tar -cv --owner=root --group=root -f release.tar bin/ sbin/ etc/ doc/
$CP $1/src/stork/condor_configure $target_dir/install-me

#tar czvf --owner=root stork.tar.gz release.tar install-me README
tarball="$start_dir/stork-`date +%F`-linux-x86-redhat72-dynamic.tar.gz"
tar -czv --owner=root --group=root -f $tarball release.tar install-me README

set +x
set +v

echo
echo
echo "Result in: $tarball"

set +e
