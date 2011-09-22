#!/bin/sh

GITDIR=/space/git/CONDOR_SRC.git
export GITDIR

PATH=/prereq/bin:$PATH

cd /home/cndrauto/continuous/x86_64_opensuse_11.4-updated

set -x
exec > run.sh.out
exec 2>&1 

git --git-dir=$GITDIR fetch
git --git-dir=/space/git/CONDOR_EXT.git fetch
git --git-dir=/space/git/CONDOR_DOC.git fetch

sha1=`git --git-dir=$GITDIR log --pretty=oneline -n 1 origin/master | awk '{print $1}'`

git --git-dir=$GITDIR archive origin/master nmi_tools | tar xf - nmi_tools

./nmi_tools/condor_nmi_submit --notify=condor-builds@cs.wisc.edu --build --git --tag=origin/master --module=FOO --use-externals-cache --clear-externals-cache-daily --desc="Continuous Build - x86_64_opensuse_11.4-updated" --platforms="x86_64_opensuse_11.4-updated"  --notify-fail-only --sha1=$sha1 > condor_nmi_submit.out

runid=`awk '/^Run ID:/ {print $3}' condor_nmi_submit.out`
echo "$runid	$sha1" >> runs.out
exit 0
