# NOTE: When updating the version change the Source0 and %setup lines

Summary: Condor's ClassAd Library
Name: classads
Version: 1.0
Release: 0.1.rc3
License: ASL 2.0
Group: Development/Libraries
URL: http://www.cs.wisc.edu/condor/classad/
Source0: ftp://ftp.cs.wisc.edu/condor/classad/c++/classads-1.0rc3.tar.gz
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

#BuildRequires: libtool
#BuildRequires: pkgconfig
BuildRequires: pcre-devel

Requires: pcre

%description
Classified Advertisements (classads) are the lingua franca of
Condor. They are used for describing jobs, workstations, and other
resources. They are exchanged by Condor processes to schedule
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
matching is used by the Condor central manager to determine the
compatibility of jobs and workstations where they may be run.

%package devel
Summary: Headers for Condor's ClassAd Library
Group: Development/System
Requires: %name = %version-%release
Requires: pcre-devel

%description devel
Header files for Condor's ClassAd Library, a powerful and flexible,
semi-structured representation of data.

%package static
Summary: Condor's ClassAd Library's static libraries
Group: Development/System
Requires: %name = %version-%release

%description static
Static versions of Condor's ClassAd Library's libraries, a powerful
and flexible, semi-structured representation of data.

%prep
%setup -q -n classads-1.0rc3

%build
%configure --enable-namespace --enable-flexible-member CC=g++
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
rm -rf %{buildroot}%_libdir/*.la

%clean
rm -rf %{buildroot}

%check
make check

%files
%defattr(-,root,root,-)
%doc LICENSE README CHANGELOG NOTICE.txt
%_libdir/libclassad.so.0
%_libdir/libclassad.so.0.0.0
%_libdir/libclassad_ns.so.0
%_libdir/libclassad_ns.so.0.0.0


%files devel
%defattr(-,root,root,-)
%doc LICENSE README CHANGELOG NOTICE.txt
%_bindir/classad_version
%_bindir/classad_version_ns
%_bindir/classad_functional_tester
%_bindir/classad_functional_tester_ns
%_bindir/cxi
%_bindir/cxi_ns
%_libdir/libclassad.so
%_libdir/libclassad_ns.so
%_includedir/classad/attrrefs.h
%_includedir/classad/cclassad.h
%_includedir/classad/classad_distribution.h
%_includedir/classad/classadErrno.h
%_includedir/classad/classad.h
%_includedir/classad/classadItor.h
%_includedir/classad/classad_containers.h
%_includedir/classad/collectionBase.h
%_includedir/classad/collection.h
%_includedir/classad/common.h
%_includedir/classad/debug.h
%_includedir/classad/exprList.h
%_includedir/classad/exprTree.h
%_includedir/classad/fnCall.h
%_includedir/classad/indexfile.h
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

%files static
%defattr(-,root,root,-)
%doc LICENSE README CHANGELOG NOTICE.txt
%_libdir/libclassad.a
%_libdir/libclassad_ns.a

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%changelog
* Thu Jan 24 2008  <mfarrellee@redhat> - 1.0-0.1.rc3
- Updated to Apache Licensed version, 1.0.0rc3
- Added LICENSE and NOTICE.txt to doc lines

* Fri Aug 17 2007  <mfarrellee@redhat> - 1.0-0.1.rc2
- Fixed Release tag

* Tue Aug 14 2007  <mfarrellee@redhat> - 1.0-rc2
- See CHANGELOG

* Fri Aug  3 2007  <mfarrellee@redhat> - 0.9.9-1
- Initial build.
