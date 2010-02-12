#!/bin/bash

#Input params
CONDOR_TARFILE=$1
OS=$2
RPM_CMD=$3
REVISION=$4

#############################################################################################################

#Root folder for build area
BUILD_DIR=.

#Script expect the following structure
#$BUILD_DIR/BUILD
#$BUILD_DIR/RPMS
#$BUILD_DIR/SRPMS
#$BUILD_DIR/SOURCES/$CONDOR_TARFILE
#$BUILD_DIR/SPECS/condor.spec
#$BUILD_DIR/SPECS/filelist.txt

#############################################################################################################
  

#Getting version number
CONDOR_VERSION=${CONDOR_TARFILE##*condor-}
CONDOR_VERSION=${CONDOR_VERSION%%-*}

echo "Building RPM for Condor $CONDOR_VERSION-$OS"

echo "Moving $CONDOR_TARFILE to SOURCES"
cp $CONDOR_TARFILE $BUILD_DIR/SOURCES/
ls -l $BUILD_DIR/SOURCES/

echo "Updating SPEC file"
RPM_DATE=`date +"%a %b %d %Y"`
#Replace VERSION and SOURCE file in original spec file
sed < $BUILD_DIR/SPECS/condor.spec \
     "s|_VERSION_|$CONDOR_VERSION|g; \
     s|_REVISION_|$REVISION|g; \
     s|_DATE_|$RPM_DATE|g; \
     s|_TARFILE_|$CONDOR_TARFILE|g" > $BUILD_DIR/SPECS/condor.spec.new

mv $BUILD_DIR/SPECS/condor.spec.new $BUILD_DIR/SPECS/condor.spec

echo "Building RPM"
BUILD_ROOT=`pwd`/$BUILD_DIR
$RPM_CMD --define="_topdir $BUILD_ROOT" -bb $BUILD_DIR/SPECS/condor.spec

ARCH=`ls -1 $BUILD_DIR/RPMS/ | sed -e 's|\/||'`
RPM_FILE=`ls -1 $BUILD_DIR/RPMS/${ARCH}`

echo "Renaming RPM"
RPM_NAME=${RPM_FILE%%.$ARCH*}
RPM_NAME="$RPM_NAME.$OS.$ARCH.rpm"

echo Orignal RPM: $RPM_FILE
echo Renamed RPM: $RPM_NAME

#Rename and throw result rpm back to parent folder
mv $BUILD_DIR/RPMS/$ARCH/$RPM_FILE ../$RPM_NAME


