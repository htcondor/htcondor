#!/bin/sh

#
# This script takes a condor distribution .tar.gz file and 
# creates an RPM from it in the current directory.
#
# Example:
#   make-condor-rpm.sh condor-6.5.1.tar.gz "6.5.1"
#
# Notes:
#  1) This script requires (the new) condor_configure at the root level 
#  (i.e. in place of old condor_install)
#  2) The default directory of installation is /opt/condor-<version>,
#    e.g. /opt/condor-6.5.1
#  3) Condor .tar.gz file should follow naming convention:
#    condor-<ver>-....tar.gz
#
#
#

if [ -z "$1" ] 
then
    echo "Usage: make-condor-rpm.sh <Condor dirstribution .tar.gz>"
    exit 1
fi

# Figure out version from file name condor-<ver>-....tar.gz
condor_version=`perl -e '@v=split(/-/, $ARGV[0]); print $v[1];' $1`;

rm -rf /tmp/make-condor-rpm && mkdir /tmp/make-condor-rpm
if [ $? != 0 ]
then
    echo "Unable to create /tmp/make-condor-rpm"
    exit 2
fi

cp $1 /tmp/make-condor-rpm/condor.tar.gz
if [ $? != 0 ]
then
    echo "InvalidCondor distribution file file: $1"
    exit 3
fi


pushd /tmp/make-condor-rpm

# Untar condor distribution
tar xzvf condor.tar.gz || exit 4

cd condor-${condor_version}

# Create RPM build directory
mkdir -p rpmbuild/BUILD
mkdir -p rpmbuild/SOURCES
mkdir -p rpmbuild/RPMS

ln -s `pwd` /opt/condor-${condor_version} 
if [ $? != 0 ]
then
    echo "Unable to create /opt/condor-${condor_version}"
    exit 5
fi

# Create RPM spec file
cat > ./condor.spec <<EOF
Summary: Condor
Name: condor
Version: ${condor_version}
Release: 1
URL: http://www.cs.wisc.edu/condor
License: See http://www.cs.wisc.edu/condor/condor-public-license.html
Group: Condor
Prefix: /opt/%{name}-%{version}

%description

%prep

%build

%install

%clean

%post
cd \${RPM_INSTALL_PREFIX}
./condor_configure --install

%postun
rm -rf \${RPM_INSTALL_PREFIX}

%files
/opt/%{name}-%{version}/condor_configure
/opt/%{name}-%{version}/DOC
/opt/%{name}-%{version}/examples
/opt/%{name}-%{version}/INSTALL
/opt/%{name}-%{version}/LICENSE.TXT
/opt/%{name}-%{version}/README
/opt/%{name}-%{version}/release.tar

%changelog
EOF

# Build RPM
rpmbuild --define "_topdir rpmbuild" -bb condor.spec && yes_rpm='y'

# Place RPM in the resulting directory
popd >> /dev/null
cp /tmp/make-condor-rpm/condor-${condor_version}/rpmbuild/RPMS/i386/*.rpm .

# Cleanup
rm /opt/condor-${condor_version}
rm -rf /tmp/make-condor-rpm    

if [ "$yes_rpm" != 'y' ] 
then
    exit 7
fi
