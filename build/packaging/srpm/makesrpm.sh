#!/bin/bash
set -e

# makesrpm.sh - generates a .src.rpm from the currently checked out HEAD,
# along with condor.spec and the sources in this directory

usage () {
  echo "usage: $(basename "$0")"
  echo "Sorry, no options yet..."
  exit
}

case $1 in
  --help ) usage ;;
esac

# Do everything in a temp dir that will go away on errors or end of script
tmpd=$(mktemp -d)
trap 'rm -rf "$tmpd"' EXIT

git_rev=$(git log -1 --pretty=format:%h)
sed -i "s/^%define git_rev .*/%define git_rev $git_rev/" condor.spec

mkdir "$tmpd/SOURCES"
pushd "$(dirname "$0")" >/dev/null
cp -p * "$tmpd/SOURCES/"

(cd ../../..; git archive HEAD) | gzip > "$tmpd/SOURCES/condor.tar.gz"

srpm=$(rpmbuild -bs -D"_topdir $tmpd" condor.spec)
srpm=${srpm#Wrote: }

popd >/dev/null
cp "$srpm" .
echo "Wrote: ${srpm##*/}"

