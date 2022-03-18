# ****************
# * BLAHP daemon * 
# ****************
#
# $Id: Makefile.am,v 1.16 2012/02/06 13:06:42 pandreet Exp $ 
#
# File:           Makefile.am
#
# Author(s):      Francesco Prelz ($Author: pandreet $)
#        e-mail: "Francesco.Prelz@mi.infn.it"
#
# Revision history:
#
# 28-Nov-2005  Created 
#

wletcwmdir = $(sysconfdir)
wletcwm_DATA = blah.config.template blparser.conf.template

#install-data-hook:
#        /bin/cp $(DESTDIR)$(sysconfdir)/blah.config.template $(DESTDIR)$(sysconfdir)/blah.config


EXTRA_DIST = blah.config.template \
             blparser.conf.template \
             glite-ce-blah-parser

initdir = rc.d/init.d
bldir = $(sysconfdir)/$(initdir)
bl_SCRIPTS = glite-ce-blah-parser

CLEANFILES =

MAINTAINERCLEANFILES = Makefile.in *~ *.*~

