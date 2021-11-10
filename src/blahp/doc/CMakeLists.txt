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

find_program(XSLTPROC_EXECUTABLE xsltproc)

find_file(XSLTPROC_MANPAGE_STYLESHEET
    NAMES docbook.xsl
    PATHS /usr/share/sgml/docbook/xsl-stylesheets/manpages)

set(MAN1PAGES_TO_CREATE
    blah_job_registry_add.1
    blah_job_registry_dump.1
    blah_job_registry_lkup.1
    blah_job_registry_scan_by_subject.1
    blah_check_config.1
    blahpd.1
)

foreach (manpage ${MAN1PAGES_TO_CREATE})
  string(REGEX REPLACE ".[1-9]$" ".xml" manpage_src ${manpage})
  add_custom_command(OUTPUT  ${manpage}
                     COMMAND ${XSLTPROC_EXECUTABLE}
                             ${XSLTPROC_MANPAGE_STYLESHEET} 
                             ${manpage_src}
                     DEPENDS ${manpage_src})
  set_source_files_properties(${manpage} PROPERTIES GENERATED TRUE)
endforeach()

add_custom_target(all_manpages ALL
    DEPENDS ${MAN1PAGES_TO_CREATE} )

install(FILES ${MAN1PAGES_TO_CREATE} DESTINATION man/man1)

