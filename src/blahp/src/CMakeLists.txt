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
# 26-Oct-2012  Created 

cmake_minimum_required(VERSION 2.6)

if(CONDOR_PACKAGE_NAME)
    set(BIN_DIR "${C_BIN}")
    set(SBIN_DIR "${C_SBIN}")
    set(LIBEXEC_DIR "${C_LIBEXEC_BLAHP}")

    set(ClassAd_LIBRARY "classads")
else()
    set(BIN_DIR "bin")
    set(SBIN_DIR "sbin")
    set(LIBEXEC_DIR "libexec")

    include(FindClassAd.cmake)
    include_directories(${ClassAd_INCLUDE_DIR})
    set(ClassAd_LIBRARY ${CLASSAD_FOUND})

    include(FindPkgConfig)

    pkg_check_modules(GLOBUS_COMMON globus-common)
    include_directories(${GLOBUS_COMMON_INCLUDE_DIRS})

    pkg_check_modules(GLOBUS_IO globus-io)
    pkg_check_modules(GLOBUS_GSSAPI_GSI globus-gssapi-gsi)
    pkg_check_modules(GLOBUS_GSS_ASSIST globus-gss-assist)
    pkg_check_modules(GLOBUS_GSI_CREDENTIAL globus-gsi-credential)
    pkg_check_modules(GLOBUS_GSI_PROXY_CORE globus-gsi-proxy-core)
endif()

include_directories(.)

set (main_common_sources 
    console.c job_status.c resbuffer.c server.c commands.c
    classad_binary_op_unwind.C classad_c_helper.C proxy_hashcontainer.c 
    config.c job_registry.c blah_utils.c env_helper.c mapped_exec.c md5.c 
    cmdbuffer.c)

set (bupdater_common_sources 
    Bfunctions.c job_registry.c md5.c config.c blah_utils.c
    job_registry_updater.c)

# programs for 'sbin'
add_executable(blahpd_daemon main_daemon.c ${main_common_sources})
if(NOT CONDOR_PACKAGE_NAME)
    set_target_properties(blahpd_daemon PROPERTIES COMPILE_FLAGS ${ClassAd_CXX_FLAGS})
endif()
target_link_libraries(blahpd_daemon -lpthread ${ClassAd_LIBRARY})
add_executable(blah_job_registry_add 
    blah_job_registry_add.c job_registry.c 
    job_registry_updater.c md5.c config.c)
add_executable(blah_job_registry_lkup 
    blah_job_registry_lkup.c job_registry.c md5.c config.c)
add_executable(blah_job_registry_scan_by_subject 
    blah_job_registry_scan_by_subject.c classad_c_helper.C
    classad_binary_op_unwind.C job_registry.c md5.c config.c)
if(NOT CONDOR_PACKAGE_NAME)
    set_target_properties(blah_job_registry_scan_by_subject PROPERTIES COMPILE_FLAGS ${ClassAd_CXX_FLAGS})
endif()
target_link_libraries(blah_job_registry_scan_by_subject ${ClassAd_LIBRARY})
add_executable(blah_check_config
    blah_check_config.c Bfunctions.c config.c blah_utils.c)
add_executable(blah_job_registry_dump
    blah_job_registry_dump.c job_registry.c md5.c config.c)
add_executable(blah_job_registry_purge
    blah_job_registry_purge.c job_registry.c md5.c)

# programs for 'bin'
add_executable(blahpd main.c ${main_common_sources})
if(NOT CONDOR_PACKAGE_NAME)
    set_target_properties(blahpd PROPERTIES COMPILE_FLAGS ${ClassAd_CXX_FLAGS})
endif()
target_link_libraries(blahpd -lpthread ${ClassAd_LIBRARY})

# programs for 'libexec'
add_executable(BLClient BLClient.c blah_utils.c BLfunctions.c)
add_executable(BLParserLSF BLParserLSF.c blah_utils.c BLfunctions.c)
target_link_libraries(BLParserLSF -lpthread)
add_executable(BLParserPBS BLParserPBS.c blah_utils.c BLfunctions.c)
target_link_libraries(BLParserPBS -lpthread)
add_executable(BUpdaterCondor BUpdaterCondor.c  ${bupdater_common_sources})
target_link_libraries(BUpdaterCondor -lpthread)
add_executable(BNotifier
    BNotifier.c Bfunctions.c job_registry.c md5.c config.c blah_utils.c)
target_link_libraries(BNotifier -lpthread)
add_executable(BUpdaterLSF BUpdaterLSF.c ${bupdater_common_sources})
target_link_libraries(BUpdaterLSF -lpthread -lm)
add_executable(BUpdaterPBS BUpdaterPBS.c ${bupdater_common_sources})
target_link_libraries(BUpdaterPBS -lpthread -lm)
add_executable(BUpdaterSGE
    BUpdaterSGE.c Bfunctions.c job_registry.c md5.c config.c 
    blah_utils.c)
add_executable(blparser_master blparser_master.c config.c blah_utils.c)

if(NOT CONDOR_PACKAGE_NAME)
    if (${GLOBUS_COMMON_FOUND} AND ${GLOBUS_IO_FOUND})
    add_executable(BPRclient BPRclient.c BPRcomm.c tokens.c)
    target_link_libraries(BPRclient 
      ${GLOBUS_GSI_PROXY_CORE_LDFLAGS}
      ${GLOBUS_GSI_CREDENTIALS_LDFLAGS}
      ${GLOBUS_GSS_ASSIST_LDFLAGS})
    add_executable(BPRserver BPRserver.c BPRcomm.c tokens.c)
    target_link_libraries(BPRserver
      ${GLOBUS_GSI_PROXY_CORE_LDFLAGS}
      ${GLOBUS_GSI_CREDENTIALS_LDFLAGS}
      ${GLOBUS_GSS_ASSIST_LDFLAGS})
    set_target_properties(BPRserver PROPERTIES COMPILE_FLAGS "-static") 
    endif (${GLOBUS_COMMON_FOUND} AND ${GLOBUS_IO_FOUND})
endif()

# test programs
add_executable(test_job_registry_create test_job_registry_create.c job_registry.c md5.c)
add_executable(test_job_registry_purge test_job_registry_purge.c job_registry.c md5.c)
add_executable(test_job_registry_update test_job_registry_update.c job_registry.c md5.c)
add_executable(test_job_registry_access test_job_registry_access.c job_registry.c md5.c)
add_executable(test_job_registry_update_from_network
    test_job_registry_update_from_network.c job_registry.c
    job_registry_updater.c md5.c config.c)
add_executable(test_cmdbuffer cmdbuffer.c)
set_target_properties(test_cmdbuffer PROPERTIES COMPILE_FLAGS "-DCMDBUF_DEBUG") 

# CPack info

install(TARGETS blahpd RUNTIME DESTINATION ${BIN_DIR})
install(TARGETS
    blahpd_daemon blah_job_registry_add blah_job_registry_lkup
    blah_job_registry_scan_by_subject blah_check_config 
    blah_job_registry_dump blah_job_registry_purge
    RUNTIME DESTINATION ${SBIN_DIR})
install(TARGETS 
    BLClient BLParserLSF BLParserPBS BUpdaterCondor BNotifier 
    BUpdaterLSF BUpdaterPBS BUpdaterSGE
    blparser_master
    RUNTIME DESTINATION ${LIBEXEC_DIR})

set(blah_scripts
    scripts/blah_load_config.sh scripts/blah_common_submit_functions.sh
    scripts/pbs_cancel.sh scripts/pbs_status.sh scripts/pbs_submit.sh 
    scripts/pbs_hold.sh scripts/pbs_resume.sh scripts/lsf_cancel.sh
    scripts/lsf_status.sh scripts/lsf_submit.sh scripts/lsf_hold.sh
    scripts/lsf_resume.sh scripts/condor_cancel.sh scripts/condor_status.sh
    scripts/condor_submit.sh scripts/condor_hold.sh scripts/condor_resume.sh
    scripts/condor_local_submit_attributes.sh
    scripts/pbs_local_submit_attributes.sh
    scripts/lsf_local_submit_attributes.sh
    scripts/sge_cancel.sh scripts/sge_helper scripts/sge_resume.sh 
    scripts/sge_submit.sh scripts/sge_filestaging scripts/sge_hold.sh 
    scripts/sge_status.sh scripts/runcmd.pl.template
    scripts/sge_local_submit_attributes.sh
    scripts/slurm_cancel.sh scripts/slurm_resume.sh scripts/slurm_status.sh
    scripts/slurm_hold.sh scripts/slurm_submit.sh
    scripts/slurm_local_submit_attributes.sh
    scripts/nqs_cancel.sh scripts/nqs_resume.sh scripts/nqs_status.sh
    scripts/nqs_hold.sh scripts/nqs_submit.sh
    scripts/nqs_local_submit_attributes.sh
    scripts/kubernetes_cancel.sh scripts/kubernetes_resume.sh scripts/kubernetes_status.sh
    scripts/kubernetes_hold.sh scripts/kubernetes_submit.sh
    scripts/kubernetes_local_submit_attributes.sh
    scripts/blah.py scripts/__init__.py
    scripts/pbs_status.py
    scripts/slurm_status.py
    scripts/lsf_status.py
   )

install(FILES
    ${blah_scripts}
    PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
    DESTINATION ${LIBEXEC_DIR})

if(NOT CONDOR_PACKAGE_NAME)
    if (${GLOBUS_COMMON_FOUND} AND ${GLOBUS_IO_FOUND})
    install(TARGETS BPRclient BPRserver RUNTIME DESTINATION ${LIBEXEC_DIR})
    endif (${GLOBUS_COMMON_FOUND} AND ${GLOBUS_IO_FOUND})

    include(CPack)
endif()

