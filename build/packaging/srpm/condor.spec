%define tarball_version 8.1.3

# optionally define any of these, here or externally
# % define fedora   16
# % define osg      0
# % define uw_build 1

%define plumage 0
%define systemd 0
%define cgroups 0
%define python 0

%if 0%{?rhel} >= 6
%define cgroups 1
%endif
%if 0%{?rhel} >= 7
%define systemd 1
%endif

# default to uw_build if neither osg nor fedora is enabled
%if %undefined uw_build
%if 0%{?osg} || 0%{?hcc}
%define uw_build 0
%else
%define uw_build 1
%endif
%endif

%if %uw_build
%define debug 1
%define verbose 1
%endif

# define these to 1 if you want to include externals in source rpm
%define bundle_uw_externals 0

# Things not turned on, or don't have Fedora packages yet
%define qmf 0

%if 0%{?fedora}
%define blahp 1
%define cream 0
%else
%define blahp 1
%define cream 1
%endif

%if 0%{?hcc}
%define blahp 0
%define cream 0
%endif

# Python on 64-bit platform or rhel6
%ifarch x86_64
%define python 1
%endif
%if 0%{?rhel} == 6
%define python 1
%endif

# Don't bother building CREAM for 32-bit RHEL7
%ifarch %{ix86}
%if 0%{?rhel} >= 7
%define cream 0
%endif
%endif

%define glexec 1

# Temporarily turn parallel_setup off
%define parallel_setup 0

# These flags are meant for developers; it allows one to build HTCondor
# based upon a git-derived tarball, instead of an upstream release tarball
%define git_build 1
# If building with git tarball, Fedora requests us to record the rev.  Use:
# git log -1 --pretty=format:'%h'
%define git_rev f9e8f64

%if ! (0%{?fedora} > 12 || 0%{?rhel} > 5)
%{!?python_sitelib: %global python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())")}
%{!?python_sitearch: %global python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(1))")}
%endif

Summary: HTCondor: High Throughput Computing
Name: condor
Version: %{tarball_version}
%global version_ %(tr . _ <<< %{version})

# Only edit the %condor_base_release to bump the rev number
%define condor_git_base_release 0.1
%define condor_base_release 1
%if %git_build
        %define condor_release %condor_git_base_release.%{git_rev}.git
%else
        %define condor_release %condor_base_release
%endif
Release: %condor_release%{?dist}

License: ASL 2.0
Group: Applications/System
URL: http://www.cs.wisc.edu/condor/

# This allows developers to test the RPM with a non-release, git tarball
%if %git_build

# git clone http://condor-git.cs.wisc.edu/repos/condor.git
# cd condor
# git archive master | gzip -7 > ~/rpmbuild/SOURCES/condor.tar.gz
Source0: condor.tar.gz

%else

# The upstream HTCondor source tarball contains some source that cannot
# be shipped as well as extraneous copies of packages the source
# depends on. Additionally, the upstream HTCondor source requires a
# click-through license. Once you have downloaded the source from:
#   http://parrot.cs.wisc.edu/v7.0.license.html
# you should process it with generate-tarball.sh:
#   ./generate-tarball.sh 7.0.4
# MD5Sum of upstream source:
#   06eec3ae274b66d233ad050a047f3c91  condor_src-7.0.0-all-all.tar.gz
#   b08743cfa2e87adbcda042896e8ef537  condor_src-7.0.2-all-all.tar.gz
#   5f326ad522b63eacf34c6c563cf46910  condor_src-7.0.4-all-all.tar.gz
#   73323100c5b2259f3b9c042fa05451e0  condor_src-7.0.5-all-all.tar.gz
#   a2dd96ea537b2c6d105b6c8dad563ddc  condor_src-7.2.0-all-all.tar.gz
#   edbac8267130ac0a0e016d0f113b4616  condor_src-7.2.1-all-all.tar.gz
#   6d9b0ef74d575623af11e396fa274174  condor_src-7.2.4-all-all.tar.gz
#   ee72b65fad02d21af0dc8f1aa5872110  condor_src-7.4.0-all-all.tar.gz
#   d4deeabbbce65980c085d8bea4c1018a  condor_src-7.4.1-all-all.tar.gz
#   4714086f58942b78cf03fef9ccb1117c  condor_src-7.4.2-all-all.tar.gz
#   2b7e7687cba85d0cf0774f9126b51779  condor_src-7.4.3-all-all.tar.gz
#   108a4b91cd10deca1554ca1088be6c8c  condor_src-7.4.4-all-all.tar.gz
#   b482c4bfa350164427a1952113d53d03  condor_src-7.5.5-all-all.tar.gz
#   2a1355cb24a56a71978d229ddc490bc5  condor_src-7.6.0-all-all.tar.gz
# Note: The md5sum of each generated tarball may be different
Source0: %{name}-%{tarball_version}.tar.gz
Source1: generate-tarball.sh
%endif

# % if %systemd
Source3: osg-env.conf
# % else
Source4: condor.osg-sysconfig
# % endif
Source5: condor_config.local.dedicated.resource

Source6: 10-batch_gahp_blahp.config
Source7: 00-restart_peaceful.config

Source8: htcondor.pp

# custom find-requires script for filtering stuff from condor-external-libs
Source90: find-requires.sh

%if %uw_build
%define __find_requires %{SOURCE90}
%define _use_internal_dependency_generator 0
%endif

%if %bundle_uw_externals
Source101: blahp-1.16.5.1.tar.gz
Source102: boost_1_49_0.tar.gz
Source103: c-ares-1.3.0.tar.gz
Source105: drmaa-1.6.1.tar.gz
Source106: glite-ce-cream-client-api-c-1.14.0-4.sl6.tar.gz
Source107: glite-ce-wsdl-1.14.0-4.sl6.tar.gz
Source108: glite-lbjp-common-gsoap-plugin-3.1.2-2.src.tar.gz
Source109: glite-lbjp-common-gss-3.1.3-2.src.tar.gz
Source110: gridsite-1.6.0.src.tar.gz
Source111: gsoap-2.7.10.tar.gz
Source112: gsoap_2.7.16.zip
Source113: gt5.2.5-all-source-installer.tar.gz
Source114: libcgroup-0.37.tar.bz2
Source116: log4cpp-1.0-3.tar.gz
Source117: unicoregahp-1.2.0.tar.gz
Source118: voms-2.0.6.tar.gz
%endif


#% if 0%osg
Patch8: osg_sysconfig_in_init_script.patch
#% endif

BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires: cmake
BuildRequires: %_bindir/flex
BuildRequires: %_bindir/byacc
BuildRequires: pcre-devel
BuildRequires: openssl-devel
BuildRequires: krb5-devel
BuildRequires: libvirt-devel
BuildRequires: bind-utils
BuildRequires: m4
#BuildRequires: autoconf
BuildRequires: libX11-devel
BuildRequires: libXScrnSaver-devel
BuildRequires: /usr/include/curl/curl.h
BuildRequires: /usr/include/expat.h
BuildRequires: openldap-devel
BuildRequires: python-devel
BuildRequires: boost-devel
BuildRequires: redhat-rpm-config
BuildRequires: sqlite-devel
BuildRequires: perl(Data::Dumper)

%if %uw_build
BuildRequires: cmake >= 2.8
BuildRequires: gcc-c++
%if 0%{?rhel} >= 6
BuildRequires: glibc-static
%if 0%{?rhel} >= 7
# libstdc++.a moved to a separate -static package in EL7
BuildRequires: libstdc++-static
%endif
BuildRequires: libuuid-devel
%else
BuildRequires: glibc-devel
BuildRequires: /usr/include/uuid/uuid.h
%endif
BuildRequires: bison-devel
BuildRequires: bison
BuildRequires: byacc
BuildRequires: flex
BuildRequires: patch
BuildRequires: libtool
BuildRequires: libtool-ltdl-devel
BuildRequires: pam-devel
BuildRequires: nss-devel
BuildRequires: openssl-devel
BuildRequires: libxml2-devel
BuildRequires: expat-devel
BuildRequires: perl(Archive::Tar)
BuildRequires: perl(XML::Parser)
BuildRequires: perl(Digest::MD5)
BuildRequires: python-devel
BuildRequires: libcurl-devel
%endif

# Globus GSI build requirements
%if ! %uw_build
BuildRequires: globus-gssapi-gsi-devel
BuildRequires: globus-gass-server-ez-devel
BuildRequires: globus-gass-transfer-devel
BuildRequires: globus-gram-client-devel
BuildRequires: globus-rsl-devel
BuildRequires: globus-gram-protocol
BuildRequires: globus-io-devel
BuildRequires: globus-xio-devel
BuildRequires: globus-gssapi-error-devel
BuildRequires: globus-gss-assist-devel
BuildRequires: globus-gsi-proxy-core-devel
BuildRequires: globus-gsi-credential-devel
BuildRequires: globus-gsi-callback-devel
BuildRequires: globus-gsi-sysconfig-devel
BuildRequires: globus-gsi-cert-utils-devel
BuildRequires: globus-openssl-module-devel
BuildRequires: globus-gsi-openssl-error-devel
BuildRequires: globus-gsi-proxy-ssl-devel
BuildRequires: globus-callout-devel
BuildRequires: globus-common-devel
BuildRequires: globus-ftp-client-devel
BuildRequires: globus-ftp-control-devel
BuildRequires: munge-devel
BuildRequires: voms-devel
%endif
BuildRequires: libtool-ltdl-devel

%if %plumage
BuildRequires: mongodb-devel >= 1.6.4-3
%endif

# libcgroup < 0.37 has a bug that invalidates our accounting.
%if %cgroups
BuildRequires: libcgroup-devel >= 0.37
Requires: libcgroup >= 0.37
%endif

%if %cream && ! %uw_build
BuildRequires: glite-ce-cream-client-devel
BuildRequires: glite-lbjp-common-gsoap-plugin-devel
BuildRequires: log4cpp-devel
BuildRequires: gridsite-devel
%endif

%if 0%{?rhel} >= 7
%ifarch x86_64
BuildRequires: python36-devel
BuildRequires: boost169-devel
BuildRequires: boost169-static
%endif
%endif

%if 0%{?rhel} >= 6 || 0%{?fedora}
BuildRequires: boost-python
BuildRequires: libuuid-devel
Requires: libuuid
%endif

%if %qmf
BuildRequires: qpid-qmf-devel
%endif

%if %systemd
BuildRequires: systemd-devel
BuildRequires: systemd-units
Requires: systemd
%endif

BuildRequires: transfig
BuildRequires: latex2html
# We don't build the manual (yet)
#BuildRequires: texlive-epstopdf

Requires: /usr/sbin/sendmail
Requires: condor-classads = %{version}-%{release}
Requires: condor-procd = %{version}-%{release}

# ecryptfs was pulled from rhel 7
%if (0%{?rhel} == 5 || 0%{?rhel} == 6)
Requires: ecryptfs-utils
%endif

%if %blahp && ! %uw_build
Requires: blahp >= 1.16.1
%endif

%if %uw_build
Requires: %name-external-libs%{?_isa} = %version-%release
%endif

# Box and Google Drive file transfer plugins require python-requests
Requires: python-requests

Requires: initscripts

Requires(pre): shadow-utils

%if %systemd
Requires(post): systemd-units
Requires(preun): systemd-units
Requires(postun): systemd-units
Requires(post): systemd-sysv
%else
Requires(post):/sbin/chkconfig
Requires(preun):/sbin/chkconfig
Requires(preun):/sbin/service
Requires(postun):/sbin/service
%endif

%if 0%{?rhel} >= 7
Requires(post): policycoreutils-python
Requires(post): selinux-policy-targeted >= 3.13.1-102
%endif

#Provides: user(condor) = 43
#Provides: group(condor) = 43

Obsoletes: condor-static < 7.2.0

%description
HTCondor is a specialized workload management system for
compute-intensive jobs. Like other full-featured batch systems, HTCondor
provides a job queueing mechanism, scheduling policy, priority scheme,
resource monitoring, and resource management. Users submit their
serial or parallel jobs to HTCondor, HTCondor places them into a queue,
chooses when and where to run the jobs based upon a policy, carefully
monitors their progress, and ultimately informs the user upon
completion.

#######################
%package procd
Summary: HTCondor Process tracking Daemon
Group: Applications/System

%description procd
A daemon for tracking child processes started by a parent.
Part of HTCondor, but able to be stand-alone

#######################
%if %qmf
%package qmf
Summary: HTCondor QMF components
Group: Applications/System
Requires: %name = %version-%release
#Requires: qmf >= %{qmf_version}
Requires: python-qmf >= 0.7.946106
Requires: %name-classads = %{version}-%{release}
Obsoletes: condor-qmf-plugins

%description qmf
Components to connect HTCondor to the QMF management bus.
%endif

#######################
%if %plumage
%package plumage
Summary: HTCondor Plumage components
Group: Applications/System
Requires: %name = %version-%release
Requires: condor-classads = %{version}-%{release}
Requires: mongodb >= 1.6.4
Requires: pymongo >= 1.9
Requires: python-dateutil >= 1.4.1

%description plumage
Components to provide a NoSQL operational data store for HTCondor.
%endif

#######################
%package kbdd
Summary: HTCondor Keyboard Daemon
Group: Applications/System
Requires: %name = %version-%release
Requires: %name-classads = %{version}-%{release}

%description kbdd
The condor_kbdd monitors logged in X users for activity. It is only
useful on systems where no device (e.g. /dev/*) can be used to
determine console idle time.

#######################
%package vm-gahp
Summary: HTCondor's VM Gahp
Group: Applications/System
Requires: %name = %version-%release
Requires: libvirt
Requires: %name-classads = %{version}-%{release}

%description vm-gahp
The condor_vm-gahp enables the Virtual Machine Universe feature of
HTCondor. The VM Universe uses libvirt to start and control VMs under
HTCondor's Startd.

#######################
%package classads
Summary: HTCondor's classified advertisement language
Group: Development/Libraries
%if 0%{?osg} || 0%{?hcc}
Obsoletes: classads <= 1.0.10
Obsoletes: classads-static <= 1.0.10
Provides: classads = %version-%release
%endif

%description classads
Classified Advertisements (classads) are the lingua franca of
HTCondor. They are used for describing jobs, workstations, and other
resources. They are exchanged by HTCondor processes to schedule
jobs. They are logged to files for statistical and debugging
purposes. They are used to enquire about current state of the system.

A classad is a mapping from attribute names to expressions. In the
simplest cases, the expressions are simple constants (integer,
floating point, or string). A classad is thus a form of property
list. Attribute expressions can also be more complicated. There is a
protocol for evaluating an attribute expression of a classad vis a vis
another ad. For example, the expression "other.size > 3" in one ad
evaluates to true if the other ad has an attribute named size and the
value of that attribute is (or evaluates to) an integer greater than
three. Two classads match if each ad has an attribute requirements
that evaluates to true in the context of the other ad. Classad
matching is used by the HTCondor central manager to determine the
compatibility of jobs and workstations where they may be run.

#######################
%package classads-devel
Summary: Headers for HTCondor's classified advertisement language
Group: Development/System
Requires: %name-classads = %version-%release
Requires: pcre-devel
%if 0%{?osg} || 0%{?hcc}
Obsoletes: classads-devel <= 1.0.10
Provides: classads-devel = %version-%release
%endif

%description classads-devel
Header files for HTCondor's ClassAd Library, a powerful and flexible,
semi-structured representation of data.

#######################
%package test
Summary: HTCondor Self Tests
Group: Applications/System
Requires: %name = %version-%release
Requires: %name-classads = %{version}-%{release}

%description test
A collection of tests to verify that HTCondor is operating properly.

#######################
%if %cream
%package cream-gahp
Summary: HTCondor's CREAM Gahp
Group: Applications/System
Requires: %name = %version-%release
Requires: %name-classads = %{version}-%{release}
%if %uw_build
Requires: %name-external-libs%{?_isa} = %version-%release
%endif

%description cream-gahp
The condor-cream-gahp enables CREAM interoperability for HTCondor.

%endif

#######################
%if %parallel_setup
%package parallel-setup
Summary: Configure HTCondor for Parallel Universe jobs
Group: Applications/System
Requires: %name = %version-%release

%description parallel-setup
Running Parallel Universe jobs in HTCondor requires some configuration;
in particular, a dedicated scheduler is required.  In order to support
running parallel universe jobs out of the box, this sub-package provides
a condor_config.local.dedicated.resource file that sets up the current
host as the DedicatedScheduler.
%endif


#######################
%if %python
%package -n python2-condor
Summary: Python bindings for HTCondor.
Group: Applications/System
Requires: python >= 2.2
Requires: %name = %version-%release
%{?python_provide:%python_provide python2-condor}
%if 0%{?rhel} >= 7
%ifarch x86_64
Requires: boost169-python2
%endif
%endif
# Remove before F30
Provides: %{name}-python = %{version}-%{release}
Provides: %{name}-python%{?_isa} = %{version}-%{release}
Obsoletes: %{name}-python < %{version}-%{release}

%if 0%{?rhel} >= 7 && ! %uw_build
# auto provides generator does not pick these up for some reason
    %ifarch x86_64
Provides: classad.so()(64bit)
Provides: htcondor.so()(64bit)
    %else
Provides: classad.so
Provides: htcondor.so
    %endif
%endif

%description -n python2-condor
The python bindings allow one to directly invoke the C++ implementations of
the ClassAd library and HTCondor from python


%if 0%{?rhel} >= 7
%ifarch x86_64
#######################
%package -n python3-condor
Summary: Python bindings for HTCondor.
Group: Applications/System
Requires: python36
Requires: %name = %version-%release
Requires: boost169-python3

%if 0%{?rhel} >= 7 && ! %uw_build
# auto provides generator does not pick these up for some reason
    %ifarch x86_64
Provides: classad.so()(64bit)
Provides: htcondor.so()(64bit)
    %else
Provides: classad.so
Provides: htcondor.so
    %endif
%endif

%description -n python3-condor
The python bindings allow one to directly invoke the C++ implementations of
the ClassAd library and HTCondor from python
%endif
%endif
%endif


#######################
%package bosco
Summary: BOSCO, a HTCondor overlay system for managing jobs at remote clusters
Url: https://osg-bosco.github.io/docs/
Group: Applications/System
Requires: python >= 2.2
Requires: %name = %version-%release
Requires: rsync

%description bosco
BOSCO allows a locally-installed HTCondor to submit jobs to remote clusters,
using SSH as a transit mechanism.  It is designed for cases where the remote
cluster is using a different batch system such as PBS, SGE, LSF, or another
HTCondor system.

BOSCO provides an overlay system so the remote clusters appear to be a HTCondor
cluster.  This allows the user to run their workflows using HTCondor tools across
multiple clusters.

%package -n minicondor
Summary: Configuration for a single-node HTCondor
Group: Applications/System
Requires: %name = %version-%release
Requires: python2-condor = %version-%release

%description -n minicondor
This example configuration is good for trying out HTCondor for the first time.
It only configures the IPv4 loopback address, turns on basic security, and
shortens many timers to be more responsive.

%if %uw_build

%ifarch %{ix86}
%package small-shadow
Summary: 32-bit condor_shadow binary
Group: Applications/System
Requires: %name-external-libs%{?_isa} = %version-%release

%description small-shadow
Provides the 32-bit condor_shadow_s, which has a smaller private
memory footprint per process.  This makes it possible to run more shadows
on a single machine at once when memory is the limiting factor.
%endif

%package externals
Summary: External packages built into HTCondor
Group: Applications/System
Requires: %name = %version-%release
Requires: %name-external-libs%{?_isa} = %version-%release
Provides: blahp

%description externals
Includes the external packages built when UW_BUILD is enabled

%package external-libs
Summary: Libraries for external packages built into HTCondor
Group: Applications/System
# disable automatic provides generation to prevent conflicts with system libs
AutoProv: 0

%description external-libs
Includes the libraries for external packages built when UW_BUILD is enabled

%endif

%package annex-ec2
Summary: Configuration and scripts to make an EC2 image annex-compatible.
Group: Applications/System
Requires: %name = %version-%release
Requires(post): /sbin/chkconfig
Requires(preun): /sbin/chkconfig

%description annex-ec2
Configures HTCondor to make an EC2 image annex-compatible.  Do NOT install
on a non-EC2 image.

%files annex-ec2
%if %systemd
%_libexecdir/condor/condor-annex-ec2
%{_unitdir}/condor-annex-ec2.service
%else
%_initrddir/condor-annex-ec2
%endif
%config(noreplace) %_sysconfdir/condor/config.d/50ec2.config
%config(noreplace) %_sysconfdir/condor/master_shutdown_script.sh

%post annex-ec2
%if %systemd
/bin/systemctl enable condor-annex-ec2
%else
/sbin/chkconfig --add condor-annex-ec2
%endif

%preun annex-ec2
%if %systemd
if [ $1 == 0 ]; then
    /bin/systemctl disable condor-annex-ec2
fi
%else
if [ $1 == 0 ]; then
    /sbin/chkconfig --del condor-annex-ec2 > /dev/null 2>&1 || :
fi
%endif

%package all
Summary: All condor packages in a typical installation
Group: Applications/System
Requires: %name = %version-%release
Requires: %name-procd = %version-%release
Requires: %name-kbdd = %version-%release
Requires: %name-vm-gahp = %version-%release
Requires: %name-classads = %version-%release
%if %cream
Requires: %name-cream-gahp = %version-%release
%endif
Requires: python2-condor = %version-%release
Requires: %name-bosco = %version-%release
%if %uw_build
Requires: %name-externals = %version-%release
Requires: %name-external-libs = %version-%release
%endif

%description all
Include dependencies for all condor packages in a typical installation

%pre
getent group condor >/dev/null || groupadd -r condor
getent passwd condor >/dev/null || \
  useradd -r -g condor -d %_var/lib/condor -s /sbin/nologin \
    -c "Owner of HTCondor Daemons" condor
exit 0


%prep
%if %git_build
%setup -q -c -n %{name}-%{tarball_version}
%else
# For release tarballs
%setup -q -n %{name}-%{tarball_version}
%endif

%if 0%{?osg} || 0%{?hcc}
%patch8 -p1
%endif

# fix errant execute permissions
find src -perm /a+x -type f -name "*.[Cch]" -exec chmod a-x {} \;


%build

# build man files
make -C doc just-man-pages

export CMAKE_PREFIX_PATH=/usr

# Since we don't package the tests and some tests require boost > 1.40, which
# causes build issues with EL5, don't even bother building the tests.

%if %uw_build
%define condor_build_id UW_development

cmake \
       -DBUILDID:STRING=%condor_build_id \
       -DPACKAGEID:STRING=%{version}-%{condor_release} \
       -DUW_BUILD:BOOL=TRUE \
       -DCONDOR_RPMBUILD:BOOL=TRUE \
%if %bundle_uw_externals
       -DEXTERNALS_SOURCE_URL:STRING="$RPM_SOURCE_DIR" \
%endif
       -D_VERBOSE:BOOL=TRUE \
       -DBUILD_TESTING:BOOL=FALSE \
       -DHAVE_BACKFILL:BOOL=FALSE \
       -DHAVE_BOINC:BOOL=FALSE \
%if %cream
       -DWITH_CREAM:BOOL=TRUE \
%else
       -DWITH_CREAM:BOOL=FALSE \
%endif
%ifarch %{ix86}
%if 0%{?rhel} >= 7
       -DWITH_PYTHON_BINDINGS:BOOL=FALSE \
%endif
%endif
       -DPLATFORM:STRING=${NMI_PLATFORM:-unknown} \
       -DCMAKE_VERBOSE_MAKEFILE=ON \
       -DCMAKE_INSTALL_PREFIX:PATH=/usr \
       -DINCLUDE_INSTALL_DIR:PATH=/usr/include \
       -DSYSCONF_INSTALL_DIR:PATH=/etc \
       -DSHARE_INSTALL_PREFIX:PATH=/usr/share \
%ifarch x86_64
       -DCMAKE_INSTALL_LIBDIR:PATH=/usr/lib64 \
       -DLIB_INSTALL_DIR:PATH=/usr/lib64 \
       -DLIB_SUFFIX=64 \
%else
       -DCMAKE_INSTALL_LIBDIR:PATH=/usr/lib \
       -DLIB_INSTALL_DIR:PATH=/usr/lib \
%endif 
       -DBUILD_SHARED_LIBS:BOOL=ON

%else

%cmake -DBUILD_TESTING:BOOL=FALSE \
%if %bundle_uw_externals
       -DEXTERNALS_SOURCE_URL:STRING="$RPM_SOURCE_DIR" \
%endif
%if 0%{?fedora}
       -DBUILDID:STRING=RH-%{version}-%{release} \
       -D_VERBOSE:BOOL=TRUE \
%endif
       -DPACKAGEID:STRING=%{version}-%{condor_release} \
       -DHAVE_BACKFILL:BOOL=FALSE \
       -DHAVE_BOINC:BOOL=FALSE \
       -DHAVE_KBDD:BOOL=TRUE \
       -DHAVE_HIBERNATION:BOOL=TRUE \
       -DWANT_HDFS:BOOL=FALSE \
       -DWITH_ZLIB:BOOL=FALSE \
       -DWANT_CONTRIB:BOOL=ON \
       -DWITH_PIGEON:BOOL=FALSE \
%if %plumage
       -DWITH_PLUMAGE:BOOL=TRUE \
%else
       -DWITH_PLUMAGE:BOOL=FALSE \
%endif
       -DWANT_FULL_DEPLOYMENT:BOOL=TRUE \
%if %qmf
       -DWITH_TRIGGERD:BOOL=TRUE \
       -DWITH_MANAGEMENT:BOOL=TRUE \
       -DWITH_QPID:BOOL=TRUE \
%else
       -DWITH_TRIGGERD:BOOL=FALSE \
       -DWITH_MANAGEMENT:BOOL=FALSE \
       -DWITH_QPID:BOOL=FALSE \
%endif
%if %blahp
       -DBLAHP_FOUND=/usr/libexec/blahp/BLClient \
       -DWITH_BLAHP:BOOL=TRUE \
%else
       -DWITH_BLAHP:BOOL=FALSE \
%endif
%if %cream
       -DWITH_CREAM:BOOL=TRUE \
%else
       -DWITH_CREAM:BOOL=FALSE \
%endif
%if %glexec
       -DWANT_GLEXEC:BOOL=TRUE \
%else
       -DWANT_GLEXEC:BOOL=FALSE \
%endif
       -DWITH_GLOBUS:BOOL=TRUE \
       -DWITH_PYTHON_BINDINGS:BOOL=TRUE \
%if %cgroups
        -DWITH_LIBCGROUP:BOOL=TRUE \
        -DLIBCGROUP_FOUND_SEARCH_cgroup=/%{_lib}/libcgroup.so.1
%endif
%endif

# Patch condor_config.generic for 64-bit rpm
(cd src/condor_examples; patch < condor_config.generic.rpm.patch)

%if %uw_build
# build externals first to avoid dependency issues
make %{?_smp_mflags} externals
%endif
make %{?_smp_mflags}

%install
# installation happens into a temporary location, this function is
# useful in moving files into their final locations
function populate {
  _dest="$1"; shift; _src="$*"
  mkdir -p "%{buildroot}/$_dest"
  mv $_src "%{buildroot}/$_dest"
}

rm -rf %{buildroot}
echo ---------------------------- makefile ---------------------------------
make install DESTDIR=%{buildroot}

# The install target puts etc/ under usr/, let's fix that.
mv %{buildroot}/usr/etc %{buildroot}/%{_sysconfdir}


# Things in /usr/lib really belong in /usr/share/condor
populate %{_datadir}/condor %{buildroot}/%{_usr}/lib/*
# Except for the shared libs
populate %{_libdir}/ %{buildroot}/%{_datadir}/condor/libclassad.so*
rm -f %{buildroot}/%{_datadir}/condor/libclassad.a
mv %{buildroot}%{_datadir}/condor/lib*.so %{buildroot}%{_libdir}/
populate %{_libdir}/condor %{buildroot}/%{_datadir}/condor/condor_ssh_to_job_sshd_config_template
# Drop in a symbolic link for backward compatability
ln -s %{_libdir}/condor/condor_ssh_to_job_sshd_config_template %{buildroot}/%_sysconfdir/condor/condor_ssh_to_job_sshd_config_template

# Only trigger on 32-bit RHEL6
if [ -d %{buildroot}%{_datadir}/condor/python2.6 ]; then
    mv %{buildroot}%{_datadir}/condor/python2.6 %{buildroot}%{_libdir}/
fi

%if %qmf
populate %{_libdir}/condor/plugins %{buildroot}/%{_usr}/libexec/*-plugin.so
%endif

# It is proper to put HTCondor specific libexec binaries under libexec/condor/
populate %_libexecdir/condor %{buildroot}/usr/libexec/*

# man pages go under %{_mandir}
mkdir -p %{buildroot}/%{_mandir}
mv %{buildroot}/usr/man/man1 %{buildroot}/%{_mandir}

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
sed -e "s:^LIB\s*=.*:LIB = \$(RELEASE_DIR)/$LIB/condor:" \
  %{buildroot}/etc/examples/condor_config.generic \
  > %{buildroot}/%{_sysconfdir}/condor/condor_config

# Install the basic configuration, a Personal HTCondor config. Allows for
# yum install condor + service condor start and go.
mkdir -p -m0755 %{buildroot}/%{_sysconfdir}/condor/config.d
%if %parallel_setup
cp %{SOURCE5} %{buildroot}/%{_sysconfdir}/condor/config.d/20dedicated_scheduler_condor.config
%endif

%ifarch %{ix86}
populate %_sysconfdir/condor/config.d %{buildroot}/etc/examples/00-small-shadow
%endif

populate %_sysconfdir/condor/config.d %{buildroot}/etc/examples/00-minicondor
populate %_sysconfdir/condor/config.d %{buildroot}/etc/examples/50ec2.config

%if %qmf
# Install condor-qmf's base plugin configuration
populate %_sysconfdir/condor/config.d %{buildroot}/etc/examples/60condor-qmf.config
%endif

%if %plumage
# Install condor-plumage's base plugin configuration
populate %_sysconfdir/condor/config.d %{buildroot}/etc/examples/62plumage.config
rm -f %{buildroot}/%{_bindir}/ods_job_etl_tool
rm -f %{buildroot}/%{_sbindir}/ods_job_etl_server
mkdir -p -m0755 %{buildroot}/%{_var}/lib/condor/ViewHist
%endif

mkdir -p -m0755 %{buildroot}/%{_var}/run/condor
mkdir -p -m0755 %{buildroot}/%{_var}/log/condor
mkdir -p -m0755 %{buildroot}/%{_var}/lock/condor
mkdir -p -m1777 %{buildroot}/%{_var}/lock/condor/local
# Note we use %{_var}/lib instead of %{_sharedstatedir} for RHEL5 compatibility
mkdir -p -m0755 %{buildroot}/%{_var}/lib/condor/spool
mkdir -p -m1777 %{buildroot}/%{_var}/lib/condor/execute

# not packaging deployment tools
rm -f %{buildroot}/%{_mandir}/man1/condor_config_bind.1
rm -f %{buildroot}/%{_mandir}/man1/condor_cold_start.1
rm -f %{buildroot}/%{_mandir}/man1/condor_cold_stop.1
rm -f %{buildroot}/%{_mandir}/man1/uniq_pid_midwife.1
rm -f %{buildroot}/%{_mandir}/man1/uniq_pid_undertaker.1
rm -f %{buildroot}/%{_mandir}/man1/filelock_midwife.1
rm -f %{buildroot}/%{_mandir}/man1/filelock_undertaker.1
rm -f %{buildroot}/%{_mandir}/man1/install_release.1
rm -f %{buildroot}/%{_mandir}/man1/cleanup_release.1

# not packaging configure/install scripts
rm -f %{buildroot}/%{_mandir}/man1/condor_configure.1

# not packaging legacy cruft
rm -f %{buildroot}/%{_mandir}/man1/condor_master_off.1
rm -f %{buildroot}/%{_mandir}/man1/condor_reconfig_schedd.1

# this one got removed but the manpage was left around
rm -f %{buildroot}/%{_mandir}/man1/condor_glidein.1

# Remove condor_top with no python bindings
%if ! %python
rm -f %{buildroot}/%{_bindir}/condor_top
%endif

# Remove junk
rm -rf %{buildroot}/%{_sysconfdir}/sysconfig
rm -rf %{buildroot}/%{_sysconfdir}/init.d

%if %systemd
# install tmpfiles.d/condor.conf
mkdir -p %{buildroot}%{_tmpfilesdir}
install -m 0644 %{buildroot}/etc/examples/condor-tmpfiles.conf %{buildroot}%{_tmpfilesdir}/%{name}.conf

install -Dp -m0755 %{buildroot}/etc/examples/condor-annex-ec2 %{buildroot}%{_libexecdir}/condor/condor-annex-ec2

mkdir -p %{buildroot}%{_unitdir}
install -m 0644 %{buildroot}/etc/examples/condor-annex-ec2.service %{buildroot}%{_unitdir}/condor-annex-ec2.service
install -m 0644 %{buildroot}/etc/examples/condor.service %{buildroot}%{_unitdir}/condor.service
# Disabled until HTCondor security fixed.
# install -m 0644 %{buildroot}/etc/examples/condor.socket %{buildroot}%{_unitdir}/condor.socket
%if 0%{?osg} || 0%{?hcc}
# Set condor service enviroment variables for LCMAPS on OSG systems
mkdir -p %{buildroot}%{_unitdir}/condor.service.d
install -Dp -m 0644 %{SOURCE3} %{buildroot}%{_unitdir}/condor.service.d/osg-env.conf
%endif
%else
# install the lsb init script
install -Dp -m0755 %{buildroot}/etc/examples/condor.init %{buildroot}%{_initrddir}/condor
install -Dp -m0755 %{buildroot}/etc/examples/condor-annex-ec2 %{buildroot}%{_initrddir}/condor-annex-ec2
%if 0%{?osg} || 0%{?hcc}
# Set condor service enviroment variables for LCMAPS on OSG systems
install -Dp -m 0644 %{SOURCE4} %buildroot/usr/share/osg/sysconfig/condor
%endif
mkdir %{buildroot}%{_sysconfdir}/sysconfig/
install -Dp -m 0644 %{buildroot}/etc/examples/condor.sysconfig %{buildroot}%{_sysconfdir}/sysconfig/condor
%endif

%if 0%{?rhel} >= 7
cp %{SOURCE8} %{buildroot}%{_datadir}/condor/
%endif

# Install perl modules

# Install python-binding libs
%if 0%{?rhel} >= 7
%ifarch x86_64
mv %{buildroot}/usr/lib64/python3.6/site-packages/py3classad.so %{buildroot}/usr/lib64/python3.6/site-packages/classad.so
mv %{buildroot}/usr/lib64/python3.6/site-packages/py3htcondor.so %{buildroot}/usr/lib64/python3.6/site-packages/htcondor.so
%endif
%endif


# we must place the config examples in builddir so %doc can find them
mv %{buildroot}/etc/examples %_builddir/%name-%tarball_version

# Remove stuff that comes from the full-deploy
rm -rf %{buildroot}%{_sbindir}/cleanup_release
rm -rf %{buildroot}%{_sbindir}/condor
rm -rf %{buildroot}%{_sbindir}/condor_cleanup_local
rm -rf %{buildroot}%{_sbindir}/condor_cold_start
rm -rf %{buildroot}%{_sbindir}/condor_cold_stop
rm -rf %{buildroot}%{_sbindir}/condor_config_bind
rm -rf %{buildroot}%{_sbindir}/condor_configure
rm -rf %{buildroot}%{_sbindir}/condor_install
rm -rf %{buildroot}%{_sbindir}/condor_install_local
rm -rf %{buildroot}%{_sbindir}/condor_local_start
rm -rf %{buildroot}%{_sbindir}/condor_local_stop
rm -rf %{buildroot}%{_sbindir}/condor_startd_factory
rm -rf %{buildroot}%{_sbindir}/condor_vm_vmware.pl
rm -rf %{buildroot}%{_sbindir}/filelock_midwife
rm -rf %{buildroot}%{_sbindir}/filelock_undertaker
rm -rf %{buildroot}%{_sbindir}/install_release
rm -rf %{buildroot}%{_sbindir}/uniq_pid_command
rm -rf %{buildroot}%{_sbindir}/uniq_pid_midwife
rm -rf %{buildroot}%{_sbindir}/uniq_pid_undertaker
rm -rf %{buildroot}%{_sbindir}/condor_master_off
rm -rf %{buildroot}%{_sbindir}/condor_reconfig_schedd
rm -rf %{buildroot}%{_datadir}/condor/Execute.pm
rm -rf %{buildroot}%{_datadir}/condor/ExecuteLock.pm
rm -rf %{buildroot}%{_datadir}/condor/FileLock.pm
rm -rf %{buildroot}%{_usrsrc}/chirp/chirp_*
rm -rf %{buildroot}%{_usrsrc}/startd_factory
rm -rf %{buildroot}%{_usrsrc}/drmaa/drmaa-*
rm -rf %{buildroot}/usr/DOC
rm -rf %{buildroot}/usr/INSTALL
rm -rf %{buildroot}/usr/LICENSE-2.0.txt
rm -rf %{buildroot}/usr/NOTICE.txt
rm -rf %{buildroot}/usr/README
rm -rf %{buildroot}/usr/examples/
rm -rf %{buildroot}%{_includedir}/MyString.h
rm -rf %{buildroot}%{_includedir}/chirp_client.h
rm -rf %{buildroot}%{_includedir}/compat_classad*
rm -rf %{buildroot}%{_includedir}/condor_classad.h
rm -rf %{buildroot}%{_includedir}/condor_constants.h
rm -rf %{buildroot}%{_includedir}/condor_event.h
rm -rf %{buildroot}%{_includedir}/condor_header_features.h
rm -rf %{buildroot}%{_includedir}/condor_holdcodes.h
rm -rf %{buildroot}%{_includedir}/file_lock.h
rm -rf %{buildroot}%{_includedir}/iso_dates.h
rm -rf %{buildroot}%{_includedir}/read_user_log.h
rm -rf %{buildroot}%{_includedir}/stl_string_utils.h
rm -rf %{buildroot}%{_includedir}/user_log.README
rm -rf %{buildroot}%{_includedir}/user_log.c++.h
rm -rf %{buildroot}%{_includedir}/usr/include/condor_ast.h
rm -rf %{buildroot}%{_includedir}/condor_astbase.h
rm -rf %{buildroot}%{_includedir}/condor_attrlist.h
rm -rf %{buildroot}%{_includedir}/condor_exprtype.h
rm -rf %{buildroot}%{_includedir}/condor_parser.h
rm -rf %{buildroot}%{_includedir}/write_user_log.h
rm -rf %{buildroot}%{_includedir}/condor_ast.h
rm -rf %{buildroot}%{_includedir}/drmaa.h
rm -rf %{buildroot}%{_includedir}/README
rm -rf %{buildroot}%{_libexecdir}/condor/bgp_*
rm -rf %{buildroot}%{_datadir}/condor/libchirp_client.a
rm -rf %{buildroot}%{_datadir}/condor/libcondorapi.a
rm -rf %{buildroot}%{_mandir}/man1/cleanup_release.1*
rm -rf %{buildroot}%{_mandir}/man1/condor_cold_start.1*
rm -rf %{buildroot}%{_mandir}/man1/condor_cold_stop.1*
rm -rf %{buildroot}%{_mandir}/man1/condor_config_bind.1*
rm -rf %{buildroot}%{_mandir}/man1/condor_configure.1*
rm -rf %{buildroot}%{_mandir}/man1/condor_load_history.1*
rm -rf %{buildroot}%{_mandir}/man1/filelock_midwife.1*
rm -rf %{buildroot}%{_mandir}/man1/filelock_undertaker.1*
rm -rf %{buildroot}%{_mandir}/man1/install_release.1*
rm -rf %{buildroot}%{_mandir}/man1/uniq_pid_midwife.1*
rm -rf %{buildroot}%{_mandir}/man1/uniq_pid_undertaker.1*

rm -rf %{buildroot}%{_datadir}/condor/python/{htcondor,classad}.so
rm -rf %{buildroot}%{_datadir}/condor/{libpyclassad*,htcondor,classad}.so
rm -rf %{buildroot}%{_datadir}/condor/python/{py3htcondor,py3classad}.so
rm -rf %{buildroot}%{_datadir}/condor/{libpy3classad*,py3htcondor,py3classad}.so

# Install BOSCO
mkdir -p %{buildroot}%{python_sitelib}
mv %{buildroot}%{_libexecdir}/condor/campus_factory/python-lib/GlideinWMS %{buildroot}%{python_sitelib}
mv %{buildroot}%{_libexecdir}/condor/campus_factory/python-lib/campus_factory %{buildroot}%{python_sitelib}
%if 0%{?hcc}
mv %{buildroot}%{_libexecdir}/condor/campus_factory/share/condor/condor_config.factory %{buildroot}%{_sysconfdir}/condor/config.d/60-campus_factory.config
%endif
%if 0%{?osg} || 0%{?hcc}
mv %{buildroot}%{_libexecdir}/condor/campus_factory/etc/campus_factory.conf %{buildroot}%{_sysconfdir}/condor/
%endif
mv %{buildroot}%{_libexecdir}/condor/campus_factory/share %{buildroot}%{_datadir}/condor/campus_factory

%if %blahp && ! %uw_build
install -p -m 0644 %{SOURCE6} %{buildroot}%{_sysconfdir}/condor/config.d/10-batch_gahp_blahp.config
%endif

%if 0%{?osg} || 0%{?hcc}
install -p -m 0644 %{SOURCE7} %{buildroot}%{_sysconfdir}/condor/config.d/00-restart_peaceful.config
%endif

%if %uw_build
populate %{_libdir}/condor %{buildroot}/%{_libdir}/libdrmaa.so
populate %{_libdir}/condor %{buildroot}/%{_datadir}/condor/condor/libglobus*.so*
populate %{_libdir}/condor %{buildroot}/%{_datadir}/condor/condor/libvomsapi*.so*
populate %{_libdir}/condor %{buildroot}/%{_datadir}/condor/libcondordrmaa.a
# these probably belong elsewhere
populate %{_libdir}/condor %{buildroot}/%{_datadir}/condor/ugahp.jar
%endif


%clean
rm -rf %{buildroot}


%check
# This currently takes hours and can kill your machine...
#cd condor_tests
#make check-seralized

#################
%files all
#################
%files
%exclude %_sbindir/openstack_gahp
%defattr(-,root,root,-)
%doc LICENSE-2.0.txt NOTICE.txt examples
%dir %_sysconfdir/condor/
%config %_sysconfdir/condor/condor_config
%if %systemd
%{_tmpfilesdir}/%{name}.conf
%{_unitdir}/condor.service
%if 0%{?osg} || 0%{?hcc}
%{_unitdir}/condor.service.d/osg-env.conf
%endif
# Disabled until HTCondor security fixed.
# %{_unitdir}/condor.socket
%else
%_initrddir/condor
%if 0%{?osg} || 0%{?hcc}
/usr/share/osg/sysconfig/condor
%endif
%config(noreplace) /etc/sysconfig/condor
%endif
%dir %_datadir/condor/
%_datadir/condor/Chirp.jar
%_datadir/condor/CondorJavaInfo.class
%_datadir/condor/CondorJavaWrapper.class
%_datadir/condor/scimark2lib.jar
%if 0%{?rhel} >= 7
%_datadir/condor/htcondor.pp
%endif
%dir %_sysconfdir/condor/config.d/
%_libdir/condor/condor_ssh_to_job_sshd_config_template
%_sysconfdir/condor/condor_ssh_to_job_sshd_config_template
%_sysconfdir/bash_completion.d/condor
%_libdir/libchirp_client.so
%_libdir/libcondor_utils_%{version_}.so
%_libdir/libcondorapi.so
%_libdir/libgetpwnam.so
%dir %_libexecdir/condor/
%_libexecdir/condor/linux_kernel_tuning
%_libexecdir/condor/accountant_log_fixer
%_libexecdir/condor/condor_chirp
%_libexecdir/condor/condor_ssh
%_libexecdir/condor/sshd.sh
%_libexecdir/condor/get_orted_cmd.sh
%_libexecdir/condor/orted_launcher.sh
%_libexecdir/condor/condor_job_router
%_libexecdir/condor/condor_pid_ns_init
%_libexecdir/condor/condor_urlfetch
%if %glexec
%_libexecdir/condor/condor_glexec_setup
%_libexecdir/condor/condor_glexec_run
%_libexecdir/condor/condor_glexec_job_wrapper
%_libexecdir/condor/condor_glexec_update_proxy
%_libexecdir/condor/condor_glexec_cleanup
%_libexecdir/condor/condor_glexec_kill
%endif
%if %blahp
%dir %_libexecdir/condor/glite/bin
%_libexecdir/condor/glite/bin/nqs_cancel.sh
%_libexecdir/condor/glite/bin/nqs_hold.sh
%_libexecdir/condor/glite/bin/nqs_resume.sh
%_libexecdir/condor/glite/bin/nqs_status.sh
%_libexecdir/condor/glite/bin/nqs_submit.sh
%_libexecdir/condor/glite/bin/slurm_cancel.sh
%_libexecdir/condor/glite/bin/slurm_hold.sh
%_libexecdir/condor/glite/bin/slurm_resume.sh
%_libexecdir/condor/glite/bin/slurm_status.py
%_libexecdir/condor/glite/bin/slurm_status.sh
%_libexecdir/condor/glite/bin/slurm_submit.sh
%if ! %uw_build
%config(noreplace) %{_sysconfdir}/condor/config.d/10-batch_gahp_blahp.config
%endif
%endif
%if 0%{?osg} || 0%{?hcc}
%config(noreplace) %{_sysconfdir}/condor/config.d/00-restart_peaceful.config
%endif
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
%_libexecdir/condor/box_plugin.pyc
%_libexecdir/condor/box_plugin.pyo
%_libexecdir/condor/gdrive_plugin.py
%_libexecdir/condor/gdrive_plugin.pyc
%_libexecdir/condor/gdrive_plugin.pyo
%_libexecdir/condor/onedrive_plugin.py
%_libexecdir/condor/onedrive_plugin.pyc
%_libexecdir/condor/onedrive_plugin.pyo
%_libexecdir/condor/curl_plugin
%_libexecdir/condor/legacy_curl_plugin
%_libexecdir/condor/condor_shared_port
%_libexecdir/condor/condor_glexec_wrapper
%_libexecdir/condor/glexec_starter_setup.sh
%_libexecdir/condor/condor_defrag
%_libexecdir/condor/interactive.sub
%_libexecdir/condor/condor_gangliad
%_libexecdir/condor/panda-plugin.so
%_libexecdir/condor/pandad
%_mandir/man1/condor_advertise.1.gz
%_mandir/man1/condor_annex.1.gz
%_mandir/man1/condor_check_userlogs.1.gz
%_mandir/man1/condor_chirp.1.gz
%_mandir/man1/condor_cod.1.gz
%_mandir/man1/condor_config_val.1.gz
%_mandir/man1/condor_convert_history.1.gz
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
%_mandir/man1/condor_reschedule.1.gz
%_mandir/man1/condor_restart.1.gz
%_mandir/man1/condor_rm.1.gz
%_mandir/man1/condor_run.1.gz
%_mandir/man1/condor_set_shutdown.1.gz
%_mandir/man1/condor_sos.1.gz
%_mandir/man1/condor_stats.1.gz
%_mandir/man1/condor_status.1.gz
%_mandir/man1/condor_store_cred.1.gz
%_mandir/man1/condor_submit.1.gz
%_mandir/man1/condor_submit_dag.1.gz
%_mandir/man1/condor_top.1.gz
%_mandir/man1/condor_transfer_data.1.gz
%_mandir/man1/condor_transform_ads.1.gz
%_mandir/man1/condor_update_machine_ad.1.gz
%_mandir/man1/condor_updates_stats.1.gz
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
%_mandir/man1/condor_install.1.gz
%_mandir/man1/condor_ping.1.gz
%_mandir/man1/condor_rmdir.1.gz
%_mandir/man1/condor_tail.1.gz
%_mandir/man1/condor_who.1.gz
%_mandir/man1/condor_now.1.gz
# bin/condor is a link for checkpoint, reschedule, vacate
%_bindir/condor_submit_dag
%_bindir/condor_who
%_bindir/condor_now
%_bindir/condor_prio
%_bindir/condor_transfer_data
%_bindir/condor_check_userlogs
%_bindir/condor_q
%_libexecdir/condor/condor_transferer
%_bindir/condor_cod
%_bindir/condor_docker_enter
%_bindir/condor_qedit
%_bindir/condor_userlog
%_bindir/condor_release
%_bindir/condor_userlog_job_counter
%_bindir/condor_config_val
%_bindir/condor_reschedule
%_bindir/condor_userprio
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
%_bindir/condor_evicted_files
# sbin/condor is a link for master_off, off, on, reconfig,
# reconfig_schedd, restart
%_sbindir/condor_advertise
%_sbindir/condor_aklog
%_sbindir/condor_c-gahp
%_sbindir/condor_c-gahp_worker_thread
%_sbindir/condor_collector
%_sbindir/condor_convert_history
%_sbindir/condor_credd
%_sbindir/condor_fetchlog
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
%_sbindir/condor_sos
%_sbindir/condor_startd
%_sbindir/condor_starter
%_sbindir/condor_store_cred
%_sbindir/condor_testwritelog
%_sbindir/condor_transferd
%_sbindir/condor_updates_stats
%_sbindir/ec2_gahp
%_sbindir/condor_gridmanager
%_sbindir/condor_gridshell
%_sbindir/gahp_server
%_sbindir/grid_monitor
%_sbindir/grid_monitor.sh
%_sbindir/remote_gahp
%_sbindir/nordugrid_gahp
%_sbindir/AzureGAHPServer
%_sbindir/gce_gahp
%if %uw_build
%_sbindir/boinc_gahp
%endif
%_libexecdir/condor/condor_gpu_discovery
%_libexecdir/condor/condor_gpu_utilization
%_sbindir/condor_vm-gahp-vmware
%_sbindir/condor_vm_vmware
%config(noreplace) %_sysconfdir/condor/ganglia.d/00_default_metrics
%defattr(-,condor,condor,-)
%dir %_var/lib/condor/
%dir %_var/lib/condor/execute/
%dir %_var/log/condor/
%dir %_var/lib/condor/spool/
%dir %_var/lock/condor
%dir %_var/lock/condor/local
%dir %_var/run/condor

#################
%files procd
%_sbindir/condor_procd
%_sbindir/gidd_alloc
%_sbindir/procd_ctl
%_mandir/man1/procd_ctl.1.gz
%_mandir/man1/gidd_alloc.1.gz
%_mandir/man1/condor_procd.1.gz

#################
%if %qmf
%files qmf
%defattr(-,root,root,-)
%doc LICENSE-2.0.txt NOTICE.txt
%_sysconfdir/condor/config.d/60condor-qmf.config
%dir %_libdir/condor/plugins
%_libdir/condor/plugins/MgmtCollectorPlugin-plugin.so
%_libdir/condor/plugins/MgmtMasterPlugin-plugin.so
%_libdir/condor/plugins/MgmtNegotiatorPlugin-plugin.so
%_libdir/condor/plugins/MgmtScheddPlugin-plugin.so
%_libdir/condor/plugins/MgmtStartdPlugin-plugin.so
%_bindir/get_trigger_data
%_sbindir/condor_trigger_config
%_sbindir/condor_triggerd
%_sbindir/condor_job_server
%endif

#################
%if %plumage
%files plumage
%defattr(-,root,root,-)
%doc LICENSE-2.0.txt NOTICE.txt
%_sysconfdir/condor/config.d/62plumage.config
%dir %_libdir/condor/plugins
%_libdir/condor/plugins/PlumageCollectorPlugin-plugin.so
%dir %_datadir/condor/plumage
%_sbindir/plumage_job_etl_server
%_bindir/plumage_history_load
%_bindir/plumage_stats
%_bindir/plumage_history
%_datadir/condor/plumage/README
%_datadir/condor/plumage/SCHEMA
%_datadir/condor/plumage/plumage_accounting
%_datadir/condor/plumage/plumage_scheduler
%_datadir/condor/plumage/plumage_utilization
%defattr(-,condor,condor,-)
%endif

#################
%files kbdd
%defattr(-,root,root,-)
%doc LICENSE-2.0.txt NOTICE.txt
%_sbindir/condor_kbdd

#################
%files vm-gahp
%defattr(-,root,root,-)
%doc LICENSE-2.0.txt NOTICE.txt
%_sbindir/condor_vm-gahp
%_libexecdir/condor/libvirt_simple_script.awk

#################
%files classads
%defattr(-,root,root,-)
%doc LICENSE-2.0.txt NOTICE.txt
%_libdir/libclassad.so.*

#################
%files classads-devel
%defattr(-,root,root,-)
%doc LICENSE-2.0.txt NOTICE.txt
%_bindir/classad_functional_tester
%_bindir/classad_version
%_libdir/libclassad.so
%dir %_includedir/classad/
%_includedir/classad/attrrefs.h
%_includedir/classad/cclassad.h
%_includedir/classad/classad_distribution.h
%_includedir/classad/classadErrno.h
%_includedir/classad/classad.h
%_includedir/classad/classadItor.h
%_includedir/classad/classadCache.h
%_includedir/classad/classad_stl.h
%_includedir/classad/collectionBase.h
%_includedir/classad/collection.h
%_includedir/classad/common.h
%_includedir/classad/debug.h
%_includedir/classad/exprList.h
%_includedir/classad/exprTree.h
%_includedir/classad/fnCall.h
%_includedir/classad/indexfile.h
%_includedir/classad/jsonSink.h
%_includedir/classad/jsonSource.h
%_includedir/classad/lexer.h
%_includedir/classad/lexerSource.h
%_includedir/classad/literals.h
%_includedir/classad/matchClassad.h
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

#################
%files test
%defattr(-,root,root,-)
%_libexecdir/condor/condor_sinful
%_libexecdir/condor/condor_testingd
%_libexecdir/condor/test_user_mapping

%if %cream
%files cream-gahp
%defattr(-,root,root,-)
%doc LICENSE-2.0.txt NOTICE.txt
%_sbindir/cream_gahp
%endif

%if %parallel_setup
%files parallel-setup
%defattr(-,root,root,-)
%config(noreplace) %_sysconfdir/condor/config.d/20dedicated_scheduler_condor.config
%endif

%if %python
%files -n python2-condor
%defattr(-,root,root,-)
%_bindir/condor_top
%_libdir/libpyclassad*.so
%_libexecdir/condor/libclassad_python_user.so
%_libexecdir/condor/libcollector_python_plugin.so
%{python_sitearch}/classad.so
%{python_sitearch}/htcondor.so

%if 0%{?rhel} >= 7
%ifarch x86_64
%files -n python3-condor
%defattr(-,root,root,-)
%_bindir/condor_top
%_libdir/libpy3classad*.so
%_libexecdir/condor/libclassad_python3_user.so
%_libexecdir/condor/libcollector_python3_plugin.so
/usr/lib64/python3.6/site-packages/classad.so
/usr/lib64/python3.6/site-packages/htcondor.so
%endif
%endif
%endif

%files bosco
%defattr(-,root,root,-)
%if 0%{?hcc}
%config(noreplace) %_sysconfdir/condor/config.d/60-campus_factory.config
%endif
%if 0%{?osg} || 0%{?hcc}
%config(noreplace) %_sysconfdir/condor/campus_factory.conf
%endif
%_libexecdir/condor/shellselector
%_libexecdir/condor/campus_factory
%_sbindir/bosco_install
%_sbindir/campus_factory
%_sbindir/condor_ft-gahp
%_sbindir/runfactory
%_bindir/bosco_cluster
%_bindir/bosco_ssh_start
%_bindir/bosco_start
%_bindir/bosco_stop
%_bindir/bosco_findplatform
%_bindir/bosco_uninstall
%_bindir/bosco_quickstart
%_bindir/htsub
%_sbindir/glidein_creation
%_datadir/condor/campus_factory
%{python_sitelib}/GlideinWMS
%{python_sitelib}/campus_factory
%_mandir/man1/bosco_cluster.1.gz
%_mandir/man1/bosco_findplatform.1.gz
%_mandir/man1/bosco_install.1.gz
%_mandir/man1/bosco_ssh_start.1.gz
%_mandir/man1/bosco_start.1.gz
%_mandir/man1/bosco_stop.1.gz
%_mandir/man1/bosco_uninstall.1.gz

%files -n minicondor
%config(noreplace) %_sysconfdir/condor/config.d/00-minicondor


%if %uw_build

%ifarch %{ix86}
%files small-shadow
%{_sbindir}/condor_shadow_s
%config(noreplace) %_sysconfdir/condor/config.d/00-small-shadow
%endif

%files external-libs
%dir %_libdir/condor
%_libdir/condor/libcondordrmaa.a
%_libdir/condor/libdrmaa.so
%_libdir/condor/libglobus*.so*
%_libdir/condor/libvomsapi*.so*
%_libdir/condor/ugahp.jar

%files externals
%_sbindir/unicore_gahp
%if %blahp
%_libexecdir/condor/glite/bin/BLClient
%_libexecdir/condor/glite/bin/BLParserLSF
%_libexecdir/condor/glite/bin/BLParserPBS
%_libexecdir/condor/glite/bin/BNotifier
%_libexecdir/condor/glite/bin/BPRclient
%_libexecdir/condor/glite/bin/BPRserver
%_libexecdir/condor/glite/bin/BUpdaterCondor
%_libexecdir/condor/glite/bin/BUpdaterLSF
%_libexecdir/condor/glite/bin/BUpdaterPBS
%_libexecdir/condor/glite/bin/BUpdaterSGE
%_libexecdir/condor/glite/bin/batch_gahp
%_libexecdir/condor/glite/bin/batch_gahp_daemon
%_libexecdir/condor/glite/bin/blah_check_config
%_libexecdir/condor/glite/bin/blah_common_submit_functions.sh
%_libexecdir/condor/glite/bin/blah_job_registry_add
%_libexecdir/condor/glite/bin/blah_job_registry_dump
%_libexecdir/condor/glite/bin/blah_job_registry_lkup
%_libexecdir/condor/glite/bin/blah_job_registry_scan_by_subject
%_libexecdir/condor/glite/bin/blah_load_config.sh
%_libexecdir/condor/glite/bin/blparser_master
%_libexecdir/condor/glite/bin/condor_cancel.sh
%_libexecdir/condor/glite/bin/condor_hold.sh
%_libexecdir/condor/glite/bin/condor_resume.sh
%_libexecdir/condor/glite/bin/condor_status.sh
%_libexecdir/condor/glite/bin/condor_submit.sh
%_libexecdir/condor/glite/bin/lsf_cancel.sh
%_libexecdir/condor/glite/bin/lsf_hold.sh
%_libexecdir/condor/glite/bin/lsf_resume.sh
%_libexecdir/condor/glite/bin/lsf_status.sh
%_libexecdir/condor/glite/bin/lsf_submit.sh
%_libexecdir/condor/glite/bin/pbs_cancel.sh
%_libexecdir/condor/glite/bin/pbs_hold.sh
%_libexecdir/condor/glite/bin/pbs_resume.sh
%_libexecdir/condor/glite/bin/pbs_status.py
%_libexecdir/condor/glite/bin/pbs_status.sh
%_libexecdir/condor/glite/bin/pbs_submit.sh
%_libexecdir/condor/glite/bin/runcmd.pl.template
%_libexecdir/condor/glite/bin/sge_cancel.sh
%_libexecdir/condor/glite/bin/sge_filestaging
%_libexecdir/condor/glite/bin/sge_helper
%_libexecdir/condor/glite/bin/sge_hold.sh
%_libexecdir/condor/glite/bin/sge_local_submit_attributes.sh
%_libexecdir/condor/glite/bin/sge_resume.sh
%_libexecdir/condor/glite/bin/sge_status.sh
%_libexecdir/condor/glite/bin/sge_submit.sh
%_libexecdir/condor/glite/bin/test_condor_logger
# does this really belong here?
%dir %_libexecdir/condor/glite/etc
%_libexecdir/condor/glite/etc/glite-ce-blahparser
%_libexecdir/condor/glite/etc/glite-ce-blparser
%_libexecdir/condor/glite/etc/glite-ce-check-blparser
%_libexecdir/condor/glite/etc/batch_gahp.config
%_libexecdir/condor/glite/etc/batch_gahp.config.template
%_libexecdir/condor/glite/etc/blparser.conf.template
%dir %_libexecdir/condor/glite/share
%dir %_libexecdir/condor/glite/share/doc
%_libexecdir/condor/glite/share/doc/glite-ce-blahp-@PVER@/LICENSE
%endif

%endif

%if %systemd

%post
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
   /usr/sbin/setsebool -P condor_domain_can_network_connect 1
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
/bin/systemctl daemon-reload >/dev/null 2>&1 || :
# Note we don't try to restart - HTCondor will automatically notice the
# binary has changed and do graceful or peaceful restart, based on its
# configuration

%triggerun -- condor < 7.7.0-0.5

/usr/bin/systemd-sysv-convert --save condor >/dev/null 2>&1 ||:

/sbin/chkconfig --del condor >/dev/null 2>&1 || :
/bin/systemctl try-restart condor.service >/dev/null 2>&1 || :

%else
%post -n condor
/sbin/chkconfig --add condor
/sbin/ldconfig

%posttrans -n condor
# If there is a saved condor_config.local, recover it
if [ -f /etc/condor/condor_config.local.rpmsave ]; then
    if [ ! -f /etc/condor/condor_config.local ]; then
        mv /etc/condor/condor_config.local.rpmsave \
           /etc/condor/condor_config.local

        # Drop a README file to tell what we have done
        # Make sure that we don't overwrite a previous README
        if [ ! -f /etc/condor/README.condor_config.local ]; then
            file="/etc/condor/README.condor_config.local"
        else
            i="1"
            while [ -f /etc/condor/README.condor_config.local.$i ]; do
                i=$((i+1))
            done
            file="/etc/condor/README.condor_config.local.$i"
        fi

cat <<EOF > $file
On `date`, while installing or upgrading to
HTCondor %version, the /etc/condor directory contained a file named
"condor_config.local.rpmsave" but did not contain one named
"condor_config.local".  This situation may be the result of prior
modifications to "condor_config.local" that were preserved after the
HTCondor RPM stopped including that file.  In any case, the contents
of the old "condor_config.local.rpmsave" file may still be useful.
So after the install it was moved back into place and this README
file was created.  Here is a directory listing for the restored file
at that time:

`ls -l /etc/condor/condor_config.local`

See the "Configuration" section (3.3) of the HTCondor manual for more
information on configuration files.
EOF

    fi
fi

%preun -n condor
if [ $1 = 0 ]; then
  /sbin/service condor stop >/dev/null 2>&1 || :
  /sbin/chkconfig --del condor
fi


%postun -n condor
# Note we don't try to restart - HTCondor will automatically notice the
# binary has changed and do graceful or peaceful restart, based on its
# configuration
/sbin/ldconfig
%endif

%changelog
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
