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

# Do everything in a temp dir that will go away on errors or end of script
tmpd=$(mktemp -d "$PWD/.tmpXXXXXX")
trap 'rm -rf "$tmpd"' EXIT

cd "$tmpd"

# Unpack the official tarball
mv "../condor-${condor_version}.tgz" "./condor_${condor_version}.orig.tar.gz"
tar xfpz "condor_${condor_version}.orig.tar.gz"
cd "condor-${condor_version}"

# copy debian files into place
cp -pr build/packaging/debian debian
(cd debian; ./prepare-build-files.sh)

# set default email address for build
export DEBEMAIL=${DEBEMAIL-htcondor-admin@cs.wisc.edu}
# if running in a condor slot, set parallelism to slot size
export DEB_BUILD_OPTIONS="parallel=${OMP_NUM_THREADS-1}"

# Distribution should be one of experimental, unstable, testing, stable, oldstable, oldoldstable
# unstable -> daily repo
# testing -> rc repo
# stable -> release repo

dist='unstable'
#dist='testing'
#dist='stable'
echo "Distribution is $dist"

# Nightly build changelog
dch --distribution $dist --newversion "$condor_version-0.$condor_build_id" "Nightly build"

# Final release changelog
#dch --release --distribution $dist ignored

if grep -qi bullseye /etc/os-release; then
    true
elif grep -qi bookworm /etc/os-release; then
    dch --distribution $dist --nmu 'place holder entry'
elif grep -qi bionic /etc/os-release; then
    true
elif grep -qi focal /etc/os-release; then
    dch --distribution $dist --nmu 'place holder entry'
elif grep -qi jammy /etc/os-release; then
    dch --distribution $dist --nmu 'place holder entry'
    dch --distribution $dist --nmu 'place holder entry'
else
    echo ERROR: Unknown codename
    exit 1
fi

dpkg-buildpackage -uc -us

cd ..

mv ./*.dsc ./*.debian.tar.xz ./*.orig.tar.gz "$dest_dir"
mv ./*.changes ./*.deb "$dest_dir"
ls -lh "$dest_dir"

