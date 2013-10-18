# we expect grep to return set(VERSION "x.y.z")
# we use $subst to convert () and " to spaces and then $word to extract x.y.z
VERLINE = $(shell grep set.VERSION ../CMakeLists.txt)
VERTEMP = ${word 3,${subst ",,${subst ),,${subst (, ,${VERLINE}}}}}
SUBDIR  = v$(VERTEMP)
SUBVER  = V$(subst .,_,$(VERTEMP))
MANDIR = /p/condor/public/html/htcondor/manual/$(SUBDIR)
TARGETDIR = $(MANDIR).new
LOCALDIR = ref-$(SUBDIR)
MANPAGES = condor_advertise.html condor_checkpoint.html condor_check_userlogs.html condor_chirp.html condor_cod.html condor_compile.html condor_configure.html condor_config_val.html condor_dagman.html condor_fetchlog.html condor_findhost.html condor_gather_info.html condor_glidein.html condor_history.html condor_hold.html condor_load_history.html condor_master.html condor_off.html condor_on.html condor_power.html condor_preen.html condor_prio.html condor_qedit.html condor_q.html condor_qsub.html condor_reconfig.html condor_release.html condor_reschedule.html condor_restart.html condor_rm.html condor_router_history.html condor_procd.html condor_router_q.html condor_router_rm.html condor_run.html condor_set_shutdown.html condor_ssh_to_job.html condor_stats.html condor_status.html condor_store_cred.html condor_submit_dag.html condor_submit.html condor_transfer_data.html condor_updates_stats.html condor_userlog.html condor_userprio.html condor_vacate.html condor_vacate_job.html condor_version.html condor_wait.html gidd_alloc.html procd_ctl.html condor_suspend.html condor_continue.html bosco_cluster.html bosco_findplatform.html bosco_install.html bosco_ssh_start.html bosco_start.html bosco_stop.html bosco_uninstall.html condor_drain.html condor_glidein.html condor_install.html condor_load_history.html condor_ping.html condor_rmdir.html condor_tail.html condor_who.html

# NOTE: older latex versions do not support -halt-on-error
#       including /p/condor/workspaces/build/bin/latex, unfortunately
LATEXARGS := -file-line-error-style $(shell latex --help | grep -o -- -halt-on-error)

all:  ref.dvi
#all:  man.ps

ref.pdf: clean
	echo "\usepackage{times} \usepackage[latex2html,ps2pdf,bookmarks,bookmarksnumbered,plainpages=false,pdfpagelabels]{hyperref}" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	rm -f figuresizing.tex
	ln -s regularfont.tex fontsize.tex
	ln -s symbol-postscript.tex symbol.tex
	ln -s figuresizing-latex.tex figuresizing.tex
	make figures
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	makeindex ref
	latex $(LATEXARGS)  ref.tex
	rm -f ref.ps
	dvips ref.dvi -o ref.ps
	rm -f ref.pdf
	gs -sDEVICE=pdfwrite -sOutputFile=ref.pdf -dNOPAUSE -dBATCH ref.ps
	rm -f ref.dvi
	rm -f ref.ps

release-pdf: ref.pdf $(TARGETDIR)
	cp ref.pdf $(TARGETDIR)/condor-$(SUBVER)-Manual.pdf

ref.dvi: clean
	echo "% do not want times" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	rm -f figuresizing.tex
	ln -s smallfont.tex fontsize.tex
	ln -s symbol-postscript.tex symbol.tex
	ln -s figuresizing-latex.tex figuresizing.tex
	make figures
	latex $(LATEXARGS) ref.tex
	latex $(LATEXARGS) ref.tex
	latex $(LATEXARGS) ref.tex
	latex $(LATEXARGS) ref.tex
	latex $(LATEXARGS) ref.tex
	makeindex ref
	latex $(LATEXARGS) ref.tex

ref.ps: ref.dvi
	dvips ref.dvi -o ref.ps

release-ps: ref.ps $(TARGETDIR)
	cp ref.ps $(TARGETDIR)/ref.ps
	cp $(TARGETDIR)/ref.ps $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	compress $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	cp $(TARGETDIR)/ref.ps $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	gzip $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	mv $(TARGETDIR)/ref.ps $(TARGETDIR)/condor-$(SUBVER)-Manual.ps

ref.html: clean $(TARGETDIR)
	echo "% do not want times" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	rm -f figuresizing.tex
	ln -s regularfont.tex fontsize.tex
	ln -s symbol-html.tex symbol.tex
	ln -s figuresizing-html.tex figuresizing.tex
	make figures
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	makeindex ref
	latex $(LATEXARGS)  ref.tex
	latex2html -split 4 -link 2 -show_section_numbers -antialias -tmp /tmp -long_titles 3 -toc_depth 2 -local_icons ref.tex
	cd ref && perl -pi.bak -e 's,\<HEAD\>,<HEAD>\n<link rel="canonical" href="http://research.cs.wisc.edu/htcondor/manual/current/$$ARGV">\n,' condor_*.html
	rm -f ref.dvi
	mv ref condor-$(SUBVER)-Manual
	tar cvf condor-$(SUBVER)-Manual.tar condor-$(SUBVER)-Manual

	gzip -c condor-$(SUBVER)-Manual.tar > condor-$(SUBVER)-Manual.tar.gz
	compress condor-$(SUBVER)-Manual.tar

	mv condor-$(SUBVER)-Manual.tar.gz condor-$(SUBVER)-Manual
	mv condor-$(SUBVER)-Manual.tar.Z condor-$(SUBVER)-Manual

	mv condor-$(SUBVER)-Manual ref.new

	mv -f ref.new/* $(TARGETDIR)
	rmdir ref.new
	touch ref.html

local.html: $(wildcard *.tex) $(wildcard */*.tex)
	echo "% do not want times" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	rm -f figuresizing.tex
	ln -s regularfont.tex fontsize.tex
	ln -s symbol-html.tex symbol.tex
	ln -s figuresizing-html.tex figuresizing.tex
	make figures
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	latex $(LATEXARGS)  ref.tex
	makeindex ref
	latex $(LATEXARGS)  ref.tex
	latex2html -split 4 -link 2 -show_section_numbers -antialias -tmp /tmp -long_titles 3 -toc_depth 2 -local_icons ref.tex
	rm -rf $(LOCALDIR)
	mv    ref $(LOCALDIR)
	touch local.html

release-html: ref.html $(TARGETDIR)
	rm -rf $(MANDIR).old
	touch $(MANDIR)
	mv -f $(MANDIR) $(MANDIR).old
	mv -f $(TARGETDIR) $(MANDIR)
	rm -f ref.html

admin-man/pool-arch.eps: admin-man/pool-arch.fig
	rm -f $@
	fig2dev -L eps -m .4 $< > $@

user-man/dagman-diamond.eps: user-man/dagman-diamond.fig
	rm -f $@
	fig2dev -L eps -m .6 $< > $@

figures: admin-man/states.eps admin-man/activities.eps admin-man/pool-arch.eps user-man/dagman-diamond.eps 


$(TARGETDIR):
	mkdir $(TARGETDIR)

clean:
	rm -rf $(LOCALDIR) ref ref.new man user admin *.1 *.dvi *.ps *.html *.log *.aux *.toc *.pdf */*.log *.ind *.idx *.ilg timesfont.tex user-man/dagman-diamond.eps admin-man/pool-arch.eps *.out
	cd makeman; make clean

reallyclean: clean
	rm -rf $(TARGETDIR)

release:
	make reallyclean
	make release-ps
	make release-pdf
	make release-html

.PHONY: just-man-pages

just-man-pages:
	echo "% do not want times" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	rm -f figuresizing.tex
	ln -s regularfont.tex fontsize.tex
	ln -s symbol-html.tex symbol.tex
	ln -s figuresizing-html.tex figuresizing.tex
	cd makeman && make clean makeman
	latex2html -split 4 -link 2 -show_section_numbers -antialias -tmp /tmp -long_titles 3 -toc_depth 2 -local_icons just-man-pages.tex
	for html in $(MANPAGES); do \
		thisfile=just-man-pages/$$html; \
		makeman/makeman -v -i $$thisfile -s 1; \
	done
	-mkdir man
	-mkdir man/man1
	mv just-man-pages/*.1 man/man1
