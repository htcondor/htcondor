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

base_name=`echo $1 | sed -e 's/.tar.gz//'`

# Figure out version from file name condor-<ver>-....tar.gz
condor_version=`perl -e '@v=split(/-/, $ARGV[0]); print $v[1];' $1`;
echo "Condor Version: $condor_version"

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


pushd /tmp/make-condor-rpm >> /dev/null

echo "*** Untarring distribution"
# Untar condor distribution
tar xzf condor.tar.gz || exit 4

cd condor-${condor_version}

echo "*** Untarring release.tar"
file_list=`tar xvf release.tar`

echo "*** Creating RPM build directories"
# Create RPM build directory
mkdir -p rpmbuild/BUILD
mkdir -p rpmbuild/SOURCES
mkdir -p rpmbuild/RPMS

ln -s `pwd` /opt/condor-${condor_version} 
if [ $? -ne 0 ]
then
    echo "Unable to create /opt/condor-${condor_version}"
    exit 5
fi

echo "*** Creating RPM spec file"
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

# Figure out the user
owner="condor"
grep condor: /etc/passwd || owner="daemon"

# This is a hack so that condor_configure works
touch ./ignore.me
tar cf ./release.tar ignore.me
rm ./ignore.me
./condor_configure --install --owner=\${owner}
rm ./ignore.me
rm ./release.tar

%postun
rm -rf \${RPM_INSTALL_PREFIX}

%files
/opt/%{name}-%{version}/condor_configure
/opt/%{name}-%{version}/DOC
/opt/%{name}-%{version}/examples
/opt/%{name}-%{version}/INSTALL
/opt/%{name}-%{version}/LICENSE.TXT
/opt/%{name}-%{version}/README
EOF

# Now print the filelist too
for f in $file_list
do
  echo '/opt/%{name}-%{version}/'$f >> ./condor.spec
done
echo '%changelog' >> ./condor.spec


# Build RPM
echo "*** Building RPM"
rpmbuild --define "_topdir rpmbuild" -bb condor.spec && yes_rpm='y'

# Place RPM in the resulting directory
popd >> /dev/null
rpm_name="condor-$condor_version-1.i386.rpm"
cp /tmp/make-condor-rpm/condor-${condor_version}/rpmbuild/RPMS/i386/${rpm_name} \
   ./${base_name}-1.i386.rpm


# Cleanup
echo "*** Cleaning up"
rm /opt/condor-${condor_version}
rm -rf /tmp/make-condor-rpm    

if [ "$yes_rpm" != 'y' ] 
then
    exit 7
fi

echo
echo "Output: "`pwd`/${base_name}-1.i386.rpm
echo

