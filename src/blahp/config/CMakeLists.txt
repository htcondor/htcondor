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

cmake_minimum_required(VERSION 2.6)

install(FILES 
    blah.config.template 
    blparser.conf.template
    DESTINATION /etc)

install(FILES 
    glite-ce-blah-parser
    PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
    DESTINATION /etc/rc.d/init.d)

