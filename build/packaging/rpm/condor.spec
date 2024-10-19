%define condor_version 1.0.0

# On EL7 don't terminate the build because of bad bytecompiling
%if 0%{?rhel} == 7
%define _python_bytecompile_errors_terminate_build 0
%endif

# Don't use changelog date in CondorVersion
%global source_date_epoch_from_changelog 0

# set uw_build to 0 for downstream (Fedora or EPEL)
# UW build includes stuff for testing and tarballs
%define uw_build 0

# Use devtoolset 11 for EL7
%define devtoolset 11
# Use gcc-toolset 13 for EL8 and later
%define gcctoolset 13

Summary: HTCondor: High Throughput Computing
Name: condor
Version: %{condor_version}
%global version_ %(tr . _ <<< %{version})

%if 0%{?suse_version}
%global _libexecdir %{_exec_prefix}/libexec
%if %{suse_version} == 1500
%global dist .leap15
%endif
%endif

# Edit the %condor_release to set the release number
%define condor_release 1
Release: %{condor_release}%{?dist}

License: Apache-2.0
Group: Applications/System
URL: https://htcondor.org/

# Do not check .so files in condor's library directory
%global __provides_exclude_from ^%{_libdir}/%{name}/.*\\.so.*$

# Do not provide libfmt
%global __requires_exclude ^libfmt\\.so.*$

Source0: %{name}-%{condor_version}.tar.gz

Source8: htcondor.pp

# Patch credmon-oauth to use Python 2 on EL7
Patch1: rhel7-python2.patch

BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires: cmake
BuildRequires: pcre2-devel
BuildRequires: openssl-devel
BuildRequires: krb5-devel
%if ! 0%{?amzn}
BuildRequires: libvirt-devel
%endif
BuildRequires: bind-utils
BuildRequires: libX11-devel
BuildRequires: libXScrnSaver-devel
%if 0%{?suse_version}
BuildRequires: openldap2-devel
%else
BuildRequires: openldap-devel
%endif
%if 0%{?rhel} == 7
BuildRequires: cmake3
BuildRequires: python-devel
BuildRequires: python-setuptools
%else
BuildRequires: cmake >= 3.16
%endif
BuildRequires: python3-devel
BuildRequires: python3-setuptools
%if 0%{?rhel} >= 8
BuildRequires: boost-devel
%endif
%if 0%{?suse_version}
BuildRequires: rpm-config-SUSE
%else
BuildRequires: redhat-rpm-config
%endif
BuildRequires: sqlite-devel
BuildRequires: perl(Data::Dumper)

BuildRequires: glibc-static
BuildRequires: gcc-c++
BuildRequires: libuuid-devel
BuildRequires: patch
BuildRequires: pam-devel
%if 0%{?suse_version}
BuildRequires: mozilla-nss-devel
%else
BuildRequires: nss-devel
%endif
BuildRequires: openssl-devel
BuildRequires: libxml2-devel
%if 0%{?suse_version}
BuildRequires: libexpat-devel
%else
BuildRequires: expat-devel
%endif
BuildRequires: perl(Archive::Tar)
BuildRequires: perl(XML::Parser)
BuildRequires: perl(Digest::MD5)
%if 0%{?rhel} >= 8 || 0%{?fedora} || 0%{?suse_version}
BuildRequires: python3-devel
%else
BuildRequires: python-devel
%endif
BuildRequires: libcurl-devel

# Authentication build requirements
%if ! 0%{?amzn}
BuildRequires: voms-devel
%endif
BuildRequires: munge-devel
BuildRequires: scitokens-cpp-devel

%if 0%{?rhel} == 7 && 0%{?devtoolset}
BuildRequires: which
BuildRequires: devtoolset-%{devtoolset}-toolchain
%endif

%if 0%{?rhel} >= 8 && 0%{?gcctoolset}
BuildRequires: which
BuildRequires: gcc-toolset-%{gcctoolset}
%endif

%if  0%{?suse_version}
BuildRequires: gcc11
BuildRequires: gcc11-c++
%endif

%if 0%{?rhel} == 7 && ! 0%{?amzn}
BuildRequires: python36-devel
BuildRequires: boost169-devel
BuildRequires: boost169-static
%endif

%if 0%{?rhel} >= 8
BuildRequires: boost-static
%endif

%if 0%{?rhel} == 7 && ! 0%{?amzn}
BuildRequires: python3-devel
BuildRequires: boost169-python2-devel
BuildRequires: boost169-python3-devel
%else
%if  0%{?suse_version}
BuildRequires: libboost_python-py3-1_75_0-devel
%else
BuildRequires: boost-python3-devel
%endif
%endif
BuildRequires: libuuid-devel
%if 0%{?suse_version}
Requires: libuuid1
%else
Requires: libuuid
%endif

BuildRequires: systemd-devel
%if 0%{?suse_version}
BuildRequires: systemd
%else
BuildRequires: systemd-units
%endif
Requires: systemd

%if 0%{?rhel} == 7
BuildRequires: python36-sphinx python36-sphinx_rtd_theme
%endif

%if 0%{?rhel} >= 8 || 0%{?amzn} || 0%{?fedora}
BuildRequires: python3-sphinx python3-sphinx_rtd_theme
%endif

%if 0%{?suse_version}
BuildRequires: python3-Sphinx python3-sphinx_rtd_theme
%endif

# openssh-server needed for condor_ssh_to_job
Requires: openssh-server

# net-tools needed to provide netstat for condor_who
Requires: net-tools

# Perl modules required for condor_gather_info
Requires: perl(Date::Manip)
Requires: perl(FindBin)

# cryptsetup needed for encrypted LVM execute partitions
Requires: cryptsetup

Requires: /usr/sbin/sendmail

# Useful tools are using the Python bindings
Requires: python3-condor = %{version}-%{release}
# The use the python-requests library in EPEL is based Python 3.6
# However, Amazon Linux 2 has Python 3.7
%if ! 0%{?amzn}
%if 0%{?rhel} == 7
Requires: python36-requests
%else
Requires: python3-requests
%endif
%endif

%if 0%{?rhel} == 7
Requires: python2-condor = %{version}-%{release}
# For some reason OSG VMU tests need python-request
Requires: python-requests
%endif

Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%if 0%{?suse_version}
Requires(pre): shadow
Requires(post): systemd
Requires(preun): systemd
Requires(postun): systemd
%else
Requires(pre): shadow-utils
Requires(post): systemd-units
Requires(preun): systemd-units
Requires(postun): systemd-units
%endif

%if 0%{?rhel} == 7
Requires(post): systemd-sysv
Requires(post): policycoreutils-python
Requires(post): selinux-policy-targeted >= 3.13.1-102
%endif

%if 0%{?rhel} >= 8 || 0%{?fedora} || 0%{?suse_version}
Requires(post): python3-policycoreutils
%if ! 0%{?suse_version}
Requires(post): selinux-policy-targeted
%endif
%endif

# Require libraries that we dlopen
# Ganglia is optional as well as nVidia and cuda libraries
%if ! 0%{?amzn}
%if 0%{?suse_version}
Requires: libvomsapi1
%else
Requires: voms
%endif
%endif
%if 0%{?suse_version}
Requires: krb5
Requires: libcom_err2
Requires: libmunge2
Requires: libopenssl1_1
Requires: libSciTokens0
Requires: libsystemd0
%else
Requires: krb5-libs
Requires: libcom_err
Requires: munge-libs
Requires: openssl-libs
Requires: scitokens-cpp >= 0.6.2
Requires: systemd-libs
%endif
Requires: rsync
Requires: condor-upgrade-checks

# Support OSDF client
%if 0%{?rhel} == 7
Requires: pelican-osdf-compat >= 7.1.4
%else
Recommends: pelican-osdf-compat >= 7.1.4
%endif

%if 0%{?rhel} != 7
# Ensure that our bash completions work
Recommends: bash-completion
%endif

#From /usr/share/doc/setup/uidgid (RPM: setup-2.12.2-11)
#Provides: user(condor) = 64
#Provides: group(condor) = 64

%if 0%{?rhel} == 7
# Standard Universe discontinued as of 8.9.0
Obsoletes: %{name}-std-universe < 8.9.0
Provides: %{name}-std-universe = %{version}-%{release}

# Cream gahp discontinued as of 8.9.9
Obsoletes: %{name}-cream-gahp < 8.9.9
Provides: %{name}-cream-gahp = %{version}-%{release}

# 32-bit shadow discontinued as of 8.9.9
Obsoletes: %{name}-small-shadow < 8.9.9
Provides: %{name}-small-shadow = %{version}-%{release}
%endif

%if 0%{?rhel} <= 8
# external-libs package discontinued as of 8.9.9
Obsoletes: %{name}-external-libs < 8.9.9
Provides: %{name}-external-libs = %{version}-%{release}

# Bosco package discontinued as of 9.5.0
Obsoletes: %{name}-bosco < 9.5.0
Provides: %{name}-bosco = %{version}-%{release}

# Blahp provided by condor-blahp as of 9.5.0
Provides: blahp = %{version}-%{release}
Obsoletes: blahp < 9.5.0
%endif

# externals package discontinued as of 10.8.0
Obsoletes: %{name}-externals < 10.8.0
Provides: %{name}-externals = %{version}-%{release}

# blahp package discontinued as of 10.8.0
Obsoletes: %{name}-blahp < 10.8.0
Provides: %{name}-blahp = %{version}-%{release}

# procd package discontinued as of 10.8.0
Obsoletes: %{name}-procd < 10.8.0
Provides: %{name}-procd = %{version}-%{release}

# all package discontinued as of 10.8.0
Obsoletes: %{name}-all < 10.8.0
Provides: %{name}-all = %{version}-%{release}

# classads package discontinued as of 10.8.0
Obsoletes: %{name}-classads < 10.8.0
Provides: %{name}-classads = %{version}-%{release}

# classads-devel package discontinued as of 10.8.0
Obsoletes: %{name}-classads-devel < 10.8.0
Provides: %{name}-classads-devel = %{version}-%{release}

%if 0%{?suse_version}
%debug_package
%endif

%description
HTCondor is a specialized workload management system for
compute-intensive jobs. Like other full-featured batch systems, HTCondor
provides a job queuing mechanism, scheduling policy, priority scheme,
resource monitoring, and resource management. Users submit their
serial or parallel jobs to HTCondor, HTCondor places them into a queue,
chooses when and where to run the jobs based upon a policy, carefully
monitors their progress, and ultimately informs the user upon
completion.

#######################
%package devel
Summary: Development files for HTCondor
Group: Applications/System

%description devel
Development files for HTCondor

%if %uw_build
#######################
%package tarball
Summary: Files needed to build an HTCondor tarball
Group: Applications/System

%description tarball
Files needed to build an HTCondor tarball

%endif

#######################
%package kbdd
Summary: HTCondor Keyboard Daemon
Group: Applications/System
Requires: %name = %version-%release

%description kbdd
The condor_kbdd monitors logged in X users for activity. It is only
useful on systems where no device (e.g. /dev/*) can be used to
determine console idle time.

#######################
%if ! 0%{?amzn}
%package vm-gahp
Summary: HTCondor's VM Gahp
Group: Applications/System
Requires: %name = %version-%release
Requires: libvirt

%description vm-gahp
The condor_vm-gahp enables the Virtual Machine Universe feature of
HTCondor. The VM Universe uses libvirt to start and control VMs under
HTCondor's Startd.

%endif

#######################
%package test
Summary: HTCondor Self Tests
Group: Applications/System
Requires: %name = %version-%release

%description test
A collection of tests to verify that HTCondor is operating properly.

#######################
%if 0%{?rhel} <= 7 && 0%{?fedora} <= 31
%package -n python2-condor
Summary: Python bindings for HTCondor
Group: Applications/System
Requires: python >= 2.2
Requires: python2-cryptography
Requires: %name = %version-%release
%{?python_provide:%python_provide python2-condor}
%if 0%{?rhel} >= 7
Requires: boost169-python2
%endif
# Remove before F30
Provides: %{name}-python = %{version}-%{release}
Provides: %{name}-python%{?_isa} = %{version}-%{release}
Obsoletes: %{name}-python < %{version}-%{release}

%description -n python2-condor
The python bindings allow one to directly invoke the C++ implementations of
the ClassAd library and HTCondor from python
%endif


%if 0%{?rhel} >= 7 || 0%{?fedora} || 0%{?suse_version}
#######################
%package -n python3-condor
Summary: Python bindings for HTCondor
Group: Applications/System
Requires: %name = %version-%release
%if 0%{?rhel} == 7
Requires: boost169-python3
%else
%if 0%{?suse_version}
Requires: libboost_python-py3-1_75_0
%else
Requires: boost-python3
%endif
%endif
Requires: python3
%if 0%{?rhel} != 7
Requires: python3-cryptography
%endif

%description -n python3-condor
The python bindings allow one to directly invoke the C++ implementations of
the ClassAd library and HTCondor from python
%endif


#######################
%package credmon-local
Summary: Local issuer credmon for HTCondor
Group: Applications/System
Requires: %name = %version-%release
%if 0%{?rhel} == 7
Requires: python2-condor = %{version}-%{release}
Requires: python-six
Requires: python2-cryptography
Requires: python2-scitokens
%else
Requires: python3-condor = %{version}-%{release}
Requires: python3-six
Requires: python3-cryptography
Requires: python3-scitokens
%endif

%description credmon-local
The local issuer credmon allows users to obtain credentials from an
admin-configured private SciToken key on the access point and to use those
credentials securely inside running jobs.


#######################
%package credmon-oauth
Summary: OAuth2 credmon for HTCondor
Group: Applications/System
Requires: %name = %version-%release
Requires: condor-credmon-local = %{version}-%{release}
%if 0%{?rhel} == 7
Requires: python2-requests-oauthlib
Requires: python-flask
Requires: mod_wsgi
%else
Requires: python3-requests-oauthlib
Requires: python3-flask
Requires: python3-mod_wsgi
%endif
Requires: httpd

%description credmon-oauth
The OAuth2 credmon allows users to obtain credentials from configured
OAuth2 endpoints and to use those credentials securely inside running jobs.


#######################
%package credmon-vault
Summary: Vault credmon for HTCondor
Group: Applications/System
Requires: %name = %version-%release
Requires: python3-condor = %{version}-%{release}
Requires: python3-six
%if 0%{?rhel} == 7 && ! 0%{?amzn}
Requires: python36-cryptography
%endif
%if 0%{?rhel} >= 8
Requires: python3-cryptography
%endif
%if %uw_build
# Although htgettoken is only needed on the submit machine and
#  condor-credmon-vault is needed on both the submit and credd machines,
#  htgettoken is small so it doesn't hurt to require it in both places.
Requires: htgettoken >= 1.1
%endif
Conflicts: %name-credmon-local

%description credmon-vault
The Vault credmon allows users to obtain credentials from Vault using
htgettoken and to use those credentials securely inside running jobs.

#######################
%package -n minicondor
Summary: Configuration for a single-node HTCondor
Group: Applications/System
Requires: %name = %version-%release
%if 0%{?rhel} >= 7 || 0%{?fedora} || 0%{?suse_version}
Requires: python3-condor = %version-%release
%endif

%description -n minicondor
This example configuration is good for trying out HTCondor for the first time.
It only configures the IPv4 loopback address, turns on basic security, and
shortens many timers to be more responsive.

#######################
%package ap
Summary: Configuration for an Access Point
Group: Applications/System
Requires: %name = %version-%release
%if 0%{?rhel} >= 7 || 0%{?fedora} || 0%{?suse_version}
Requires: python3-condor = %version-%release
%endif

%description ap
This example configuration is good for installing an Access Point.
After installation, one could join a pool or start an annex.

#######################
%package ep
Summary: Configuration for an Execution Point
Group: Applications/System
Requires: %name = %version-%release
%if 0%{?rhel} >= 7 || 0%{?fedora} || 0%{?suse_version}
Requires: python3-condor = %version-%release
%endif

%description ep
This example configuration is good for installing an Execution Point.
After installation, one could join a pool or start an annex.

#######################
%package annex-ec2
Summary: Configuration and scripts to make an EC2 image annex-compatible
Group: Applications/System
Requires: %name = %version-%release
Requires(post): /sbin/chkconfig
Requires(preun): /sbin/chkconfig

%description annex-ec2
Configures HTCondor to make an EC2 image annex-compatible.  Do NOT install
on a non-EC2 image.

%files annex-ec2
%_libexecdir/condor/condor-annex-ec2
%{_unitdir}/condor-annex-ec2.service
%config(noreplace) %_sysconfdir/condor/config.d/50ec2.config
%config(noreplace) %_sysconfdir/condor/master_shutdown_script.sh

%post annex-ec2
/bin/systemctl enable condor-annex-ec2

%preun annex-ec2
if [ $1 == 0 ]; then
    /bin/systemctl disable condor-annex-ec2
fi

#######################
%package upgrade-checks
Summary: Script to check for manual interventions needed to upgrade
Group: Applications/System
Requires: pcre2-tools

%description upgrade-checks
Examines the current HTCondor installation and recommends changes to ensure
a smooth upgrade to a subsequent HTCondor version.

%files upgrade-checks
%_bindir/condor_upgrade_check

%pre
getent group condor >/dev/null || groupadd -r condor
getent passwd condor >/dev/null || \
  useradd -r -g condor -d %_var/lib/condor -s /sbin/nologin \
    -c "Owner of HTCondor Daemons" condor
exit 0


%prep
# For release tarballs
%setup -q -n %{name}-%{condor_version}

# Patch credmon-oauth to use Python 2 on EL7
%if 0%{?rhel} == 7
%patch1 -p1
%endif

# fix errant execute permissions
find src -perm /a+x -type f -name "*.[Cch]" -exec chmod a-x {} \;


%build

%if 0%{?suse_version}
export CC=/usr/bin/gcc-11
export CXX=/usr/bin/g++-11
%endif

%if 0%{?rhel} == 7 && 0%{?devtoolset}
. /opt/rh/devtoolset-%{devtoolset}/enable
export CC=$(which cc)
export CXX=$(which c++)
%endif

%if 0%{?rhel} >= 8 && 0%{?gcctoolset}
. /opt/rh/gcc-toolset-%{gcctoolset}/enable
export CC=$(which cc)
export CXX=$(which c++)
%endif

# build man files
%if 0%{?amzn}
# if this environment variable is set, sphinx-build cannot import markupsafe
env -u RPM_BUILD_ROOT make -C docs man
%else
%if 0%{?rhel} == 7
make -C docs SPHINXBUILD=sphinx-build-3.6 man
%else
make -C docs man
%endif
%endif

%if %uw_build
%define condor_build_id UW_development
%define condor_git_sha -1
%endif

# Any changes here should be synchronized with
# ../debian/rules 

%if 0%{?suse_version}
%cmake \
%else
%cmake3 \
%endif
%if %uw_build
       -DBUILDID:STRING=%condor_build_id \
       -DPLATFORM:STRING=${NMI_PLATFORM:-unknown} \
%if "%{condor_git_sha}" != "-1"
       -DCONDOR_GIT_SHA:STRING=%condor_git_sha \
%endif
       -DBUILD_TESTING:BOOL=TRUE \
%else
       -DBUILD_TESTING:BOOL=FALSE \
%endif
%if 0%{?suse_version}
       -DCMAKE_SHARED_LINKER_FLAGS="%{?build_ldflags} -Wl,--as-needed -Wl,-z,now" \
%endif
%if 0%{?rhel} == 7 || 0%{?rhel} == 8
       -DPython3_EXECUTABLE=%__python3 \
%endif
       -DCMAKE_SKIP_RPATH:BOOL=TRUE \
       -DPACKAGEID:STRING=%{version}-%{condor_release} \
       -DCONDOR_PACKAGE_BUILD:BOOL=TRUE \
       -DCONDOR_RPMBUILD:BOOL=TRUE \
%if 0%{?amzn}
       -DWITH_VOMS:BOOL=FALSE \
       -DWITH_LIBVIRT:BOOL=FALSE \
%endif
       -DCMAKE_INSTALL_PREFIX:PATH=/

%if 0%{?amzn}
cd amazon-linux-build
%else
%if 0%{?rhel} == 9 || 0%{?fedora}
cd redhat-linux-build
%endif
%endif
make %{?_smp_mflags}
%if %uw_build
make %{?_smp_mflags} tests
%endif

%install
%if 0%{?amzn}
cd amazon-linux-build
%else
%if 0%{?rhel} == 9 || 0%{?fedora}
cd redhat-linux-build
%endif
%endif
# installation happens into a temporary location, this function is
# useful in moving files into their final locations
function populate {
  _dest="$1"; shift; _src="$*"
  mkdir -p "%{buildroot}/$_dest"
  mv $_src "%{buildroot}/$_dest"
}

rm -rf %{buildroot}
echo ---------------------------- makefile ---------------------------------
%if 0%{?suse_version}
cd build
%endif
make install DESTDIR=%{buildroot}

%if %uw_build
make tests-tar-pkg
# tarball of tests
%if 0%{?amzn}
cp -p %{_builddir}/%{name}-%{version}/amazon-linux-build/condor_tests-*.tar.gz %{buildroot}/%{_libdir}/condor/condor_tests-%{version}.tar.gz
%else
%if 0%{?rhel} == 9 || 0%{?fedora}
cp -p %{_builddir}/%{name}-%{version}/redhat-linux-build/condor_tests-*.tar.gz %{buildroot}/%{_libdir}/condor/condor_tests-%{version}.tar.gz
%else
%if 0%{?suse_version}
cp -p %{_builddir}/%{name}-%{version}/build/condor_tests-*.tar.gz %{buildroot}/%{_libdir}/condor/condor_tests-%{version}.tar.gz
%else
cp -p %{_builddir}/%{name}-%{version}/condor_tests-*.tar.gz %{buildroot}/%{_libdir}/condor/condor_tests-%{version}.tar.gz
%endif
%endif
%endif
%endif

# Drop in a symbolic link for backward compatibility
ln -s ../..%{_libdir}/condor/condor_ssh_to_job_sshd_config_template %{buildroot}/%_sysconfdir/condor/condor_ssh_to_job_sshd_config_template

%if %uw_build
%if 0%{?rhel} == 7 && ! 0%{?amzn}
# Drop in a link for backward compatibility for small shadow
ln -s condor_shadow %{buildroot}/%{_sbindir}/condor_shadow_s
%endif
%endif

populate /usr/share/doc/condor-%{version}/examples %{buildroot}/usr/share/doc/condor-%{version}/etc/examples/*

mkdir -p %{buildroot}/%{_sysconfdir}/condor
# the default condor_config file is not architecture aware and thus
# sets the LIB directory to always be /usr/lib, we want to do better
# than that. this is, so far, the best place to do this
# specialization. we strip the "lib" or "lib64" part from _libdir and
# stick it in the LIB variable in the config.
LIB=$(echo %{?_libdir} | sed -e 's:/usr/\(.*\):\1:')
if [ "$LIB" = "%_libdir" ]; then
  echo "_libdir does not contain /usr, sed expression needs attention"
  exit 1
fi

# Install the basic configuration, a Personal HTCondor config. Allows for
# yum install condor + service condor start and go.
mkdir -p -m0755 %{buildroot}/%{_sysconfdir}/condor/config.d
mkdir -p -m0700 %{buildroot}/%{_sysconfdir}/condor/passwords.d
mkdir -p -m0700 %{buildroot}/%{_sysconfdir}/condor/tokens.d

populate %_sysconfdir/condor/config.d %{buildroot}/usr/share/doc/condor-%{version}/examples/00-security
populate %_sysconfdir/condor/config.d %{buildroot}/usr/share/doc/condor-%{version}/examples/00-minicondor
populate %_sysconfdir/condor/config.d %{buildroot}/usr/share/doc/condor-%{version}/examples/00-access-point
populate %_sysconfdir/condor/config.d %{buildroot}/usr/share/doc/condor-%{version}/examples/00-execution-point
populate %_sysconfdir/condor/config.d %{buildroot}/usr/share/doc/condor-%{version}/examples/00-kbdd
populate %_sysconfdir/condor/config.d %{buildroot}/usr/share/doc/condor-%{version}/examples/50ec2.config

# Install a second config.d directory under /usr/share, used for the
# convenience of software built on top of Condor such as GlideinWMS.
mkdir -p -m0755 %{buildroot}/usr/share/condor/config.d

mkdir -p -m0755 %{buildroot}/%{_var}/log/condor
# Note we use %{_var}/lib instead of %{_sharedstatedir} for RHEL5 compatibility
mkdir -p -m0755 %{buildroot}/%{_var}/lib/condor/spool
mkdir -p -m0755 %{buildroot}/%{_var}/lib/condor/execute
mkdir -p -m0755 %{buildroot}/%{_var}/lib/condor/krb_credentials
mkdir -p -m2770 %{buildroot}/%{_var}/lib/condor/oauth_credentials


# not packaging configure/install scripts
%if ! %uw_build
rm -f %{buildroot}%{_bindir}/make-ap-from-tarball
rm -f %{buildroot}%{_bindir}/make-personal-from-tarball
rm -f %{buildroot}%{_sbindir}/condor_configure
rm -f %{buildroot}%{_sbindir}/condor_install
rm -f %{buildroot}/%{_mandir}/man1/condor_configure.1
rm -f %{buildroot}/%{_mandir}/man1/condor_install.1
%endif

mkdir -p %{buildroot}/%{_var}/www/wsgi-scripts/condor_credmon_oauth
mv %{buildroot}/%{_libexecdir}/condor/condor_credmon_oauth.wsgi %{buildroot}/%{_var}/www/wsgi-scripts/condor_credmon_oauth/condor_credmon_oauth.wsgi

# Move oauth credmon config files out of examples and into config.d
mv %{buildroot}/usr/share/doc/condor-%{version}/examples/condor_credmon_oauth/config/condor/40-oauth-credmon.conf %{buildroot}/%{_sysconfdir}/condor/config.d/40-oauth-credmon.conf
mv %{buildroot}/usr/share/doc/condor-%{version}/examples/condor_credmon_oauth/config/condor/40-oauth-tokens.conf %{buildroot}/%{_sysconfdir}/condor/config.d/40-oauth-tokens.conf
mv %{buildroot}/usr/share/doc/condor-%{version}/examples/condor_credmon_oauth/README.credentials %{buildroot}/%{_var}/lib/condor/oauth_credentials/README.credentials

# Move vault credmon config file out of examples and into config.d
mv %{buildroot}/usr/share/doc/condor-%{version}/examples/condor_credmon_oauth/config/condor/40-vault-credmon.conf %{buildroot}/%{_sysconfdir}/condor/config.d/40-vault-credmon.conf

###
# Backwards compatibility on EL7 with the previous versions and configs of scitokens-credmon
%if 0%{?rhel} == 7
ln -s ../..%{_sbindir}/condor_credmon_oauth          %{buildroot}/%{_bindir}/condor_credmon_oauth
ln -s ../..%{_sbindir}/scitokens_credential_producer %{buildroot}/%{_bindir}/scitokens_credential_producer
mkdir -p %{buildroot}/%{_var}/www/wsgi-scripts/scitokens-credmon
ln -s ../../../..%{_var}/www/wsgi-scripts/condor_credmon_oauth/condor_credmon_oauth.wsgi %{buildroot}/%{_var}/www/wsgi-scripts/scitokens-credmon/scitokens-credmon.wsgi
%endif
###

# install tmpfiles.d/condor.conf
mkdir -p %{buildroot}%{_tmpfilesdir}
install -m 0644 %{buildroot}/usr/share/doc/condor-%{version}/examples/condor-tmpfiles.conf %{buildroot}%{_tmpfilesdir}/%{name}.conf

install -Dp -m0755 %{buildroot}/usr/share/doc/condor-%{version}/examples/condor-annex-ec2 %{buildroot}%{_libexecdir}/condor/condor-annex-ec2

mkdir -p %{buildroot}%{_unitdir}
install -m 0644 %{buildroot}/usr/share/doc/condor-%{version}/examples/condor-annex-ec2.service %{buildroot}%{_unitdir}/condor-annex-ec2.service
install -m 0644 %{buildroot}/usr/share/doc/condor-%{version}/examples/condor.service %{buildroot}%{_unitdir}/condor.service
# Disabled until HTCondor security fixed.
# install -m 0644 %{buildroot}/usr/share/doc/condor-%{version}/examples/condor.socket %{buildroot}%{_unitdir}/condor.socket

%if 0%{?rhel} >= 7
mkdir -p %{buildroot}%{_datadir}/condor/
cp %{SOURCE8} %{buildroot}%{_datadir}/condor/
%endif

# Install perl modules

#Fixups for packaged build, should have been done by cmake

mkdir -p %{buildroot}/usr/share/condor
mv %{buildroot}/usr/lib64/condor/Chirp.jar %{buildroot}/usr/share/condor
mv %{buildroot}/usr/lib64/condor/CondorJava*.class %{buildroot}/usr/share/condor
mv %{buildroot}/usr/lib64/condor/libchirp_client.so %{buildroot}/usr/lib64
mv %{buildroot}/usr/lib64/condor/libcondor_utils_*.so %{buildroot}/usr/lib64
%if 0%{?rhel} == 7
mv %{buildroot}/usr/lib64/condor/libpyclassad2*.so %{buildroot}/usr/lib64
%endif
mv %{buildroot}/usr/lib64/condor/libpyclassad3*.so %{buildroot}/usr/lib64

rm -rf %{buildroot}/usr/share/doc/condor-%{version}/LICENSE
rm -rf %{buildroot}/usr/share/doc/condor-%{version}/NOTICE.txt
rm -rf %{buildroot}/usr/share/doc/condor-%{version}/README

# we must place the config examples in builddir so %doc can find them
mv %{buildroot}/usr/share/doc/condor-%{version}/examples %_builddir/%name-%condor_version

# Fix up blahp installation
%if 0%{?rhel} == 7
# Don't rely on Python 3 on EL7 (not installed by default)
sed -i 's;/usr/bin/python3;/usr/bin/python2;' %{buildroot}%{_libexecdir}/blahp/*status.py
%endif
# Move batch system customization files to /etc, with symlinks in the
# original location. Admins will need to edit these.
install -m 0755 -d -p %{buildroot}%{_sysconfdir}/blahp
for batch_system in condor kubernetes lsf nqs pbs sge slurm; do
    mv %{buildroot}%{_libexecdir}/blahp/${batch_system}_local_submit_attributes.sh %{buildroot}%{_sysconfdir}/blahp
    ln -s ../../../etc/blahp/${batch_system}_local_submit_attributes.sh \
        %{buildroot}%{_libexecdir}/blahp/${batch_system}_local_submit_attributes.sh
done

# htcondor/dags only works with Python3
rm -rf %{buildroot}/usr/lib64/python2.7/site-packages/htcondor/dags

# htcondor/personal.py only works with Python3
rm -f %{buildroot}/usr/lib64/python2.7/site-packages/htcondor/personal.py

# New fangled stuff does not work with Python2
rm -rf %{buildroot}/usr/lib64/python2.7/site-packages/classad2
rm -rf %{buildroot}/usr/lib64/python2.7/site-packages/classad3
rm -rf %{buildroot}/usr/lib64/python2.7/site-packages/htcondor2

# classad3 shouldn't be distributed yet
rm -rf %{buildroot}/usr/lib*/python%{python3_version}/site-packages/classad3

%clean
rm -rf %{buildroot}


%check
# This currently takes hours and can kill your machine...
#cd condor_tests
#make check-seralized

#################
%files
%defattr(-,root,root,-)
%doc LICENSE NOTICE.txt examples
%dir %_sysconfdir/condor/
%config %_sysconfdir/condor/condor_config
%{_tmpfilesdir}/%{name}.conf
%{_unitdir}/condor.service
# Disabled until HTCondor security fixed.
# % {_unitdir}/condor.socket
%dir %_datadir/condor/
%_datadir/condor/Chirp.jar
%_datadir/condor/CondorJavaInfo.class
%_datadir/condor/CondorJavaWrapper.class
%if 0%{?rhel} >= 7
%_datadir/condor/htcondor.pp
%endif
%dir %_sysconfdir/condor/passwords.d/
%dir %_sysconfdir/condor/tokens.d/
%dir %_sysconfdir/condor/config.d/
%config(noreplace) %{_sysconfdir}/condor/config.d/00-security
%dir /usr/share/condor/config.d/
%_libdir/condor/condor_ssh_to_job_sshd_config_template
%_sysconfdir/condor/condor_ssh_to_job_sshd_config_template
%_sysconfdir/bash_completion.d/condor
%_libdir/libchirp_client.so
%_libdir/libcondor_utils_%{version_}.so
%_libdir/condor/libfmt.so
%_libdir/condor/libfmt.so.10
%_libdir/condor/libfmt.so.10.1.0

%_libdir/condor/libgetpwnam.so
%dir %_libexecdir/condor/
%_libexecdir/condor/cleanup_locally_mounted_checkpoint
%_libexecdir/condor/linux_kernel_tuning
%_libexecdir/condor/accountant_log_fixer
%_libexecdir/condor/condor_chirp
%_libexecdir/condor/condor_ssh
%_libexecdir/condor/sshd.sh
%_libexecdir/condor/get_orted_cmd.sh
%_libexecdir/condor/orted_launcher.sh
%_libexecdir/condor/set_batchtok_cmd
%_libexecdir/condor/cred_producer_krb
%_libexecdir/condor/condor_job_router
%_libexecdir/condor/condor_pid_ns_init
%_libexecdir/condor/condor_urlfetch
%_libexecdir/condor/htcondor_docker_test
%ifarch aarch64 ppc64le x86_64
%_libexecdir/condor/exit_37.sif
%endif
%dir %_libexecdir/condor/singularity_test_sandbox/
%dir %_libexecdir/condor/singularity_test_sandbox/dev/
%dir %_libexecdir/condor/singularity_test_sandbox/proc/
%_libexecdir/condor/singularity_test_sandbox/exit_37
%_libexecdir/condor/condor_limits_wrapper.sh
%_libexecdir/condor/condor_rooster
%_libexecdir/condor/condor_schedd.init
%_libexecdir/condor/condor_ssh_to_job_shell_setup
%_libexecdir/condor/condor_ssh_to_job_sshd_setup
%_libexecdir/condor/condor_power_state
%_libexecdir/condor/condor_kflops
%_libexecdir/condor/condor_mips
%_libexecdir/condor/data_plugin
%_libexecdir/condor/box_plugin.py
%_libexecdir/condor/gdrive_plugin.py
%_libexecdir/condor/common-cloud-attributes-google.py
%_libexecdir/condor/common-cloud-attributes-aws.py
%_libexecdir/condor/common-cloud-attributes-aws.sh
%_libexecdir/condor/onedrive_plugin.py
# TODO: get rid of these
# Not sure where these are getting built
%if 0%{?rhel} <= 7 && ! 0%{?fedora} && ! 0%{?suse_version}
%_libexecdir/condor/box_plugin.pyc
%_libexecdir/condor/box_plugin.pyo
%_libexecdir/condor/gdrive_plugin.pyc
%_libexecdir/condor/gdrive_plugin.pyo
%_libexecdir/condor/onedrive_plugin.pyc
%_libexecdir/condor/onedrive_plugin.pyo
%_libexecdir/condor/adstash/__init__.pyc
%_libexecdir/condor/adstash/__init__.pyo
%_libexecdir/condor/adstash/ad_sources/__init__.pyc
%_libexecdir/condor/adstash/ad_sources/__init__.pyo
%_libexecdir/condor/adstash/ad_sources/registry.pyc
%_libexecdir/condor/adstash/ad_sources/registry.pyo
%_libexecdir/condor/adstash/interfaces/__init__.pyc
%_libexecdir/condor/adstash/interfaces/__init__.pyo
%_libexecdir/condor/adstash/interfaces/generic.pyc
%_libexecdir/condor/adstash/interfaces/generic.pyo
%_libexecdir/condor/adstash/interfaces/null.pyc
%_libexecdir/condor/adstash/interfaces/null.pyo
%_libexecdir/condor/adstash/interfaces/registry.pyc
%_libexecdir/condor/adstash/interfaces/registry.pyo
%_libexecdir/condor/adstash/interfaces/opensearch.pyc
%_libexecdir/condor/adstash/interfaces/opensearch.pyo
%endif
%_libexecdir/condor/curl_plugin
%_libexecdir/condor/condor_shared_port
%_libexecdir/condor/condor_defrag
%_libexecdir/condor/interactive.sub
%_libexecdir/condor/condor_gangliad
%_libexecdir/condor/ce-audit.so
%_libexecdir/condor/adstash/__init__.py
%_libexecdir/condor/adstash/adstash.py
%_libexecdir/condor/adstash/config.py
%_libexecdir/condor/adstash/convert.py
%_libexecdir/condor/adstash/utils.py
%_libexecdir/condor/adstash/ad_sources/__init__.py
%_libexecdir/condor/adstash/ad_sources/ad_file.py
%_libexecdir/condor/adstash/ad_sources/generic.py
%_libexecdir/condor/adstash/ad_sources/registry.py
%_libexecdir/condor/adstash/ad_sources/schedd_history.py
%_libexecdir/condor/adstash/ad_sources/startd_history.py
%_libexecdir/condor/adstash/ad_sources/schedd_job_epoch_history.py
%_libexecdir/condor/adstash/ad_sources/schedd_transfer_epoch_history.py
%_libexecdir/condor/adstash/interfaces/__init__.py
%_libexecdir/condor/adstash/interfaces/elasticsearch.py
%_libexecdir/condor/adstash/interfaces/opensearch.py
%_libexecdir/condor/adstash/interfaces/generic.py
%_libexecdir/condor/adstash/interfaces/json_file.py
%_libexecdir/condor/adstash/interfaces/null.py
%_libexecdir/condor/adstash/interfaces/registry.py
%_libexecdir/condor/annex
%_mandir/man1/condor_advertise.1.gz
%_mandir/man1/condor_annex.1.gz
%_mandir/man1/condor_check_password.1.gz
%_mandir/man1/condor_check_userlogs.1.gz
%_mandir/man1/condor_chirp.1.gz
%_mandir/man1/condor_config_val.1.gz
%_mandir/man1/condor_dagman.1.gz
%_mandir/man1/condor_fetchlog.1.gz
%_mandir/man1/condor_findhost.1.gz
%_mandir/man1/condor_gpu_discovery.1.gz
%_mandir/man1/condor_history.1.gz
%_mandir/man1/condor_hold.1.gz
%_mandir/man1/condor_job_router_info.1.gz
%_mandir/man1/condor_master.1.gz
%_mandir/man1/condor_off.1.gz
%_mandir/man1/condor_on.1.gz
%_mandir/man1/condor_pool_job_report.1.gz
%_mandir/man1/condor_preen.1.gz
%_mandir/man1/condor_prio.1.gz
%_mandir/man1/condor_q.1.gz
%_mandir/man1/condor_qsub.1.gz
%_mandir/man1/condor_qedit.1.gz
%_mandir/man1/condor_reconfig.1.gz
%_mandir/man1/condor_release.1.gz
%_mandir/man1/condor_remote_cluster.1.gz
%_mandir/man1/condor_reschedule.1.gz
%_mandir/man1/condor_restart.1.gz
%_mandir/man1/condor_rm.1.gz
%_mandir/man1/condor_run.1.gz
%_mandir/man1/condor_set_shutdown.1.gz
%_mandir/man1/condor_ssh_start.1.gz
%_mandir/man1/condor_sos.1.gz
%_mandir/man1/condor_ssl_fingerprint.1.gz
%_mandir/man1/condor_stats.1.gz
%_mandir/man1/condor_status.1.gz
%_mandir/man1/condor_store_cred.1.gz
%_mandir/man1/condor_submit.1.gz
%_mandir/man1/condor_submit_dag.1.gz
%_mandir/man1/condor_test_token.1.gz
%_mandir/man1/condor_token_create.1.gz
%_mandir/man1/condor_token_fetch.1.gz
%_mandir/man1/condor_token_list.1.gz
%_mandir/man1/condor_token_request.1.gz
%_mandir/man1/condor_token_request_approve.1.gz
%_mandir/man1/condor_token_request_auto_approve.1.gz
%_mandir/man1/condor_token_request_list.1.gz
%_mandir/man1/condor_top.1.gz
%_mandir/man1/condor_transfer_data.1.gz
%_mandir/man1/condor_transform_ads.1.gz
%_mandir/man1/condor_update_machine_ad.1.gz
%_mandir/man1/condor_updates_stats.1.gz
%_mandir/man1/condor_upgrade_check.1.gz
%_mandir/man1/condor_urlfetch.1.gz
%_mandir/man1/condor_userlog.1.gz
%_mandir/man1/condor_userprio.1.gz
%_mandir/man1/condor_vacate.1.gz
%_mandir/man1/condor_vacate_job.1.gz
%_mandir/man1/condor_version.1.gz
%_mandir/man1/condor_wait.1.gz
%_mandir/man1/condor_router_history.1.gz
%_mandir/man1/condor_continue.1.gz
%_mandir/man1/condor_suspend.1.gz
%_mandir/man1/condor_router_q.1.gz
%_mandir/man1/condor_ssh_to_job.1.gz
%_mandir/man1/condor_power.1.gz
%_mandir/man1/condor_gather_info.1.gz
%_mandir/man1/condor_router_rm.1.gz
%_mandir/man1/condor_drain.1.gz
%_mandir/man1/condor_ping.1.gz
%_mandir/man1/condor_rmdir.1.gz
%_mandir/man1/condor_tail.1.gz
%_mandir/man1/condor_who.1.gz
%_mandir/man1/condor_now.1.gz
%_mandir/man1/classad_eval.1.gz
%_mandir/man1/classads.1.gz
%_mandir/man1/condor_adstash.1.gz
%_mandir/man1/condor_evicted_files.1.gz
%_mandir/man1/condor_watch_q.1.gz
%_mandir/man1/get_htcondor.1.gz
%_mandir/man1/htcondor.1.gz
# bin/condor is a link for checkpoint, reschedule, vacate
%_bindir/condor_submit_dag
%_bindir/condor_who
%_bindir/condor_now
%_bindir/condor_prio
%_bindir/condor_transfer_data
%_bindir/condor_check_userlogs
%_bindir/condor_q
%_libexecdir/condor/condor_transferer
%_bindir/condor_docker_enter
%_bindir/condor_qedit
%_bindir/condor_qusers
%_bindir/condor_userlog
%_bindir/condor_release
%_bindir/condor_userlog_job_counter
%_bindir/condor_config_val
%_bindir/condor_reschedule
%_bindir/condor_userprio
%_bindir/condor_check_password
%_bindir/condor_check_config
%_bindir/condor_dagman
%_bindir/condor_rm
%_bindir/condor_vacate
%_bindir/condor_run
%_bindir/condor_router_history
%_bindir/condor_router_q
%_bindir/condor_router_rm
%_bindir/condor_vacate_job
%_bindir/condor_findhost
%_bindir/condor_stats
%_bindir/condor_version
%_bindir/condor_history
%_bindir/condor_status
%_bindir/condor_wait
%_bindir/condor_hold
%_bindir/condor_submit
%_bindir/condor_ssh_to_job
%_bindir/condor_power
%_bindir/condor_gather_info
%_bindir/condor_continue
%_bindir/condor_ssl_fingerprint
%_bindir/condor_suspend
%_bindir/condor_test_match
%_bindir/condor_token_create
%_bindir/condor_token_fetch
%_bindir/condor_token_request
%_bindir/condor_token_request_approve
%_bindir/condor_token_request_auto_approve
%_bindir/condor_token_request_list
%_bindir/condor_token_list
%_bindir/condor_scitoken_exchange
%_bindir/condor_drain
%_bindir/condor_ping
%_bindir/condor_tail
%_bindir/condor_qsub
%_bindir/condor_pool_job_report
%_bindir/condor_job_router_info
%_bindir/condor_transform_ads
%_bindir/condor_update_machine_ad
%_bindir/condor_annex
%_bindir/condor_nsenter
%_bindir/condor_evicted_files
%_bindir/condor_adstash
%_bindir/condor_remote_cluster
%_bindir/bosco_cluster
%_bindir/condor_ssh_start
%_bindir/condor_test_token
%_bindir/condor_manifest
# sbin/condor is a link for master_off, off, on, reconfig,
# reconfig_schedd, restart
%_sbindir/condor_advertise
%_sbindir/condor_aklog
%_sbindir/condor_credmon_krb
%_sbindir/condor_c-gahp
%_sbindir/condor_c-gahp_worker_thread
%_sbindir/condor_collector
%_sbindir/condor_credd
%_sbindir/condor_fetchlog
%_sbindir/condor_ft-gahp
%_sbindir/condor_had
%_sbindir/condor_master
%_sbindir/condor_negotiator
%_sbindir/condor_off
%_sbindir/condor_on
%_sbindir/condor_preen
%_sbindir/condor_reconfig
%_sbindir/condor_replication
%_sbindir/condor_restart
%_sbindir/condor_schedd
%_sbindir/condor_set_shutdown
%_sbindir/condor_shadow
%if %uw_build
%if 0%{?rhel} == 7 && ! 0%{?amzn}
%{_sbindir}/condor_shadow_s
%endif
%endif
%_sbindir/condor_sos
%_sbindir/condor_startd
%_sbindir/condor_starter
%_sbindir/condor_store_cred
%_sbindir/condor_testwritelog
%_sbindir/condor_updates_stats
%_sbindir/ec2_gahp
%_sbindir/condor_gridmanager
%_sbindir/remote_gahp
%_sbindir/rvgahp_client
%_sbindir/rvgahp_proxy
%_sbindir/rvgahp_server
%_sbindir/AzureGAHPServer
%_sbindir/gce_gahp
%_sbindir/arc_gahp
%_libexecdir/condor/condor_gpu_discovery
%_libexecdir/condor/condor_gpu_utilization
%config(noreplace) %_sysconfdir/condor/ganglia.d/00_default_metrics
%defattr(-,condor,condor,-)
%dir %_var/lib/condor/
%dir %_var/lib/condor/execute/
%dir %_var/lib/condor/spool/
%dir %_var/log/condor/
%defattr(-,root,condor,-)
%dir %_var/lib/condor/oauth_credentials
%defattr(-,root,root,-)
%dir %_var/lib/condor/krb_credentials

###### blahp files #######
%config %_sysconfdir/blah.config
%config %_sysconfdir/blparser.conf
%dir %_sysconfdir/blahp/
%config %_sysconfdir/blahp/condor_local_submit_attributes.sh
%config %_sysconfdir/blahp/kubernetes_local_submit_attributes.sh
%config %_sysconfdir/blahp/lsf_local_submit_attributes.sh
%config %_sysconfdir/blahp/nqs_local_submit_attributes.sh
%config %_sysconfdir/blahp/pbs_local_submit_attributes.sh
%config %_sysconfdir/blahp/sge_local_submit_attributes.sh
%config %_sysconfdir/blahp/slurm_local_submit_attributes.sh
%_bindir/blahpd
%_sbindir/blah_check_config
%_sbindir/blahpd_daemon
%dir %_libexecdir/blahp
%_libexecdir/blahp/*

####### procd files #######
%_sbindir/condor_procd
%_sbindir/gidd_alloc
%_sbindir/procd_ctl
%_mandir/man1/procd_ctl.1.gz
%_mandir/man1/gidd_alloc.1.gz
%_mandir/man1/condor_procd.1.gz

####### classads files #######
%defattr(-,root,root,-)
%_libdir/libclassad.so.*

#################
%files devel
%{_includedir}/condor/chirp_client.h
%{_includedir}/condor/condor_event.h
%{_includedir}/condor/file_lock.h
%{_includedir}/condor/read_user_log.h
%{_libdir}/condor/libchirp_client.a
%{_libdir}/libclassad.a

####### classads-devel files #######
%defattr(-,root,root,-)
%_bindir/classad_functional_tester
%_bindir/classad_version
%_libdir/libclassad.so
%dir %_includedir/classad/
%_includedir/classad/attrrefs.h
%_includedir/classad/cclassad.h
%_includedir/classad/classad_distribution.h
%_includedir/classad/classadErrno.h
%_includedir/classad/classad.h
%_includedir/classad/classadCache.h
%_includedir/classad/classad_containers.h
%_includedir/classad/classad_flat_map.h
%_includedir/classad/collectionBase.h
%_includedir/classad/collection.h
%_includedir/classad/common.h
%_includedir/classad/debug.h
%_includedir/classad/exprList.h
%_includedir/classad/exprTree.h
%_includedir/classad/flat_set.h
%_includedir/classad/fnCall.h
%_includedir/classad/indexfile.h
%_includedir/classad/jsonSink.h
%_includedir/classad/jsonSource.h
%_includedir/classad/lexer.h
%_includedir/classad/lexerSource.h
%_includedir/classad/literals.h
%_includedir/classad/matchClassad.h
%_includedir/classad/natural_cmp.h
%_includedir/classad/operators.h
%_includedir/classad/query.h
%_includedir/classad/sink.h
%_includedir/classad/source.h
%_includedir/classad/transaction.h
%_includedir/classad/util.h
%_includedir/classad/value.h
%_includedir/classad/view.h
%_includedir/classad/xmlLexer.h
%_includedir/classad/xmlSink.h
%_includedir/classad/xmlSource.h

%if %uw_build
#################
%files tarball
%{_bindir}/make-ap-from-tarball
%{_bindir}/make-personal-from-tarball
%{_sbindir}/condor_configure
%{_sbindir}/condor_install
%{_mandir}/man1/condor_configure.1.gz
%{_mandir}/man1/condor_install.1.gz
%endif

#################
%files kbdd
%defattr(-,root,root,-)
%config(noreplace) %_sysconfdir/condor/config.d/00-kbdd
%_sbindir/condor_kbdd

#################
%if ! 0%{?amzn}
%files vm-gahp
%defattr(-,root,root,-)
%_sbindir/condor_vm-gahp
%_libexecdir/condor/libvirt_simple_script.awk

%endif
#################
%files test
%defattr(-,root,root,-)
%_libexecdir/condor/condor_sinful
%_libexecdir/condor/condor_testingd
%_libexecdir/condor/test_user_mapping
%_libexecdir/condor/test_std_f_timer_d
%if %uw_build
%_libdir/condor/condor_tests-%{version}.tar.gz
%endif

%if 0%{?rhel} <= 7 && 0%{?fedora} <= 31 && ! 0%{?suse_version}
%files -n python2-condor
%defattr(-,root,root,-)
%_bindir/condor_top
%_bindir/classad_eval
%_bindir/condor_watch_q
%_libdir/libpyclassad2*.so
%_libexecdir/condor/libclassad_python_user.so
%{python_sitearch}/classad/
%{python_sitearch}/htcondor/
%{python_sitearch}/htcondor-*.egg-info/
%endif

%if 0%{?rhel} >= 7 || 0%{?fedora} || 0%{?suse_version}
%files -n python3-condor
%defattr(-,root,root,-)
%_bindir/condor_top
%_bindir/condor_diagnostics
%_bindir/classad_eval
%_bindir/condor_watch_q
%_bindir/htcondor
%_libdir/libpyclassad3*.so
%_libexecdir/condor/libclassad_python_user.cpython-3*.so
%_libexecdir/condor/libclassad_python3_user.so
/usr/lib64/python%{python3_version}/site-packages/classad/
/usr/lib64/python%{python3_version}/site-packages/htcondor/
/usr/lib64/python%{python3_version}/site-packages/htcondor-*.egg-info/
/usr/lib64/python%{python3_version}/site-packages/htcondor_cli/
/usr/lib64/python%{python3_version}/site-packages/classad2/
/usr/lib64/python%{python3_version}/site-packages/htcondor2/
%endif

%files credmon-local
%doc examples/condor_credmon_oauth
%_sbindir/condor_credmon_oauth
%_sbindir/scitokens_credential_producer
%_libexecdir/condor/credmon
%_var/lib/condor/oauth_credentials/README.credentials
%config(noreplace) %_sysconfdir/condor/config.d/40-oauth-credmon.conf
%ghost %_var/lib/condor/oauth_credentials/CREDMON_COMPLETE
%ghost %_var/lib/condor/oauth_credentials/pid
%if 0%{?rhel} == 7
###
# Backwards compatibility with the previous versions and configs of scitokens-credmon
%_bindir/condor_credmon_oauth
%_bindir/scitokens_credential_producer
###
%endif

%files credmon-oauth
%_var/www/wsgi-scripts/condor_credmon_oauth
%config(noreplace) %_sysconfdir/condor/config.d/40-oauth-tokens.conf
%ghost %_var/lib/condor/oauth_credentials/wsgi_session_key
%if 0%{?rhel} == 7
###
# Backwards compatibility with the previous versions and configs of scitokens-credmon
%_var/www/wsgi-scripts/scitokens-credmon
###
%endif

%files credmon-vault
%doc examples/condor_credmon_oauth
%_sbindir/condor_credmon_vault
%_bindir/condor_vault_storer
%_libexecdir/condor/credmon
%config(noreplace) %_sysconfdir/condor/config.d/40-vault-credmon.conf
%ghost %_var/lib/condor/oauth_credentials/CREDMON_COMPLETE
%ghost %_var/lib/condor/oauth_credentials/pid

%files -n minicondor
%config(noreplace) %_sysconfdir/condor/config.d/00-minicondor

%files ap
%config(noreplace) %_sysconfdir/condor/config.d/00-access-point

%files ep
%config(noreplace) %_sysconfdir/condor/config.d/00-execution-point

%post
/sbin/ldconfig
# Remove obsolete security configuration
rm -f /etc/condor/config.d/00-htcondor-9.0.config
%if 0%{?fedora}
test -x /usr/sbin/selinuxenabled && /usr/sbin/selinuxenabled
if [ $? = 0 ]; then
   restorecon -R -v /var/lock/condor
   setsebool -P condor_domain_can_network_connect 1
   setsebool -P daemons_enable_cluster_mode 1
   semanage port -a -t condor_port_t -p tcp 12345
   # the number of extraneous SELinux warnings on f17 is very high
fi
%endif
%if 0%{?rhel} >= 7
test -x /usr/sbin/selinuxenabled && /usr/sbin/selinuxenabled
if [ $? = 0 ]; then
   /usr/sbin/semodule -i /usr/share/condor/htcondor.pp
%if 0%{?rhel} < 9
   /usr/sbin/setsebool -P condor_domain_can_network_connect 1
%endif
   /usr/sbin/setsebool -P daemons_enable_cluster_mode 1
fi
%endif
if [ $1 -eq 1 ] ; then
    # Initial installation 
    /bin/systemctl daemon-reload >/dev/null 2>&1 || :
fi

%preun
if [ $1 -eq 0 ] ; then
    # Package removal, not upgrade
    /bin/systemctl --no-reload disable condor.service > /dev/null 2>&1 || :
    /bin/systemctl stop condor.service > /dev/null 2>&1 || :
fi

%postun
/sbin/ldconfig
/bin/systemctl daemon-reload >/dev/null 2>&1 || :
# Note we don't try to restart - HTCondor will automatically notice the
# binary has changed and do graceful or peaceful restart, based on its
# configuration

%triggerun -- condor < 7.7.0-0.5

/usr/bin/systemd-sysv-convert --save condor >/dev/null 2>&1 ||:

/sbin/chkconfig --del condor >/dev/null 2>&1 || :
/bin/systemctl try-restart condor.service >/dev/null 2>&1 || :

%changelog
* Thu Oct 10 2024 Tim Theisen <tim@cs.wisc.edu> - 23.0.16-1
- Backport all cgroup v2 fixes and enhancements from the 23.10.1 release

* Thu Oct 03 2024 Tim Theisen <tim@cs.wisc.edu> - 23.10.1-1
- Improvements to disk usage enforcement when using LVM
  - Can encrypt job sandboxes when using LVM
  - More precise tracking of disk usage when using LVM
  - Reduced disk usage tracking overhead
- Improvements tracking CPU and memory usage with cgroup v2 (on EL9)
  - Don't count kernel cache pages against job's memory usage
  - Avoid rare inclusion of previous job's CPU and peak memory usage
- HTCondor now re-checks DNS before re-connecting to a collector
- HTCondor now writes out per job epoch history
- HTCondor can encrypt network connections without requiring authentication
- htcondor CLI can now show status for local server, AP, and CM
- htcondor CLI can now display OAUTH2 credentials
- Uses job's sandbox to convert image format for Singularity/Apptainer
- Bug fix to not lose GPUs in Docker job on systemd reconfig
- Bug fix for PID namespaces and condor_ssh_to_job on EL9

* Mon Sep 30 2024 Tim Theisen <tim@cs.wisc.edu> - 23.0.15-1
- Fix bug where Docker universe jobs reported zero memory usage on EL9
- Fix bug where Docker universe images would not be removed from EP cache
- Fix bug where condor_watch_q could crash
- Fix bug that could cause the file transfer hold reason to be truncated
- Fix bug where a Windows job with a bad executable would not go on hold

* Thu Aug 08 2024 Tim Theisen <tim@cs.wisc.edu> - 23.9.6-1
- Add config knob to not have cgroups count kernel memory for jobs on EL9
- Remove support for numeric unit suffixes (k,M,G) in ClassAd expressions
- In submit files, request_disk & request_memory still accept unit suffixes
- Hide GPUs not allocated to the job on cgroup v2 systems such as EL9
- DAGMan can now produce credentials when using direct submission
- Singularity jobs have a contained home directory when file transfer is on
- Avoid using IPv6 link local addresses when resolving hostname to IP addr
- New 'htcondor credential' command to aid in debugging

* Thu Aug 08 2024 Tim Theisen <tim@cs.wisc.edu> - 23.0.14-1
- Docker and Container jobs run on EPs that match AP's CPU architecture
- Fixed premature cleanup of credentials by the condor_credd
- Fixed bug where a malformed SciToken could cause a condor_schedd crash
- Fixed crash in condor_annex script
- Fixed daemon crash after IDTOKEN request is approved by the collector

* Thu Jun 27 2024 Tim Theisen <tim@cs.wisc.edu> - 23.8.1-1
- Add new condor-ap package to facilitate Access Point installation
- HTCondor Docker images are now based on Alma Linux 9
- HTCondor Docker images are now available for the arm64 CPU architecture
- The user can now choose which submit method DAGMan will use
- Can add custom attributes to the User ClassAd with condor_qusers -edit
- Add use-projection option to condor_gangliad to reduce memory footprint
- Fix bug where interactive submit does not work on cgroup v2 systems (EL9)

* Thu Jun 13 2024 Tim Theisen <tim@cs.wisc.edu> - 23.0.12-1
- Remote condor_history queries now work the same as local queries
- Improve error handling when submitting to a remote scheduler via ssh
- Fix bug on Windows where condor_procd may crash when suspending a job
- Fix Python binding crash when submitting a DAG which has empty lines

* Thu May 16 2024 Tim Theisen <tim@cs.wisc.edu> - 23.7.2-1
- Warns about deprecated multiple queue statements in a submit file
- The semantics of 'skip_if_dataflow' have been improved
- Removing large DAGs is now non-blocking, preserving schedd performance
- Periodic policy expressions are now checked during input file transfer
- Local universe jobs can now specify a container image
- File transfer plugins can now advertise extra attributes
- DAGMan can rescue and abort if pending jobs are missing from the job queue
- Fix so 'condor_submit -interactive' works on cgroup v2 execution points

* Thu May 09 2024 Tim Theisen <tim@cs.wisc.edu> - 23.0.10-1
- Preliminary support for Ubuntu 22.04 (Noble Numbat)
- Warns about deprecated multiple queue statements in a submit file
- Fix bug where plugins could not signify to retry a file transfer
- The condor_upgrade_check script checks for proper token file permissions
- Fix bug where the condor_upgrade_check script crashes on older platforms
- The bundled version of apptainer was moved to libexec in the tarball

* Tue Apr 16 2024 Tim Theisen <tim@cs.wisc.edu> - 23.6.2-1
- Fix bug where file transfer plugin error was not in hold reason code

* Mon Apr 15 2024 Tim Theisen <tim@cs.wisc.edu> - 23.6.1-1
- Add the ability to force vanilla universe jobs to run in a container
- Add the ability to override the entrypoint for a Docker image
- condor_q -better-analyze includes units for memory and disk quantities

* Thu Apr 11 2024 Tim Theisen <tim@cs.wisc.edu> - 23.0.8-1
- Fix bug where ssh-agent processes were leaked with grid universe jobs
- Fix DAGMan crash when a provisioner node was given a parent
- Fix bug that prevented use of "ftp:" URLs in file transfer
- Fix bug where jobs that matched an offline slot never start

* Mon Mar 25 2024 Tim Theisen <tim@cs.wisc.edu> - 23.5.3-1
- HTCondor tarballs now contain Pelican 7.6.2

* Thu Mar 14 2024 Tim Theisen <tim@cs.wisc.edu> - 23.5.2-1
- Old ClassAd based syntax is disabled by default for the job router
- Can efficiently manage/enforce disk space using LVM partitions
- GPU discovery is enabled on all Execution Points by default
- Prevents accessing unallocated GPUs using cgroup v1 enforcement
- New condor_submit commands for constraining GPU properties
- Add ability to transfer EP's starter log back to the Access Point
- Can use VOMS attributes when mapping identities of SSL connections
- The CondorVersion string contains the source git SHA

* Thu Mar 14 2024 Tim Theisen <tim@cs.wisc.edu> - 23.0.6-1
- Fix DAGMan where descendants of removed retry-able jobs are marked futile
- Ensure the condor_test_token works correctly when invoked as root
- Fix bug where empty multi-line values could cause a crash
- condor_qusers returns proper exit code for errors in formatting options
- Fix crash in job router when a job transform is missing an argument

* Thu Feb 08 2024 Tim Theisen <tim@cs.wisc.edu> - 23.4.0-1
- condor_submit warns about unit-less request_disk and request_memory
- Separate condor-credmon-local RPM package provides local SciTokens issuer
- Fix bug where NEGOTIATOR_SLOT_CONSTRAINT was ignored since version 23.3.0
- The htcondor command line tool can process multiple event logs at once
- Prevent Docker daemon from keeping a duplicate copy of the job's stdout

* Thu Feb 08 2024 Tim Theisen <tim@cs.wisc.edu> - 23.0.4-1
- NVIDIA_VISIBLE_DEVICES environment variable lists full uuid of slot GPUs
- Fix problem where some container jobs would see GPUs not assigned to them
- Restore condor keyboard monitoring that was broken since HTCondor 23.0.0
- In condor_adstash, the search engine timeouts now apply to all operations
- Ensure the prerequisite perl modules are installed for condor_gather_info

* Tue Jan 23 2024 Tim Theisen <tim@cs.wisc.edu> - 23.3.1-1
- HTCondor tarballs now contain Pelican 7.4.0

* Thu Jan 04 2024 Tim Theisen <tim@cs.wisc.edu> - 23.3.0-1
- Restore limited support for Enterprise Linux 7 systems
- Additional assistance converting old syntax job routes to new syntax
- Able to capture output to debug DAGMan PRE and POST scripts
- Execution Points advertise when jobs are running with cgroup enforcement

* Thu Jan 04 2024 Tim Theisen <tim@cs.wisc.edu> - 23.0.3-1
- Preliminary support for openSUSE LEAP 15
- All non-zero exit values from file transfer plugins are now errors
- Fix crash in Python bindings when job submission fails
- Chirp uses a 5120 byte buffer and errors out for bigger messages
- condor_adstash now recognizes GPU usage values as floating point numbers

* Wed Nov 29 2023 Tim Theisen <tim@cs.wisc.edu> - 23.2.0-1
- Add 'periodic_vacate' submit command to restart jobs that are stuck
- EPs now advertises whether the execute directory is on rotational storage
- Add two log events for the time a job was running and occupied a slot
- Files written by HTCondor are now written in binary mode on Windows
- HTCondor now uses the Pelican Platform for OSDF file transfers

* Mon Nov 20 2023 Tim Theisen <tim@cs.wisc.edu> - 23.0.2-1
- Fix bug where OIDC login information was missing when submitting jobs
- Improved sandbox and ssh-agent clean up for batch grid universe jobs
- Fix bug where daemons with a private network address couldn't communicate
- Fix cgroup v2 memory enforcement for custom configurations
- Add DISABLE_SWAP_FOR_JOB support on cgroup v2 systems
- Fix log rotation for OAuth and Vault credmon daemons

* Thu Nov 16 2023 Tim Theisen <tim@cs.wisc.edu> - 9.0.20-1
- Other authentication methods are tried if mapping fails using SSL

* Tue Oct 31 2023 Tim Theisen <tim@cs.wisc.edu> - 23.1.0-1
- Enhanced filtering with 'condor_watch_q'
- Can specify alternate ssh port with 'condor_remote_cluster'
- Performance improvement for the 'condor_schedd' and other daemons
- Jobs running on cgroup v2 systems can subdivide their cgroup
- The curl plugin can now find CA certificates via an environment variable

* Tue Oct 31 2023 Tim Theisen <tim@cs.wisc.edu> - 23.0.1-1
- Fix 10.6.0 bug that broke PID namespaces
- Fix bug where execution times for ARC CE jobs were 60 times too large
- Fix bug where a failed 'Service' node would crash DAGMan
- Condor-C and Job Router jobs now get resources provisioned updates

* Fri Sep 29 2023 Tim Theisen <tim@cs.wisc.edu> - 23.0.0-1
- Absent slot configuration, execution points will use a partitionable slot
- Linux cgroups enforce maximum memory utilization by default
- Can now define DAGMan save points to be able to rerun DAGs from there
- Much better control over environment variables when using DAGMan
- Administrators can enable and disable job submission for a specific user
- Can set a minimum number of CPUs allocated to a user
- condor_status -gpus shows nodes with GPUs and the GPU properties
- condor_status -compact shows a row for each slot type
- Container images may now be transferred via a file transfer plugin
- Support for Enterprise Linux 9, Amazon Linux 2023, and Debian 12
- Can write job information in AP history file for every execution attempt
- Can run defrag daemons with different policies on distinct sets of nodes
- Add condor_test_token tool to generate a short lived SciToken for testing
- The job’s executable is no longer renamed to ‘condor_exec.exe’

* Thu Sep 28 2023 Tim Theisen <tim@cs.wisc.edu> - 10.9.0-1
- The condor_upgrade_check script now provides guidance on updating to 23.0
- The htchirp Python binding now properly locates the chirp configuration
- Fix bug that prevented deletion of HTCondor passwords on Windows

* Thu Sep 28 2023 Tim Theisen <tim@cs.wisc.edu> - 10.0.9-1
- The condor_upgrade_check script now provides guidance on updating to 23.0
- The htchirp Python binding now properly locates the chirp configuration
- Fix bug that prevented deletion of HTCondor passwords on Windows

* Thu Sep 14 2023 Tim Theisen <tim@cs.wisc.edu> - 10.8.0-1
- Fold the classads, blahp, and procd RPMs into the main condor RPM
- Align the Debian packages and package names with the RPM packaging
- On Linux, the default configuration enforces memory limits with cgroups
- condor_status -gpus shows nodes with GPUs and the GPU properties
- condor_status -compact shows a row for each slot type
- New ENV command controls which environment variables are present in DAGMan

* Thu Sep 14 2023 Tim Theisen <tim@cs.wisc.edu> - 10.0.8-1
- Avoid kernel panic on some Enterprise Linux 8 systems
- Fix bug where early termination of service nodes could crash DAGMan
- Limit email about long file transfer queue to once daily
- Various fixes to condor_adstash

* Wed Aug 09 2023 Tim Theisen <tim@cs.wisc.edu> - 10.7.1-1
- Fix performance problem detecting futile nodes in a large and bushy DAG

* Mon Jul 31 2023 Tim Theisen <tim@cs.wisc.edu> - 10.7.0-1
- Support for Debian 12 (Bookworm)
- Can run defrag daemons with different policies on distinct sets of nodes
- Added want_io_proxy submit command
- Apptainer is now included in the HTCondor tarballs
- Fix 10.5.0 bug where reported CPU time is very low when using cgroups v1
- Fix 10.5.0 bug where .job.ad and .machine.ad were missing for local jobs

* Tue Jul 25 2023 Tim Theisen <tim@cs.wisc.edu> - 10.0.7-1
- Fixed bug where held condor cron jobs would never run when released
- Improved daemon IDTOKENS logging to make useful messages more prominent
- Remove limit on certificate chain length in SSL authentication
- condor_config_val -summary now works with a remote configuration query
- Prints detailed message when condor_remote_cluster fails to fetch a URL
- Improvements to condor_preen

* Fri Jun 30 2023 Tim Theisen <tim@cs.wisc.edu> - 9.0.19-1
- Remove limit on certificate chain length in SSL authentication

* Thu Jun 29 2023 Tim Theisen <tim@cs.wisc.edu> - 10.6.0-1
- Administrators can enable and disable job submission for a specific user
- Work around memory leak in libcurl on EL7 when using the ARC-CE GAHP
- Container images may now be transferred via a file transfer plugin
- Add ClassAd stringlist subset match function
- Add submit file macro '$(JobId)' which expands to full ID of the job
- The job's executable is no longer renamed to 'condor_exec.exe'

* Thu Jun 22 2023 Tim Theisen <tim@cs.wisc.edu> - 10.0.6-1
- In SSL Authentication, use the identity instead of the X.509 proxy subject
- Can use environment variable to locate the client's SSL X.509 credential
- ClassAd aggregate functions now tolerate undefined values
- Fix Python binding bug where accounting ads were omitted from the result
- The Python bindings now properly report the HTCondor version
- remote_initial_dir works when submitting a grid batch job remotely via ssh
- Add a ClassAd stringlist subset match function

* Thu Jun 22 2023 Tim Theisen <tim@cs.wisc.edu> - 9.0.18-1
- Can configure clients to present an X.509 proxy during SSL authentication
- Provides script to assist updating from HTCondor version 9 to version 10

* Fri Jun 09 2023 Tim Theisen <tim@cs.wisc.edu> - 10.0.5-1
- Rename upgrade9to10checks.py script to condor_upgrade_check
- Fix spurious warning from condor_upgrade_check about regexes with spaces

* Tue Jun 06 2023 Tim Theisen <tim@cs.wisc.edu> - 10.5.1-1
- Fix issue with grid batch jobs interacting with older Slurm versions

* Mon Jun 05 2023 Tim Theisen <tim@cs.wisc.edu> - 10.5.0-1
- Can now define DAGMan save points to be able to rerun DAGs from there
- Expand default list of environment variables passed to the DAGMan manager
- Administrators can prevent users using "getenv = true" in submit files
- Improved throughput when submitting a large number of ARC-CE jobs
- Execute events contain the slot name, sandbox path, resource quantities
- Can add attributes of the execution point to be recorded in the user log
- Enhanced condor_transform_ads tool to ease offline job transform testing
- Fixed a bug where memory limits over 2 GiB might not be correctly enforced

* Tue May 30 2023 Tim Theisen <tim@cs.wisc.edu> - 10.0.4-1
- Provides script to assist updating from HTCondor version 9 to version 10
- Fixes a bug where rarely an output file would not be transferred back
- Fixes counting of submitted jobs, so MAX_JOBS_SUBMITTED works correctly
- Fixes SSL Authentication failure when PRIVATE_NETWORK_NAME was set
- Fixes rare crash when SSL or SCITOKENS authentication was attempted
- Can allow client to present an X.509 proxy during SSL authentication
- Fixes issue where a users jobs were ignored by the HTCondor-CE on restart
- Fixes issues where some events that HTCondor-CE depends on were missing

* Tue May 30 2023 Tim Theisen <tim@cs.wisc.edu> - 9.0.17-3
- Improved upgrade9to10checks.py script

* Tue May 09 2023 Tim Theisen <tim@cs.wisc.edu> - 9.0.17-2
- Add upgrade9to10checks.py script

* Tue May 09 2023 Tim Theisen <tim@cs.wisc.edu> - 10.4.3-1
- Fix bug than could cause the collector audit plugin to crash

* Tue May 02 2023 Tim Theisen <tim@cs.wisc.edu> - 10.4.2-1
- Fix bug where remote submission of batch grid universe jobs fail
- Fix bug where HTCondor-CE fails to handle jobs after HTCondor restarts

* Wed Apr 12 2023 Tim Theisen <tim@cs.wisc.edu> - 10.4.1-1
- Preliminary support for Ubuntu 20.04 (Focal Fossa) on PowerPC (ppc64el)

* Thu Apr 06 2023 Tim Theisen <tim@cs.wisc.edu> - 10.4.0-1
- DAGMan no longer carries the entire environment into the DAGMan job
- Allows EGI CheckIn tokens to be used the with SciTokens authentication

* Thu Apr 06 2023 Tim Theisen <tim@cs.wisc.edu> - 10.0.3-1
- GPU metrics continues to be reported after the startd is reconfigured
- Fixed issue where GPU metrics could be wildly over-reported
- Fixed issue that kept jobs from running when installed on Debian or Ubuntu
- Fixed DAGMan problem when retrying a proc failure in a multi-proc node

* Tue Mar 07 2023 Tim Theisen <tim@cs.wisc.edu> - 10.3.1-1
- Execution points now advertise if an sshd is available for ssh to job

* Mon Mar 06 2023 Tim Theisen <tim@cs.wisc.edu> - 10.3.0-1
- Now evicts OOM killed jobs when they are under their requested memory
- HTCondor glideins can now use cgroups if one has been prepared
- Can write job information in an AP history file for each execution attempt
- Can now specify a lifetime for condor_gangliad metrics
- The condor_schedd now advertises a count of unmaterialized jobs

* Thu Mar 02 2023 John Knoeller <johnkn@cs.wisc.edu> - 10.0.2-1
- HTCondor can optionally create intermediate directories for output files
- Improved condor_schedd scalability when a user runs more than 1,000 jobs
- Fix issue where condor_ssh_to_job fails if the user is not in /etc/passwd
- The Python Schedd.query() now returns the ServerTime attribute for Fifemon
- VM Universe jobs pass through the host CPU model to support newer kernels
- HTCondor Python wheel is now available for Python 3.11
- Fix issue that prevented HTCondor installation on Ubuntu 18.04

* Tue Feb 28 2023 Tim Theisen <tim@cs.wisc.edu> - 10.2.5-1
- Fix counting of unmaterialized jobs in the condor_schedd

* Fri Feb 24 2023 Tim Theisen <tim@cs.wisc.edu> - 10.2.4-1
- Improve counting of unmaterialized jobs in the condor_schedd

* Tue Feb 21 2023 Tim Theisen <tim@cs.wisc.edu> - 10.2.3-1
- Add a count of unmaterialized jobs to condor_schedd statistics

* Tue Feb 07 2023 Tim Theisen <tim@cs.wisc.edu> - 10.2.2-1
- Fixed bugs with configuration knob SINGULARITY_USE_PID_NAMESPACES

* Tue Jan 24 2023 Tim Theisen <tim@cs.wisc.edu> - 10.2.1-1
- Improved condor_schedd scalability when a user runs more than 1,000 jobs
- Fix issue where condor_ssh_to_job fails if the user is not in /etc/passwd
- The Python Schedd.query() now returns the ServerTime attribute
- Fixed issue that prevented HTCondor installation on Ubuntu 18.04

* Thu Jan 05 2023 Tim Theisen <tim@cs.wisc.edu> - 10.2.0-1
- Preliminary support for Enterprise Linux 9
- Preliminary support for cgroups v2
- Can now set minimum floor for number of CPUs that a submitter gets
- Improved validity testing of Singularity/Apptainer runtinme
- Improvements to jobs hooks, including new PREPARE_JOB_BEFORE_TRANSFER hook
- OpenCL jobs now work inside Singularity, if OpenCL drivers are on the host

* Thu Jan 05 2023 Tim Theisen <tim@cs.wisc.edu> - 10.0.1-1
- Add Ubuntu 22.04 (Jammy Jellyfish) support
- Add file transfer plugin that supports stash:// and osdf:// URLs
- Fix bug where cgroup memory limits were not enforced on Debian and Ubuntu
- Fix bug where forcibly removing DAG jobs could crash the condor_schedd
- Fix bug where Docker repository images cannot be run under Singularity
- Fix issue where blahp scripts were missing on Debian and Ubuntu platforms
- Fix bug where curl file transfer plugins would fail on Enterprise Linux 8

* Tue Nov 22 2022 Tim Theisen <tim@cs.wisc.edu> - 10.1.3-1
- Improvements to jobs hooks, including new PREPARE_JOB_BEFORE_TRANSFER hook

* Tue Nov 15 2022 Tim Theisen <tim@cs.wisc.edu> - 10.1.2-1
- OpenCL jobs now work inside Singularity, if OpenCL drivers are on the host

* Thu Nov 10 2022 Tim Theisen <tim@cs.wisc.edu> - 10.1.1-1
- Improvements to job hooks and the ability to save stderr from a job hook
- Fix bug where Apptainer only systems couldn't run with Docker style images

* Thu Nov 10 2022 Tim Theisen <tim@cs.wisc.edu> - 10.1.0-1
- Release HTCondor 10.0.0 bug fixes into 10.1.0

* Thu Nov 10 2022 Tim Theisen <tim@cs.wisc.edu> - 10.0.0-1
- Users can prevent runaway jobs by specifying an allowed duration
- Able to extend submit commands and create job submit templates
- Initial implementation of htcondor <noun> <verb> command line interface
- Initial implementation of Job Sets in the htcondor CLI tool
- Add Container Universe
- Support for heterogeneous GPUs
- Improved File transfer error reporting
- GSI Authentication method has been removed
- HTCondor now utilizes ARC-CE's REST interface
- Support for ARM and PowerPC for Enterprise Linux 8
- For IDTOKENS, signing key not required on every execution point
- Trust on first use ability for SSL connections
- Improvements against replay attacks

* Wed Oct 05 2022 Tim Theisen <tim@cs.wisc.edu> - 9.12.0-1
- Provide a mechanism to bootstrap secure authentication within a pool
- Add the ability to define submit templates
- Administrators can now extend the help offered by condor_submit
- Add DAGMan ClassAd attributes to record more information about jobs
- On Linux, advertise the x86_64 micro-architecture in a slot attribute
- Added -drain option to condor_off and condor_restart
- Administrators can now set the shared memory size for Docker jobs
- Multiple improvements to condor_adstash
- HAD daemons now use SHA-256 checksums by default

* Thu Sep 29 2022 Tim Theisen <tim@cs.wisc.edu> - 9.0.17-1
- Fix file descriptor leak when schedd fails to launch scheduler jobs
- Fix failure to forward batch grid universe job's refreshed X.509 proxy
- Fix DAGMan failure when the DONE keyword appeared in the JOB line
- Fix HTCondor's handling of extremely large UIDs on Linux
- Fix bug where OAUTH tokens lose their scope and audience upon refresh
- Support for Apptainer in addition to Singularity

* Tue Sep 13 2022 Tim Theisen <tim@cs.wisc.edu> - 9.11.2-1
- In 9.11.0, STARTD_NOCLAIM_SHUTDOWN restarted instead. Now, it shuts down.

* Tue Sep 06 2022 Tim Theisen <tim@cs.wisc.edu> - 9.11.1-1
- File transfer errors are identified as occurring during input or output

* Thu Aug 25 2022 Tim Theisen <tim@cs.wisc.edu> - 9.11.0-1
- Modified GPU attributes to support the new 'require_gpus' submit command
- Add (PREEMPT|HOLD)_IF_DISK_EXCEEDED configuration templates
- ADVERTISE authorization levels now also provide READ authorization
- Periodic release expressions no longer apply to manually held jobs
- If a #! interpreter doesn't exist, a proper hold and log message appears
- Can now set the Singularity target directory with 'container_target_dir'
- If SciToken and X.509 available, uses SciToken for arc job authentication

* Tue Aug 16 2022 Tim Theisen <tim@cs.wisc.edu> - 9.0.16-1
- Singularity now mounts /tmp and /var/tmp under the scratch directory
- Fix bug where Singularity jobs go on hold at the first checkpoint
- Fix bug where gridmanager deletes the X.509 proxy file instead of the copy
- Fix file descriptor leak when using SciTokens for authentication

* Thu Jul 21 2022 Tim Theisen <tim@cs.wisc.edu> - 9.0.15-1
- Report resources provisioned by the Slurm batch scheduler when available

* Mon Jul 18 2022 Tim Theisen <tim@cs.wisc.edu> - 9.10.1-1
- ActivationSetupDuration is now correct for jobs that checkpoint

* Thu Jul 14 2022 Tim Theisen <tim@cs.wisc.edu> - 9.10.0-1
- With collector administrator access, can manage all HTCondor pool daemons
- SciTokens can now be used for authentication with ARC CE servers
- Preliminary support for ARM and POWER RC on AlmaLinux 8
- Prevent negative values when using huge files with a file transfer plugin

* Tue Jul 12 2022 Tim Theisen <tim@cs.wisc.edu> - 9.0.14-1
- SciToken mapping failures are now recorded in the daemon logs
- Fix bug that stopped file transfers when output and error are the same
- Ensure that the Python bindings version matches the installed HTCondor
- $(OPSYSANDVER) now expand properly in job transforms
- Fix bug where context managed Python htcondor.SecMan sessions would crash
- Fix bug where remote CPU times would rarely be set to zero

* Tue Jun 14 2022 Tim Theisen <tim@cs.wisc.edu> - 9.9.1-1
- Fix bug where jobs would not match when using a child collector

* Tue May 31 2022 Tim Theisen <tim@cs.wisc.edu> - 9.9.0-1
- A new authentication method for remote HTCondor administration
- Several changes to improve the security of connections
- Fix issue where DAGMan direct submission failed when using Kerberos
- The submission method is now recorded in the job ClassAd
- Singularity jobs can now pull from Docker style repositories
- The OWNER authorization level has been folded into the ADMINISTRATOR level

* Thu May 26 2022 Tim Theisen <tim@cs.wisc.edu> - 9.0.13-1
- Schedd and startd cron jobs can now log output upon non-zero exit
- condor_config_val now produces correct syntax for multi-line values
- The condor_run tool now reports submit errors and warnings to the terminal
- Fix issue where Kerberos authentication would fail within DAGMan
- Fix HTCondor startup failure with certain complex network configurations

* Mon Apr 25 2022 Tim Theisen <tim@cs.wisc.edu> - 9.8.1-1
- Fix HTCondor startup failure with certain complex network configurations

* Thu Apr 21 2022 Tim Theisen <tim@cs.wisc.edu> - 9.8.0-1
- Support for Heterogeneous GPUs, some configuration required
- Allow HTCondor to utilize grid sites requiring two-factor authentication
- Technology preview: bring your own resources from (some) NSF HPC clusters

* Tue Apr 19 2022 Tim Theisen <tim@cs.wisc.edu> - 9.0.12-1
- Fix bug in parallel universe that could cause the schedd to crash
- Fix rare crash where a daemon tries to use a discarded security session

* Tue Apr 05 2022 Tim Theisen <tim@cs.wisc.edu> - 9.7.1-1
- Fix recent bug where jobs may go on hold without a hold reason or code

* Tue Mar 15 2022 Tim Theisen <tim@cs.wisc.edu> - 9.7.0-1
- Support environment variables, other application elements in ARC REST jobs
- Container universe supports Singularity jobs with hard-coded command
- DAGMan submits jobs directly (does not shell out to condor_submit)
- Meaningful error message and sub-code for file transfer failures
- Add file transfer statistics for file transfer plugins
- Add named list policy knobs for SYSTEM_PERIODIC_ policies

* Tue Mar 15 2022 Tim Theisen <tim@cs.wisc.edu> - 9.0.11-1
- The Job Router can now create an IDTOKEN for use by the job
- Fix bug where a self-checkpointing job may erroneously be held
- Fix bug where the Job Router could erroneously substitute a default value
- Fix bug where a file transfer error may identify the wrong file
- Fix bug where condor_ssh_to_job may fail to connect

* Tue Mar 15 2022 Tim Theisen <tim@cs.wisc.edu> - 8.8.17-1
- Fixed a memory leak in the Job Router

* Tue Mar 15 2022 Tim Theisen <tim@cs.wisc.edu> - 9.6.0-1
- Fixes for security issues
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0001.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0002.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0003.html

* Tue Mar 15 2022 Tim Theisen <tim@cs.wisc.edu> - 9.0.10-1
- Fixes for security issues
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0001.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0002.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0003.html

* Tue Mar 15 2022 Tim Theisen <tim@cs.wisc.edu> - 8.8.16-1
- Fix for security issue
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2022-0003.html

* Tue Feb 08 2022 Tim Theisen <tim@cs.wisc.edu> - 9.5.4-1
- The access point more robustly detects execution points that disappear
- The condor_procd will now function if /proc is mounted with hidepid=2

* Tue Feb 01 2022 Tim Theisen <tim@cs.wisc.edu> - 9.5.3-1
- Fix daemon crash where one of multiple collectors is not in DNS
- Fix bug where initial schedd registration was rarely delayed by an hour
- Can set CCB_TIMEOUT and choose to not start up if CCB address unavailable

* Tue Jan 25 2022 Tim Theisen <tim@cs.wisc.edu> - 9.5.2-1
- Fix bug where job may not go on hold when exceeding allowed_job_duration
- Fix bug where the condor_shadow could run indefinitely
- Fix bug where condor_ssh_to_job may fail to connect
- Fix bug where a file transfer error may identify the wrong file

* Tue Jan 18 2022 Tim Theisen <tim@cs.wisc.edu> - 9.5.1-1
- Fix bug where a self-checkpointing job may erroneously be held

* Thu Jan 13 2022 Tim Theisen <tim@cs.wisc.edu> - 9.5.0-1
- Initial implementation of Container Universe
- HTCondor will automatically detect container type and where it can run
- The blahp is no longer separate, it is now an integral part of HTCondor
- Docker Universe jobs can now self-checkpoint
- Added Debian 11 (bullseye) as a supported platform
- Since CentOS 8 has reached end of life, we build and test on Rocky Linux 8

* Thu Jan 13 2022 Tim Theisen <tim@cs.wisc.edu> - 9.0.9-1
- Added Debian 11 (bullseye) as a supported platform
- Since CentOS 8 has reached end of life, we build and test on Rocky Linux 8
- The OAUTH credmon is now packaged for Enterprise Linux 8

* Tue Dec 21 2021 Tim Theisen <tim@cs.wisc.edu> - 9.4.1-1
- Add the ability to track slot activation metrics
- Fix bug where a file transfer plugin failure code may not be reported

* Thu Dec 02 2021 Tim Theisen <tim@cs.wisc.edu> - 9.4.0-1
- Initial implementation of Job Sets in the htcondor CLI tool
- The access point administrator can add keywords to the submit language
- Add submit commands that limit job run time
- Fix bug where self check-pointing jobs may be erroneously held

* Thu Dec 02 2021 Tim Theisen <tim@cs.wisc.edu> - 9.0.8-1
- Fix bug where huge values of ImageSize and others would end up negative
- Fix bug in how MAX_JOBS_PER_OWNER applied to late materialization jobs
- Fix bug where the schedd could choose a slot with insufficient disk space
- Fix crash in ClassAd substr() function when the offset is out of range
- Fix bug in Kerberos code that can crash on macOS and could leak memory
- Fix bug where a job is ignored for 20 minutes if the startd claim fails

* Tue Nov 30 2021 Tim Theisen <tim@cs.wisc.edu> - 9.3.2-1
- Add allowed_execute_duration condor_submit command to cap job run time
- Fix bug where self check-pointing jobs may be erroneously held

* Tue Nov 09 2021 Tim Theisen <tim@cs.wisc.edu> - 9.3.1-1
- Add allowed_job_duration condor_submit command to cap job run time

* Wed Nov 03 2021 Tim Theisen <tim@cs.wisc.edu> - 9.3.0-1
- Discontinue support for Globus GSI
- Discontinue support for grid type 'nordugrid', use 'arc' instead
- MacOS version strings now include the major version number (10 or 11)
- File transfer plugin sample code to aid in developing new plugins
- Add generic knob to set the slot user for all slots

* Tue Nov 02 2021 Tim Theisen <tim@cs.wisc.edu> - 9.0.7-1
- Fix bug where condor_gpu_discovery could crash with older CUDA libraries
- Fix bug where condor_watch_q would fail on machines with older kernels
- condor_watch_q no longer has a limit on the number of job event log files
- Fix bug where a startd could crash claiming a slot with p-slot preemption
- Fix bug where a job start would not be recorded when a shadow reconnects

* Thu Sep 23 2021 Tim Theisen <tim@cs.wisc.edu> - 9.2.0-1
- Add SERVICE node that runs alongside the DAG for the duration of the DAG
- Fix problem where proxy delegation to older HTCondor versions failed
- Jobs are now re-run if the execute directory unexpectedly disappears
- HTCondor counts the number of files transfered at the submit node
- Fix a bug that caused jobs to fail when using newer Singularity versions

* Thu Sep 23 2021 Tim Theisen <tim@cs.wisc.edu> - 9.0.6-1
- CUDA_VISIBLE_DEVICES can now contain GPU-<uuid> formatted values
- Fixed a bug that caused jobs to fail when using newer Singularity versions
- Fixed a bug in the Windows MSI installer for the latest Windows 10 version
- Fixed bugs relating to the transfer of standard out and error logs
- MacOS 11.x now reports as 10.16.x (which is better than reporting x.0)

* Thu Aug 19 2021 Tim Theisen <tim@cs.wisc.edu> - 9.1.3-1
- Globus GSI is no longer needed for X.509 proxy delegation
- Globus GSI authentication is disabled by default
- The job ad now contains a history of job holds and hold reasons
- If a user job policy expression evaluates to undefined, it is ignored

* Wed Aug 18 2021 Tim Theisen <tim@cs.wisc.edu> - 9.0.5-1
- Other authentication methods are tried if mapping fails using SciTokens
- Fix rare crashes from successful condor_submit, which caused DAGMan issues
- Fix bug where ExitCode attribute would be suppressed when OnExitHold fired
- condor_who now suppresses spurious warnings coming from netstat
- The online manual now has detailed instructions for installing on MacOS
- Fix bug where misconfigured MIG devices confused condor_gpu_discovery
- The transfer_checkpoint_file list may now include input files

* Thu Jul 29 2021 Tim Theisen <tim@cs.wisc.edu> - 9.1.2-1
- Fixes for security issues
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004.html

* Thu Jul 29 2021 Tim Theisen <tim@cs.wisc.edu> - 9.0.4-1
- Fixes for security issues
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004.html

* Thu Jul 29 2021 Tim Theisen <tim@cs.wisc.edu> - 8.8.15-1
- Fix for security issue
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003.html

* Tue Jul 27 2021 Tim Theisen <tim@cs.wisc.edu> - 9.1.1-1
- Fixes for security issues
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004.html

* Tue Jul 27 2021 Tim Theisen <tim@cs.wisc.edu> - 9.0.3-1
- Fixes for security issues
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0004.html

* Tue Jul 27 2021 Tim Theisen <tim@cs.wisc.edu> - 8.8.14-1
- Fix for security issue
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0003.html

* Thu Jul 08 2021 Tim Theisen <tim@cs.wisc.edu> - 9.0.2-1
- HTCondor can be set up to use only FIPS 140-2 approved security functions
- If the Singularity test fails, the job goes idle rather than getting held
- Can divide GPU memory, when making multiple GPU entries for a single GPU
- Startd and Schedd cron job maximum line length increased to 64k bytes
- Added first class submit keywords for SciTokens
- Fixed MUNGE authentication
- Fixed Windows installer to work when the install location isn't C:\Condor

* Thu May 20 2021 Tim Theisen <tim@cs.wisc.edu> - 9.1.0-1
- Support for submitting to ARC-CE via the REST interface
- DAGMan can put failed jobs on hold (user can correct problems and release)
- Can run gdb and ptrace within Docker containers
- A small Docker test job is run on the execute node to verify functionality
- The number of instructions executed is reported in the job Ad on Linux

* Mon May 17 2021 Tim Theisen <tim@cs.wisc.edu> - 9.0.1-1
- Fix problem where X.509 proxy refresh kills job when using AES encryption
- Fix problem when jobs require a different machine after a failure
- Fix problem where a job matched a machine it can't use, delaying job start
- Fix exit code and retry checking when a job exits because of a signal
- Fix a memory leak in the job router when a job is removed via job policy
- Fixed the back-end support for the 'bosco_cluster --add' command
- An updated Windows installer that supports IDTOKEN authentication

* Wed Apr 14 2021 Tim Theisen <tim@cs.wisc.edu> - 9.0.0-1
- Absent any configuration, HTCondor denies authorization to all users
- AES encryption is used for all communication and file transfers by default
- New IDTOKEN authentication method enables fine-grained authorization
- IDTOKEN authentication method is designed to replace GSI
- Improved support for GPUs, including machines with multiple GPUs
- New condor_watch_q tool that efficiently provides live job status updates
- Many improvements to the Python bindings
- New Python bindings for DAGMan and chirp
- Improved file transfer plugins supporting uploads and authentication
- File transfer times are now recorded in the job log
- Added support for jobs that need to acquire and use OAUTH tokens
- Many memory footprint and performance improvements in DAGMan
- Submitter ceilings can limit the number of jobs per user in a pool

* Tue Mar 30 2021 Tim Theisen <tim@cs.wisc.edu> - 8.9.13-1
- Host based security is no longer the default security model
- Hardware accelerated integrity and AES encryption used by default
- Normally, AES encryption is used for all communication and file transfers
- Fallback to Triple-DES or Blowfish when interoperating with older versions
- Simplified and automated new HTCondor installations
- HTCondor now detects instances of multi-instance GPUs
- Fixed memory leaks (collector updates in 8.9 could leak a few MB per day)
- Many other enhancements and bug fixes, see version history for details

* Thu Mar 25 2021 Tim Theisen <tim@cs.wisc.edu> - 8.9.12-1
- Withdrawn due to compatibility issues with prior releases

* Tue Mar 23 2021 Tim Theisen <tim@cs.wisc.edu> - 8.8.13-1
- condor_ssh_to_job now maps CR and NL to work with editors like nano
- Improved the performance of data transfer in condor_ssh_to_job
- HA replication now accepts SHA-2 checksums to prepare for MD5 removal
- Submission to NorduGrid ARC CE works with newer ARC CE versions
- Fixed condor_annex crashes on platforms with newer compilers
- Fixed "use feature: GPUsMonitor" to locate the monitor binary on Windows
- Fixed a bug that prevented using the '@' character in an event log path

* Wed Jan 27 2021 Tim Theisen <tim@cs.wisc.edu> - 8.9.11-1
- This release of HTCondor fixes security-related bugs described at
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0001.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2021-0002.html

* Tue Nov 24 2020 Tim Theisen <tim@cs.wisc.edu> - 8.9.10-1
- Fix bug where negotiator stopped making matches when group quotas are used
- Support OAuth, SciTokens, and Kerberos credentials in local universe jobs
- The Python schedd.submit method now takes a Submit object
- DAGMan can now optionally run a script when a job goes on hold
- DAGMan now provides a method for inline jobs to share submit descriptions
- Can now add arbitrary tags to condor annex instances
- Runs the "singularity test" before running the a singularity job

* Mon Nov 23 2020 Tim Theisen <tim@cs.wisc.edu> - 8.8.12-1
- Added a family of version comparison functions to ClassAds
- Increased default Globus proxy key length to meet current NIST guidance

* Mon Oct 26 2020 Tim Theisen <tim@cs.wisc.edu> - 8.9.9-1
- The RPM packages requires globus, munge, scitokens, and voms from EPEL
- Improved cgroup memory policy settings that set both hard and soft limit
- Cgroup memory usage reporting no longer includes the kernel buffer cache
- Numerous Python binding improvements, see version history
- Can create a manifest of files on the execute node at job start and finish
- Added provisioner nodes to DAGMan, allowing users to provision resources
- DAGMan can now produce .dot graphs without running the workflow

* Wed Oct 21 2020 Tim Theisen <tim@cs.wisc.edu> - 8.8.11-1
- HTCondor now properly tracks usage over vanilla universe checkpoints
- New ClassAd equality and inequality operators in the Python bindings
- Fixed a bug where removing in-use routes could crash the job router
- Fixed a bug where condor_chirp would abort after success on Windows
- Fixed a bug where using MACHINE_RESOURCE_NAMES could crash the startd
- Improved condor c-gahp to prioritize commands over file transfers
- Fixed a rare crash in the schedd when running many local universe jobs
- With GSI, avoid unnecessary reverse DNS lookup when HOST_ALIAS is set
- Fix a bug that could cause grid universe jobs to fail upon proxy refresh

* Thu Aug 06 2020 Tim Theisen <tim@cs.wisc.edu> - 8.9.8-1
- Added htcondor.dags and htcondor.htchirp to the HTCondor Python bindings
- New condor_watch_q tool that efficiently provides live job status updates
- Added support for marking a GPU offline while other jobs continue
- The condor_master command does not return until it is fully started
- Deprecated several Python interfaces in the Python bindings

* Thu Aug 06 2020 Tim Theisen <tim@cs.wisc.edu> - 8.8.10-1
- condor_qedit can no longer be used to disrupt the condor_schedd
- Fixed a bug where the SHARED_PORT_PORT configuration setting was ignored
- Ubuntu 20.04 and Amazon Linux 2 are now supported
- In MacOSX, HTCondor now requires LibreSSL, available since MacOSX 10.13

* Wed May 20 2020 Tim Theisen <tim@cs.wisc.edu> - 8.9.7-1
- Multiple enhancements in the file transfer code
- Support for more regions in s3:// URLs
- Much more flexible job router language
- Jobs may now specify cuda_version to match equally-capable GPUs
- TOKENS are now called IDTOKENS to differentiate from SCITOKENS
- Added the ability to blacklist TOKENS via an expression
- Can simultaneously handle Kerberos and OAUTH credentials
- The getenv submit command now supports a blacklist and whitelist
- The startd supports a remote history query similar to the schedd
- condor_q -submitters now works with accounting groups
- Fixed a bug reading service account credentials for Google Compute Engine

* Thu May 07 2020 Tim Theisen <tim@cs.wisc.edu> - 8.8.9-1
- Proper tracking of maximum memory used by Docker universe jobs
- Fixed preempting a GPU slot for a GPU job when all GPUs are in use
- Fixed a Python crash when queue_item_data iterator raises an exception
- Fixed a bug where slot attribute overrides were ignored
- Calculates accounting group quota correctly when more than 1 CPU requested
- Updated HTCondor Annex to accommodate API change for AWS Spot Fleet
- Fixed a problem where HTCondor would not start on AWS Fargate
- Fixed where the collector could wait forever for a partial message
- Fixed streaming output to large files (>2Gb) when using the 32-bit shadow

* Mon Apr 06 2020 Tim Theisen <tim@cs.wisc.edu> - 8.9.6-1
- Fixes addressing CVE-2019-18823
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0001.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0002.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0003.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0004.html

* Mon Apr 06 2020 Tim Theisen <tim@cs.wisc.edu> - 8.8.8-1
- Fixes addressing CVE-2019-18823
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0001.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0002.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0003.html
- https://htcondor.org/security/vulnerabilities/HTCONDOR-2020-0004.html

* Thu Jan 02 2020 Tim Theisen <tim@cs.wisc.edu> - 8.9.5-1
- Added a new mode that skips jobs whose outputs are newer than their inputs
- Added command line tool to help debug ClassAd expressions
- Added port forwarding to Docker containers
- You may now change some DAGMan throttles while the DAG is running
- Added support for session tokens for pre-signed S3 URLs
- Improved the speed of the negotiator when custom resources are defined
- Fixed interactive submission of Docker jobs
- Fixed a bug where jobs wouldn't be killed when getting an OOM notification

* Thu Dec 26 2019 Tim Theisen <tim@cs.wisc.edu> - 8.8.7-1
- Updated condor_annex to work with upcoming AWS Lambda function changes
- Added the ability to specify the order that job routes are applied
- Fixed a bug that could cause remote condor submits to fail
- Fixed condor_wait to work when the job event log is on AFS
- Fixed RPM packaging to be able to install condor-all on CentOS 8
- Period ('.') is allowed again in DAGMan node names

* Tue Nov 19 2019 Tim Theisen <tim@cs.wisc.edu> - 8.9.4-1
- Amazon S3 file transfers using pre-signed URLs
- Further reductions in DAGMan memory usage
- Added -idle option to condor_q to display information about idle jobs
- Support for SciTokens authentication
- A tool, condor_evicted_files, to examine the SPOOL of an idle job

* Wed Nov 13 2019 Tim Theisen <tim@cs.wisc.edu> - 8.8.6-1
- Initial support for CentOS 8
- Fixed a memory leak in SSL authentication
- Fixed a bug where "condor_submit -spool" would only submit the first job
- Reduced encrypted file transfer CPU usage by a factor of six
- "condor_config_val -summary" displays changes from a default configuration
- Improved the ClassAd documentation, added many functions that were omitted

* Tue Sep 17 2019 Tim Theisen <tim@cs.wisc.edu> - 8.9.3-1
- TOKEN and SSL authentication methods are now enabled by default
- The job and global event logs use ISO 8601 formatted dates by default
- Added Google Drive multifile transfer plugin
- Added upload capability to Box multifile transfer plugin
- Added Python bindings to submit a DAG
- Python 'JobEventLog' can be pickled to facilitate intermittent readers
- 2x matchmaking speed for partitionable slots with simple START expressions
- Improved the performance of the condor_schedd under heavy load
- Reduced the memory footprint of condor_dagman
- Initial implementation to record the circumstances of a job's termination

* Thu Sep 05 2019 Tim Theisen <tim@cs.wisc.edu> - 8.8.5-1
- Fixed two performance problems on Windows
- Fixed Java universe on Debian and Ubuntu systems
- Added two knobs to improve performance on large scale pools
- Fixed a bug where requesting zero GPUs would require a machine with GPUs
- HTCondor can now recognize nVidia Volta and Turing GPUs

* Tue Jul 09 2019 Tim Theisen <tim@cs.wisc.edu> - 8.8.4-1
- Python 3 bindings - see version history for details (requires EPEL on EL7)
- Can configure DAGMan to dramatically reduce memory usage on some DAGs
- Improved scalability when using the python bindings to qedit jobs
- Fixed infrequent schedd crashes when removing scheduler universe jobs
- The condor_master creates run and lock directories when systemd doesn't
- The condor daemon obituary email now contains the last 200 lines of log

* Tue Jun 04 2019 Tim Theisen <tim@cs.wisc.edu> - 8.9.2-1
- The HTTP/HTTPS file transfer plugin will timeout and retry transfers
- A new multi-file box.com file transfer plugin to download files
- The manual has been moved to Read the Docs
- Configuration options for job-log time-stamps (UTC, ISO 8601, sub-second)
- Several improvements to SSL authentication
- New TOKEN authentication method enables fine-grained authorization control

* Wed May 22 2019 Tim Theisen <tim@cs.wisc.edu> - 8.8.3-1
- Fixed a bug where jobs were killed instead of peacefully shutting down
- Fixed a bug where a restarted schedd wouldn't connect to its running jobs
- Improved file transfer performance when sending multiple files
- Fix a bug that prevented interactive submit from working with Singularity
- Orphaned Docker containers are now cleaned up on execute nodes
- Restored a deprecated Python interface that is used to read the event log

* Wed Apr 17 2019 Tim Theisen <tim@cs.wisc.edu> - 8.9.1-1
- An efficient curl plugin that supports uploads and authentication tokens
- HTCondor automatically supports GPU jobs in Docker and Singularity
- File transfer times are now recorded in the user job log and the job ad

* Thu Apr 11 2019 Tim Theisen <tim@cs.wisc.edu> - 8.8.2-1
- Fixed problems with condor_ssh_to_job and Singularity jobs
- Fixed a problem that could cause condor_annex to crash
- Fixed a problem where the job queue would very rarely be corrupted
- condor_userprio can report concurrency limits again
- Fixed the GPU discovery and monitoring code to map GPUs in the same way
- Made the CHIRP_DELAYED_UPDATE_PREFIX configuration knob work again
- Fixed restarting HTCondor from the Service Control Manager on Windows
- Fixed a problem where local universe jobs could not use condor_submit
- Restored a deprecated Python interface that is used to read the event log
- Fixed a problem where condor_shadow reuse could confuse DAGMan

* Thu Feb 28 2019 Tim Theisen <tim@cs.wisc.edu> - 8.9.0-1
- Absent any configuration, HTCondor denies authorization to all users
- All HTCondor daemons under a condor_master share a security session
- Scheduler Universe jobs are prioritized by job priority

* Tue Feb 19 2019 Tim Theisen <tim@cs.wisc.edu> - 8.8.1-1
- Fixed excessive CPU consumption with GPU monitoring
- GPU monitoring is off by default; enable with "use feature: GPUsMonitor"
- HTCondor now works with the new CUDA version 10 libraries
- Fixed a bug where sometimes jobs would not start on a Windows execute node
- Fixed a bug that could cause DAGman to go into an infinite loop on exit
- The JobRouter doesn't forward the USER attribute between two UID Domains
- Made Collector.locateAll() more efficient in the Python bindings
- Improved efficiency of the negotiation code in the condor_schedd

* Thu Jan 03 2019 Tim Theisen <tim@cs.wisc.edu> - 8.8.0-1
- Automatically add AWS resources to your pool using HTCondor Annex
- The Python bindings now include submit functionality
- Added the ability to run a job immediately by replacing a running job
- A new minicondor package makes single node installations easy
- HTCondor now tracks and reports GPU utilization
- Several performance enhancements in the collector
- The grid universe can create and manage VM instances in Microsoft Azure
- The MUNGE security method is now supported on all Linux platforms

* Wed Oct 31 2018 Tim Theisen <tim@cs.wisc.edu> - 8.7.10-1
- Can now interactively submit Docker jobs
- The administrator can now add arguments to the Singularity command line
- The MUNGE security method is now supported on all Linux platforms
- The grid universe can create and manage VM instances in Microsoft Azure
- Added a single-node package to facilitate using a personal HTCondor

* Wed Oct 31 2018 Tim Theisen <tim@cs.wisc.edu> - 8.6.13-1
- Made the Python 'in' operator case-insensitive for ClassAd attributes
- Python bindings are now built for the Debian and Ubuntu platforms
- Fixed a memory leak in the Python bindings
- Fixed a bug where absolute paths failed for output/error files on Windows
- Fixed a bug using Condor-C to run Condor-C jobs
- Fixed a bug where Singularity could not be used if Docker was not present

* Wed Aug 01 2018 Tim Theisen <tim@cs.wisc.edu> - 8.7.9-1
- Support for Debian 9, Ubuntu 16, and Ubuntu 18
- Improved Python bindings to support the full range of submit functionality
- Allows VMs to shutdown when the job is being gracefully evicted
- Can now specify a host name alias (CNAME) for NETWORK_HOSTNAME
- Added the ability to run a job immediately by replacing a running job

* Wed Aug 01 2018 Tim Theisen <tim@cs.wisc.edu> - 8.6.12-1
- Support for Debian 9, Ubuntu 16, and Ubuntu 18
- Fixed a memory leak that occurred when SSL authentication fails
- Fixed a bug where invalid transform REQUIREMENTS caused a Job to match
- Fixed a bug to allow a queue super user to edit protected attributes
- Fixed a problem setting the job environment in the Singularity container
- Fixed several other minor problems

* Tue May 22 2018 Tim Theisen <tim@cs.wisc.edu> - 8.7.8-2
- Reinstate man pages
- Drop centos from dist tag in 32-bit Enterprise Linux 7 RPMs

* Thu May 10 2018 Tim Theisen <tim@cs.wisc.edu> - 8.7.8-1
- The condor annex can easily use multiple regions simultaneously
- HTCondor now uses CUDA_VISIBLE_DEVICES to tell which GPU devices to manage
- HTCondor now reports GPU memory utilization

* Thu May 10 2018 Tim Theisen <tim@cs.wisc.edu> - 8.6.11-1
- Can now do an interactive submit of a Singularity job
- Shared port daemon is more resilient when starved for TCP ports
- The Windows installer configures the environment for the Python bindings
- Fixed several other minor problems

* Tue Mar 13 2018 Tim Theisen <tim@cs.wisc.edu> - 8.7.7-1
- condor_ssh_to_job now works with Docker Universe jobs
- A 32-bit condor_shadow is available for Enterprise Linux 7 systems
- Tracks and reports custom resources, e.g. GPUs, in the job ad and user log
- condor_q -unmatchable reports jobs that will not match any slots
- Several updates to the parallel universe
- Spaces are now allowed in input, output, and error paths in submit files
- In DAG files, spaces are now allowed in submit file paths

* Tue Mar 13 2018 Tim Theisen <tim@cs.wisc.edu> - 8.6.10-1
- Fixed a problem where condor_preen would crash on an active submit node
- Improved systemd configuration to clean up processes if the master crashes
- Fixed several other minor problems

* Thu Jan 04 2018 Tim Theisen <tim@cs.wisc.edu> - 8.7.6-1
- Machines won't enter "Owner" state unless using the Desktop policy
- One can use SCHEDD and JOB instead of MY and TARGET in SUBMIT_REQUIREMENTS
- HTCondor now reports all submit warnings, not just the first one
- The HTCondor Python bindings in pip are now built from the release branch

* Thu Jan 04 2018 Tim Theisen <tim@cs.wisc.edu> - 8.6.9-1
- Fixed a bug where some Accounting Groups could get too much surplus quota
- Fixed a Python binding bug where some queries could corrupt memory
- Fixed a problem where preen could block the schedd for a long time
- Fixed a bug in Windows where the job sandbox would not be cleaned up
- Fixed problems with the interaction between the master and systemd
- Fixed a bug where MAX_JOBS_SUBMITTED could be permanently reduced
- Fixed problems with very large disk requests

* Tue Nov 14 2017 Tim Theisen <tim@cs.wisc.edu> - 8.7.5-1
- Fixed an issue validating VOMS proxies

* Tue Nov 14 2017 Tim Theisen <tim@cs.wisc.edu> - 8.6.8-1
- Fixed an issue validating VOMS proxies

* Tue Oct 31 2017 Tim Theisen <tim@cs.wisc.edu> - 8.7.4-1
- Improvements to DAGMan including support for late job materialization
- Updates to condor_annex including improved status reporting
- When submitting jobs, HTCondor can now warn about job requirements
- Fixed a bug where remote CPU time was not recorded in the history
- Improved support for OpenMPI jobs
- The high availability daemon now works with IPV6 and shared_port
- The HTCondor Python bindings are now available for Python 2 and 3 in pip

* Tue Oct 31 2017 Tim Theisen <tim@cs.wisc.edu> - 8.6.7-1
- Fixed a bug where memory limits might not be updated in cgroups
- Add SELinux type enforcement rules to allow condor_ssh_to_job to work
- Updated systemd configuration to shutdown HTCondor in an orderly fashion
- The curl_plugin utility can now do HTTPS transfers
- Specifying environment variables now works with the Python Submit class

* Tue Sep 12 2017 Tim Theisen <tim@cs.wisc.edu> - 8.7.3-1
- Further updates to the late job materialization technology preview
- An improved condor_top tool
- Enhanced the AUTO setting for ENABLE_IPV{4,6} to be more selective
- Fixed several small memory leaks

* Tue Sep 12 2017 Tim Theisen <tim@cs.wisc.edu> - 8.6.6-1
- HTCondor daemons no longer crash on reconfig if syslog is used for logging
- HTCondor daemons now reliably leave a core file when killed by a signal
- Negotiator won't match jobs to machines with incompatible IPv{4,6} network
- On Ubuntu, send systemd alive messages to prevent HTCondor restarts
- Fixed a problem parsing old ClassAd string escapes in the python bindings
- Properly parse CPU time used from Slurm grid universe jobs
- Claims are released when parallel univ jobs are removed while claiming
- Starter won't get stuck when a job is removed with JOB_EXIT_HOOK defined
- To reduce audit logging, added cgroup rules to SELinux profile

* Mon Aug 07 2017 Tim Theisen <tim@cs.wisc.edu> - 8.6.5-2
- Update SELinux profile for Red Hat 7.4

* Tue Aug 01 2017 Tim Theisen <tim@cs.wisc.edu> - 8.6.5-1
- Fixed a memory leak that would cause the HTCondor collector to slowly grow
- Prevent the condor_starter from hanging when using cgroups on Debian
- Fixed several issues that occur when IPv6 is in use
- Support for using an ImDisk RAM drive on Windows as the execute directory
- Fixed a bug where condor_rm rarely removed another one of the user's jobs
- Fixed a bug with parallel universe jobs starting on partitionable slots

* Thu Jul 13 2017 Tim Theisen <tim@cs.wisc.edu> - 8.4.12-1
- Can configure the condor_startd to compute free disk space once

* Thu Jun 22 2017 Tim Theisen <tim@cs.wisc.edu> - 8.7.2-1
- Improved condor_schedd performance by turning off file checks by default
- condor_annex -status finds VM instances that have not joined the pool
- Able to update an annex's lease without adding new instances
- condor_annex now keeps a command log
- condor_q produces an expanded multi-line summary
- Automatically retry and/or resume http file transfers when appropriate
- Reduced load on the condor_collector by optimizing queries
- A python based condor_top tool

* Thu Jun 22 2017 Tim Theisen <tim@cs.wisc.edu> - 8.6.4-1
- Python bindings are now available on MacOSX
- Fixed a bug where PASSWORD authentication could fail to exchange keys
- Pslot preemption now properly handles custom resources, such as GPUs
- condor_submit now checks X.509 proxy expiration

* Tue May 09 2017 Tim Theisen <tim@cs.wisc.edu> - 8.6.3-1
- Fixed a bug where using an X.509 proxy might corrupt the job queue log
- Fixed a memory leak in the Python bindings

* Mon Apr 24 2017 Tim Theisen <tim@cs.wisc.edu> - 8.7.1-1
- Several performance enhancements in the collector
- Further refinement and initial documentation of the HTCondor Annex
- Enable chirp for Docker jobs
- Job Router uses first match rather than round-robin matching
- The schedd tracks jobs counts by status for each owner
- Technology preview of late job materialization in the schedd

* Mon Apr 24 2017 Tim Theisen <tim@cs.wisc.edu> - 8.6.2-1
- New metaknobs for mapping users to groups
- Now case-insensitive with Windows user names when storing credentials
- Signal handling in the OpenMPI script
- Report RemoteSysCpu for Docker jobs
- Allow SUBMIT_REQUIREMENT to refer to X509 secure attributes
- Linux kernel tuning script takes into account the machine's role

* Thu Mar 02 2017 Tim Theisen <tim@cs.wisc.edu> - 8.7.0-1
- Performance improvements in collector's ingestion of ClassAds
- Added collector attributes to report query times and forks
- Removed extra white space around parentheses when unparsing ClassAds
- Technology preview of the HTCondor Annex

* Thu Mar 02 2017 Tim Theisen <tim@cs.wisc.edu> - 8.6.1-1
- condor_q works in situations where user authentication is not configured
- Updates to work with Docker version 1.13
- Fix several problems with the Job Router
- Update scripts to support current versions of Open MPI and MPICH2
- Fixed a bug that could corrupt the job queue log when the disk is full

* Thu Jan 26 2017 Tim Theisen <tim@cs.wisc.edu> - 8.6.0-1
- condor_q shows shows only the current user's jobs by default
- condor_q summarizes related jobs (batches) on a single line by default
- Users can define their own job batch name at job submission time
- Immutable/protected job attributes make SUBMIT_REQUIREMENTS more useful
- The shared port daemon is enabled by default
- Jobs run in cgroups by default
- HTCondor can now use IPv6 addresses (Prefers IPv4 when both present)
- DAGMan: Able to easily define SCRIPT, VARs, etc., for all nodes in a DAG
- DAGMan: Revamped priority implementation
- DAGMan: New splice connection feature
- New slurm grid type in the grid universe for submitting to Slurm
- Numerous improvements to Docker support
- Several enhancements in the python bindings

* Mon Jan 23 2017 Tim Theisen <tim@cs.wisc.edu> - 8.4.11-1
- Fixed a bug which delayed startd access to stard cron job results
- Fixed a bug in pslot preemption that could delay jobs starting
- Fixed a bug in job cleanup at job lease expiration if using glexec
- Fixed a bug in locating ganglia shared libraries on Debian and Ubuntu

* Tue Dec 13 2016 Tim Theisen <tim@cs.wisc.edu> - 8.5.8-1
- The starter puts all jobs in a cgroup by default
- Added condor_submit commands that support job retries
- condor_qedit defaults to the current user's jobs
- Ability to add SCRIPTS, VARS, etc. to all nodes in a DAG using one command
- Able to conditionally add Docker volumes for certain jobs
- Initial support for Singularity containers
- A 64-bit Windows release

* Tue Dec 13 2016 Tim Theisen <tim@cs.wisc.edu> - 8.4.10-1
- Updated SELinux profile for Enterprise Linux
- Fixed a performance problem in the schedd when RequestCpus was an expression
- Preserve permissions when transferring sub-directories of the job's sandbox
- Fixed HOLD_IF_CPUS_EXCEEDED and LIMIT_JOB_RUNTIMES metaknobs
- Fixed a bug in handling REMOVE_SIGNIFICANT_ATTRIBUTES

* Thu Sep 29 2016 Tim Theisen <tim@cs.wisc.edu> - 8.5.7-1
- The schedd can perform job ClassAd transformations
- Specifying dependencies between DAGMan splices is much more flexible
- The second argument of the ClassAd ? : operator may be omitted
- Many usability improvements in condor_q and condor_status
- condor_q and condor_status can produce JSON, XML, and new ClassAd output
- To prepare for a 64-bit Windows release, HTCondor identifies itself as X86
- Automatically detect Daemon Core daemons and pass localname to them

* Thu Sep 29 2016 Tim Theisen <tim@cs.wisc.edu> - 8.4.9-1
- The condor_startd removes orphaned Docker containers on restart
- Job Router and HTCondor-C job job submission prompts schedd reschedule
- Fixed bugs in the Job Router's hooks
- Improved systemd integration on Enterprise Linux 7
- Upped default number of Chirp attributes to 100, and made it configurable
- Fixed a bug where variables starting with STARTD. or STARTER. were ignored

* Tue Aug 02 2016 Tim Theisen <tim@cs.wisc.edu> - 8.5.6-1
- The -batch output for condor_q is now the default
- Python bindings for job submission and machine draining
- Numerous Docker usability changes
- New options to limit condor_history results to jobs since last invocation
- Shared port daemon can be used with high availability and replication
- ClassAds can be written out in JSON format
- More flexible ordering of DAGMan commands
- Efficient PBS and SLURM job monitoring
- Simplified leases for grid universe jobs

* Tue Jul 05 2016 Tim Theisen <tim@cs.wisc.edu> - 8.4.8-1
- Fixed a memory leak triggered by the python htcondor.Schedd().query() call
- Fixed a bug that could cause Bosco file transfers to fail
- Fixed a bug that could cause the schedd to crash when using schedd cron jobs
- condor_schedd now rejects jobs when owner has no account on the machine
- Fixed a new bug in 8.4.7 where remote condor_history failed without -limit
- Fixed bugs triggered by the reconfiguration of the high-availability daemon
- Fixed a bug where condor_master could hang when using shared port on Windows 
- Fixed a bug with the -xml option on condor_q and condor_status

* Mon Jun 06 2016 Tim Theisen <tim@cs.wisc.edu> - 8.5.5-1
- Improvements for scalability of EC2 grid universe jobs
- Docker Universe jobs advertises remote user and system CPU time
- Improved systemd support
- The master can now run an administrator defined script at shutdown
- DAGMan includes better support for the batch name feature

* Mon Jun 06 2016 Tim Theisen <tim@cs.wisc.edu> - 8.4.7-1
- fixed a bug that could cause the schedd to become unresponsive
- fixed a bug where the Docker Universe would not set the group ID
- Docker Universe jobs now drop all Linux capabilities by default
- fixed a bug where subsystem specific configuration parameters were ignored
- fixed bugs with history file processing on the Windows platform

* Mon May 02 2016 Tim Theisen <tim@cs.wisc.edu> - 8.5.4-1
- Fixed a bug that delays schedd response when significant attributes change
- Fixed a bug where the group ID was not set in Docker universe jobs
- Limit update rate of various attributes to not overload the collector
- To make job router configuration easier, added implicit "target" scoping
- To make BOSCO work, the blahp does not generate limited proxies by default
- condor_status can now display utilization per machine rather than per slot
- Improve performance of condor_history and other tools

* Thu Apr 21 2016 Tim Theisen <tim@cs.wisc.edu> - 8.4.6-1
- fixed a bug that could cause a job to fail to start in a dynamic slot
- fixed a negotiator memory leak when using partitionable slot preemption
- fixed a bug that caused supplemental groups to be wrong during file transfer
- properly identify the Windows 10 platform
- fixed a typographic error in the LIMIT_JOB_RUNTIMES policy
- fixed a bug where maximum length IPv6 addresses were not parsed

* Thu Mar 24 2016 Tim Theisen <tim@cs.wisc.edu> - 8.5.3-1
- Use IPv6 (and IPv4) interfaces if they are detected
- Prefer IPv4 addresses when both are available
- Count Idle and Running jobs in Submitter Ads for Local and Scheduler universes
- Can submit jobs to SLURM with the new "slurm" type in the Grid universe
- HTCondor is built and linked with Globus 6.0

* Tue Mar 22 2016 Tim Theisen <tim@cs.wisc.edu> - 8.4.5-1
- fixed a bug that would cause the condor_schedd to send no flocked jobs
- fixed a bug that caused a 60 second delay using tools when DNS lookup failed
- prevent using accounting groups with embedded spaces that crash the negotiator
- fixed a bug that could cause use of ports outside the port range on Windows
- fixed a bug that could prevent dynamic slot reuse when using many slots
- fixed a bug that prevented correct utilization reports from the job router
- tune kernel when using cgroups to avoid OOM killing of jobs doing heavy I/O

* Thu Feb 18 2016 Tim Theisen <tim@cs.wisc.edu> - 8.5.2-1
- condor_q now defaults to showing only the current user's jobs
- condor_q -batch produces a single line report for a batch of jobs
- Docker Universe jobs now report and update memory and network usage
- immutable and protected job attributes
- improved performance when querying a HTCondor daemon's location
- Added the ability to set ClassAd attributes within the DAG file
- DAGMan now provides event timestamps in dagman.out

* Tue Feb 02 2016 Tim Theisen <tim@cs.wisc.edu> - 8.4.4-1
- fixed a bug that could cause the collector to crash when DNS lookup fails
- fixed a bug that caused Condor-C jobs with short lease durations to fail
- fixed bugs that affected EC2 grid universe jobs
- fixed a bug that prevented startup if a prior version shared port file exists
- fixed a bug that could cause the condor_shadow to hang on Windows

* Fri Jan 08 2016 Tim Theisen <tim@cs.wisc.edu> - 8.5.1-2
- optimized binaries

* Fri Jan 08 2016 Tim Theisen <tim@cs.wisc.edu> - 8.4.3-2
- optimized binaries

* Mon Dec 21 2015 Tim Theisen <tim@cs.wisc.edu> - 8.5.1-1
- the shared port daemon is enabled by default
- the condor_startd now records the peak memory usage instead of recent
- the condor_startd advertises CPU submodel and cache size
- authorizations are automatically setup when "Match Password" is enabled
- added a schedd-constraint option to condor_q

* Wed Dec 16 2015 Tim Theisen <tim@cs.wisc.edu> - 8.4.3-1
- fixed the processing of the -append option in the condor_submit command
- fixed a bug to run more that 100 dynamic slots on a single execute node
- fixed bugs that would delay daemon startup when using shared port on Windows
- fixed a bug where the cgroup VM limit would not be set for sizes over 2 GiB
- fixed a bug to use the ec2_iam_profile_name for Amazon EC2 Spot instances

* Tue Nov 17 2015 Tim Theisen <tim@cs.wisc.edu> - 8.4.2-1
- a bug fix to prevent the condor_schedd from crashing
- a bug fix to honor TCP_FORWARDING_HOST
- Standard Universe works properly in RPM installations of HTCondor
- the RPM packages no longer claim to provide Globus libraries
- bug fixes to DAGMan's "maximum idle jobs" throttle

* Tue Oct 27 2015 Tim Theisen <tim@cs.wisc.edu> - 8.4.1-1
- four new policy metaknobs to make configuration easier
- a bug fix to prevent condor daemons from crashing on reconfiguration
- an option natural sorting option on condor_status
- support of admin to mount certain directories into Docker containers

* Thu Oct 22 2015 Tim Theisen <tim@cs.wisc.edu> - 8.2.10-1
- an updated RPM to work with SELinux on EL7 platforms
- fixes to the condor_kbdd authentication to the X server
- a fix to allow the condor_kbdd to work with shared port enabled
- avoid crashes when using more than 1024 file descriptors on EL7
- fixed a memory leak in the ClassAd split() function
- condor_vacate will error out rather than ignore conflicting arguments
- a bug fix to the JobRouter to properly process the queue on restart
- a bug fix to prevent sending spurious data on a SOAP file transfer
- a bug fix to always present jobs in order in condor_history

* Mon Oct 12 2015 Tim Theisen <tim@cs.wisc.edu> - 8.5.0-1
- multiple enhancements to the python bindings
- the condor_schedd no longer changes the ownership of spooled job files
- spooled job files are visible to only the user account by default
- the condor_startd records when jobs are evicted by preemption or draining

* Mon Sep 14 2015 Tim Theisen <tim@cs.wisc.edu> - 8.4.0-1
- a Docker Universe to run a Docker container as an HTCondor job
- the submit file can queue a job for each file found
- the submit file can contain macros
- a dry-run option to condor_submit to test the submit file without any actions
- HTCondor pools can use IPv4 and IPv6 simultaneously
- execute directories can be encrypted upon user or administrator request
- Vanilla Universe jobs can utilize periodic application-level checkpoints
- the administrator can establish job requirements
- numerous scalability changes

* Thu Aug 27 2015 Tim Theisen <tim@cs.wisc.edu> - 8.3.8-1
- a script to tune Linux kernel parameters for better scalability
- support for python bindings on Windows platforms
- a mechanism to remove Docker images from the local machine

* Thu Aug 13 2015 Tim Theisen <tim@cs.wisc.edu> - 8.2.9-1
- a mechanism for the preemption of dynamic slots, such that the partitionable slot may use the dynamic slot in the match of a different job
- default configuration bug fixes for the desktop policy, such that it can both start jobs and monitor the keyboard

* Mon Jul 27 2015 Tim Theisen <tim@cs.wisc.edu> - 8.3.7-1
- default configuration settings have been updated to reflect current usage
- the ability to preempt dynamic slots, such that a job may match with a partitionable slot
- the ability to limit the number of jobs per submission and the number of jobs per owner by setting configuration variables

* Tue Jun 23 2015 Tim Theisen <tim@cs.wisc.edu> - 8.3.6-1
- initial Docker universe support
- IPv4/IPv6 mixed mode support

* Mon Apr 20 2015 Tim Theisen <tim@cs.wisc.edu> - 8.3.5-1
- new features that increase the power of job specification in the submit description file
- RPMs for Red Hat Enterprise Linux 6 and 7 are modularized and only distributed via our YUM repository
- The new condor-all RPM requires the other HTCondor RPMs of a typical HTCondor installation

* Tue Apr 07 2015 Tim Theisen <tim@cs.wisc.edu> - 8.2.8-1
- a bug fix to reconnect a TCP session when an HTCondorView collector restarts
- a bug fix to avoid starting too many jobs, only to kill some chosen at random

* Thu Mar 05 2015 Tim Theisen <tim@cs.wisc.edu> - 8.3.4-1
- a bug fix for a problem that can cause jobs to not be matched to resources when the condor_schedd is flocking

* Thu Feb 19 2015 Tim Theisen <tim@cs.wisc.edu> - 8.3.3-1
- the ability to encrypt a job's directory on Linux execute hosts
- enhancements to EC2 grid universe jobs
- a more efficient query protocol, including the ability to query the condor_schedd daemon's autocluster set

* Tue Feb 10 2015 Tim Theisen <tim@cs.wisc.edu> - 8.2.7-1
- sendmail is used by default for sending notifications (CVE-2014-8126)
- corrected input validation, which prevents daemon crashes
- an update, such that grid jobs work within the current Google Compute Engine
- a bug fix to prevent an infinite loop in the python bindings
- a bug fix to prevent infinite recursion when evaluating ClassAd attributes

* Tue Dec 23 2014 Tim Theisen <tim@cs.wisc.edu> - 8.3.2-1
- the next installment of IPv4/IPv6 mixed mode support: a submit node can simultaneously interact with an IPv4 and an IPv6 HTCondor pool
- scalability improvements: a reduced memory foot-print of daemons, a reduced number of TCP connections between submit and execute machines, and an improved responsiveness from a busy condor_schedd to queries

* Tue Dec 16 2014 Tim Theisen <tim@cs.wisc.edu> - 8.2.6-1
- a bug fix to the log rotation of the condor_schedd on Linux platforms
- transfer_input_files now works for directories on Windows platforms
- a correction of the flags passed to the mail program on Linux platforms
- a RHEL 7 platform fix of a directory permission that prevented daemons from starting

* Mon Dec 01 2014 Tim Theisen <tim@cs.wisc.edu> - 8.2.5-1
- an updated RPM installation script that preserves a modified condor_config.local file
- OpenSSL version 1.0.1j for Windows platforms

* Wed Nov 12 2014 Tim Theisen <tim@cs.wisc.edu> - 8.2.4-1
- a bug fix for an 8.2.3 condor_schedd that could not obtain a claim from an 8.0.x condor_startd
- a bug fix for removed jobs that return to the queue
- a workaround for a condor_schedd performance issue when handling a large number of jobs
- a bug fix to prevent the condor_kbdd from crashing on Windows
- a bug fix to correct the reporting of available disk on Windows

* Wed Oct 01 2014 Tim Theisen <tim@cs.wisc.edu> - 8.2.3-1
- support for Ubuntu 14.04 LTS, including support for the standard universe

* Thu Sep 11 2014 Tim Theisen <tim@cs.wisc.edu> - 8.3.1-1
- a variety of changes that reduce memory usage and improve performance
- if cgroups are used to limit memory utilization, HTCondor sets both the memory and swap limits.

* Wed Aug 27 2014 Carl Edquist <edquist@cs.wisc.edu> - 8.2.2-2.3
- Include config file for MASTER_NEW_BINARY_RESTART = PEACEFUL (SOFTWARE-850)

* Tue Aug 26 2014 Carl Edquist <edquist@cs.wisc.edu> - 8.2.2-2.2
- Include peaceful_off patch (SOFTWARE-1307)

* Mon Aug 25 2014 Carl Edquist <edquist@cs.wisc.edu> - 8.2.2-2.1
- Include condor_gt4540_aws patch for #4540

* Fri Aug 22 2014 Carl Edquist <edquist@cs.wisc.edu> - 8.2.2-2
- Strict pass-through with fixes from 8.2.2-1.1

* Thu Aug 21 2014 Carl Edquist <edquist@cs.wisc.edu> - 8.2.2-1.1
- Update to 8.2.2 with build fixes for non-UW builds

* Mon Sep 09 2013  <edquist@cs.wisc.edu> - 8.1.2-0.3
- Include misc unpackaged files from 8.x.x

* Sun Sep 08 2013  <edquist@cs.wisc.edu> - 8.1.2-0.1.unif
- Packaging fixes to work with latest 8.1.2 source from master
- Move condor.spec into git master-unified_rpm-branch
- Apply patches to upstream branch and remove from rpm / spec
- Always build man pages / remove references to include_man
- Always include systemd sources for passthrough rebuilds of source rpms
- Add macros to bundle external source tarballs with the source rpm to support
  offline builds with externals

* Tue Aug 20 2013 Carl Edquist <edquist@cs.wisc.edu> - 7.9.6-8.unif.8
- Remove externals dependency from std-universe subpackage

* Mon Aug 19 2013 Carl Edquist <edquist@cs.wisc.edu> - 7.9.6-8.unif.7
- Merge init script improvements from trunk
- Have std_local_ref depend on senders,receivers instead of stub_gen
- Carve out std universe files into separate subpackage
- Move uw_build-specific non-std-universe files into externals subpackage
- Condor_config changes for #3645
- Use %osg / %std_univ macros to control build type default
- Support PROPER builds of std universe (ie, without UW_BUILD)
- Use make jobserver when building glibc external instead of make -j2
- Move python requirement out of main condor package (#3704)
- Move condor_config.local from /var/lib/condor/ to /etc/condor/

* Fri Jul 05 2013 Carl Edquist <edquist@cs.wisc.edu> - 7.9.6-8.unif.6
- Address build dependency issue seen with -j24

* Fri Jun 21 2013 Carl Edquist <edquist@cs.wisc.edu> - 7.9.6-8.unif.5
- Initial support for UW_BUILD

* Tue Jun 18 2013 Carl Edquist <edquist@cs.wisc.edu> - 7.9.6-8.unif.4
- Remove service restart for upgrades

* Tue Jun 11 2013 Carl Edquist <edquist@cs.wisc.edu> - 7.9.6-8.unif.2
- Add a parallel-setup sub-package for parallel universe configuration,
  namely setting up the host as a dedicated resource

* Mon Jun 10 2013 Brian Lin <blin@cs.wisc.edu> - 7.8.8-2
- Init script improvements

* Fri Jun 07 2013 Carl Edquist <edquist@cs.wisc.edu> - 7.9.6-8.unif.1
- Add in missing features from Fedora rpm
- Reorganize to reduce the diff size between this and the Fedora rpm

* Fri Jun 07 2013 Brian Lin <blin@cs.wisc.edu> - 7.9.6-8
- Remove glexec runtime dependency

* Tue May 28 2013 Brian Lin <blin@cs.wisc.edu> - 7.9.6-7
- Mark /usr/share/osg/sysconfig/condor as non-config file

* Thu May 23 2013 Brian Lin <blin@cs.wisc.edu> - 7.9.6-6
- Rebuild against fixed glite-ce-cream-client-api-c

* Wed May 22 2013 Brian Lin <blin@cs.wisc.edu> - 7.9.6-5
- Enable plumage for x86{,_64}

* Wed May 22 2013 Brian Lin <blin@cs.wisc.edu> - 7.9.6-4
- Enable cgroups for EL6

* Tue May 21 2013 Brian Lin <blin@cs.wisc.edu> - 7.9.6-3
- Building with blahp/cream

* Tue May 21 2013 Brian Lin <blin@cs.wisc.edu> - 7.9.6-2
- Build without blahp/cream

* Tue May 21 2013 Brian Lin <blin@cs.wisc.edu> - 7.9.6-1
- New version

* Wed May 08 2013 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.8-1
- New version
- Removed condor_glidein -- was removed upstream

* Wed Feb 13 2013 Dave Dykstra <dwd@fnal.gov> - 7.8.6-3
- Renamed /etc/sysconfig/condor-lcmaps-env to /usr/share/osg/sysconfig/condor
  to match the new OSG method for handling daemon environment variables, 
  which keeps non-replaceable settings out of /etc/sysonfig
- Change settings in /usr/share/osg/sysconfig/condor to use the latest variable
  name LLGT_LIFT_PRIVILEGED_PROTECTION instead of LLGT4_NO_CHANGE_USER,
  eliminate obsolete variable LLGT_VOMS_DISABLE_CREDENTIAL_CHECK, and change
  the default debug level from 3 to 2.

* Fri Dec 21 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.6-2
- Patch to fix default BATCH_GAHP config value (#SOFTWARE-873)

* Thu Oct 25 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.6-1
- New version

* Mon Oct 22 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.5-1
- New version

* Wed Sep 19 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.4-1
- New version

* Fri Sep 07 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.3-1
- New version

* Mon Aug 27 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.2-2
- Add patch to fix unnecessary GSI callouts (condor_gt2104_pt2.patch in gittrac #2104)
- Fixed BLClient location

* Tue Aug 14 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.2-1
- New version

* Mon Jul 30 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.1-7
- Put cream_gahp into separate subpackage

* Mon Jul 16 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.1-6
- Remove cream_el6.patch; change proper_cream.diff to work on both el5 and el6
  instead.

* Thu Jul 05 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.1-5
- Bump to rebuild

* Tue Jun 26 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.1-4
- Add CREAM

* Tue Jun 19 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.1-3
- Add Provides lines for classads and classads-devel

* Mon Jun 18 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.1-2
- Add environment variables for interacting with lcmaps (condor-lcmaps-env)

* Fri Jun 15 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.1-1
- Version bump

* Wed Jun 13 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.0-3
- Fix wrong paths for shared libraries

* Wed Jun 13 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.0-2
- Build blahp

* Thu May 31 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 7.8.0-1
- Version bump
- Updated condor_config.generic.patch
- Removed glexec-patch.diff

* Sun Apr  1 2012 Alain Roy <roy@cs.wisc.edu> - 7.6.6-4
- Backported patch from Condor 7.7 to fix glexec bugs
- Enabled glexec

* Fri Feb 10 2012 Derek Weitzel <dweitzel@cse.unl.edu> - 7.6.6-3
- Adding sticky bit to condor_root_switchboard

* Wed Jan 18 2012 Derek Weitzel <dweitzel@cse.unl.edu> - 7.6.6-2
- Added support for rhel6

* Wed Jan 18 2012 Tim Cartwright <cat@cs.wisc.edu> - 7.6.6-1
- Updated to upstream tagged 7.6.6 release

* Wed Jan 11 2012 Tim Cartwright <cat@cs.wisc.edu> - 7.6.4-1
- Simplified revision number

* Tue Nov 29 2011 Derek Weitzel <dweitzel@cse.unl.edu> - 7.6.4-0.6.2
- Rebasing to 7.6.4

* Fri Oct 28 2011 Matyas Selmeci <matyas@cs.wisc.edu> - 7.6.2-0.6.3
- rebuilt

* Mon Sep 12 2011 Matyas Selmeci <matyas@cs.wisc.edu> - 7.6.2-0.6.2
- Rev bump to rebuild with updated Globus libs

* Thu Aug 11 2011 Derek Weitzel <dweitzel@cse.unl.edu> - 7.6.2-0.5.2
- Updated to upstream official 7.6.2 release

* Thu Aug 04 2011 Derek Weitzel <dweitzel@cse.unl.edu> - 7.6.2-0.5.672537b1git.1
- Made LOCAL_DIR always point to /var/lib/condor rather than TILDE

* Wed Jun  8 2011 <bbockelm@cse.unl.edu> - 7.7.0-0.5
- Start to break build products into conditionals for future EPEL5 support.
- Begun integration of a systemd service file.

* Tue Jun  7 2011 <matt@redhat> - 7.7.0-0.4
- Added tmpfiles.d/condor.conf (BZ711456)

* Tue Jun  7 2011 <matt@redhat> - 7.7.0-0.3
- Fast forward to 7.7.0 pre-release at 1babb324
- Catch libdeltacloud 0.8 update

* Fri May 20 2011 <matt@redhat> - 7.7.0-0.2
- Added GSI support, dependency on Globus

* Fri May 13 2011 <matt@redhat> - 7.7.0-0.1
- Fast forward to 7.7.0 pre-release at 79952d6b
- Introduced ec2_gahp
- 79952d6b brings schema expectations inline with Cumin

* Tue May 10 2011 <matt@redhat> - 7.6.1-0.1
- Upgrade to 7.6.0 release, pre-release of 7.6.1 at 5617a464
- Upstreamed patch: log_lock_run.patch
- Introduced condor-classads to obsolete classads
- Introduced condor-aviary, package of the aviary contrib
- Introduced condor-deltacloud-gahp
- Introduced condor-qmf, package of the mgmt/qmf contrib
- Transitioned from LOCAL_CONFIG_FILE to LOCAL_CONFIG_DIR
- Stopped building against gSOAP,
-  use aviary over birdbath and ec2_gahp (7.7.0) over amazon_gahp

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 7.5.5-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Thu Jan 27 2011 <matt@redhat> - 7.5.5-1
- Rebase to 7.5.5 release
-  configure+imake -> cmake
-  Removed patches:
-   only_dynamic_unstripped.patch
-   gsoap-2.7.16-wsseapi.patch
-   gsoap-2.7.16-dom.patch
-  man pages are now built with source
-  quill is no longer present
-  condor_shared_port added
-  condor_power added
-  condor_credd added
-  classads now built from source

* Thu Jan 13 2011 <matt@redhat> - 7.4.4-1
- Upgrade to 7.4.4 release
- Upstreamed: stdsoap2.h.patch.patch

* Mon Aug 23 2010  <matt@redhat> - 7.4.3-1
- Upgrade to 7.4.3 release
- Upstreamed: dso_link_change

* Fri Jun 11 2010  <matt@redhat> - 7.4.2-2
- Rebuild for classads DSO version change (1:0:0)
- Updated stdsoap2.h.patch.patch for gsoap 2.7.16
- Added gsoap-2.7.16-wsseapi/dom.patch for gsoap 2.7.16

* Wed Apr 21 2010  <matt@redhat> - 7.4.2-1
- Upgrade to 7.4.2 release

* Tue Jan  5 2010  <matt@redhat> - 7.4.1-1
- Upgrade to 7.4.1 release
- Upstreamed: guess_version_from_release_dir, fix_platform_check
- Security update (BZ549577)

* Fri Dec  4 2009  <matt@redhat> - 7.4.0-1
- Upgrade to 7.4.0 release
- Fixed POSTIN error (BZ540439)
- Removed NOTICE.txt source, now provided by upstream
- Removed no_rpmdb_query.patch, applied upstream
- Removed no_basename.patch, applied upstream
- Added only_dynamic_unstripped.patch to reduce build time
- Added guess_version_from_release_dir.patch, for previous
- Added fix_platform_check.patch
- Use new --with-platform, to avoid modification of make_final_tarballs
- Introduced vm-gahp package to hold libvirt deps

* Fri Aug 28 2009  <matt@redhat> - 7.2.4-1
- Upgrade to 7.2.4 release
- Removed gcc44_const.patch, accepted upstream
- New log, lock, run locations (BZ502175)
- Filtered innocuous semanage message

* Fri Aug 21 2009 Tomas Mraz <tmraz@redhat.com> - 7.2.1-3
- rebuilt with new openssl

* Fri Jul 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 7.2.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Wed Feb 25 2009  <matt@redhat> - 7.2.1-1
- Upgraded to 7.2.1 release
- Pruned changes accepted upstream from condor_config.generic.patch
- Removed Requires in favor of automatic dependencies on SONAMEs
- Added no_rmpdb_query.patch to avoid rpm -q during a build

* Tue Feb 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 7.2.0-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Thu Jan 15 2009 Tomas Mraz <tmraz@redhat.com> - 7.2.0-4
- rebuild with new openssl

* Wed Jan 14 2009  <matt@redhat> - 7.2.0-3
- Fixed regression: initscript was on by default, now off again

* Thu Jan  8 2009  <matt@redhat> - 7.2.0-2
- (Re)added CONDOR_DEVELOPERS=NONE to the default condor_config.local
- Added missing Obsoletes for condor-static (thanks Michael Schwendt)

* Wed Jan  7 2009  <matt@redhat> - 7.2.0-1
- Upgraded to 7.2.0 release
- Removed -static package
- Added Fedora specific buildid
- Enabled KBDD, daemon to monitor X usage on systems with only USB devs
- Updated install process

* Wed Oct  8 2008  <matt@redhat> - 7.0.5-1
- Rebased on 7.0.5, security update

* Wed Aug  6 2008  <mfarrellee@redhat> - 7.0.4-1
- Updated to 7.0.4 source
- Stopped using condor_configure in install step

* Tue Jun 10 2008  <mfarrellee@redhat> - 7.0.2-1
- Updated to 7.0.2 source
- Updated config, specifically HOSTALLOW_WRITE, for Personal Condor setup
- Added condor_config.generic

* Mon Apr  7 2008  <mfarrellee@redhat> - 7.0.0-8
- Modified init script to be off by default, resolves bz441279

* Fri Apr  4 2008  <mfarrellee@redhat> - 7.0.0-7
- Updated to handle changes in gsoap dependency

* Mon Feb 11 2008  <mfarrellee@redhat> - 7.0.0-6
- Added note about how to download the source
- Added generate-tarball.sh script

* Sun Feb 10 2008  <mfarrellee@redhat> - 7.0.0-5
- The gsoap package is compiled with --disable-namespaces, which means
  soap_set_namespaces is required after each soap_init. The
  gsoap_nonamespaces.patch handles this.

* Fri Feb  8 2008  <mfarrellee@redhat> - 7.0.0-4
- Added patch to detect GCC 4.3.0 on F9
- Added patch to detect GLIBC 2.7.90 on F9
- Added BuildRequires: autoconf to allow for regeneration of configure
  script after GCC 4.3.0 detection and GLIBC 2.7.90 patches are
  applied
- Condor + GCC 4.3.0 + -O2 results in an internal compiler error
  (BZ 432090), so -O2 is removed from optflags for the time
  being. Thanks to Mike Bonnet for the suggestion on how to filter
  -O2.

* Tue Jan 22 2008  <mfarrellee@redhat> - 7.0.0-3
- Update to UW's really-final source for Condor 7.0.0 stable series
  release. It is based on the 72173 build with minor changes to the
  configure.ac related to the SRB external version.
- In addition to removing externals from the UW tarball, the NTconfig
  directory was removed because it had not gone through IP audit.

* Tue Jan 22 2008  <mfarrellee@redhat> - 7.0.0-2
- Update to UW's final source for Condor 7.0.0 stable series release

* Thu Jan 10 2008  <mfarrellee@redhat> - 7.0.0-1
- Initial package of Condor's stable series under ASL 2.0
- is_clipped.patch replaced with --without-full-port option to configure
- zlib_is_soft.patch removed, outdated by configure.ac changes
- removed autoconf dependency needed for zlib_is_soft.patch

* Tue Dec  4 2007  <mfarrellee@redhat> - 6.9.5-2
- SELinux was stopping useradd in pre because files specified root as
  the group owner for /var/lib/condor, fixed, much thanks to Phil Knirsch

* Fri Nov 30 2007  <mfarrellee@redhat> - 6.9.5-1
- Fixed release tag
- Added gSOAP support and packaged WSDL files

* Thu Nov 29 2007  <mfarrellee@redhat> - 6.9.5-0.2
- Packaged LSB init script
- Changed pre to not create the condor user's home directory, it is
  now a directory owned by the package

* Thu Nov 29 2007  <mfarrellee@redhat> - 6.9.5-0.1
- Condor 6.9.5 release, the 7.0.0 stable series candidate
- Removed x86_64_no_multilib-200711091700cvs.patch, merged upstream
- Added patch to make zlib a soft requirement, which it should be
- Disabled use of smp_mflags because of make dependency issues
- Explicitly not packaging WSDL files until the SOAP APIs are available

* Tue Nov 20 2007  <mfarrellee@redhat> - 6.9.5-0.3.200711091700cvs
- Rebuild for repo inheritance update: dependencies are now pulled
  from RHEL 5 U1 before RH Application Stack

* Thu Nov 15 2007 <mfarrellee@redhat> - 6.9.5-0.2.200711091700cvs
- Added support for building on x86_64 without multilib packages
- Made the install section more flexible, reduced need for
  make_final_tarballs to be updated

* Fri Nov 9 2007 <mfarrellee@redhat> - 6.9.5-0.1.200711091700cvs
- Working source with new ASL 2.0 license

* Fri Nov 9 2007 <mfarrellee@redhat> - 6.9.5-0.1.200711091330cvs
- Source is now stamped ASL 2.0, as of Nov 9 2007 1:30PM Central
- Changed license to ASL 2.0
- Fixed find in prep to work if no files have bad permissions
- Changed the name of the LICENSE file to match was is now release in
  the source tarball

* Tue Nov 6 2007  <mfarrellee@redhat> - 6.9.5-0.1.rc
- Added m4 dependency not in RHEL 5.1's base
- Changed chmod a-x script to use find as more files appear to have
  improper execute bits set
- Added ?dist to Release:
- condor_privsep became condor_root_switchboard

* Tue Sep 11 2007  <mfarrellee@redhat> - 6.9.5-0.3.20070907cvs
- Instead of releasing libcondorapi.so, which is improperly created
  and poorly versioned, we release libcondorapi.a, which is apparently
  more widely used, in a -static package
- Instead of installing the stripped tarball, the unstripped is now
  installed, which gives a nice debuginfo package
- Found and fixed permissions problems on a number of src files,
  issue raised by rpmlint on the debuginfo package

* Mon Sep 10 2007  <mfarrellee@redhat> - 6.9.5-0.2.20070907cvs
- Updated pre section to create condor's home directory with adduser, and
  removed _var/lib/condor from files section
- Added doc LICENSE.TXT to all files sections
- Shortened lines longer than 80 characters in this spec (except the sed line)
- Changed install section so untar'ing a release can handle fedora7 or fedora8
- Simplified the site.def and config file updates (sed + mv over mv + sed + rm)
- Added a patch (fedora_rawhide_7.91-20070907cvs.patch) to support building on
  a fedora 7.91 (current Rawhide) release
- Moved the examples from /usr/share/doc/condor... into builddir and just
  marked them as documentation
- Added a number of dir directives to force all files to be listed, no implicit
  inclusion

* Fri Sep  7 2007  <mfarrellee@redhat> - 6.9.5-0.1.20070907cvs
- Initial release
