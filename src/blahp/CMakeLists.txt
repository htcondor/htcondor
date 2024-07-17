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

set(CMAKE_UNITY_BUILD false)

if (NOT CONDOR_PACKAGE_NAME)

    cmake_minimum_required(VERSION 3.16)

    project(glite-ce-blahp)
    project(BLAH)

    # CPack info

    set(CPACK_RPM_PACKAGE_RELEASE "0")

    if (UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "STGZ;DEB;RPM")
    execute_process(COMMAND uname -i 
        OUTPUT_VARIABLE local_arch OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(CPACK_SYSTEM_NAME "${CPACK_RPM_PACKAGE_RELEASE}.${local_arch}")
    endif (UNIX AND NOT APPLE)

    set(CMAKE_INSTALL_PREFIX "/usr")

    set(CPACK_PACKAGE_NAME "BLAH")
    set(CPACK_PACKAGE_VENDOR "EMI")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Batch Local ASCII Helper Protocol suite")
    set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/blah_description.txt")
    set(CPACK_PACKAGE_VERSION_MAJOR "2")
    set(CPACK_PACKAGE_VERSION_MINOR "2")
    set(CPACK_PACKAGE_VERSION_PATCH "0")
    set(CPACK_PACKAGE_VERSION 
       "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

    # For the following setting, see CMAKE manual, page 155
    # or http://public.kitware.com/Bug/view.php?id=7000
    set(CPACK_SET_DESTDIR ON)
    set(CPACK_PACKAGE_RELOCATABLE "false")

    set(CPACK_PACKAGE_CONTACT "blah@mi.infn.it")
    set(CPACK_RPM_PACKAGE_LICENSE "Apache Software License")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")

    set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE 
        ${CMAKE_CURRENT_SOURCE_DIR}/project/glite-ce-blahp.post)
    set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE 
        ${CMAKE_CURRENT_SOURCE_DIR}/project/glite-ce-blahp.preun)

    install(FILES LICENSE 
        DESTINATION share/doc/BLAH-2.2.0)

endif(NOT CONDOR_PACKAGE_NAME)

if (NOT CONDOR_PACKAGE_NAME OR WITH_BLAHP)
    add_subdirectory(src build)
    add_subdirectory(config)
    add_subdirectory(doc)
endif(NOT CONDOR_PACKAGE_NAME OR WITH_BLAHP)

