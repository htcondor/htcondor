#!/bin/bash
set -e
if [[ $VERBOSE ]]; then
  set -x
fi

# makerpm.sh - generates source and binary rpms with a source tarball
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
  exit 1
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

[[ $dest_dir ]] || dest_dir=$PWD

check_version_string () {
  [[ ${!1} =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "Bad ${1//_/ }: '${!1}'"
}

# get the version and build id
condor_build_id=$(<BUILD-ID)
condor_version=$(echo condor-*.tgz | sed -e s/^condor-// -e s/.tgz$//)
condor_git_sha=-1
if [ -f GIT-SHA ]; then
    condor_git_sha=$(<GIT-SHA)
fi

[[ $condor_version ]] || fail "Condor version string not found"
check_version_string  condor_version

# Do everything in a temp dir that will go away at end of script
tmpd=$(mktemp -d "$PWD/build-XXXXXX")

cd "$tmpd"
mkdir SOURCES BUILD BUILDROOT RPMS SPECS SRPMS
mv "../condor-${condor_version}.tgz" "SOURCES/condor-${condor_version}.tar.gz"

# copy rpm files from condor sources into the SOURCES directory
tar xvfpz "SOURCES/condor-${condor_version}.tar.gz" "condor-${condor_version}/build/packaging/rpm" "condor-${condor_version}/CMakeLists.txt"
cp -p condor-"${condor_version}"/build/packaging/rpm/* SOURCES

# Extract prerelease value from top level CMake file
PRE_RELEASE=$(grep '^set(PRE_RELEASE' "condor-${condor_version}/CMakeLists.txt")
PRE_RELEASE=${PRE_RELEASE#* } # Trim up to and including space
PRE_RELEASE=${PRE_RELEASE%\)*} # Trim off the closing parenthesis
rm -rf "condor-${condor_version}"

# inject the version and build id into the spec file
update_spec_define () {
  sed -i "s|^ *%define * $1 .*|%define $1 $2|" SOURCES/condor.spec
}

update_spec_define uw_build "1"
update_spec_define condor_version "$condor_version"
update_spec_define condor_build_id "$condor_build_id"
if [ $condor_git_sha != -1 ]; then
    update_spec_define condor_git_sha "$condor_git_sha"
fi

if [ "$PRE_RELEASE" = 'OFF' ]; then
    # Set HTCondor base release to 1 for final release.
    update_spec_define condor_release "1"
else
    # Set HTCondor base release for pre-release build
    update_spec_define condor_release "0.$condor_build_id"
fi

# Use as many CPUs as are in the condor slot we are in, 1 if undefined
export RPM_BUILD_NCPUS=${OMP_NUM_THREADS-1}

rpmbuild "$buildmethod" "$@" --define="_topdir $tmpd" SOURCES/condor.spec

# shellcheck disable=SC2046 # Intended splitting of find output
mv $(find ./*RPMS -name \*.rpm) "$dest_dir"
rm -rf "$tmpd"
cd "$dest_dir"
ls -lh [a-z]*.rpm
