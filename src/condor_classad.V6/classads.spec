%define version 1.0rc2
%define revision 1


Summary: Condor's ClassAd Library
Name: classads
Version: %version
Release: %revision
License: Condor Public License
Group: Development/Libraries
URL: http://www.cs.wisc.edu/condor/classad/
Vendor: Condor Project
Packager: Matthew Farrellee <mfarrellee@redhat.com>
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

BuildRequires: libtool
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
Summary: Header files for the ClassAd library
Group: Development/System
Requires: %name = %version-%release
Requires: pcre-devel

%description devel
Header files for the ClassAd library, a powerful and flexible,
semi-structured representation of data.

%prep
%setup -q

%build
%configure --enable-namespace --enable-flexible-member CC=g++
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
rm -rf %{buildroot}%_libdir/*.la
rm -rf %{buildroot}%_libdir/*.so

%clean
rm -rf %{buildroot}

%check
make check

%files
%defattr(-,root,root,-)
%doc LICENSE README CHANGELOG
%_libdir/libclassad.so.0
%_libdir/libclassad.so.0.0.0
%_libdir/libclassad_ns.so.0
%_libdir/libclassad_ns.so.0.0.0


%files devel
%defattr(-,root,root,-)
%doc LICENSE README CHANGELOG
%_bindir/classad_version
%_bindir/classad_version_ns
%_bindir/classad_functional_tester
%_bindir/classad_functional_tester_ns
%_bindir/cxi
%_bindir/cxi_ns
%_libdir/libclassad.a
%_libdir/libclassad_ns.a
%_includedir/classad/attrrefs.h
%_includedir/classad/cclassad.h
%_includedir/classad/classad_distribution.h
%_includedir/classad/classadErrno.h
%_includedir/classad/classad.h
%_includedir/classad/classadItor.h
%_includedir/classad/classad_stl.h
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
%doc

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%changelog
* Fri Aug  3 2007  <mfarrellee@redhat.com> - 0.9.9-1
- Initial build.
