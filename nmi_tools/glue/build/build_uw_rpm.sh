#!/bin/bash
set -e
if [[ $VERBOSE ]]; then
  set -x
fi

# makesrpm.sh - generates source and binary rpms with a source tarball
# along with condor.spec and the source files in this directory

usage () {
  echo "usage: [VERBOSE=1] $(basename "$0") [options] [--] [rpmbuild-options]"
  echo "Options:"
  echo "  -ba          Build binary and source packages (default)"
  echo "  -bs          Build source package only"
  echo
  echo "  -o DESTDIR   Output rpms to DESTDIR (default=\$PWD)"
  echo
  echo "Environment:"
  echo "  VERBOSE=1                         Show all commands run by script"
  echo
  exit $1
}

fail () { echo "$@" >&2; exit 1; }

# defaults
buildmethod=-ba

while [[ $1 = -* ]]; do
case $1 in
  -ba | -bs ) buildmethod=$1;              shift ;;
         -o ) dest_dir=$(cd "$2" && pwd);  shift 2 ;;

  --*=*  ) set -- "${1%%=*}" "${1#*=}" "${@:2}" ;;  # parse --option=value

  --help ) usage ;;
  -- ) shift; break ;;
  *  ) usage 1 ;;             # assume remaining args are usage errors
esac
done

if [[ $buildmethod = -bs ]]; then
  buildmethod+=" --nodeps"
fi

top_dir=$PWD
[[ $dest_dir ]] || dest_dir=$PWD

check_version_string () {
  [[ ${!1} =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "Bad ${1//_/ }: '${!1}'"
}

# get the version and build id
condor_build_id=$(<BUILD-ID)
condor_version=$(echo condor-*.tgz | sed -e s/^condor-// -e s/.tgz$//)

[[ $condor_version ]] || fail "Condor version string not found"
check_version_string  condor_version

# Do everything in a temp dir that will go away on errors or end of script
tmpd=$(mktemp -d "$PWD/.tmpXXXXXX")
trap 'rm -rf "$tmpd"' EXIT

cd "$tmpd"
mkdir SOURCES BUILD BUILDROOT RPMS SPECS SRPMS
mv ../condor-${condor_version}.tgz SOURCES/condor-${condor_version}.tar.gz

# copy srpm files from condor sources into the SOURCES directory
tar xvfpz SOURCES/condor-${condor_version}.tar.gz condor-${condor_version}/build/packaging/srpm
cp -p condor-${condor_version}/build/packaging/srpm/* SOURCES
rm -rf condor-${condor_version}

# inject the version and build id into the spec file
update_spec_define () {
  sed -i "s|^ *%define * $1 .*|%define $1 $2|" SOURCES/condor.spec
}

update_spec_define git_build 0
update_spec_define tarball_version "$condor_version"
update_spec_define condor_build_id "$condor_build_id"
# Set HTCondor base release for pre-release build
update_spec_define condor_base_release "0.$condor_build_id"
# Set HTCondor base release to 1 for final release.
#update_spec_define condor_base_release "1"

VERBOSE=1
export VERBOSE

# Use as many CPUs as are in the condor slot we are in, 1 if undefined
export RPM_BUILD_NCPUS=${OMP_NUM_THREADS-1}

rpmbuild -v $buildmethod "$@" --define="_topdir $tmpd" SOURCES/condor.spec


mv $(find *RPMS -name \*.rpm) "$dest_dir"
ls -lh "$dest_dir"
