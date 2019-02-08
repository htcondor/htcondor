# This TeXLive installation (on ingwe) has the TikZ fix.
TEX_DIR = /scratch/tlmiller/install/texlive/2017/bin/x86_64-linux/
export PATH := $(TEX_DIR):$(PATH)

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

LATEX_ARGS =	-file-line-error-style -halt-on-error

# Convert the list of each ${manpage}.tex sourced to build the manual
# into the list of ${manpage}.html generate by latex2html.  This is
# rather a hack, but I don't care enough about the man pages to fix it.
MANPAGES = $(shell grep '^\\input' man-pages/man.tex | grep -v condor_dagman_metrics_reporter.tex | grep -v tool_common.tex | awk -F/ '{print $$2}' | awk -F. '{printf "%s.html ",$$1}')

all : ref.pdf

# Only helpful for debugging.  Do NOT insert this as a dependency
# of ref.pdf; that will force it to be rebuilt every time.
figures : $(FIGURES)

MANUAL_SOURCES = \
    man-macros.tex \
	$(wildcard man-pages/*.tex)

just-man-pages : makeman just-man-pages.tex $(MANUAL_SOURCES)
	@cd makeman; make clean && make makeman
	@latex2html -split 4 -link 2 -antialias -tmp /tmp -long_titles 3 -toc_depth 2 -local_icons just-man-pages.tex
	@cd just-man-pages; for html in $(MANPAGES); do \
		../makeman/makeman -v -i $${html} -s 1; \
	done
	@cd just-man-pages; mv condor_dagman_metrics_repor.html condor_dagman_metrics_reporter.html; ../makeman/makeman -v -i condor_dagman_metrics_reporter.html -s 1
	@rm -fr man
	@mkdir man
	@mkdir man/man1
	@mv just-man-pages/*.1 man/man1

%.pdf : %.ps
	@ps2pdf -dEPSCrop $< $@

%.pdf : %.eps
	@epstopdf $< > $@

%.eps : %.fig
	@fig2dev -L eps -m .6 $< > $@

# Make automatically deletes intermediate files (files created as the
# result of a implicit rule whose result is not a target).  The special
# target .SECONDARY says to not delete the listed intermediate files;
# this -- as far as I know -- is the only way to prevent Make from reporting
# that they're being deleted (which is just confusing noise).
.SECONDARY : $(FIG_FIGS)

clean : figures-clean tex-clean pdf-clean html-clean man-clean

figures-clean :
	@rm -f $(FIG_FIGS)
	@rm -f $(FIGURES)

tex-clean :
	@rm -f ref.log ref.aux ref.toc ref.ind ref.idx ref.ilg ref.out

pdf-clean :
	@rm -f ref.pdf

html-clean :
	@rm -fr *.html *.png ref ref.html
	@rm -fr ref.dvi texput.log
	@rm -fr idxmake.dvi idxmake.log
	@rm -fr ref.4ct ref.4dx ref.4ix ref.4tc ref.css ref.idv ref.lg ref.tmp ref.xref

man-clean :
	@rm -fr man just-man-pages
	@cd makeman; make clean > /dev/null 2>&1

release : ref.pdf ref.html
	@/bin/bash -e make-release

# Note that the wildcard glob-expansion may pick up more latex source files
# than we actually include, but that's OK, as this make macro is just used
# for determining rebuild depencies, it doesn't define what to actually
# build into the manual.  Don't wildcard the document root, since those
# source files are where we do define what's actually built into the manual.

LATEX_SOURCES = $(MANUAL_SOURCES) \
                condor-macros.sty \
                symbol.tex license.tex faq.tex \
				$(wildcard admin-main/*.tex) \
				$(wildcard clouds/*.tex) \
                $(wildcard contrib/*.tex) \
                $(wildcard grids/*.tex) \
                $(wildcard misc/*.tex) \
                $(wildcard overview/*.tex) \
                $(wildcard platforms/*.tex) \
                $(wildcard user-man/*.tex) \
                $(wildcard version-history/*.tex)

# Only the v8.7 branch requires the fourth run to remove the 'labels may
# changed' warning.  We should figure out why.
#
# The index files (ref.ind, at least) produced by htlatex confuse pdflatex,
# so we need to make sure that they're gone before building.  We don't
# depend on tex-clean because then we always rebuild.
ref.pdf : ref.tex $(FIGURES) $(LATEX_SOURCES)
	@make tex-clean
	@pdflatex $(LATEX_ARGS) ref.tex
	@makeindex ref
	@pdflatex $(LATEX_ARGS) ref.tex
	@pdflatex $(LATEX_ARGS) ref.tex
	@pdflatex $(LATEX_ARGS) ref.tex

# 'ref' is what we're building.  '3' is how many sections deep to split
# the pages.  'sec-filename' is to use the symbolic section names in
# the URLs, which should make the links more stable.  'next' adds the 'next'
# link to the header and footer.  'info' is a debugging option?
#
# 'NoFonts' disables the broken-ass default that uses SVG's tspan tags.
# 'fonts' turns on something that's based on span tags with classes
# corresponding to LaTeX font classes (textit, textbf, and texttt).
HTLATEX_OPTIONS=ref,3,sec-filename,next,info,NoFonts,fonts
ref.html : ref.tex condor-macros.4ht ref.cfg $(FIGURES) $(LATEX_SOURCES)
	@make tex-clean
	@rm -fr ref.html
	@htlatex ref.tex $(HTLATEX_OPTIONS)
	@tex '\def\filename{{ref}{idx}{4dx}{ind}} \input idxmake.4ht'
	@makeindex -o ref.ind ref.4dx
	@htlatex ref.tex $(HTLATEX_OPTIONS)
	@mkdir ref && cd ref && mv ../ref.css ../*.html ../*.svg ../*.png . && cd .. && mv ref ref.html

