#!/bin/sh

##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************



# This script takes a condor distribution .tar.gz file and 
# creates an RPM that will be put into the current directory.
#
# Example:
#   make-condor-rpm.sh condor-6.5.1.tar.gz /tmp [release-number]
#
# Notes:
#  1) The RPM's install directory is /opt/condor-<version>,
#    e.g. /opt/condor-6.5.1
#  2) Condor .tar.gz file should follow naming convention:
#    condor-<ver>-....tar.gz
#
# TO DO:
#   + Don't require root
#   + Allow release to be set

if [ $# -ne 3 -a $# -ne 4 ]
then
    echo "Usage: make-condor-rpm.sh <condor_dist.tar.gz> <rpmcmd> <build-dir> [release]"
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

if [ ! -x $2 ]; then
	echo "$2 is not executable."
	exit 3
fi

if [ ! -e $3 ]; then 
	echo "$3 does not exist."
	exit 3
fi

if [ ! -d $3 ]; then 
	echo "$3 is not a directory."
	exit 3
fi

if [ -z $4 ]; then
	release="1"
else
	release=$4
fi

base_name=`basename $1`
#Remove .tar.gz from the name of the distribution
naked_name=`echo $base_name | sed -e 's/.tar.gz//'`

# Figure out version from file name condor-<ver>-....tar.gz
condor_version=`perl -e '@v=split(/-/, $ARGV[0]); print $v[1];' $1`;
echo "*** This appears to be Condor version: $condor_version"

rpm_build_cmd=${2}
builddir=${3}/make-condor-rpm

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

./condor_configure --install --owner=\${owner}

%postun


%files
%defattr(-,root,root)
/opt/%{name}-%{version}

%changelog
EOF

# Build RPM
echo "*** Building the RPM..."

# The determination to use rpm, rpmbuild, or whatever happened with configure.
RPMBUILD_CMD=${rpm_build_cmd}

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

