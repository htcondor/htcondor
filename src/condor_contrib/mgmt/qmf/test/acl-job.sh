#!/bin/sh
#
# Copyright 2009-2011 Red Hat, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# test QMF/QPID ACL and SASL access to scheduler plug-in
# for job control methods

# assumes /etc/sasl2/qpidd.conf is in place with uncommented content like...
# pwcheck_method: auxprop
# auxprop_plugin: sasldb
# sasldb_path: /var/lib/qpidd/qpidd.sasldb
# sql_select: dummy select

# add cumin and guest users (for plugins) using...
# $ sudo saslpasswd2 -f /var/lib/qpidd/qpidd.sasldb -u QPID cumin

# run broker something like this...
# $ sudo qpidd --load-module /usr/lib/qpid/daemon/acl.so --acl-file <your grid repo>/src/management/qpidd.acl --auth=yes

# mech is PLAIN for this test

condor_config_val QUEUE_ALL_USERS_TRUSTED > /dev/null 2>&1
if [ $? -eq 0 ] ; then
echo "acl submit test - warning QUEUE_ALL_USERS_TRUSTED should be unset or false for test"
fi

# good user for submit
echo "step #1 - good user cumin/cumin SubmitJob"
job_id=$(./submit.py -q amqp://cumin/cumin@localhost:5672)
if [ $? -ne 0 ] ; then
echo "acl submit test - step #1 failed"
exit 1
fi

echo "$job_id"

# bad user tries to mess with it
echo "step #2 - bad user evil/evil HoldJob"
./jobcontrol.py HoldJob $job_id amqp://evil/evil@localhost:5672 > ./acl-job.2.out 2>&1
if [ $? -eq 0 ] ; then
echo "acl submit test - step #2 failed"
exit 1
fi

echo "step #3 - bad user evil/evil ReleaseJob"
./jobcontrol.py ReleaseJob $job_id amqp://evil/evil@localhost:5672 > ./acl-job.3.out 2>&1
if [ $? -eq 0 ] ; then
echo "acl submit test - step #3 failed"
exit 1
fi

echo "step #4 - bad user evil/evil RemoveJob"
./jobcontrol.py RemoveJob $job_id amqp://evil/evil@localhost:5672 > ./acl-job.4.out 2>&1
if [ $? -eq 0 ] ; then
echo "acl submit test - step #4 failed"
exit 1
fi

echo "step #5 - bad user evil/evil SetJobAttribute"
./setattr.py $job_id TEST Value amqp://evil/evil@localhost:5672 > ./acl-job.5.out 2>&1
if [ $? -eq 0 ] ; then
echo "acl submit test - step #5 failed"
exit 1
fi

echo "step #6 - guest user (ACL forbidden) SetJobAttribute"
./setattr.py $job_id TEST Value amqp://guest/guest@localhost:5672 > ./acl-job.6.out 2>&1
if [ $? -eq 0 ] ; then
echo "acl submit test - step #6 failed"
exit 1
fi

echo "all tests passed"
