all:  ref.ps admin.ps user.ps man.ps admin.html user.html man.html

ref.ps: admin.tex user.tex man.tex condor-macros.tex
	latex ref.tex
	latex ref.tex
	dvips ref.dvi

admin.ps:  admin.tex condor-macros.tex
	latex admin.tex
	latex admin.tex
	dvips admin.dvi

user.ps: user.tex condor-macros.tex
	latex user.tex
	latex user.tex
	dvips user.dvi

man.ps: man.tex condor-macros.tex
	latex man.tex
	latex man.tex
	dvips man.dvi

ref.html: ref.ps
	latex2html -split 4 -link 2 ref.tex
	mv -f ref ~/public/html

admin.html: admin.tex condor-macros.tex admin.ps
	latex2html -split 4 -link 2 admin.tex
	mv -f admin ~/public/html

user.html: user.tex condor-macros.tex user.ps
	latex2html -split 4 -link 1 user.tex

man.html: man.tex condor-macros.tex man.ps
	latex2html -split 4 -link 1 man.tex

clean:
	rm -rf ref man user admin *.dvi *.ps *.html *.log *.aux *.toc
