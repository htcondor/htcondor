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
  exit 1
}

fail () { echo "$@" >&2; exit 1; }

[[ $dest_dir ]] || dest_dir=$PWD

check_version_string () {
  [[ ${!1} =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "Bad ${1//_/ }: '${!1}'"
}

# get the version and build id
condor_build_id=$(<BUILD-ID)
condor_version=$(echo condor-*.tgz | sed -e s/^condor-// -e s/.tgz$//)

[[ $condor_version ]] || fail "Condor version string not found"
check_version_string  condor_version

# Do everything in a temp dir that will go away at end of script
tmpd=$(mktemp -d "$PWD/build-XXXXXX")

cd "$tmpd"

# Unpack the official tarball
mv "../condor-${condor_version}.tgz" "./condor_${condor_version}.orig.tar.gz"
tar xfpz "condor_${condor_version}.orig.tar.gz"
cd "condor-${condor_version}"

# copy debian files into place
cp -pr build/packaging/debian debian
(cd debian; ./prepare-build-files.sh -DUW_BUILD)

# set default email address for build
export DEBEMAIL=${DEBEMAIL-htcondor-admin@cs.wisc.edu}
# if running in a condor slot, set parallelism to slot size
export DEB_BUILD_OPTIONS="parallel=${OMP_NUM_THREADS-1} terse"

# Extract prerelease value from top level CMake file
PRE_RELEASE=$(grep '^set(PRE_RELEASE' CMakeLists.txt)
PRE_RELEASE=${PRE_RELEASE#* } # Trim up to and including space
PRE_RELEASE=${PRE_RELEASE%\)*} # Trim off the closing parenthesis

# Distribution should be one of experimental, unstable, testing, stable, oldstable, oldoldstable
# unstable -> daily repo
# testing -> rc repo
# stable -> release repo

if [ "$PRE_RELEASE" = 'OFF' ]; then
    dist='stable'
elif [ "$PRE_RELEASE" = '"RC"' ]; then
    dist='testing'
else
    dist='unstable'
fi

echo "Distribution is $dist"

if [ "$PRE_RELEASE" = 'OFF' ]; then
    # Changelog entry is present for final release build
    dch --release --distribution $dist ignored
    sed -i s/$condor_version-[0-9]*/\&+SYS99/
else
    # Generate a changelog entry
    dch --distribution $dist --newversion "$condor_version-0.$condor_build_id+SYS99" "Automated build"
fi

. /etc/os-release
if [ "$VERSION_CODENAME" = 'bullseye' ]; then
    SYS99='deb11'
elif [ "$VERSION_CODENAME" = 'bookworm' ]; then
    SYS99='deb12'
elif [ "$VERSION_CODENAME" = 'focal' ]; then
    SYS99='ubu20'
elif [ "$VERSION_CODENAME" = 'jammy' ]; then
    SYS99='ubu22'
elif [ "$VERSION_CODENAME" = 'noble' ]; then
    SYS99='ubu24'
elif [ "$VERSION_CODENAME" = 'chimaera' ]; then
    SYS99='dev04'
else
    echo ERROR: Unknown codename "${VERSION_CODENAME}"
    exit 1
fi

if grep -q SYS99 debian/changelog; then
    sed -i s/SYS99/$SYS99/ debian/changelog
else
    echo ERROR: SYS99 not present in changelog
    exit 1
fi

dpkg-buildpackage -uc -us

cd ..

files="[a-z]*.buildinfo [a-z]*.changes [a-z]*deb [a-z]*.debian.tar.* [a-z]*.dsc [a-z]*.orig.tar.*"
# shellcheck disable=SC2086 # Intended splitting of files
mv $files "$dest_dir"
rm -rf "$tmpd"
cd "$dest_dir"
# shellcheck disable=SC2086 # Intended splitting of files
ls -lh $files
