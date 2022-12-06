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

cmake_minimum_required(VERSION 3.0)

if(CONDOR_PACKAGE_NAME)
    set(MAN_DIR "${C_MAN}")
else()
    set(MAN_DIR "man/man1")
endif()

if(WITH_FULL_BLAHP)

	find_program(XSLTPROC_EXECUTABLE xsltproc)

	# If we don't have xsltproc on the system, don't bother making manpages

	if (NOT ("${XSLTPROC_EXECUTABLE}" STREQUAL "XSLTPROC_EXECUTABLE-NOTFOUND"))

		find_file(XSLTPROC_MANPAGE_STYLESHEET
			NAMES docbook.xsl
			PATHS /usr/share/sgml/docbook/xsl-stylesheets/manpages
			/usr/share/sgml/docbook/stylesheet/xsl/docbook-xsl/manpages)

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
				${CMAKE_CURRENT_SOURCE_DIR}/${manpage_src}
				DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${manpage_src})
			set_source_files_properties(${manpage} PROPERTIES GENERATED TRUE)
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${manpage} DESTINATION ${MAN_DIR})
		endforeach()

		add_custom_target(all_manpages ALL
			DEPENDS ${MAN1PAGES_TO_CREATE} )

	endif()

endif()
