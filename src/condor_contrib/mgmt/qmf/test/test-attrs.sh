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
# cluster.proc from submit
echo "step #1 - SubmitJob"
job_id=$(./submit.py -q amqp://cumin/cumin@localhost:5672)
if [ $? -ne 0 ] ; then
echo "acl submit test - step #1 failed"
exit 1
fi

echo "step #2 - set good attribute name"
./setattr.py $job_id "TEST_" "Value" amqp://cumin/cumin@localhost:5672 > ./test-attr.2.out 2>&1
if [ $? -ne 0 ] ; then
echo "attr test - step #2 failed"
exit 1
fi

echo "step #3 - set really bad attribute name"
./setattr.py $job_id "##@hds" "Value" amqp://cumin/cumin@localhost:5672 > ./test-attr.3.out 2>&1
if [ $? -eq 0 ] ; then
echo "attr test - step #3 failed"
exit 1
fi

echo "step #4 - set slightly bad attribute name"
./setattr.py $job_id "Test_." "Value" amqp://cumin/cumin@localhost:5672 > ./test-attr.4.out 2>&1
if [ $? -eq 0 ] ; then
echo "attr test - step #4 failed"
exit 1
fi

echo "step #5 - set reserved attribute name"
./setattr.py $job_id "iSNt" "Value" amqp://cumin/cumin@localhost:5672 > ./test-attr.5.out 2>&1
if [ $? -eq 0 ] ; then
echo "attr test - step #5 failed"
exit 1
fi

echo "all tests passed"
