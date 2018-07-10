#!/bin/bash
set -e
if [[ $VERBOSE ]]; then
  set -x
fi

# build_uw_deb

usage () {
  echo "usage: [VERBOSE=1] $(basename "$0")"
  echo
  echo "Environment:"
  echo "  VERBOSE=1                         Show all commands run by script"
  echo
  exit $1
}

fail () { echo "$@" >&2; exit 1; }

top_dir=$PWD
[[ $dest_dir ]] || dest_dir=$PWD

check_version_string () {
  [[ ${!1} =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "Bad ${1//_/ }: '${!1}'"
}

# Do everything in a temp dir that will go away on errors or end of script
tmpd=$(mktemp -d "$PWD/.tmpXXXXXX")
trap 'rm -rf "$tmpd"' EXIT

cd "$tmpd"

# untar the condor sources into a temporary directory
mkdir "condor_src"
cd "condor_src"
tar xfpz "$TMP"/condor.tar.gz

# get the version and build id out of the sources
condor_build_id=$(<BUILD-ID)
condor_version=$(awk -F\" '/^set\(VERSION / {print $2}' CMakeLists.txt)

[[ $condor_version ]] || fail "Condor version string not found"
check_version_string  condor_version

# Update condor_version for pre-release build
condor_release="0.$condor_build_id"

# copy srpm files from condor sources into the SOURCES directory
cp -pr build/packaging/new-debian debian
#date=$(date)
#changelog="condor ($condor_version-$condor_release~1) unstable; urgency=medium
#
#  * Nightly build
#
# -- Tim Theisen <tim@cs.wisc.edu> $date
#"
#echo "$changelog" > debian/changelog
#cat build/packaging/new-debian/changelog >> debian/changelog
dch --distribution unstable --newversion "$condor_version-$condor_release" "Nightly build"
cd ..

# rename the condor_src directory to have the version number in it, then tar that up
mv "condor_src" "condor-$condor_version"
tar cfz condor_$condor_version.orig.tar.gz "condor-$condor_version"
#rm -rf "condor-$condor_version"
cd "condor-$condor_version"

dpkg-buildpackage -uc -us

cd ..

mv condor* htcondor* libclassad* "$dest_dir"
ls -lh "$dest_dir"

