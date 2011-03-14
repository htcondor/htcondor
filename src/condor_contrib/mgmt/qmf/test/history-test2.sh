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
# 1) Run test_history reader while this script it running
# 2) Run test_history reader after this script completes
# (1) == (2)
# 3) Run test_history reader while this script is running, pause test_history
# reader
#   a) short enough to avoid a rotation (output should be == (1))
#   b) long enough to hit a rotation (sorted output == sorted (1,2,3a))

DIR=history-test2

mkdir $DIR > /dev/null 2>&1

i=0; j=0;
while [ $i -lt 7 ]; do
   echo "ClusterId = $i" >> $DIR/history
   echo "ProcId = $j" >> $DIR/history
   echo "QDate = $((i*1000 + j))" >> $DIR/history
   echo "Cmd = \"cmd.$i\"" >> $DIR/history
   echo "Args = \"$i $j\"" >> $DIR/history
   echo "GlobalJobId = \"scheduler#$i.$j#$((i*1000 + j))\"" >> $DIR/history
   echo "JobStatus = 4" >> $DIR/history
   echo "EnteredCurrentStatus = $((i*1000 + j))" >> $DIR/history
   echo "Sum = $((i + j))" >> $DIR/history
   echo "***" >> $DIR/history

   sleep 1

   j=$((j+1))

   if [ $j -eq 10 ]; then
      i=$((i+1));
      j=0;

      if [ $((i % 2)) -eq 0 ]; then
         mv $DIR/history $DIR/history.$i;
      fi
   fi
done
