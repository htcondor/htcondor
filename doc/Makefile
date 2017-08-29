# The .fig sources are stored in git.
FIG_FIGS =		user-man/dagman-node.eps \
				user-man/dagman-diamond.eps \
				user-man/dagman-connect.eps

# The .dia sources are stored in git, but Make doesn't make the .eps files.
DIA_FIGS =		admin-man/states.eps \
				admin-man/activities.eps

# The .dot sources are stored in git, but Make doesn't make the .eps files.
DOT_FIGS = 		user-man/splice-simple.eps \
				user-man/splice-X.eps \
				user-man/splice-s1.eps \
				user-man/splice-complex.eps

# The .ps file is stored in git; there is no "source."
PS_FIGS =		contrib/view-screenshot.ps

FIGURES =		$(FIG_FIGS:%.eps=%.pdf) \
				$(DIA_FIGS:%.eps=%.pdf) \
				$(DOT_FIGS:%.eps=%.pdf) \
				$(PS_FIGS:%.ps=%.pdf)

LINKS =			font.tex \
				symbol.tex \
				fontsize.tex \
				figuresizing.tex

LATEX_ARGS =	-file-line-error-style -halt-on-error

# Convert the list of each ${manpage}.tex sourced to build the manual
# into the list of ${manpage}.html generate by latex2html.  This is
# rather a hack, but I don't care enough about the man pages to fix it.
MANPAGES = $(shell grep '^\\input' man-pages/man.tex | grep -v tool_common.tex | awk -F/ '{print $$2}' | awk -F. '{printf "%s.html ",$$1}')

all : ref.pdf

# Only helpful for debugging.  Do NOT insert this as a dependency
# of ref.pdf; that will force it to be rebuilt every time.
links.pdf :
	@rm -f font.tex ; ln -s font.pdf.tex font.tex
	@rm -f symbol.tex ; ln -s symbol.pdf.tex symbol.tex
	@rm -f fontsize.tex; ln -s fontsize.pdf.tex fontsize.tex
	@rm -f figuresizing.tex; ln -s figuresizing.pdf.tex figuresizing.tex

# Only helpful for debugging.  Do NOT insert this as a dependency
# of ref.pdf; that will force it to be rebuilt every time.
figures : $(FIGURES)

# It's sad that the least-awful way to manage our symlinks is this way.
# We'd be better off if we could merge the contents of these four symlinks
# into one top-level file for each build target which would \input
# (most or all of) the current ref.tex.
ref.pdf : ref.tex $(FIGURES) $(LINKS:%.tex=%.pdf.tex)
	@rm -f font.tex ; ln -s font.pdf.tex font.tex
	@rm -f symbol.tex ; ln -s symbol.pdf.tex symbol.tex
	@rm -f fontsize.tex; ln -s fontsize.pdf.tex fontsize.tex
	@rm -f figuresizing.tex; ln -s figuresizing.pdf.tex figuresizing.tex
	@perl -p -i -e's#\includegraphics{([^\.]+)\.eps}#\includegraphics{\1.pdf}#g' `git grep -l includegraphics`
	@pdflatex $(LATEX_ARGS) ref.tex
	@makeindex ref
	@pdflatex $(LATEX_ARGS) ref.tex
	@pdflatex $(LATEX_ARGS) ref.tex

# See ref.pdf comment above.
#
# Also, while we hope to be able to upgrade to a version of latex2html that
# supports converting .pdf as well .eps, or maybe a version of pdflatex
# that doesn't barf on .eps, until we do, rewriting the \includegraphics
# line to have the extension that the program in question needs is the
# least-awful way to make things work.
#
# The extra conversion lines are to handle when you interrupt latex2html.
ref.html : ref.tex $(FIGURES) $(LINKS:%.tex=%.html.tex)
	@rm -f font.tex ; ln -s font.html.tex font.tex
	@rm -f symbol.tex ; ln -s symbol.html.tex symbol.tex
	@rm -f fontsize.tex; ln -s fontsize.html.tex fontsize.tex
	@rm -f figuresizing.tex; ln -s figuresizing.html.tex figuresizing.tex
	@perl -p -i -e's#\includegraphics{([^\.]+)\.eps}#\includegraphics{\1.pdf}#g' `git grep -l includegraphics`
	@pdflatex $(LATEX_ARGS) ref.tex
	@makeindex ref
	@pdflatex $(LATEX_ARGS) ref.tex
	@pdflatex $(LATEX_ARGS) ref.tex
	@perl -p -i -e's#\includegraphics{([^\.]+)\.pdf}#\includegraphics{\1.pdf}#g' `git grep -l includegraphics`
	@latex2html -split 4 -link 2 -show_section_numbers -antialias -tmp /tmp -long_titles 3 -toc_depth 2 -local_icons ref.tex
	@perl -p -i -e's#\includegraphics{([^\.]+)\.eps}#\includegraphics{\1.pdf}#g' `git grep -l includegraphics`
	@mv ref ref.html

# See ref.pdf comment above.  Also note that we don't have a full set of
# dependencies for the man pages, because I just don't care enough.
just-man-pages : just-man-pages.tex $(LINKS:%.tex=%.html.tex)
	@rm -f font.tex ; ln -s font.html.tex font.tex
	@rm -f symbol.tex ; ln -s symbol.html.tex symbol.tex
	@rm -f fontsize.tex; ln -s fontsize.html.tex fontsize.tex
	@rm -f figuresizing.tex; ln -s figuresizing.html.tex figuresizing.tex
	@cd makeman ; make clean makeman
	@latex2html -split 4 -link 2 -show_section_numbers -antialias -tmp /tmp -long_titles 3 -toc_depth 2 -local_icons just-man-pages.tex
	@for html in $(MANPAGES); do \
		thisfile=just-man-pages/$$html; \
		makeman/makeman -v -i $$thisfile -s 1; \
	done
	-@mkdir man
	-@mkdir man/man1
	@mv just-man-pages/*.1 man/man1
	@rm -fr just-man-pages

%.pdf : %.ps
	@ps2pdf $< $@

%.pdf : %.eps
	@epstopdf $< > $@

%.eps : %.fig
	@fig2dev -L eps -m .6 $< > $@

# Make automatically deletes intermediate files (files created as the
# result of a implicit rule whose result is not a target); because
# dagman-node.eps required its own rule, it's not considered an
# intermediate file.  The special target .INTERMEDIATE changes that.
# The special target .SECONDARY says to not delete the listed
# intermediate files; this -- as far as I know -- is the only way to
# make Make shut up about them.
.SECONDARY : $(FIG_FIGS)

clean : figures-clean tex-clean pdf-clean html-clean man-clean
	@rm -f $(LINKS)

figures-clean :
	@rm -f $(FIG_FIGS)
	@rm -f $(FIGURES)

tex-clean :
	@rm -f *.log *.aux *.toc *.ind *.idx *.ilg *.out

pdf-clean :
	@rm -f *.pdf

html-clean :
	@rm -fr ref.html

man-clean :
	@rm -fr man just-man-pages
	@cd makeman; make clean >& /dev/null

release-pdf : ref.pdf
	@echo "Ask TimT what to do here."

release-html : ref.html
	@echo "Ask TimT what to do here."

release : release-pdf release-html

# Generated by 'tex-dependencies.pl ref.tex' and edited to remove the symlinks.
# I'm not bothering with the extra code necessary to regenerate the
# dependencies automatically; that way lies madness.
       ref.pdf : condor-macros.tex man-macros.tex license.tex overview/overview.tex user-man/limitations.tex overview/contributions.tex user-man/user.tex user-man/limitations.tex admin-man/if-else.tex admin-man/function-macros.tex user-man/filetransfer.tex user-man/interactive.tex user-man/managing.tex user-man/java.tex user-man/parallel.tex user-man/dagman.tex user-man/vm.tex user-man/docker.tex user-man/time-scheduling.tex admin-man/admin.tex admin-man/intro.tex admin-man/install.tex admin-man/install-windows.tex admin-man/configure-intro.tex admin-man/if-else.tex admin-man/function-macros.tex admin-man/configure-metaknobs.tex admin-man/configure.tex admin-man/if-else.tex admin-man/function-macros.tex admin-man/userprio.tex admin-man/policy.tex admin-man/multi-core.tex admin-man/security.tex admin-man/networking.tex admin-man/ports.tex admin-man/shared-port-daemon.tex admin-man/multiple-interfaces.tex admin-man/ccb.tex admin-man/tcp-update.tex admin-man/ipv6.tex admin-man/ckpt-server.tex admin-man/daemoncore.tex admin-man/monitoring.tex admin-man/absent.tex admin-man/high-availability.tex admin-man/ha.tex admin-man/special.tex admin-man/afs.tex admin-man/url-transfer.tex admin-man/multiple-platforms.tex admin-man/full-condor-compile.tex admin-man/kbdd.tex admin-man/condor-view-server.tex admin-man/virtual-machines.tex admin-man/dedicated.tex admin-man/backfill.tex admin-man/PID-namespaces.tex admin-man/group-tracking.tex admin-man/limits.tex admin-man/limits-cgroup.tex admin-man/concurrency.tex admin-man/java.tex admin-man/vm.tex admin-man/singularity.tex admin-man/power.tex misc/misc.tex misc/classad.tex misc/ckpt.tex misc/cod.tex misc/hooks.tex misc/job-hooks.tex misc/logging.tex grids/grid-computing.tex grids/intro.tex grids/flocking.tex grids/grid-universe.tex grids/condor-c.tex grids/condor-g.tex grids/using.tex grids/grid-monitor.tex grids/nordugrid.tex grids/unicore.tex grids/batch.tex grids/amazon.tex grids/gce.tex grids/cream.tex grids/boinc.tex grids/grid-matchmaking.tex grids/job_router.tex misc/API.tex misc/webservice.tex misc/drmaa.tex misc/user-log.tex misc/chirp.tex misc/commandline.tex misc/condor.pm.tex misc/pythonbindings.tex platforms/platforms.tex platforms/linux.tex platforms/windows.tex platforms/macosx.tex faq.tex contrib/contrib.tex contrib/hdfs.tex contrib/quill.tex contrib/quill-schema.tex contrib/condor-view-client.tex contrib/logview.tex version-history/history.tex version-history/upgradingto8-6.tex version-history/8-6.history.tex version-history/8-5.history.tex version-history/8-4.history.tex man-pages/man.tex man-pages/tool_common.tex man-pages/bosco_cluster.tex man-pages/bosco_findplatform.tex man-pages/bosco_install.tex man-pages/bosco_ssh_start.tex man-pages/bosco_start.tex man-pages/bosco_stop.tex man-pages/bosco_uninstall.tex man-pages/condor_advertise.tex man-pages/condor_check_userlogs.tex man-pages/condor_checkpoint.tex man-pages/condor_chirp.tex man-pages/condor_cod.tex man-pages/condor_compile.tex man-pages/condor_config_val.tex man-pages/condor_configure.tex man-pages/condor_continue.tex man-pages/condor_convert_history.tex man-pages/condor_dagman.tex man-pages/condor_dagman_metrics_reporter.tex man-pages/condor_drain.tex man-pages/condor_fetchlog.tex man-pages/condor_findhost.tex man-pages/condor_gather_info.tex man-pages/condor_gpu_discovery.tex man-pages/condor_history.tex man-pages/condor_hold.tex man-pages/condor_install.tex man-pages/condor_job_router_info.tex man-pages/condor_master.tex man-pages/condor_off.tex man-pages/condor_on.tex man-pages/condor_ping.tex man-pages/condor_pool_job_report.tex man-pages/condor_power.tex man-pages/condor_preen.tex man-pages/condor_prio.tex man-pages/condor_procd.tex man-pages/condor_q.tex man-pages/condor_qedit.tex man-pages/condor_qsub.tex man-pages/condor_reconfig.tex man-pages/condor_release.tex man-pages/condor_reschedule.tex man-pages/condor_restart.tex man-pages/condor_rm.tex man-pages/condor_rmdir.tex man-pages/condor_router_history.tex man-pages/condor_router_q.tex man-pages/condor_router_rm.tex man-pages/condor_run.tex man-pages/condor_set_shutdown.tex man-pages/condor_ssh_to_job.tex man-pages/condor_sos.tex man-pages/condor_stats.tex man-pages/condor_status.tex man-pages/condor_store_cred.tex man-pages/condor_submit.tex man-pages/submitcmds.tex man-pages/condor_submit_dag.tex man-pages/condor_suspend.tex man-pages/condor_tail.tex man-pages/condor_transfer_data.tex man-pages/condor_transform_ads.tex man-pages/condor_update_machine_ad.tex man-pages/condor_updates_stats.tex man-pages/condor_urlfetch.tex man-pages/condor_userlog.tex man-pages/condor_userprio.tex man-pages/condor_vacate.tex man-pages/condor_vacate_job.tex man-pages/condor_version.tex man-pages/condor_wait.tex man-pages/condor_who.tex man-pages/gidd_alloc.tex man-pages/procd_ctl.tex misc/attributes.tex misc/jobad.tex misc/machad.tex misc/masterad.tex misc/scheddad.tex misc/negotiatorad.tex misc/submitterad.tex misc/defragad.tex misc/collectorad.tex misc/statsattributes.tex misc/numbers.tex 
      ref.html : condor-macros.tex man-macros.tex license.tex overview/overview.tex user-man/limitations.tex overview/contributions.tex user-man/user.tex user-man/limitations.tex admin-man/if-else.tex admin-man/function-macros.tex user-man/filetransfer.tex user-man/interactive.tex user-man/managing.tex user-man/java.tex user-man/parallel.tex user-man/dagman.tex user-man/vm.tex user-man/docker.tex user-man/time-scheduling.tex admin-man/admin.tex admin-man/intro.tex admin-man/install.tex admin-man/install-windows.tex admin-man/configure-intro.tex admin-man/if-else.tex admin-man/function-macros.tex admin-man/configure-metaknobs.tex admin-man/configure.tex admin-man/if-else.tex admin-man/function-macros.tex admin-man/userprio.tex admin-man/policy.tex admin-man/multi-core.tex admin-man/security.tex admin-man/networking.tex admin-man/ports.tex admin-man/shared-port-daemon.tex admin-man/multiple-interfaces.tex admin-man/ccb.tex admin-man/tcp-update.tex admin-man/ipv6.tex admin-man/ckpt-server.tex admin-man/daemoncore.tex admin-man/monitoring.tex admin-man/absent.tex admin-man/high-availability.tex admin-man/ha.tex admin-man/special.tex admin-man/afs.tex admin-man/url-transfer.tex admin-man/multiple-platforms.tex admin-man/full-condor-compile.tex admin-man/kbdd.tex admin-man/condor-view-server.tex admin-man/virtual-machines.tex admin-man/dedicated.tex admin-man/backfill.tex admin-man/PID-namespaces.tex admin-man/group-tracking.tex admin-man/limits.tex admin-man/limits-cgroup.tex admin-man/concurrency.tex admin-man/java.tex admin-man/vm.tex admin-man/singularity.tex admin-man/power.tex misc/misc.tex misc/classad.tex misc/ckpt.tex misc/cod.tex misc/hooks.tex misc/job-hooks.tex misc/logging.tex grids/grid-computing.tex grids/intro.tex grids/flocking.tex grids/grid-universe.tex grids/condor-c.tex grids/condor-g.tex grids/using.tex grids/grid-monitor.tex grids/nordugrid.tex grids/unicore.tex grids/batch.tex grids/amazon.tex grids/gce.tex grids/cream.tex grids/boinc.tex grids/grid-matchmaking.tex grids/job_router.tex misc/API.tex misc/webservice.tex misc/drmaa.tex misc/user-log.tex misc/chirp.tex misc/commandline.tex misc/condor.pm.tex misc/pythonbindings.tex platforms/platforms.tex platforms/linux.tex platforms/windows.tex platforms/macosx.tex faq.tex contrib/contrib.tex contrib/hdfs.tex contrib/quill.tex contrib/quill-schema.tex contrib/condor-view-client.tex contrib/logview.tex version-history/history.tex version-history/upgradingto8-6.tex version-history/8-6.history.tex version-history/8-5.history.tex version-history/8-4.history.tex man-pages/man.tex man-pages/tool_common.tex man-pages/bosco_cluster.tex man-pages/bosco_findplatform.tex man-pages/bosco_install.tex man-pages/bosco_ssh_start.tex man-pages/bosco_start.tex man-pages/bosco_stop.tex man-pages/bosco_uninstall.tex man-pages/condor_advertise.tex man-pages/condor_check_userlogs.tex man-pages/condor_checkpoint.tex man-pages/condor_chirp.tex man-pages/condor_cod.tex man-pages/condor_compile.tex man-pages/condor_config_val.tex man-pages/condor_configure.tex man-pages/condor_continue.tex man-pages/condor_convert_history.tex man-pages/condor_dagman.tex man-pages/condor_dagman_metrics_reporter.tex man-pages/condor_drain.tex man-pages/condor_fetchlog.tex man-pages/condor_findhost.tex man-pages/condor_gather_info.tex man-pages/condor_gpu_discovery.tex man-pages/condor_history.tex man-pages/condor_hold.tex man-pages/condor_install.tex man-pages/condor_job_router_info.tex man-pages/condor_master.tex man-pages/condor_off.tex man-pages/condor_on.tex man-pages/condor_ping.tex man-pages/condor_pool_job_report.tex man-pages/condor_power.tex man-pages/condor_preen.tex man-pages/condor_prio.tex man-pages/condor_procd.tex man-pages/condor_q.tex man-pages/condor_qedit.tex man-pages/condor_qsub.tex man-pages/condor_reconfig.tex man-pages/condor_release.tex man-pages/condor_reschedule.tex man-pages/condor_restart.tex man-pages/condor_rm.tex man-pages/condor_rmdir.tex man-pages/condor_router_history.tex man-pages/condor_router_q.tex man-pages/condor_router_rm.tex man-pages/condor_run.tex man-pages/condor_set_shutdown.tex man-pages/condor_ssh_to_job.tex man-pages/condor_sos.tex man-pages/condor_stats.tex man-pages/condor_status.tex man-pages/condor_store_cred.tex man-pages/condor_submit.tex man-pages/submitcmds.tex man-pages/condor_submit_dag.tex man-pages/condor_suspend.tex man-pages/condor_tail.tex man-pages/condor_transfer_data.tex man-pages/condor_transform_ads.tex man-pages/condor_update_machine_ad.tex man-pages/condor_updates_stats.tex man-pages/condor_urlfetch.tex man-pages/condor_userlog.tex man-pages/condor_userprio.tex man-pages/condor_vacate.tex man-pages/condor_vacate_job.tex man-pages/condor_version.tex man-pages/condor_wait.tex man-pages/condor_who.tex man-pages/gidd_alloc.tex man-pages/procd_ctl.tex misc/attributes.tex misc/jobad.tex misc/machad.tex misc/masterad.tex misc/scheddad.tex misc/negotiatorad.tex misc/submitterad.tex misc/defragad.tex misc/collectorad.tex misc/statsattributes.tex misc/numbers.tex 
just-man-pages : condor-macros.tex man-macros.tex man-pages/man.tex man-pages/tool_common.tex man-pages/bosco_cluster.tex man-pages/bosco_findplatform.tex man-pages/bosco_install.tex man-pages/bosco_ssh_start.tex man-pages/bosco_start.tex man-pages/bosco_stop.tex man-pages/bosco_uninstall.tex man-pages/condor_advertise.tex man-pages/condor_check_userlogs.tex man-pages/condor_checkpoint.tex man-pages/condor_chirp.tex man-pages/condor_cod.tex man-pages/condor_compile.tex man-pages/condor_config_val.tex man-pages/condor_configure.tex man-pages/condor_continue.tex man-pages/condor_convert_history.tex man-pages/condor_dagman.tex man-pages/condor_dagman_metrics_reporter.tex man-pages/condor_drain.tex man-pages/condor_fetchlog.tex man-pages/condor_findhost.tex man-pages/condor_gather_info.tex man-pages/condor_gpu_discovery.tex man-pages/condor_history.tex man-pages/condor_hold.tex man-pages/condor_install.tex man-pages/condor_job_router_info.tex man-pages/condor_master.tex man-pages/condor_off.tex man-pages/condor_on.tex man-pages/condor_ping.tex man-pages/condor_pool_job_report.tex man-pages/condor_power.tex man-pages/condor_preen.tex man-pages/condor_prio.tex man-pages/condor_procd.tex man-pages/condor_q.tex man-pages/condor_qedit.tex man-pages/condor_qsub.tex man-pages/condor_reconfig.tex man-pages/condor_release.tex man-pages/condor_reschedule.tex man-pages/condor_restart.tex man-pages/condor_rm.tex man-pages/condor_rmdir.tex man-pages/condor_router_history.tex man-pages/condor_router_q.tex man-pages/condor_router_rm.tex man-pages/condor_run.tex man-pages/condor_set_shutdown.tex man-pages/condor_ssh_to_job.tex man-pages/condor_sos.tex man-pages/condor_stats.tex man-pages/condor_status.tex man-pages/condor_store_cred.tex man-pages/condor_submit.tex man-pages/submitcmds.tex man-pages/condor_submit_dag.tex man-pages/condor_suspend.tex man-pages/condor_tail.tex man-pages/condor_transfer_data.tex man-pages/condor_transform_ads.tex man-pages/condor_update_machine_ad.tex man-pages/condor_updates_stats.tex man-pages/condor_urlfetch.tex man-pages/condor_userlog.tex man-pages/condor_userprio.tex man-pages/condor_vacate.tex man-pages/condor_vacate_job.tex man-pages/condor_version.tex man-pages/condor_wait.tex man-pages/condor_who.tex man-pages/gidd_alloc.tex man-pages/procd_ctl.tex
