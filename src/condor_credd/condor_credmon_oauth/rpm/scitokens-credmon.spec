%global pypi_name scitokens-credmon

Name:           %{pypi_name}
Version:        0.7
Release:        1%{?dist}
Summary:        SciTokens credential monitor for use with HTCondor

License:        MIT
URL:            https://github.com/htcondor/scitokens-credmon
Source0:        %{pypi_name}-%{version}.tar.gz
BuildArch:      noarch
 
BuildRequires:  python2-devel >= 2.7
BuildRequires:  python2-setuptools

%description
A HTCondor credentials monitor specific for OAuth2 and SciTokens workflows.

%package -n     python2-%{pypi_name}
Summary:        SciTokens credential monitor for use with HTCondor
%{?python_provide:%python_provide python2-%{pypi_name}}
 
Requires:       python2-condor
Requires:       python2-requests-oauthlib
Requires:       python-six
Requires:       python-flask
Requires:       python2-cryptography
Requires:       python2-scitokens
Requires:       condor >= 8.8.2
Requires:       httpd
Requires:       mod_wsgi

%description -n python2-%{pypi_name}


%prep
%autosetup -n %{pypi_name}-%{version}
# Remove pre-built egg-info
rm -rf %{pypi_name}.egg-info

%build
%py2_build

%install
%py2_install
ln -s %{_bindir}/condor_credmon_oauth %{buildroot}/%{_bindir}/scitokens_credmon
mkdir -p %{buildroot}/%{_var}/lib/condor/oauth_credentials
mv examples/config/README.credentials %{buildroot}/%{_var}/lib/condor/oauth_credentials
mkdir -p %{buildroot}/%{_var}/www/wsgi-scripts/scitokens-credmon
mv examples/wsgi/scitokens-credmon.wsgi %{buildroot}/%{_var}/www/wsgi-scripts/scitokens-credmon/scitokens-credmon.wsgi
rmdir examples/wsgi

%files -n python2-%{pypi_name}
%doc LICENSE README.md examples
%{_bindir}/condor_credmon_oauth
%{_bindir}/scitokens_credmon
%{_bindir}/scitokens_credential_producer
%{python2_sitelib}/credmon
%{python2_sitelib}/scitokens_credmon-*.egg-info
%{_var}/lib/condor/oauth_credentials/README.credentials
%ghost %{_var}/lib/condor/oauth_credentials/wsgi_session_key
%ghost %{_var}/lib/condor/oauth_credentials/CREDMON_COMPLETE
%ghost %{_var}/lib/condor/oauth_credentials/pid
%{_var}/www/wsgi-scripts/scitokens-credmon

%changelog
* Mon Jun 01 2020 Jason Patton <jpatton@cs.wisc.edu> - 0.7-1
- Use dweitzel\'s subprocess queuing

* Tue Mar 31 2020 Jason Patton <jpatton@cs.wisc.edu> - 0.6-1
- Conform to new HTCondor OAuth config behavior

* Thu Mar 05 2020 Jason Patton <jpatton@cs.wisc.edu> - 0.5-1
- Change token deletion behavior.

* Tue Oct 08 2019 Jason Patton <jpatton@cs.wisc.edu> - 0.4-1
- Move configuration into examples directory.

* Thu May 02 2019 Jason Patton <jpatton@cs.wisc.edu> - 0.3-1
- Remove automatic install of config files. Put README in creddir.

* Fri Feb 08 2019 Brian Bockelman <brian.bockelman@cern.ch> - 0.2-1
- Include proper packaging and WSGI scripts for credmon.

* Fri Feb 08 2019 Brian Bockelman <brian.bockelman@cern.ch> - 0.1-1
- Initial package version as uploaded to the Test PyPI instance.
