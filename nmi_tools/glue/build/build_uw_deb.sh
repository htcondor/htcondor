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
mv ../condor-${condor_version}.tgz ./condor_${condor_version}.orig.tar.gz
tar xfpz condor_${condor_version}.orig.tar.gz
cd condor-${condor_version}

# copy srpm files from condor sources into the SOURCES directory
cp -pr build/packaging/new-debian debian

if $(grep -qi buster /etc/os-release); then
    suffix=''
elif $(grep -qi bullseye /etc/os-release); then
    suffix='n1'
    mv debian/control.focal debian/control
    mv debian/htcondor.install.focal debian/htcondor.install
    mv debian/rules.focal debian/rules
    mv debian/patches/series.focal debian/patches/series
elif $(grep -qi bionic /etc/os-release); then
    suffix=''
elif $(grep -qi focal /etc/os-release); then
    suffix='n1'
    mv debian/control.focal debian/control
    mv debian/htcondor.install.focal debian/htcondor.install
    mv debian/rules.focal debian/rules
    mv debian/patches/series.focal debian/patches/series
else
    suffix=''
fi

# Distribution should be one of experimental, unstable, testing, stable, oldstable, oldoldstable
# unstable -> daily repo
# testing -> rc repo
# stable -> release repo

dist='unstable'
#dist='testing'
#dist='stable'
echo "Distribution is $dist"
echo "Suffix is '$suffix'"

# Nightly build changelog
dch --distribution $dist --newversion "$condor_version-0.$condor_build_id" "Nightly build"

# Final release changelog
#dch --release --distribution $dist ignored

if [ "$suffix" = '' ]; then
    build='full'
    dpkg-buildpackage -uc -us
elif [ "$suffix" = 'b1' ]; then
    build='binary'
    dch --distribution $dist --bin-nmu 'place holder entry'
    dpkg-buildpackage --build=$build -uc -us
elif [ "$suffix" = 'b2' ]; then
    build='binary'
    dch --distribution $dist --bin-nmu 'place holder entry'
    dch --distribution $dist --bin-nmu 'place holder entry'
    dpkg-buildpackage --build=$build -uc -us
elif [ "$suffix" = 'n1' ]; then
    build='full'
    dch --distribution $dist --nmu 'place holder entry'
    dpkg-buildpackage --build=$build -uc -us
elif [ "$suffix" = 'n2' ]; then
    build='full'
    dch --distribution $dist --nmu 'place holder entry'
    dch --distribution $dist --nmu 'place holder entry'
    dpkg-buildpackage --build=$build -uc -us
fi

cd ..

if [ "$build" = 'full' ]; then
    mv *.dsc *.debian.tar.xz *.orig.tar.gz "$dest_dir"
fi
mv *.changes *.deb "$dest_dir"
ls -lh "$dest_dir"

