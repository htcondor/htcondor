# Have gitrev be the short hash or branch name if doing a prerelease build
#define gitrev
%define bl_sysconfdir %{_sysconfdir}/%{name}
%define bl_libexecdir %{_libexecdir}/%{name}

# Force brp-python-bytecompile to use Python 3
%if ! 0%{?rhel} == 7
%global __python /usr/bin/python3
%endif

Name:		blahp
Version:	2.2.0
Release:	1%{?gitrev:.%{gitrev}}%{?dist}
Summary:	gLite BLAHP daemon

Group:		System/Libraries
License:	Apache 2.0
URL:		https://github.com/htcondor/BLAH

# Pre-release build tarballs should be generated with:
# git archive %{gitrev} | gzip -9 > %{name}-%{version}-%{gitrev}.tar.gz
Source0:        %{name}-%{version}%{?gitrev:-%{gitrev}}.tar.gz

BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  gcc-c++
BuildRequires:  libtool
BuildRequires:  make
BuildRequires:  condor-classads-devel
BuildRequires:  docbook-style-xsl, libxslt

%if ! 0%{?rhel} == 7
Requires:       python3
%endif

%description
%{summary}

%prep
%setup

%build
%if 0%{?rhel} == 7
# Don't rely on Python 3 on EL7 (not installed by default)
sed -i 's;/usr/bin/python3;/usr/bin/python2;' src/scripts/*status.py
%endif
./bootstrap
export CPPFLAGS="-I/usr/include/classad -std=c++11 -fcommon"
export LDFLAGS="-lclassad"
%configure --with-classads-prefix=/usr --without-globus --with-glite-location=/usr
unset CPPFLAGS
unset LDFLAGS
make %{?_smp_mflags}

%install
make install DESTDIR=$RPM_BUILD_ROOT

rm -f $RPM_BUILD_ROOT%{_libdir}/*.la
rm -f $RPM_BUILD_ROOT%{_libdir}/*.a

# Move all the blahp scripts into /usr/libexec/blahp
mkdir blahp
mv $RPM_BUILD_ROOT%{_libexecdir}/* blahp
install -m 0755 -d -p $RPM_BUILD_ROOT%{bl_libexecdir}/
mv blahp/* $RPM_BUILD_ROOT%{bl_libexecdir}/

# Correct the config file location
install -m 0755 -d -p $RPM_BUILD_ROOT%{_sysconfdir}
mv $RPM_BUILD_ROOT%{_sysconfdir}/blah.config.template $RPM_BUILD_ROOT%{_sysconfdir}/blah.config
mv $RPM_BUILD_ROOT%{_sysconfdir}/blparser.conf.template $RPM_BUILD_ROOT%{_sysconfdir}/blparser.conf
echo "blah_libexec_directory=/usr/libexec/blahp" >> $RPM_BUILD_ROOT%{_sysconfdir}/blah.config

# Insert appropriate templates for LSF, SGE, Slurm, and HTCondor; admins will need to change these
install -m 0755 -d -p $RPM_BUILD_ROOT%{bl_sysconfdir}

for batch_system in condor kubernetes lsf nqs pbs sge slurm; do
    mv $RPM_BUILD_ROOT%{bl_libexecdir}/${batch_system}_local_submit_attributes.sh $RPM_BUILD_ROOT%{bl_sysconfdir}/
done

# Create local_submit_attributes.sh symlinks in /etc/blahp
for batch_system in condor kubernetes lsf nqs pbs sge slurm; do
    ln -s %{bl_sysconfdir}/${batch_system}_local_submit_attributes.sh \
       $RPM_BUILD_ROOT%{bl_libexecdir}/${batch_system}_local_submit_attributes.sh
done

mv $RPM_BUILD_ROOT%{_docdir}/glite-ce-blahp-@PVER@ $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}

%post

if [ $1 -eq 1 ] ; then
    /sbin/chkconfig --add glite-ce-blah-parser
fi

%preun

if [ $1 -eq 0 ] ; then
    /sbin/service glite-ce-blah-parser stop >/dev/null 2>&1
    /sbin/chkconfig --del glite-ce-blah-parser
fi

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_sbindir}/*
%{_libexecdir}/%{name}
%{_docdir}/%{name}-%{version}
%config(noreplace) %{_sysconfdir}/blparser.conf
%config(noreplace) %{_sysconfdir}/blah.config
%dir %{_sysconfdir}/%{name}
%config(noreplace) %{bl_sysconfdir}/*.sh
%{_mandir}/man1/*
%{_initrddir}/glite-ce-*

%changelog
* Fri Oct 22 2021 Tim Theisen <tim@cs.wisc.edu> 2.2.0-1
- Build without Globus

* Tue Oct 12 2021 Tim Theisen <tim@cs.wisc.edu> 2.1.3-1
- Fix status caching on EL7 for PBS, Slurm, and LSF

* Mon Sep 20 2021 Tim Theisen <tim@cs.wisc.edu> 2.1.2-1
- Include the more efficient LSF status script

* Tue Aug 17 2021 Tim Theisen <tim@cs.wisc.edu> 2.1.1-1
- Add Python 2 support back for Enterprise Linux 7
- Allow the user to override system configuration files
- Enable flexible configuration via a configuration directory
- Fix Slurm resource usage reporting

* Tue Jul 06 2021 Tim Theisen <tim@cs.wisc.edu> 2.1.0-1
- Fix bug where GPU request was not passed onto the batch script
- Fix problem where proxy symlinks were not cleaned up by not creating them
- Fix bug where output files are overwritten if no transfer output remap
- Added support for passing in extra submit arguments from the job ad

* Wed Apr 28 2021 Tim Theisen <tim@cs.wisc.edu> 2.0.2-1
- Fix periodic remove expression, otherwise jobs go on hold

* Thu Apr 08 2021 Tim Theisen <tim@cs.wisc.edu> 2.0.1-1
- Convert to pure Python 3 (no Python 2 compatibility)
- Restore blah_libexec_directory to the blah.config file

* Tue Mar 30 2021 Brian Lin <blin@cs.wisc.edu> 2.0.0-1
- Add GPU support (#12)
- Improve multi-node support for Slurm (HTCONDOR-166)
- More efficient status queries for Slurm (HTCONDOR-104)
- More robust job ID extraction from SLurm (HTCONDOR-259)
- Better interfacing with PBS and Slurm
- More Python 3 fixes for PBS and Slurm scripts
- Convert LSF script to Python 3 (HTCONDOR-333, #34)
- Add Debian build files

* Tue Sep 15 2020 Brian Lin <blin@cs.wisc.edu> - 1.18.48-2
- Update RPM packaging

* Tue Sep 15 2020 Brian Lin <blin@cs.wisc.edu> - 1.18.48-1
- Add an option for HTCondor batch system to expand references to $HOME in the job environment (#23)
- Add efficient querying of LSF job statuses (#14)
- Improve logic when proxy refresh is disabled (#24)
- Fix invalid argument, output, and error format for HTCondor job submission (#22)

* Mon Aug 03 2020 Carl Edquist <edquist@cs.wisc.edu> - 1.18.47
- EL8 build fixes
- Add submit debugging support to condor_submit.sh (SOFTWARE-3994)

* Tue Apr 21 2020 Carl Edquist <edquist@cs.wisc.edu> - 1.18.46
- Fix an issue where the slurm binpath always returned scontrol (SOFTWARE-3986)
- Python 3 compatibility (#7)
- Handle extra-quoted arguments to condor_submit.sh (SOFTWARE-3993)
- Expand env vars in configured slurm_binpath (#10)
- Fix cluster handling in slurm_status.py (#13)
- Introduce blah_job_env_confs for dynamic env var expansion (SOFTWARE-2521)
- Add EXIT trap to remove barrier file (SOFTWARE-3930)
- Amended file credits for Matt Farrellee (#17)

* Tue Oct 15 2019 Carl Edquist <edquist@cs.wisc.edu> - 1.18.45
- Fix cherry-picking error from htcondor patch in 1.18.44 (SOFTWARE-3824)

* Wed Oct 09 2019 Carl Edquist <edquist@cs.wisc.edu> - 1.18.44-2
- Add new local submit attr scripts to build rules (SOFTWARE-3749)

* Tue Oct 08 2019 Carl Edquist <edquist@cs.wisc.edu> - 1.18.44
- Add Slurm SystemCpu accounting when job completes (SOFTWARE-3825)
- Add support for decimal seconds fields from sacct (SOFTWARE-3826)
- Pull in various updates to slurm scripts from htcondor's blap (SOFTWARE-3587)
- Move local submit attributes scripts from spec to sources (SOFTWARE-3749)

* Tue Jun 25 2019 Carl Edquist <edquist@cs.wisc.edu> - 1.18.43
- Support batch_gahp.config paths from condor (SOFTWARE-3587)

* Fri Jun 21 2019 Carl Edquist <edquist@cs.wisc.edu> - 1.18.42
- Move glite-build-common-cpp m4 files to vendor area (SOFTWARE-3587)
- Pull rpm spec file into source repo

* Wed May 15 2019 Carl Edquist <edquist@cs.wisc.edu> - 1.18.41.bosco-1
- Fix double-escaping in new condor env format (SOFTWARE-3589)

* Fri May 10 2019 Carl Edquist <edquist@cs.wisc.edu> - 1.18.40.bosco-1
- Update PBS Pro qstat options for completed jobs (SOFTWARE-3675)
- Apply patches from condor blahp (SOFTWARE-3587)
- Use the original proxy if blahp proxy delegation is disabled (SOFTWARE-3661)
- Use new condor env format (SOFTWARE-3589)

* Mon Feb 11 2019 Carl Edquist <edquist@cs.wisc.edu> - 1.18.39.bosco-1
- Propagate signals to payload jobs (SOFTWARE-3554)

* Fri Sep 14 2018 Mátyás Selmeci <matyas@cs.wisc.edu> - 1.18.38.bosco-1
- Disable blahp proxy renewal/limited proxies in the default config (SOFTWARE-3409)

* Sun Jul 29 2018 Edgar Fajardo <emfajard@ucsd.edu> 1.18.37.bosco-2
- Rebuild against condor 8.7.9 (SOFTWARE-3356)

* Wed Jun 13 2018 Carl Edquist <edquist@cs.wisc.edu> - 1.18.37.bosco-1
- Disable command substitution in shell word expansion (SOFTWARE-3288)

* Mon May 07 2018 Carl Edquist <edquist@cs.wisc.edu> - 1.18.36.bosco-2
- Rebuild against condor 8.7.8 (SOFTWARE-3250)

* Thu Mar 15 2018 Brian Lin <blin@cs.wisc.edu> - 1.18.36.bosco-1
- Verify input file existence before submission (SOFTWARE-3154)
- Save debugging dirs if job submission fails (SOFTWARE-2827)

* Fri Dec 1 2017 Brian Lin <blin@cs.wisc.edu> - 1.18.35.bosco-1
- Fix segfault when submitting jobs with limited proxies

* Tue Oct 31 2017 Brian Lin <blin@cs.wisc.edu> - 1.18.34.bosco-1
- Fix memory usage parsing for SLURM and PBS (SOFTWARE-2929)
- Fix UnicodeDecodeError when reading blah.config (SOFTWARE-2953)

* Tue Aug 29 2017 Brian Lin <blin@cs.wisc.edu> - 1.18.33.bosco-1
- Fix bug that caused jobs submitted to PBS batch systems to be held
  with "Error parsing classad or job not found" (SOFTWARE-2875)
- Fix parsing of time fields for slurm jobs (SOFTWARE-2871)

* Tue Jul 25 2017 Brian Lin <blin@cs.wisc.edu> - 1.18.32.bosco-1
- Fix bug that broke shell parsing of `*_binpath` config values
- Set default bin paths to `/usr/bin` to remove the overhead of `which` for each PBS, LSF, and SGE call.

* Tue Jul 11 2017 Brian Lin <blin@cs.wisc.edu> - 1.18.31.bosco-1
- Add blahp configuration to differentiate PBS flavors (SOFTWARE-2628)

* Fri Jun 23 2017 Brian Lin <blin@cs.wisc.edu> - 1.18.30.bosco-1
- Fix multicore request for SLURM batch systems (SOFTWARE-2774)

* Tue Jun 20 2017 Carl Edquist <edquist@cs.wisc.edu> - 1.18.29.bosco-4
- Rebuild against condor-8.7.2

* Thu Mar 16 2017 Brian Lin <blin@cs.wisc.edu> - 1.18.29.bosco-2
- Rebuild against condor-8.7.1

* Wed Mar 1 2017 Brian Lin <blin@cs.wisc.edu> - 1.18.29.bosco-1
- Blahp python scripts should ignore optional '-w' argument (SOFTWARE-2603)
- Fail gracefully when encountering unexpected sacct output (SOFTWARE-2604)
- Some #SBATCH commands are being ignored (SOFTWARE-2605)

* Tue Feb 28 2017 Edgar Fajardo <emfajard@ucsd.edu> - 1.18.28.bosco-5
- Build against condor-8.6.1

* Thu Jan 26 2017 Brian Lin <blin@cs.wisc.edu> - 1.18.28.bosco-4
- Build against condor-8.7.0

* Thu Jan 26 2017 Brian Lin <blin@cs.wisc.edu> - 1.18.28.bosco-3
- Build against condor-8.4.11

* Mon Dec 19 2016 Brian Lin <blin@cs.wisc.edu> - 1.18.28.bosco-2
- Build against condor-8.4.10

* Thu Oct 27 2016 Brian Lin <blin@cs.wisc.edu> - 1.18.28.bosco-1
- Fixed incompatibility between blahp_results_cache and torque-4.2.9
  that caused jobs to be held when performing status updates on
  HTCondor-CE (SOFTWARE-2516)

* Thu Oct 20 2016 Brian Lin <blin@cs.wisc.edu> - 1.18.27.bosco-1
- Fix segfault when using glexec and disabling limited proxies (SOFTWARE-2475)

* Fri Sep 23 2016 Brian Lin <blin@cs.wisc.edu> - 1.18.26.bosco-1
- Refactor scontrol calls to use subprocess (SOFTWARE-2450)

* Fri Sep 09 2016 Brian Lin <blin@cs.wisc.edu> - 1.18.25.bosco-1
- Fix qstart parsing errors that caused blank caches

* Fri Aug 26 2016 Brian Lin <blin@cs.wisc.edu> - 1.18.24.bosco-1
- Fixed slurm multicore requests in slurm_submit.sh
- Added slurm_submit_attributes.sh
- Enabled multicore support to PBS Pro (SOFTWARE-2326)
- Allow users to set the SGE parallel environment policy (SOFTWARE-2334)
- Fixed issues with qstat() (SOFTWARE-2358)

* Tue Jul 26 2016 Edgar Fajardo <emfajard@ucsd.edu> - 1.18.23.bosco-1
- Fixed a bug in HTConodor Ticket-5804. (SOFTWARE-2404)

* Thu Jul 21 2016 Edgar Fajardo <emfajard@ucsd.edu> - 1.18.22.bosco-2
- The code was taken from the osg-bosco instead of Edgar's fork.

* Wed Jul 20 2016 Edgar Fajardo <emfajard@ucsd.edu> - 1.18.22.bosco-1
- Merge HTCondor Ticket-5722. Cache output of slurm-status. (SOFTWARE-2399)

* Thu Jun 23 2016 Brian Lin <blin@cs.wisc.edu> - 1.18.21.bosco-1
- Fix Slurm file leak (SOFTWARE-2367)
- Package slurm_hold.sh (SOFTWARE-2375)

* Fri Jun 03 2016 Brian Lin <blin@cs.wisc.edu> - 1.18.20.bosco-1
- Add multicore HTCondor support (SOFTWARE-2303)
- Support dynamic assignment of env variables (SOFTWARE-2221)

* Mon May 02 2016 Matyas Selmeci <matyas@cs.wisc.edu> - 1.18.19.bosco-2
- Built against HTCondor 8.5.4 (SOFTWARE-2307)

* Mon Apr 25 2016 Brian Lin <blin@cs.wisc.edu> - 1.18.19.bosco-1
- Add SLURM support (SOFTWARE-2256)
- Fix mem requests (SOFTWARE-2260)

* Fri Feb 26 2016 Brian Lin <blin@cs.wisc.edu> - 1.18.18.bosco-1
- Bug fixes for PBS installations without qstat in their PATH

* Mon Feb 22 2016 Brian Lin <blin@cs.wisc.edu> - 1.18.17.bosco-1
- Re-apply lost SGE script changes (SOFTWARE-2199)
- Handle LSF suspended states (SOFTWARE-2168)
- Modify BLAHP to report gratia necessary attributes (SOFTWARE-2019)

* Wed Dec 16 2015 Brian Lin <blin@cs.wisc.edu> - 1.18.16.bosco-1
- Allow for disabling limited proxies in glexec
- Fix bug in pbs_status.py when /tmp/ and /var/tmp were on different filesystems
- Resync job registry to prevent jobs from being incorrectly marked as completed

* Mon Nov 23 2015 Edgar Fajardo <efajardo@physics.ucsd.edu> - 1.18.15.bosco-2
- Built against HTCondor 8.5.1 SOFTWARE-2077

* Wed Nov 11 2015 Carl Edquist <edquist@cs.wisc.edu> - 1.18.15.bosco-3
- Build against condor 8.4.2 (SOFTWARE-2084)

* Mon Nov 2 2015 Edgar Fajardo <emfajard@ucsd.edu> - 1.18.15.bosco-2
- Build aginst condor 8.4.0 (SOFTWARE-2084)

* Tue Oct 27 2015 Jeff Dost <jdost@ucsd.edu> - 1.18.15.bosco-1
- Build against HTCondor 8.4.1 (SOFTWARE-2084)
- Added error reporting to pbs_submit

* Fri Oct 23 2015 Edgar Fajardo <efajardo@physics.ucsd.edu> - 1.18.15.bosco-1
- Built against HTCOndor 8.5.0 SOFTWARE-2077
- Added error reporting to pbs_submit

* Tue Sep 29 2015 Brian Lin <blin@cs.wisc.edu> - 1.18.14.bosco-1
- Added PBS Pro support (SOFTWARE-1958)
- Fix for job registry losing track of LSF jobs in its registry (gittrac #5062)
- Added 'blah_disable_limited_proxies' to disable creation of limited proxies
- Reduce 'blah_max_threaded_commands' to 50 (SOFTWARE-1980)

* Mon Aug 31 2015 Carl Edquist <edquist@cs.wisc.edu> - 1.18.13.bosco-4
- Rebuild against HTCondor 8.3.8 (SOFTWARE-1995)

* Mon Jul 20 2015 Mátyás Selmeci <matyas@cs.wisc.edu> 1.18.13.bosco-3
- bump to rebuild

* Thu Jun 25 2015 Brian Lin <blin@cs.wisc.edu> - 1.18.13.bosco-2
- Rebuild against HTCondor 8.3.6

* Thu May 28 2015 Brian Lin <blin@cs.wisc.edu> - 1.18.13.bosco-1
- Fixes to PBS and HTCondor submission

* Tue Apr 28 2015 Brian Lin <blin@cs.wisc.edu> - 1.18.12.bosco-2
- Rebuild against HTCondor 8.3.5

* Mon Mar 30 2015 Brian Lin <blin@cs.wisc.edu> - 1.18.12.bosco-1
- Source profile.lsf for LSF job submission

* Wed Dec 03 2014 Mátyás Selmeci <matyas@cs.wisc.edu> 1.18.11.bosco-4
- Fix syntax error in condor_submit.sh
- Source OSG job environment variables in generated submit scripts for pbs,
  lsf, sge, and slurm jobmanagers (SOFTWARE-1709)

* Mon Oct 27 2014 Brian Lin <blin@cs.wisc.edu> - 1.18.11.bosco-3
- Rebuild against condor-8.2.3

* Mon Oct 20 2014 Carl Edquist <edquist@cs.wisc.edu> - 1.18.11.bosco-2
- Build fixes for el7 (SOFTWARE-1604)

* Mon Sep 29 2014 Brian Lin <blin@cs.wisc.edu> - 1.18.11.bosco-1
- Fix bug in PBS status script

* Thu Sep 25 2014 Brian Lin <blin@cs.wisc.edu> - 1.18.10.bosco-1
- Fixes to LSF scripts pushed upstream (SOFTWARE-1589, creating a temp file in /tmp)
- Fix to PBS script that tracks job status (SOFTWARE-1594)

* Mon Aug 25 2014 Brian Lin <blin@cs.wisc.edu> - 1.18.9.bosco-2
- Fix for memory allocation failure when tracking LSF jobs (SOFTWARE-1589)

* Thu Jan 09 2014 Brian Bockelman <bbockelm@cse.unl.edu> - 1.18.9.bosco-1
- Fix proxy renewal in the case where no home directory exists.
- Improve packaging of local customization scripts and include defaults.
  These are now marked as config files and places in /etc.
- Change name of documentation directory to reflect RPM name.

* Tue Jan 07 2014 Brian Bockelman <bbockelm@cse.unl.edu> - 1.18.8.bosco-1
- Fixes from PBS testing.  Blahp now handles multiple arguments correctly
  and the wrapper script will remove the job proxy after it finishes.

* Wed Oct 30 2013 Matyas Selmeci <matyas@cs.wisc.edu> - 1.18.7.bosco-2
- Bump to rebuild against condor-7.8.8-x (OSG-3.1) and condor-8.0.4-x (OSG 3.2)

* Fri Sep 20 2013 Brian Bockelman <bbockelm@cse.unl.edu> - 1.18.7.bosco-1
- Do not close stderr fd from the blah.

* Tue May 14 2013 Brian Bockelman <bbockelm@cse.unl.edu> - 1.18.5.bosco-1
- Alter the pbs_status.py locking algorithm to add random component to 
  sleeps between poll.

* Thu Jan 17 2013 Derek Weitzel <dweitzel@cse.unl.edu> - 1.18.4.bosco-1
- Fixing pbs_status.py via upstream SOFTWARE-905

* Thu Dec 13 2012 Brian Bockelman <bbockelm@cse.unl.edu> 1.18.3.bosco-1.osg
- Merge BOSCO and OSG distribution of blahp.

* Wed Dec 05 2012 John Thiltges <jthiltges2@unl.edu> 1.18.0.4-9.osg
- Fix pbs_status.sh in spec file

* Fri Oct 12 2012 Brian Bockelman <bbockelm@cse.unl.edu> - 1.18.0.4-8.osg
- Pull in all remaining patches from the OSG-CE work.
- Fix non-standard qstat locations.
- Fix arg escaping in Condor.
- Fix submissions with a relative proxy path.
- Release bumped a few extra versions to stay in line with the Caltech Koji.

* Wed Aug 29 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 1.18.0.4-5.osg
- Fixed paths in init script
- Added default options for condor

* Wed Jul 25 2012 Matyas Selmeci <matyas@cs.wisc.edu> - 1.18.0.4-4.osg
- Disable autostart of blah parser

* Thu May 31 2012 Brian Bockelman <bbockelm@cse.unl.edu> - 1.18.0.4-3
- Add caching for PBS script.

* Mon May 28 2012 Brian Bockelman <bbockelm@cse.unl.edu> -1.18.0.4-2
- Import patches from Condor team.

* Mon May 28 2012 Brian Bockelman <bbockelm@cse.unl.edu> -1.18.0.4-1
- Update to latest upstream.

* Fri Sep 16 2011 Brian Bockelman <bbockelm@cse.unl.edu> - 1.16.1-3
- Rev bump for GT 5.2 recompile.

* Wed Jan 05 2011 Brian Bockelman <bbockelm@cse.unl.edu> 1.16.1-1
- Initial RPM packaging

