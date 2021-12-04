Summary: Batch Local ASCII Helper Protocol suite
Name: glite-ce-blahp
Version: %{extversion}
Release: %{extage}.%{extdist}
License: Apache Software License
Vendor: EMI
URL: http://glite.cern.ch/
Group: Applications/Internet
BuildArch: %{_arch}
BuildRequires: libtool, classads-devel, docbook-style-xsl, libxslt
Requires(post): chkconfig
Requires(preun): chkconfig
Requires(preun): initscripts
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
AutoReqProv: yes
Source: %{name}-%{version}-%{release}.tar.gz

%global debug_package %{nil}

%description
The BLAHP daemon is a light component accepting commands to manage jobs 
on different Local Resources Management Systems

%prep
 

%setup -c -q

%build
%{!?extbuilddir:%define extbuilddir "--"}
if test "x%{extbuilddir}" == "x--" ; then
  ./configure --prefix=%{buildroot}/usr --sysconfdir=%{buildroot}/etc --disable-static PVER=%{version}
  make
fi

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
%{!?extbuilddir:%define extbuilddir "--"}
if test "x%{extbuilddir}" == "x--" ; then
  make install
else
  cp -R %{extbuilddir}/* %{buildroot}
fi
strip -s %{buildroot}/usr/sbin/blah_job_registry_*
strip -s %{buildroot}/usr/sbin/blahpd_daemon
strip -s %{buildroot}/usr/sbin/blah_check_config
strip -s %{buildroot}/usr/libexec/blparser_master
strip -s %{buildroot}/usr/libexec/BLClient
strip -s %{buildroot}/usr/libexec/BUpdater*
strip -s %{buildroot}/usr/libexec/BNotifier
strip -s %{buildroot}/usr/libexec/BLParser*
strip -s %{buildroot}/usr/bin/blahpd


%clean
rm -rf %{buildroot} 

%post

if [ $1 -eq 1 ] ; then
    /sbin/chkconfig --add glite-ce-blah-parser

    if [ ! "x`grep tomcat /etc/passwd`" == "x" ] ; then
        mkdir -p /var/log/cream/accounting
        chown root.tomcat /var/log/cream/accounting
        chmod 0730 /var/log/cream/accounting
        
        mkdir -p /var/blah
        chown tomcat.tomcat /var/blah
        chmod 771 /var/blah
    
    fi
fi

%preun
if [ $1 -eq 0 ] ; then
    /sbin/service glite-ce-blah-parser stop >/dev/null 2>&1
    /sbin/chkconfig --del glite-ce-blah-parser

    if [ -d /var/log/cream/accounting ] ; then
        rm -rf /var/log/cream/accounting 
    fi
    
    if [ -d /var/blah ] ; then
        rm -rf /var/blah
    fi
fi

%files
%defattr(-,root,root)
%config(noreplace) /etc/blparser.conf.template
%config(noreplace) /etc/blah.config.template
%dir /etc/rc.d/init.d/
/etc/rc.d/init.d/glite-ce-*
/usr/libexec/*
/usr/sbin/*
/usr/bin/*
%dir /usr/share/doc/glite-ce-blahp-%{version}/
%doc /usr/share/doc/glite-ce-blahp-%{version}/LICENSE
%doc /usr/share/man/man1/*.1.gz

%changelog
* %{extcdate} CREAM group <cream-support@lists.infn.it> - %{extversion}-%{extage}.%{extdist}
- %{extclog}

