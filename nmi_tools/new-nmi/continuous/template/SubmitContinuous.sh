#!/bin/sh

GITDIR=/space/git/CONDOR_SRC.git
export GITDIR

PATH=/prereq/bin:$PATH

cd /home/cndrauto/continuous/PLATFORM

set -x
exec > run.sh.out
exec 2>&1 

git --git-dir=$GITDIR fetch
git --git-dir=/space/git/CONDOR_EXT.git fetch
git --git-dir=/space/git/CONDOR_DOC.git fetch

sha1=`git --git-dir=$GITDIR log --pretty=oneline -n 1 origin/master | awk '{print $1}'`

git --git-dir=$GITDIR archive origin/master nmi_tools | tar xf - nmi_tools

# Sadly, the description is important to have the dashboard function correctly.  
# So make sure that you replace PLATFORM with the exact NMI platform name, 
# e.g. x86_64_rhap_6.1 (and not a shortcut, like RHAP61)
./nmi_tools/condor_nmi_submit --notify=condor-builds@cs.wisc.edu --build --git --tag=origin/master --module=FOO --use-externals-cache --clear-externals-cache-daily --desc="Continuous Build - PLATFORM" --platforms="PLATFORM"  --notify-fail-only --sha1=$sha1 > condor_nmi_submit.out

runid=`awk '/^Run ID:/ {print $3}' condor_nmi_submit.out`
echo "$runid	$sha1" >> runs.out
exit 0
