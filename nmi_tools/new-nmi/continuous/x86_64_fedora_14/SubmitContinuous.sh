#!/bin/sh

GITDIR=/home/condorauto/condor.git
export GITDIR

cd /home/condorauto/continuous/x86_64_fedora_14

set -x
exec > run.sh.out
exec 2>&1 

git --git-dir=$GITDIR fetch

sha1=`git --git-dir=$GITDIR log --pretty=oneline -n 1 origin/master | awk '{print $1}'`

git --git-dir=$GITDIR archive origin/master nmi_tools | tar xf - nmi_tools

GIT_DIR=/home/condorauto/condor.git NMI_BIN=/usr/local/nmi/bin CONDOR_BIN=/usr/bin ./nmi_tools/condor_nmi_submit --notify=gthain@cs.wisc.edu --build --git --tag=origin/master --module=FOO --desc="Continuous Build - x86_64_fedora_14" --platforms="x86_64_fedora_14"  --notify-fail-only --sha1=$sha1 > condor_nmi_submit.out

runid=`awk '/^Run ID:/ {print $3}' condor_nmi_submit.out`
echo "$runid	$sha1" >> runs.out
exit 0
