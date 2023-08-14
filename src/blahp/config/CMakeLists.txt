# ****************
# * BLAHP daemon * 
# ****************
#
# $Id: $ 
#
# File:           CMakeLists.txt
#
# Author(s):      Francesco Prelz ($Author: $)
#        e-mail: "Francesco.Prelz@mi.infn.it"
#
# Revision history:
#
# 5-Nov-2012  Created 

cmake_minimum_required(VERSION 3.16)

install(FILES 
    blah.config.template 
    RENAME blah.config
    DESTINATION etc)

install(FILES 
    blparser.conf.template
    RENAME blparser.conf
    DESTINATION etc)

