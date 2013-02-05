#!/bin/sh -x

# condor_chirp in /usr/libexec/condor
export PATH=$PATH:/usr/libexec/condor

HADOOP_FULL_TARBALL=$1
HADOOP_TARBALL=${HADOOP_FULL_TARBALL##*/}

# Note: bin/hadoop uses JAVA_HOME to find the runtime and tools.jar,
#       except tools.jar does not seem necessary therefore /usr works
#       (there's no /usr/lib/tools.jar, but there is /usr/bin/java)
export JAVA_HOME=/usr

# When we get SIGTERM, which Condor will send when
# we are kicked, kill off the namenode
function term {
   ./bin/hadoop-daemon.sh stop namenode
}

if [ -z "$_CONDOR_SCRATCH_DIR" ]; then
    echo "Environment variable _CONDOR_SCRATCH_DIR is empty and required to run script"
    exit 1
fi

# Unpack
tar xzfv $HADOOP_TARBALL
if [ $? -ne 0 ]; then
    echo "Failed to extract $HADOOP_TARBALL"
    exit 1
fi 

# Move into tarball, inefficiently
cd $(tar tzf $HADOOP_TARBALL | head -n1)

# Configure,
#  . fs.default.name,dfs.http.address must be set to port 0 (ephemeral)
#  . dfs.name.dir must be in _CONDOR_SCRATCH_DIR for cleanup
# FIX: Figure out why a hostname is required, instead of 0.0.0.0:0 for
#      fs.default.name
cat > conf/hdfs-site.xml <<EOF
<?xml version="1.0"?>
<?xml-stylesheet type="text/xsl" href="configuration.xsl"?>
<configuration>
  <property>
    <name>fs.default.name</name>
    <value>hdfs://$HOSTNAME:0</value>
  </property>
  <property>
    <name>dfs.name.dir</name>
    <value>$_CONDOR_SCRATCH_DIR/name</value>
  </property>
  <property>
    <name>dfs.http.address</name>
    <value>0.0.0.0:0</value>
  </property>
</configuration>
EOF

# Try to shutdown cleanly
trap term SIGTERM

export HADOOP_CONF_DIR=$PWD/conf
export HADOOP_LOG_DIR=$_CONDOR_SCRATCH_DIR/logs
export HADOOP_PID_DIR=$PWD

###############################################
# Format the name node
./bin/hadoop namenode -format
if [ $? -ne 0 ]; then
    echo "Failed to format name node"
    exit 1
fi 

###############################################
# 
./bin/hadoop-daemon.sh start namenode
if [ $? -ne 0 ]; then
    echo "Failed to start the daemon"
    exit 1
fi 


# Wait for pid file
PID_FILE=$(echo hadoop-*-namenode.pid)
while [ ! -s $PID_FILE ]; do sleep 1; done
PID=$(cat $PID_FILE)

# Wait for the log
LOG_FILE=$(echo $HADOOP_LOG_DIR/hadoop-*-namenode-*.log)
while [ ! -s $LOG_FILE ]; do sleep 1; done

# It would be nice if there were a way to get these without grepping logs
while [ ! $(grep "IPC Server listener on" $LOG_FILE) ]; do sleep 1; done
IPC_PORT=$(grep "IPC Server listener on" $LOG_FILE | sed 's/.* on \(.*\):.*/\1/')
while [ ! $(grep "Jetty bound to port" $LOG_FILE) ]; do sleep 1; done
HTTP_PORT=$(grep "Jetty bound to port" $LOG_FILE | sed 's/.* to port \(.*\)$/\1/')

my_hostname=$(hostname -f)

curl --connect-timeout 3 --silent http://169.254.169.254/2012-01-12
if [ $? -eq 0 ]; then
  my_hostname=$(curl http://169.254.169.254/2012-01-12/meta-data/public-hostname)
  echo "Found EC2 public hostname: $my_hostname"
fi

# Record the port number where everyone can see it
condor_chirp set_job_attr IPCAddress \"hdfs://$my_hostname:$IPC_PORT\"
if [ $? -ne 0 ]; then
    echo "Failed to chirp update, exiting"
    term
    exit 1
fi 

condor_chirp set_job_attr HTTPAddress \"http://$my_hostname:$HTTP_PORT\"
if [ $? -ne 0 ]; then
    echo "Failed to chirp update, exiting"
    term
    exit 1
fi 

# While namenode is running, collect and report back stats
while kill -0 $PID; do
   # Collect stats and chirp them back into the job ad
   # Nothing to do.
   sleep 30
done
