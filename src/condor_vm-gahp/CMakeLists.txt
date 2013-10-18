 ###############################################################
 #
 # Copyright 2011 Red Hat, Inc.
 #
 # Licensed under the Apache License, Version 2.0 (the "License"); you
 # may not use this file except in compliance with the License.  You may
 # obtain a copy of the License at
 #
 #    http://www.apache.org/licenses/LICENSE-2.0
 #
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
 #
 ###############################################################

if(NOT WIN_EXEC_NODE_ONLY)

	file( GLOB RmvSrcs *_t.cpp )
	condor_glob( HeaderFiles SourceFiles "${RmvSrcs}" )

	condor_exe( condor_vm-gahp "${HeaderFiles};${SourceFiles}" ${C_SBIN}
	"${CONDOR_LIBS};${LIBVIRT_FOUND}"
	OFF )

    if (LINUX AND WANT_FULL_DEPLOYMENT)
        condor_exe( condor_vm-gahp-vmware "${HeaderFiles};${SourceFiles}" ${C_SBIN} "${CONDOR_LIBS}" OFF )
        set_property( TARGET condor_vm-gahp-vmware APPEND PROPERTY COMPILE_DEFINITIONS VMWARE_ONLY)
        install ( FILES condor_vm_vmware DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	# CRUFT: condor_vm_vmware.pl was renamed to condor_vm_vmware in
	#   Condor 7.7.6. The default in the config file was changed as
	#   well. But we keep a symlink for the old location for
	#   installations upgrading from older versions. This should
	#   be removed eventually.
 	clone_install(condor_vm_vmware "${C_SBIN}" "condor_vm_vmware.pl" "${C_SBIN}")
    endif()

    if (WINDOWS)
        install ( FILES condor_vm_vmware DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
        # use install with rename for condor_vm_vmware.pl since clone_install on windows tacks on ".exe" 
        install ( FILES condor_vm_vmware DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} RENAME condor_vm_vmware.pl )
    else ()
        install ( FILES libvirt_simple_script.awk DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
    endif(WINDOWS)

endif(NOT WIN_EXEC_NODE_ONLY)
