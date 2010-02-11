Summary: Condor: High Throughput Computing
Name: condor
Version: _VERSION_
Release: _REVISION_
License: Apache License, Version 2.0
Group: Applications/System
Vendor: Condor Project
Packager: Condor Project
URL: http://www.cs.wisc.edu/condor/
Source0: _TARFILE_
#Additional files
#condor_config.local  <= condor_config.local.generic
#condor_config        <= condor_config.generic + condor_config.generic.rpm.patch

#Note:
#Condor original tarfile must be place in SOURCES folder together with others SourceX files.
#We will extract tarfile to BUILD folder and create _topdir/root/ and use it as BuildRoot.
#After that we will move files from BUILD to BuildRoot.
#If there is files that we do not specify in %files section, build process will fail.
#Except /usr/bin and /usr/sbin which are generate automatically

#Prevent brp-java-repack-jars from being run.
#%define __jar_repack %{nil} 

#Packaging will pass even if we don't specify everything in files section
#%define _unpackaged_files_terminate_build 0

Prefix: /etc
Prefix: /usr
Prefix: /var
BuildRoot: %{_topdir}/root

#BuildRequires: imake
#BuildRequires: flex
#Requires: gsoap >= 2.7.12-1
#Requires: mailx
#Requires: python >= 2.2

Requires(pre): shadow-utils

Requires(post):/sbin/chkconfig
Requires(preun):/sbin/chkconfig
Requires(preun):/sbin/service
Requires(postun):/sbin/service

Obsoletes: condor-static < 7.2.0

%description
Condor is a specialized workload management system for
compute-intensive jobs. Like other full-featured batch systems, Condor
provides a job queueing mechanism, scheduling policy, priority scheme,
resource monitoring, and resource management. Users submit their
serial or parallel jobs to Condor, Condor places them into a queue,
chooses when and where to run the jobs based upon a policy, carefully
monitors their progress, and ultimately informs the user upon
completion.


%pre
#Add condor group if not existed
getent group condor >/dev/null || groupadd -r condor

#Add condor user if not existed
getent passwd condor >/dev/null || \
  useradd -r -g condor -d %_var/lib/condor -s /sbin/nologin \
    -c "Owner of Condor Daemons" condor

if [ "$1" -ge "2" ]; then
  #Stopping condor if there is existing version
  /sbin/service condor stop >/dev/null 2>&1 || :
fi


exit 0


%prep

# Extract binaries folder
PREFIX=%_builddir/%name-%version
rm -rf $PREFIX

#Extract binaries and remove tar file
cd %_builddir
cp %{SOURCE0} .
source_file=%{SOURCE0}
tar xzf ${source_file##*/}
rm ${source_file##*/}

#Create local folder
cd %name-%version
mkdir -p -m1777 local/execute
mkdir -p -m0755 local/log
mkdir -p -m0755 local/spool


#Patching files
#Use VDT init script, modify permission
chmod 755 etc/examples/condor.boot.vdt

#Prepare default configuration files
mkdir -p -m0755 etc/condor
cp etc/examples/condor_config.local.generic  etc/condor/condor_config.local
patch etc/examples/condor_config.generic etc/examples/condor_config.generic.rpm.patch -o etc/examples/condor_config.generic.new

#Patch condor_config for 32 or 64 bit system
#From Fedora's SPEC
#Replace "lib" with "lib64" if RPM detect 64 
LIB=$(echo %{?_libdir} | sed -e 's:/usr/\(.*\):\1:')
if [ "$LIB" = "%_libdir" ]; then
  echo "_libdir does not contain /usr, sed expression needs attention"
  exit 1
fi
sed -e "s:^LIB\s*=.*:LIB = \$(RELEASE_DIR)/$LIB/condor:" \
  etc/examples/condor_config.generic.new > etc/condor/condor_config

#Fixing softlinks

cp --remove-destination $PREFIX/src/chirp/chirp_client.h $PREFIX/include

%build

%install

#Detect system's arch so we can place in /usr/lib or /usr/lib64
LIB=$(echo %{?_libdir} | sed -e 's:/usr/\(.*\):\1:')
if [ "$LIB" = "%_libdir" ]; then
  echo "_libdir does not contain /usr, sed expression needs attention"
  exit 1
fi


function move {
  _src="$1"; shift; _dest="$*"  
  _dest_dir=$(dirname $_dest)
  mkdir -p "%{buildroot}$_dest_dir"
  #Treat target folder as normal file
  mv $_src "%{buildroot}$_dest"

}

# Extracted binaries folder
PREFIX=%_builddir/%name-%version
echo $PREFIX

#Clean existing root folder

rm -rf %{buildroot}
mkdir -p %{buildroot}

#Clean up install links
rm -f $PREFIX/condor_configure $PREFIX/condor_install

# Relocate main path layout
move $PREFIX/bin				/usr/bin			
move $PREFIX/etc/condor				/etc/condor
move $PREFIX/etc/examples/condor.boot.vdt	/etc/init.d/condor
move $PREFIX/include				/usr/include/condor	
move $PREFIX/lib				/usr/$LIB/condor		
move $PREFIX/libexec				/usr/libexec/condor	
move $PREFIX/local				/var/lib/condor		
move $PREFIX/man				/usr/share/man
move $PREFIX/sbin				/usr/sbin			
move $PREFIX/sql				/usr/share/condor/sql	
move $PREFIX/src				/usr/src

#Create RUN LOG LOCK 
mkdir -p -m0755 "%{buildroot}"/var/run/condor
mkdir -p -m0755 "%{buildroot}"/var/log/condor
mkdir -p -m0755 "%{buildroot}"/var/lock/condor

#Put the rest into documentation
move $PREFIX				/usr/share/doc/%{name}-%{version}


#Generating file list for /usr/bin and /usr/sbin
FILELIST=%{_topdir}/SPECS/filelist.txt

#Clean previous entries (except 1 line which define default attribute
head -n 1 $FILELIST > $FILELIST.new
mv $FILELIST.new $FILELIST

#Filling file list
find %{buildroot}/usr/bin -type f >> $FILELIST
find %{buildroot}/usr/sbin -type f >> $FILELIST

#Modify file path
sed < $FILELIST \
     "s|%{buildroot}/usr/bin|%_bindir|g; \
      s|%{buildroot}/usr/sbin|%_sbindir|g" > $FILELIST.new

mv $FILELIST.new $FILELIST

%clean

%check

%files -f %{_topdir}/SPECS/filelist.txt

#filelist.txt contains the following sections
#/usr/bin
#/usr/sbin (_sbindir/ = /usr/sbin)

#Configuration scripts
%defattr(-,root,root,-)
%dir %_sysconfdir/condor/
%config(noreplace) %_sysconfdir/condor/condor_config
%config(noreplace) %_sysconfdir/condor/condor_config.local

#Init script
%defattr(-,root,root,-)
%_sysconfdir/init.d/condor


#/usr/include/condor (_includedir/ = /usr/include)
%dir %_includedir/condor/
%_includedir/condor/*

#/usr/lib/condor (_libdir/ = /usr/lib | /usr/lib64)
%dir %_libdir/condor/
%_libdir/condor/*

#/usr/libexec/condor (_libexecdir/ = /usr/libexec)
%dir %_libexecdir/condor/
%_libexecdir/condor/*

#/usr/man/man1 (_mandir/ = /usr/man)
%_mandir/man1/condor_*

#/usr/share/condor (_datadir/ = /usr/share)
%dir %_datadir/condor/
%_datadir/condor/*

#Documentation (/usr/share/doc/%{name}-%{version})
%dir %_datadir/doc/%{name}-%{version}
%_datadir/doc/%{name}-%{version}/*

#/usr/src/ (_usrsrc/ = /usr/src)
%dir %_usrsrc/chirp/
%dir %_usrsrc/drmaa/
%dir %_usrsrc/startd_factory/
%_usrsrc/chirp/*
%_usrsrc/drmaa/*
%_usrsrc/startd_factory/*

#/var (_var/ = /var)
%defattr(-,condor,condor,-)
%dir %_var/lib/condor/
%dir %_var/lib/condor/execute/
%dir %_var/lib/condor/spool/
%dir %_var/log/condor/
%dir %_var/lock/condor/
%dir %_var/run/condor/


%post -n condor

#Get relocated prefix
ETC=$RPM_INSTALL_PREFIX0
USR=$RPM_INSTALL_PREFIX1
VAR=$RPM_INSTALL_PREFIX2

#Patch config file if relocated

if [ $USR != "/usr" ] ; then
  perl -p -i -e "s:^CONDOR_CONFIG_VAL=.*:CONDOR_CONFIG_VAL=$USR/bin/condor_config_val:" $ETC/init.d/condor 
  perl -p -i -e "s:^RELEASE_DIR(\s*)=.*:RELEASE_DIR\$1= $USR:" $ETC/condor/condor_config   
fi

if [ $VAR != "/var" ] ; then
  perl -p -i -e "s:^LOCAL_DIR(\s*)=.*:LOCAL_DIR\$1= $VAR:" $ETC/condor/condor_config   
fi

if [ $ETC != "/etc" ] ; then
  perl -p -i -e "s:^CONDOR_CONFIG=.*:CONDOR_CONFIG=$ETC/condor/condor_config:" $ETC/init.d/condor 
  perl -p -i -e "s:^LOCAL_CONFIG_FILE(\s*)=\s*/etc(.*):LOCAL_CONFIG_FILE\$1= $ETC\$2:" $ETC/condor/condor_config 
  
  #Install init.d script
  cp -f $ETC/init.d/condor /etc/init.d/condor  
fi


#Add condor service
/sbin/chkconfig --add condor
/sbin/ldconfig
test -x /usr/sbin/selinuxenabled && /usr/sbin/selinuxenabled
if [ $? = 0 ]; then
   semanage fcontext -a -t unconfined_execmem_exec_t $USR/sbin/condor_startd 2>&1| grep -v "already defined"
   restorecon  $USR/sbin/condor_startd
fi
exit 0


%preun -n condor

#Stop condor
/sbin/service condor stop >/dev/null 2>&1 || :
if [ $1 = 0 ]; then
  
  /sbin/chkconfig --del condor
  
  #Remove init.d if relocated
  ETC=$RPM_INSTALL_PREFIX0
  if [ $ETC != "/etc" ] ; then    
    rm /etc/init.d/condor
  fi

fi


%postun -n condor
#if [ "$1" -ge "1" ]; then
  #Upgrading or remove but other version existed 
  #/sbin/service condor restart >/dev/null 2>&1 || :
#fi
/sbin/ldconfig


%changelog
* _DATE_  <condor-users@cs.wisc.edu> - _VERSION_-_REVISION_
- Please see version history at http://www.cs.wisc.edu/condor/manual/v_VERSION_/8_Version_History.html

* Sun Jan 24 2010  <kooburat@cs.wisc.edu> - 7.5.0-2
- Make RPM relocatable and support multiple version install

* Fri Nov 13 2009  <kooburat@cs.wisc.edu> - 7.4.0-1
- Initial release based on Fedora's RPM by <matt@redhat>
