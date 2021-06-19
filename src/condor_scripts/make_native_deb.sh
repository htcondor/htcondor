#!/bin/bash

###############################################################################
# Input parameters
###############################################################################
#Usage
#condor_dep.sh <tarfile> <arch> [<revison>]

#Original tarfile
SOURCE0=$1
#SOURCE0=condor-7.4.0-linux-x86-debian50-dynamic.tar.gz

#Additional files
#condor_config.local  <= condor_config.local.generic
#condor_config        <= condor_config.generic + condor_config.generic.debian.patch

#Default revision is 1
REVISION=${2:-"1"}

ARCH=`dpkg-architecture -qDEB_HOST_ARCH`
#ARCH=i386
CODENAME=`lsb_release -cs`
#CODENAME=lenny

###############################################################################
# Script variables
###############################################################################

#Folder structure during before process
#$BUILD_ROOT/DEBIAN			Contains input files for building
#$BUILD_ROOT/$SOURCE0			Condor tarfile

#Folder structure during build process
#$BUILD_ROOT/				Contains input files for building
#$BUILD_ROOT/debroot			Build area	
#$BUILD_ROOT/debroot/DEBIAN		Debian control folder
#$BUILD_ROOT/debroot/etc
#$BUILD_ROOT/debroot/usr
#$BUILD_ROOT/debroot/var

BUILD_ROOT=`pwd`

#Root of package file
DEB_ROOT=$BUILD_ROOT/debroot

#Debian control folder
META_DIR=$DEB_ROOT/DEBIAN

#Control file
CONTROL=$DEB_ROOT/DEBIAN/control

#Folder containing all patches files
SOURCE_DIR=$BUILD_ROOT

#Extract condor version from input tarfile
VERSION=${SOURCE0##*condor-}
VERSION=${VERSION%%-*}

#Convert to absolute path
SOURCE0=$SOURCE_DIR/$SOURCE0

#Result file name
DEB_FILE_NAME="condor_"$VERSION-$REVISION$CODENAME"_$ARCH.deb"

echo "Building Debian package for Condor $VERSION-$REVISION on $ARCH"

#Prepare package area
rm -rf $DEB_ROOT
mkdir -p $DEB_ROOT

mv DEBIAN $DEB_ROOT/DEBIAN

#Make sure control files has correct permission
chmod 755 $META_DIR/pre*
chmod 755 $META_DIR/post*

############################################################################################
#Updating timestamp in changelog and copy right file
############################################################################################
echo "Generating changelog and copyright file"
echo

DEB_DATE=`date -R`
DEB_YEAR=`date +%Y`

#Changelog
sed < $META_DIR/changelog \
     "s|_VERSION_|$VERSION-$REVISION|g; \
      s|_DATE_|$DEB_DATE|g; \
      s|_DIST_|$CODENAME|g; \
      s|_CONDORVERSION_|$VERSION|g" > $META_DIR/changelog.new

mv $META_DIR/changelog.new $META_DIR/changelog

#Copyright
sed < $META_DIR/copyright \
     "s|_DATE_|$DEB_DATE|g; \
      s|_YEAR_|$DEB_YEAR|g" > $META_DIR/copyright.new

mv $META_DIR/copyright.new $META_DIR/copyright

############################################################################################


echo "Current content in DEBIAN folder"
ls -l $META_DIR

###############################################################################
# Extract files, prepare source and fix some file-related issue
###############################################################################
echo "Extracting files"
echo
# Remove existing folder
PREFIX=$SOURCE_DIR/condor-$VERSION
rm -rf $PREFIX

#Extract binaries and remove tar file
cd $SOURCE_DIR
tar xzf ${SOURCE0##*/}
#rm $SOURCE0

#Create local folder
cd $PREFIX
mkdir -p -m1777 local/execute
mkdir -p -m0755 local/log
mkdir -p -m0755 local/spool

#Patching files
#Use rpm init script
chmod 755 etc/examples/condor.boot.rpm
#Debian use /etc/default instead of /etc/sysconfig
perl -p -i -e "s|/etc/sysconfig/condor|/etc/default/condor|g;" etc/examples/condor.boot.rpm


#Fix permission
chmod 644 etc/examples/condor.boot
chmod 644 etc/examples/mp1script
chmod 755 src/drmaa/configure

#Prepare default configuration files
mkdir -p -m0755 etc/condor
cp etc/examples/condor_config.local.generic etc/condor/condor_config.local
patch etc/examples/condor_config.generic etc/examples/condor_config.generic.debian.patch -o etc/condor/condor_config
chmod 644 etc/condor/condor_config

#No need to patch condor_config because debian use /usr/lib for both 32 and 64 bit

#Fixing softlinks
cp --remove-destination $PREFIX/src/chirp/chirp_client.h $PREFIX/include

#Remove execute permission from file type .jar .class .wsdl .xsd
cd $PREFIX
find lib/ -name \*.jar -o -name \*.wsdl -o -name \*.class -o -name \*.xsd | xargs chmod a-x

#Strip some unstripped binaries (These options are from dh_strip)
#Mostly they are 3rd-party component
#Executables
find lib/glite/bin/ -type f -not -name \*.sh | xargs strip --remove-section=.comment --remove-section=.note
#Shared lib
find lib/ -type f -name \*.so | xargs strip --remove-section=.comment --remove-section=.note
find libexec/ -type f -name *server | xargs strip --remove-section=.comment --remove-section=.note



###############################################################################
# Relocate files
###############################################################################

function move {  
  _src="$1"; shift; _dest="$*"  
  _dest_dir=$(dirname $_dest)
  mkdir -p "$DEB_ROOT$_dest_dir"
  mv $_src "$DEB_ROOT$_dest"
  echo "Source: $_src"
  echo "Target: $DEB_ROOT$_dest"
  echo
}

echo "Relocating files"
echo

#Clean existing root folder
rm -rf $DEB_ROOT/etc $DEB_ROOT/usr $DEB_ROOT/var

#Clean up install links
rm -f $PREFIX/condor_configure $PREFIX/condor_install

# Relocate main path layout
move $PREFIX/bin				/usr/bin			
move $PREFIX/etc/condor				/etc/condor
move $PREFIX/etc/examples/condor.sysconfig	/etc/default/condor
move $PREFIX/etc/examples/condor.boot.rpm	/etc/init.d/condor
move $PREFIX/include				/usr/include/condor	
move $PREFIX/lib				/usr/lib/condor		
move $PREFIX/libexec				/usr/lib/condor/libexec
move $PREFIX/local				/var/lib/condor		
move $PREFIX/man				/usr/share/man
move $PREFIX/sbin				/usr/sbin			
move $PREFIX/sql				/usr/share/condor/sql	
move $PREFIX/src				/usr/src


#Create RUN LOG LOCK CONFIG.D
mkdir -p -m0755 $DEB_ROOT/var/run/condor
mkdir -p -m0755 $DEB_ROOT/var/log/condor
mkdir -p -m0755 $DEB_ROOT/var/lock/condor
mkdir -p -m0755 $DEB_ROOT/etc/condor/config.d

#Put the rest into documentation
move $PREFIX				/usr/share/doc/condor

#Move metadata files
move $META_DIR/copyright		/usr/share/doc/condor
move $META_DIR/changelog		/usr/share/doc/condor/changelog.Debian

gzip -9 $DEB_ROOT/usr/share/doc/condor/changelog.Debian

#Compress manpages
cd $DEB_ROOT/usr/share/man/man1
ls | xargs gzip -9


echo "Preparing metadata files"

############################################################################################
#Generate md5sum
############################################################################################
echo "Generating md5sums"
echo
cd $DEB_ROOT

(find . -type f  ! -regex '.*/DEBIAN/.*' -printf '%P\0' | xargs -r0 md5sum > DEBIAN/md5sums) >/dev/null


############################################################################################
#Generate dependency list
############################################################################################
echo "Generating dependency list"
echo 
#dpkg-shlibdeps require control file at this location
mkdir debian
cp $CONTROL debian/

#List all file in /usr/bin, /usr/sbin and /usr/lib
find . -type f -wholename "*usr/bin/*" -o -wholename "*usr/sbin/*" -o -wholename "*usr/lib/*" > temp1.txt

#Filter possible not executable file out
cat temp1.txt | grep -v '.sh\|.jar\|.xml\|.wsdd\|.class\|.wsdl' > temp2.txt 

#Check if file is executable and dynamically linked
for file_name in `cat temp2.txt`
do  
  if file $file_name | grep 'ELF' | grep 'dynamically linked' &> /dev/null
  then
    echo -n "$file_name " >> filelist
    #echo "$file_name"
  fi
done

#Generate dependency list from filelist
echo "Calling dpkg-shlibdeps"
echo 
filelist=`cat filelist`
dpkg-shlibdeps $filelist

#Read depedency list from output
DEPENDS=`cat debian/substvars`
DEPENDS=${DEPENDS#*=}


#Remove temp files
rm temp1.txt temp2.txt filelist
rm -rf debian

#Append other dependecy 
DEPENDS="$DEPENDS, python, adduser"

#Print dependecy list
echo "Dependency list: $DEPENDS"
echo 

#Update dependency list into control file
sed < $CONTROL \
     "s|^Depends:.*|Depends: $DEPENDS|" > $CONTROL.new

mv $CONTROL.new $CONTROL

############################################################################################
#Modify version, architecture and installed-size
############################################################################################

#Get build folder size in kilobytes
INSTALL_SIZE=`du -sk $DEB_ROOT | awk '{print $1}'`

echo "Estimated install size: $INSTALL_SIZE KB"
echo

sed < $CONTROL \
     "s|^Version:.*|Version: $VERSION-$REVISION|; \
      s|^Installed-Size:.*|Installed-Size: $INSTALL_SIZE|; \
      s|^Architecture:.*|Architecture: $ARCH|" > $CONTROL.new
mv $CONTROL.new $CONTROL

############################################################################################
#Building package
############################################################################################

echo "Building package"
cd $BUILD_ROOT
fakeroot dpkg-deb --build $DEB_ROOT $DEB_FILE_NAME

#This might return error if package is not properly created
#Indirect way of verifing the package
echo "Package Summary"
echo 
dpkg-deb -I $DEB_FILE_NAME
echo 

#Clean up
rm -rf $DEB_ROOT

#Throw result deb file to parent folder.
mv $DEB_FILE_NAME ../
