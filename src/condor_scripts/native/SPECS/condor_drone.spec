Summary: Condor: High Throughput Computing - (Drone)
Name: condor-drone
Version: _VERSION_
Release: _REVISION_
License: Apache License, Version 2.0
Group: Applications/System
Vendor: Condor Project
Packager: Condor Project
BuildArch: noarch
URL: http://www.cs.wisc.edu/condor/
Source0: master_wrapper
Source1: condor.boot.rpm
Source2: condor.sh
Source3: condor.csh
Source4: condor.sysconfig
Source5: condor.sysconfig.drone.patch

#Note:
#This Condor package is intended to install on worker node where 
#Condor binaries can be accessed via a shared file system.

#Relocation is not supported 
#Prefix: /etc
#Prefix: /usr
#Prefix: /var

BuildRoot: %{_topdir}/root


Requires(pre): shadow-utils

Requires(post):/sbin/chkconfig
Requires(preun):/sbin/chkconfig
Requires(preun):/sbin/service
Requires(postun):/sbin/service
Conflicts: condor


%description
Condor is a specialized workload management system for
compute-intensive jobs. Like other full-featured batch systems, Condor
provides a job queueing mechanism, scheduling policy, priority scheme,
resource monitoring, and resource management. Users submit their
serial or parallel jobs to Condor, Condor places them into a queue,
chooses when and where to run the jobs based upon a policy, carefully
monitors their progress, and ultimately informs the user upon
completion.
Note: This Condor package is intended to install on worker node where 
Condor binaries can be accessed via a shared file system.


%pre
#Add condor group if not existed
getent group condor >/dev/null || groupadd -r condor

#Add condor user if not existed
getent passwd condor >/dev/null || \
  useradd -r -g condor -d %_var/lib/condor -s /sbin/nologin \
    -c "Owner of Condor Daemons" condor

exit 0


%prep

#Mark scripts as executable
chmod 755 %{SOURCE0}
chmod 755 %{SOURCE1}


#Source4: condor.sysconfig
#Source5: condor.sysconfig.drone.patch
patch %{SOURCE4} %{SOURCE5} -o %{SOURCE4}.drone
chmod 755 %{SOURCE4}.drone

%build

%install

function move {
  _src="$1"; shift; _dest="$*"  
  _dest_dir=$(dirname $_dest)
  mkdir -p "%{buildroot}$_dest_dir"
  #Treat target folder as normal file
  cp $_src "%{buildroot}$_dest"

}


#Clean existing root folder
rm -rf %{buildroot}
mkdir -p %{buildroot}


#Source0: condor_master_wrapper
#Source1: condor.boot.vdt
#Source2: condor.sh
#Source3: condor.csh
#Source4: condor.sysconfig

# Relocate main path layout
move %{SOURCE0}					/usr/sbin/condor_master_wrapper
move %{SOURCE1}					/etc/init.d/condor
move %{SOURCE2}					/etc/profile.d/condor.sh
move %{SOURCE3}					/etc/profile.d/condor.csh
move %{SOURCE4}.drone				/etc/sysconfig/condor

#Create RUN LOG LOCK LOCAL
mkdir -p -m0755 "%{buildroot}"/var/run/condor
mkdir -p -m0755 "%{buildroot}"/var/log/condor
mkdir -p -m0755 "%{buildroot}"/var/lock/condor
mkdir -p -m0755 "%{buildroot}"/var/lib/condor/spool
mkdir -p -m0755 "%{buildroot}"/var/lib/condor/log
mkdir -p -m1777 "%{buildroot}"/var/lib/condor/execute

%clean

%check

%files 

#/usr/sbin (_sbindir/ = /usr/sbin)
%_sbindir/condor_master_wrapper

#Init script and sysconfig
%defattr(-,root,root,-)
%config(noreplace) %_sysconfdir/init.d/condor
%config(noreplace) %_sysconfdir/sysconfig/condor

#Environment variable script
%_sysconfdir/profile.d/condor.sh
%_sysconfdir/profile.d/condor.csh

#/var (_var/ = /var)
%defattr(-,condor,condor,-)
%dir %_var/lib/condor/
%dir %_var/lib/condor/execute/
%dir %_var/lib/condor/spool/
%dir %_var/lib/condor/log
%dir %_var/log/condor/
%dir %_var/lock/condor/
%dir %_var/run/condor/


%post 

#Add condor service
/sbin/chkconfig --add condor


%preun

#Stop and remove condor only when this is uninstalling
if [ $1 = 0 ]; then
  #This should fail if it is unable to stop condor in timelimit
  /sbin/service condor stop
  if [ $? = 1 ]; then
     echo "Abort uninstallation"
     exit 1;
  fi

  if [ -e /etc/init.d/condor ]; then
     /sbin/chkconfig --del condor
  fi  

fi


%postun 


%changelog
* _DATE_  <condor-users@cs.wisc.edu> - _VERSION_-_REVISION_
- Please see version history at http://www.cs.wisc.edu/condor/manual/v_VERSION_/8_Version_History.html
