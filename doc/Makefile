SUBDIR = v6.1
SUBVER = V6_1
all:  ref.dvi
#all:  man.ps

ref.pdf:
	rm -f timesfont.tex
	echo "\usepackage{times}" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	ln -s regularfont.tex fontsize.tex
	ln -s symbol-postscript.tex symbol.tex
	rm -rf *.dvi *.ps *.log *.aux *.toc
	make figures
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex
	rm -f ref.ps
	dvips ref.dvi
	rm -f ref.pdf
	gs -sDEVICE=pdfwrite -sOutputFile=ref.pdf -dNOPAUSE -dBATCH ref.ps
	rm -f ref.dvi
	rm -f ref.ps

release-pdf: ref.pdf
	cp ref.pdf /p/condor/public/html/manual/$(SUBDIR).new/condor-$(SUBVER)-Manual.pdf


ref.dvi: 
	rm -f timesfont.tex
	echo "% do not want times" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	ln -s smallfont.tex fontsize.tex
	ln -s symbol-postscript.tex symbol.tex
	rm -rf *.dvi *.ps *.log *.aux *.toc
	make figures
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex

ref.ps: ref.dvi
	dvips ref.dvi

release-ps: ref.ps
	cp ref.ps /p/condor/public/html/manual/$(SUBDIR).new/ref.ps
	cp /p/condor/public/html/manual/$(SUBDIR).new/ref.ps /p/condor/public/html/manual/$(SUBDIR).new/condor-$(SUBVER)-Manual.ps
	compress /p/condor/public/html/manual/$(SUBDIR).new/condor-$(SUBVER)-Manual.ps
	cp /p/condor/public/html/manual/$(SUBDIR).new/ref.ps /p/condor/public/html/manual/$(SUBDIR).new/condor-$(SUBVER)-Manual.ps
	gzip /p/condor/public/html/manual/$(SUBDIR).new/condor-$(SUBVER)-Manual.ps
	mv /p/condor/public/html/manual/$(SUBDIR).new/ref.ps /p/condor/public/html/manual/$(SUBDIR).new/condor-$(SUBVER)-Manual.ps

ref.html:  
	rm -f timesfont.tex
	echo "% do not want times" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	ln -s regularfont.tex fontsize.tex
	ln -s symbol-html.tex symbol.tex
	rm -rf *.dvi *.ps *.log *.aux *.toc
	make figures
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex2html -address condor-admin@cs.wisc.edu -split 4 -link 2 -show_section_numbers ref.tex
	rm -f ref.dvi
	mv ref ref.new
	mv -f ref.new/*  /p/condor/public/html/manual/$(SUBDIR).new
	mv -f ref.new/.??*  /p/condor/public/html/manual/$(SUBDIR).new
	rmdir ref.new

release-html: 
	make ref.html
	rm -rf /p/condor/public/html/manual/$(SUBDIR).old
	mv -f /p/condor/public/html/manual/$(SUBDIR) /p/condor/public/html/manual/$(SUBDIR).old
	mv -f /p/condor/public/html/manual/$(SUBDIR).new /p/condor/public/html/manual/$(SUBDIR)
	/p/condor/public/cgi-bin/manual/$(SUBDIR)-links/human_header_links.pl

admin-man/startd-states.eps: admin-man/startd-states.fig
	rm -f admin-man/startd-states.eps
	fig2dev -L ps -m .6 admin-man/startd-states.fig > admin-man/startd-states.eps 

user-man/dagman-diamond.eps: user-man/dagman-diamond.fig
	rm -f $@
	fig2dev -L ps -m .6 $< > $@

admin-man/startd-activities.eps: admin-man/startd-activities.fig
	rm -f admin-man/startd-activities.eps
	fig2dev -L ps -m .6 admin-man/startd-activities.fig > admin-man/startd-activities.eps 

admin-man/pool-arch.eps: admin-man/pool-arch.fig
	rm -f admin-man/pool-arch.eps
	fig2dev -L ps -m .4 admin-man/pool-arch.fig > admin-man/pool-arch.eps 

figures: admin-man/startd-states.eps admin-man/startd-activities.eps admin-man/pool-arch.eps user-man/dagman-diamond.eps

clean:
	rm -rf ref man user admin *.dvi *.ps *.html *.log *.aux *.toc *.pdf
	rm -rf /p/condor/public/html/manual/$(SUBDIR).new

release:
	make clean
	mkdir /p/condor/public/html/manual/$(SUBDIR).new
	make release-ps
	make release-pdf
	make release-html
