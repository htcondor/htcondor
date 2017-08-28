FIGS =			user-man/dagman-node.eps \
				user-man/dagman-diamond.eps \
				user-man/dagman-connect.eps

FIGURES =		admin-man/states.eps \
				admin-man/activities.eps \
				user-man/splice-simple.eps \
				user-man/splice-X.eps \
				user-man/splice-s1.eps \
				user-man/splice-complex.eps

PSFIGS =		contrib/view-screenshot.ps

LINKS =			font.tex \
				symbol.tex \
				fontsize.tex \
				figuresizing.tex

LATEX_ARGS =	-file-line-error-style -halt-on-error

all : ref.pdf

ref.pdf : ref.tex figures $(LINKS:%.tex=%.pdf.tex)
	@rm font.tex ; ln -s font.pdf.tex font.tex
	@rm symbol.tex ; ln -s symbol.pdf.tex symbol.tex
	@rm fontsize.tex; ln -s fontsize.pdf.tex fontsize.tex
	@rm figuresizing.tex; ln -s figuresizing.pdf.tex figuresizing.tex
	@pdflatex $(LATEX_ARGS) ref.tex

figures : $(FIGS:%.eps=%.pdf) $(FIGURES:%.eps=%.pdf) $(PSFIGS:%.ps=%.pdf)

%.pdf : %.ps
	@ps2pdf $< $@

%.eps : %.fig
	@fig2dev -L eps -m .4 $< > $@

user-man/dagman-node.eps : user-man/dagman-node.fig
	@fig2dev -L eps -m .6 $< > $@

%.pdf : %.eps
	@epstopdf $< > $@

clean : figures-clean tex-clean pdf-clean html-clean man-clean
	@rm -f $(LINKS)

figures-clean :
	@rm -f $(FIGS)
	@rm -f $(FIGS:%.eps=%.pdf) $(FIGURES:%.eps=%.pdf) $(PSFIGS:%.ps=%.pdf)

tex-clean :
	@rm -f *.log *.aux *.toc *.ind *.idx *.ilg *.out

pdf-clean :
	@rm -f *.pdf

# @rm -fr html-directory
# maybe @rm -f *.bak, but probably just change the perl line.
html-clean :
	@rm -f *.html

man-clean :
	@cd makeman; make clean
