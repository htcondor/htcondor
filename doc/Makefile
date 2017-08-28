# The .fig sources are stored in git.
FIG_FIGS =		user-man/dagman-node.eps \
				user-man/dagman-diamond.eps \
				user-man/dagman-connect.eps

# The .dia sources are stored in git, but Make doesn't make the .eps files.
DIA_FIGS =		admin-man/states.eps \
				admin-man/activities.eps

# The .dot sources are stored in git, but Make doesn't maek the .eps files.
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

all : ref.pdf

# Only helpful for debugging.  Do NOT insert this as a dependency
# of ref.pdf; that will force it to be rebuilt every time.
links.pdf :
	@rm -f font.tex ; ln -s font.pdf.tex font.tex
	@rm -f symbol.tex ; ln -s symbol.pdf.tex symbol.tex
	@rm -f fontsize.tex; ln -s fontsize.pdf.tex fontsize.tex
	@rm -f figuresizing.tex; ln -s figuresizing.pdf.tex figuresizing.tex

ref.pdf : ref.tex $(FIGURES) $(LINKS:%.tex=%.pdf.tex)
	@rm -f font.tex ; ln -s font.pdf.tex font.tex
	@rm -f symbol.tex ; ln -s symbol.pdf.tex symbol.tex
	@rm -f fontsize.tex; ln -s fontsize.pdf.tex fontsize.tex
	@rm -f figuresizing.tex; ln -s figuresizing.pdf.tex figuresizing.tex
	@pdflatex $(LATEX_ARGS) ref.tex
	@makeindex ref
	@pdflatex $(LATEX_ARGS) ref.tex

%.pdf : %.ps
	@ps2pdf $< $@

%.pdf : %.eps
	@epstopdf $< > $@

%.eps : %.fig
	@fig2dev -L eps -m .4 $< > $@

user-man/dagman-node.eps : user-man/dagman-node.fig
	@fig2dev -L eps -m .6 $< > $@

# Make automatically deletes intermediate files (files created as the
# result of a implicit rule whose result is not a target); because
# dagman-node.eps required its own rule, it's not considered an
# intermediate file.  The special target .INTERMEDIATE changes that.
# The special target .SECONDARY says to not delete the listed
# intermediate files; this -- as far as I know -- is the only way to
# make Make shut up about them.
.INTERMEDIATE : user-man/dagman-node.eps
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
	@rm -f *.html

man-clean :
	@cd makeman; make clean >& /dev/null
