#!/bin/sh

WHOAMI=`whoami`
cd $1
umask 077
mkdir -p condor.$2.$3.$WHOAMI
cd condor.$2.$3.$WHOAMI
mkdir -p execute
mkdir -p log
echo $WHOAMI
