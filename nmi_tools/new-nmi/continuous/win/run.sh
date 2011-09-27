#!/bin/sh



GITDIR=/space/git/CONDOR_SRC.git
export GITDIR

PATH=/prereq/bin:$PATH

cd /home/cndrauto/continuous/win

set -x
exec > run.sh.out
exec 2>&1 

git --git-dir=$GITDIR fetch
git --git-dir=/space/git/CONDOR_EXT.git fetch
git --git-dir=/space/git/CONDOR_DOC.git fetch

sha1=`git --git-dir=$GITDIR log --pretty=oneline -n 1 origin/master | awk '{print $1}'`

git --git-dir=$GITDIR archive origin/master nmi_tools | tar xf - nmi_tools

./nmi_tools/condor_nmi_submit --notify=condor-builds@cs.wisc.edu --build --git --tag=origin/master --module=FOO --use-externals-cache --clear-externals-cache-daily --desc="Continuous Build - x86_winnt_5.1" --platforms="x86_winnt_5.1"  --notify-fail-only --sha1=$sha1 > condor_nmi_submit.out
# There are not non-virtualized Windows machines. Also, it didn't
# actually do anything prior to Jun 21, 2011
#--append="append_requirements=(VirtualizedMachine =!= TRUE)" 

runid=`awk '/^Run ID:/ {print $3}' condor_nmi_submit.out`
echo "$runid	$sha1" >> runs.out
exit 0
