all:  ref.ps admin.ps user.ps man.ps admin.html user.html man.html
#all:  man.ps

ref.dvi: ref.tex admin.tex user.tex man.tex condor-macros.tex
	latex ref.tex
	latex ref.tex

ref.ps: ref.dvi
	dvips ref.dvi

ADMIN_FILES = admin-man/admin.tex \
              admin-man/afs.tex \
              admin-man/ckpt-server.tex \
              admin-man/configure.tex \
              admin-man/daemoncore.tex \
              admin-man/full-condor-compile.tex \
              admin-man/host-security.tex \
              admin-man/intro.tex \
              admin-man/kbdd.tex \
              admin-man/non-root.tex \
              admin-man/policy.tex \
              admin-man/root-config.tex \
              admin-man/security.tex \
              admin-man/special.tex \
              admin-man/startd-states.eps \
              admin-man/startd-states.fig \
              admin-man/uid.tex

admin.dvi:  admin.tex condor-macros.tex $(ADMIN_FILES)
	latex admin.tex
	latex admin.tex

admin.ps: admin.dvi
	dvips admin.dvi

user.dvi: user.tex condor-macros.tex
	latex user.tex
	latex user.tex

user.ps: user.dvi
	dvips user.dvi

MAN_PAGES = man-pages/man.tex \
            man-pages/condor_q.tex \
            man-pages/condor_history.tex \
            man-pages/condor_rm.tex \
            man-pages/condor_prio.tex \
            man-pages/condor_userprio.tex \
            man-pages/condor_compile.tex \
            man-pages/condor_checkpoint.tex \
            man-pages/condor_submit.tex \
            man-pages/condor_status.tex

man.dvi: man.tex condor-macros.tex $(MAN_PAGES)
	latex man.tex
	latex man.tex

man.ps: man.dvi
	dvips man.dvi

ref.html: ref.ps
	latex2html -split 4 -link 2 ref.tex
	mv -f ref ${HOME}/public/html

admin.html: admin.tex condor-macros.tex admin.ps 
	latex2html -split 4 -link 2 admin.tex
	mv -f admin ${HOME}/public/html

user.html: user.tex condor-macros.tex user.ps
	latex2html -split 4 -link 1 user.tex

man.html: man.tex condor-macros.tex man.ps
	latex2html -split 4 -link 1 man.tex

admin-man/startd-states.eps: admin-man/startd-states.fig
	rm -f admin-man/startd-states.eps
	fig2dev -L ps -m .6 admin-man/startd-states.fig > admin-man/startd-states.eps 


clean:
	rm -rf ref man user admin *.dvi *.ps *.html *.log *.aux *.toc
