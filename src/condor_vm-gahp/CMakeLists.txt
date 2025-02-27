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

if (LINUX AND WITH_LIBVIRT)
  set(SourceFiles
    pbuffer.cpp
    vmgahp_common.cpp
    vmgahp_config.cpp
    vmgahp.cpp
    vmgahp_main.cpp
    vm_request.cpp
    vm_type.cpp
    xen_type.linux.cpp
  )

  condor_exe( condor_vm-gahp "${SourceFiles}" ${C_SBIN} "${CONDOR_LIBS}" OFF )
  target_link_libraries(condor_vm-gahp LIBVIRT::LIBVIRT)


   install ( FILES libvirt_simple_script.awk DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

endif(LINUX AND WITH_LIBVIRT)
