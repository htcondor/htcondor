#!/bin/sh
CLASSPATH=.

if [ -z "$GLOBUS_LOCATION" ]
then
    echo "ERROR: GLOBUS_LOCATION undefined" >> /dev/stderr
    exit 1
fi


pushd $GLOBUS_LOCATION/lib

jar_files=`ls -1 *.jar`

echo "Jar Files: "
echo $jar_files
echo


for jar_file in $jar_files
do
  CLASSPATH=$GLOBUS_LOCATION/lib/$jar_file:$CLASSPATH
done
popd
export CLASSPATH

export JAVA_HOME=/s/jdk1.4.2_03
export PATH=$JAVA_HOME/bin:$PATH

set -x

javac -deprecation \
condor/gahp/*.java \
condor/gahp/gt4/*.java


set +x

if [ $? -eq 0 ]
then
    jar cf gt4-gahp.jar condor/gahp/*.class condor/gahp/gt4/*.class condor/gahp/gt4/commands.properties && echo "Created gt4-gahp.jar"
fi



