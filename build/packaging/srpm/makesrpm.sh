#!/bin/bash
set -e
if [[ $VERBOSE ]]; then
  set -x
fi

# makesrpm.sh - generates rpms from the currently checked out HEAD,
# along with condor.spec and the sources in this directory

usage () {
  echo "usage: [VERBOSE=1] $(basename "$0") [-ba|-bs]"
  echo "  -ba    Build binary and source packages"
  echo "  -bs    Build source package only (default)"
  exit
}

fail () { echo "$@" >&2; exit 1; }

case $1 in
   -ba | -bs ) buildmethod=$1 ;;
          '' ) buildmethod=-bs ;;
  --help | * ) usage ;;
esac

# Do everything in a temp dir that will go away on errors or end of script
tmpd=$(mktemp -d "$PWD/.tmpXXXXXX")
trap 'rm -rf "$tmpd"' EXIT

WD=$PWD                 # save working dir
cd "$(dirname "$0")"    # go to srpm dir
srpm_dir=$PWD
cd ../../..             # go to root of git tree

condor_version=$(
  git show HEAD:CMakeLists.txt | awk -F\" '/^set\(VERSION / {print $2}'
)

[[ $condor_version ]] || fail "Condor version string not found"
[[ $condor_version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] \
|| fail "Bad condor version string '$condor_version'"

git archive HEAD | gzip > "$tmpd/condor.tar.gz"

git_rev=$(git rev-parse --short HEAD)

cd "$tmpd"
mkdir SOURCES
cp -p "$srpm_dir"/* SOURCES
mv condor.tar.gz SOURCES

sed -i "
  s/^%define git_rev .*/%define git_rev $git_rev/
  s/^%define tarball_version .*/%define tarball_version $condor_version/
" SOURCES/condor.spec

rpmbuild $buildmethod -D"_topdir $tmpd" SOURCES/condor.spec

mv *RPMS/*.rpm "$WD"

