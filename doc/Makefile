SUBDIR = v6.1
SUBVER = V6_1
MANDIR = /p/condor/public/html/manual/$(SUBDIR)
TARGETDIR = $(MANDIR).new
all:  ref.dvi
#all:  man.ps

ref.pdf:
	rm -f timesfont.tex
	echo "\usepackage{times}" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	rm -f figuresizing.tex
	ln -s regularfont.tex fontsize.tex
	ln -s symbol-postscript.tex symbol.tex
	ln -s figuresizing-latex.tex figuresizing.tex
	make clean
	make figures
	latex ref.tex
	latex ref.tex -interaction=batchmode
	latex ref.tex -interaction=batchmode
	latex ref.tex -interaction=batchmode
	latex ref.tex -interaction=batchmode
	makeindex ref
	latex ref.tex -interaction=batchmode
	rm -f ref.ps
	dvips ref.dvi
	rm -f ref.pdf
	gs -sDEVICE=pdfwrite -sOutputFile=ref.pdf -dNOPAUSE -dBATCH ref.ps
	rm -f ref.dvi
	rm -f ref.ps

release-pdf: ref.pdf $(TARGETDIR)
	cp ref.pdf $(TARGETDIR)/condor-$(SUBVER)-Manual.pdf

ref.dvi: 
	rm -f timesfont.tex
	echo "% do not want times" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	rm -f figuresizing.tex
	ln -s smallfont.tex fontsize.tex
	ln -s symbol-postscript.tex symbol.tex
	ln -s figuresizing-latex.tex figuresizing.tex
	make clean
	make figures
	latex ref.tex 
	latex ref.tex -interaction=batchmode
	latex ref.tex -interaction=batchmode
	latex ref.tex -interaction=batchmode
	latex ref.tex -interaction=batchmode
	makeindex ref
	latex ref.tex -interaction=batchmode

ref.ps: ref.dvi
	dvips ref.dvi

release-ps: ref.ps $(TARGETDIR)
	cp ref.ps $(TARGETDIR)/ref.ps
	cp $(TARGETDIR)/ref.ps $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	compress $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	cp $(TARGETDIR)/ref.ps $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	gzip $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	mv $(TARGETDIR)/ref.ps $(TARGETDIR)/condor-$(SUBVER)-Manual.ps

ref.html: $(TARGETDIR)
	rm -f timesfont.tex
	echo "% do not want times" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	rm -f figuresizing.tex
	ln -s regularfont.tex fontsize.tex
	ln -s symbol-html.tex symbol.tex
	ln -s figuresizing-html.tex figuresizing.tex
	make clean
	make figures
	latex ref.tex
	latex ref.tex -interaction=batchmode
	latex ref.tex -interaction=batchmode
	latex ref.tex -interaction=batchmode
	latex ref.tex -interaction=batchmode
	makeindex ref
	latex ref.tex -interaction=batchmode
	latex2html -address condor-admin@cs.wisc.edu -split 4 -link 2 -show_section_numbers -antialias -tmp /tmp -long_titles 3 -toc_depth 2 -local_icons ref.tex
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

release-html: ref.html $(TARGETDIR)
	rm -rf $(MANDIR).old
	mv -f $(MANDIR) $(MANDIR).old
	mv -f $(TARGETDIR) $(MANDIR)
	/p/condor/public/cgi-bin/manual/human_header_links.$(SUBDIR).pl
	rm -f ref.html

admin-man/machine-states.eps: admin-man/machine-states.fig
	rm -f $@
	fig2dev -L ps -m .6 $< > $@

admin-man/machine-activities.eps: admin-man/machine-activities.fig
	rm -f $@
	fig2dev -L ps -m .6 $< > $@

admin-man/pool-arch.eps: admin-man/pool-arch.fig
	rm -f $@
	fig2dev -L ps -m .4 $< > $@

user-man/dagman-diamond.eps: user-man/dagman-diamond.fig
	rm -f $@
	fig2dev -L ps -m .6 $< > $@

figures: admin-man/machine-states.eps admin-man/machine-activities.eps admin-man/pool-arch.eps user-man/dagman-diamond.eps

$(TARGETDIR):
	mkdir $(TARGETDIR)

clean:
	rm -rf ref ref.new man user admin *.dvi *.ps *.html *.log *.aux *.toc *.pdf */*.log *.ind *.idx *.ilg

reallyclean: clean
	rm -rf $(TARGETDIR)

release:
	make reallyclean
	make release-ps
	make release-pdf
	make release-html
