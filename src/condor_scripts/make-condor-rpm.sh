#!/bin/sh

##/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
##
## Condor Software Copyright Notice
## Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
##
## This source code is covered by the Condor Public License, which can
## be found in the accompanying LICENSE.TXT file, or online at
## www.condorproject.org.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
## AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
## IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
## WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
## FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
## HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
## MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
## ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
## PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
## RIGHT.
##
##***************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


# This script takes a condor distribution .tar.gz file and 
# creates an RPM that will be put into the current directory.
#
# Example:
#   make-condor-rpm.sh condor-6.5.1.tar.gz /tmp [release-number]
#
# Notes:
#  1) This script requires (the new) condor_configure at the root level 
#  (i.e. in place of old condor_install)
#  2) The RPM's install directory is /opt/condor-<version>,
#    e.g. /opt/condor-6.5.1
#  3) Condor .tar.gz file should follow naming convention:
#    condor-<ver>-....tar.gz
#
# TO DO:
#   + Don't require root
#   + Allow release to be set

if [ $# -ne 2 -a $# -ne 3 ]
then
    echo "Usage: make-condor-rpm.sh <condor_dist.tar.gz> <build-dir> [release]"
    echo "       The build directory needs to have about twice as much space as"
    echo "       the untarred Condor distribution. /tmp will be okay on most machines."
	echo "       Release is optional"
    exit 1
fi

echo "*** Verifying Environment..."

if [ ! -e $1 ]; then
	echo "$1 does not exist."
	exit 3
fi

if [ ! -e $2 ]; then 
	echo "$2 does not exist."
	exit 3
fi

if [ ! -d $2 ]; then 
	echo "$2 is not a directory."
	exit 3
fi

if [ -z $3 ]; then
	release="1"
else
	release=$3
fi

base_name=`basename $1`
#Remove .tar.gz from the name of the distribution
naked_name=`echo $base_name | sed -e 's/.tar.gz//'`

# Figure out version from file name condor-<ver>-....tar.gz
condor_version=`perl -e '@v=split(/-/, $ARGV[0]); print $v[1];' $1`;
echo "*** This appears to be Condor version: $condor_version"

builddir=${2}/make-condor-rpm

if [ -e ${builddir} ]; then
	echo "*** Removing old build directory: ${builddir}..."
	rm -rf ${builddir}
fi

mkdir ${builddir}
if [ $? != 0 ]
then
    echo "Unable to create ${builddir}"
    exit 2
fi

echo "*** Copying ${base_name}..."
cp $1 ${builddir}/condor.tar.gz
if [ $? != 0 ]
then
    echo "Unable to copy $1"
    exit 3
fi

pushd ${builddir} >> /dev/null

echo "*** Creating RPM build directories..."
# Create RPM build directory
mkdir -p rpmbuild/BUILD
mkdir -p rpmbuild/SOURCES
mkdir -p rpmbuild/RPMS
mkdir -p rpmbuild/SRPMS
mkdir -p rpmbuild/opt

mv condor.tar.gz rpmbuild/opt

cd rpmbuild/opt

echo "*** Untarring ${base_name}..."
tar xzf condor.tar.gz 
if [ $? != 0 ]; then
	echo "Unable to untar ${base_name}"
	exit 4
fi

rm condor.tar.gz
cd condor-${condor_version}

echo "*** Untarring release.tar..."
tar xf release.tar
rm condor_install
rm release.tar

cd ${builddir}

# Note something important in the spec file below: In the %files
# section we have: %defattr(-,root,root). The hyphen means to leave
# the permission as they are, and the "root, root" means to make
# the owner and group root. If we don't do this, it will preserve
# the owner, and if you create it as "roy", it will complain if
# user "roy" doesn't exist. Since RPMs have to be installed as 
# root, this is a safe thing to do. Also, when the condor_configure
# script runs during installation, we'll change the ownership
# as best we can--see %install below

echo "*** Creating RPM spec file..."
cat > condor.spec <<EOF
Summary: Condor ${condor_version}
Name: condor
Version: ${condor_version}
Release: ${release}
URL: http://www.cs.wisc.edu/condor
License: See http://www.cs.wisc.edu/condor/condor-public-license.html
Group: Condor
Vendor: Condor Project
Packager: Condor Project
Prefix: /opt/%{name}-%{version}
Buildroot: ${builddir}/rpmbuild

%description
This RPM contains Condor version ${condor_version}.

%prep

%build

%install

%clean

%post
cd \${RPM_INSTALL_PREFIX}

# Figure out the user
owner="condor"
grep condor: /etc/passwd > /dev/null || owner="daemon"

# This is a hack so that condor_configure works
touch ./ignore.me
tar cf ./release.tar ignore.me
rm ./ignore.me
mv sbin sbin.tmp
./condor_configure --install --owner=\${owner}
mv sbin.tmp sbin
rm ./ignore.me
rm ./release.tar

%postun


%files
%defattr(-,root,root)
/opt/%{name}-%{version}

%changelog
EOF

# Build RPM
echo "*** Building the RPM..."

# determine if I should use rpm or rpmbuild
if [ `rpm -bb --help > /dev/null 2>&1` ]; then
	RPMBUILD_CMD="rpm"
else
	RPMBUILD_CMD="rpmbuild"
fi

echo "$RPMBUILD_CMD --define "_topdir rpmbuild" -bb condor.spec"
$RPMBUILD_CMD --define "_topdir rpmbuild" -bb condor.spec

if [ $? != 0 ]; then
	echo "Couldn't build rpm!"
	exit 7
fi

arch=`ls -1 rpmbuild/RPMS/ | sed -e 's|\/||'`
rpm_name=`ls -1 rpmbuild/RPMS/${arch}`

# Place RPM in the resulting directory
echo "*** Copying the RPM: (arch=$arch, rpm_name=$rpm_name)"
popd >> /dev/null
cp ${builddir}/rpmbuild/RPMS/${arch}/${rpm_name} \
   ./${naked_name}-${release}.${arch}.rpm

# Cleanup
echo "*** Cleaning up..."
rm -rf ${builddir}

echo
echo "Output: "`pwd`/${naked_name}-${release}.${arch}.rpm
echo

