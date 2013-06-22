#!/bin/sh -x

# condor_chirp in /usr/libexec/condor
export PATH=$PATH:/usr/libexec/condor

HADOOP_FULL_TARBALL=$1
HADOOP_TARBALL=${HADOOP_FULL_TARBALL##*/}
JOBTRACKER_ENDPOINT=$2

# Note: bin/hadoop uses JAVA_HOME to find the runtime and tools.jar,
#       except tools.jar does not seem necessary therefore /usr works
#       (there's no /usr/lib/tools.jar, but there is /usr/bin/java)
export JAVA_HOME=/usr

# When we get SIGTERM, which Condor will send when
# we are kicked, kill off the datanode and gather logs
function term {
   ./bin/hadoop-daemon.sh stop tasktracker
# Useful if we can transfer data back
#   tar czf logs.tgz logs
#   cp logs.tgz $_CONDOR_SCRATCH_DIR
}

# Unpack
tar xzfv $HADOOP_TARBALL

# Move into tarball, inefficiently
cd $(tar tzf $HADOOP_TARBALL | head -n1)

# Configure,
#  . http.address must be set to port 0 (ephemeral)
cat > conf/mapred-site.xml <<EOF
<?xml version="1.0"?>
<?xml-stylesheet type="text/xsl" href="configuration.xsl"?>
<configuration>
  <property>
    <name>mapred.job.tracker</name>
    <value>$JOBTRACKER_ENDPOINT</value>
  </property>
  <property>
    <name>mapred.task.tracker.http.address</name>
    <value>0.0.0.0:0</value>
  </property>
</configuration>
EOF

# Try to shutdown cleanly
trap term SIGTERM

export HADOOP_CONF_DIR=$PWD/conf
export HADOOP_PID_DIR=$PWD
export HADOOP_LOG_DIR=$_CONDOR_SCRATCH_DIR/logs
./bin/hadoop-daemon.sh start tasktracker

# Wait for pid file
PID_FILE=$(echo hadoop-*-tasktracker.pid)
while [ ! -s $PID_FILE ]; do sleep 1; done
PID=$(cat $PID_FILE)

# Report back some static data about the tasktracker
# e.g. condor_chirp set_job_attr SomeAttr SomeData
# None at the moment.

# While the tasktracker is running, collect and report back stats
while kill -0 $PID; do
   # Collect stats and chirp them back into the job ad
   # None at the moment.
   sleep 30
done
