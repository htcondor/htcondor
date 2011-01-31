#!/bin/bash
# CreateKeyPair.
curl -i 'localhost:21737/?Action=CreateKeyPair&AWSAccessKeyId=1&KeyName=kn-1'; echo

# DescribeKeyPairs.
curl -i 'localhost:21737/?Action=DescribeKeyPairs&AWSAccessKeyId=1'; echo

# Minimal RunInstances command.
curl -i 'localhost:21737/?Action=RunInstances&AWSAccessKeyId=1&MaxCount=1&MinCount=1&ImageId=1'; echo

# Individual options.
curl -i 'localhost:21737/?Action=RunInstances&AWSAccessKeyId=1&MaxCount=1&MinCount=1&ImageId=1&KeyName=kn-1'; echo
curl -i 'localhost:21737/?Action=RunInstances&AWSAccessKeyId=1&MaxCount=1&MinCount=1&ImageId=1&InstanceType=mx.example'; echo
curl -i 'localhost:21737/?Action=RunInstances&AWSAccessKeyId=1&MaxCount=1&MinCount=1&ImageId=1&SecurityGroup.1=sg-name-1'; echo
curl -i 'localhost:21737/?Action=RunInstances&AWSAccessKeyId=1&MaxCount=1&MinCount=1&ImageId=1&SecurityGroup.1=sg-name-2&SecurityGroup.2=sg-name-3'; echo
curl -i 'localhost:21737/?Action=RunInstances&AWSAccessKeyId=1&MaxCount=1&MinCount=1&ImageId=1&UserData=somethingbase64encoded'; echo

# All options.
curl -i 'localhost:21737/?Action=RunInstances&AWSAccessKeyId=1&MaxCount=1&MinCount=1&ImageId=1'; echo

# DescribeInstances.
curl -i 'localhost:21737/?Action=DescribeInstances&AWSAccessKeyId=1'; echo

# TerminateInstances.
curl -i 'localhost:21737/?Action=TerminateInstances&AWSAccessKeyId=1&InstanceId.1=i-00000000'; echo

curl -i 'localhost:21737/?Action=TerminateInstances&AWSAccessKeyId=1&InstanceId.1=i-00000001'; echo
curl -i 'localhost:21737/?Action=TerminateInstances&AWSAccessKeyId=1&InstanceId.1=i-00000002'; echo
curl -i 'localhost:21737/?Action=TerminateInstances&AWSAccessKeyId=1&InstanceId.1=i-00000003'; echo
curl -i 'localhost:21737/?Action=TerminateInstances&AWSAccessKeyId=1&InstanceId.1=i-00000004'; echo
curl -i 'localhost:21737/?Action=TerminateInstances&AWSAccessKeyId=1&InstanceId.1=i-00000005'; echo

curl -i 'localhost:21737/?Action=TerminateInstances&AWSAccessKeyId=1&InstanceId.1=i-00000006'; echo

# DeleteKeyPair
curl -i 'localhost:21737/?Action=DeleteKeyPair&AWSAccessKeyId=1&KeyName=kn-1'; echo
