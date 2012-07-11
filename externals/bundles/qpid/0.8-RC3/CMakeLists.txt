###############################################################
 # 
 # Copyright (C) 1990-2010, Red Hat Inc. 
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

# qpid is for condor_contrib/mgmt/qmf
if (WANT_CONTRIB AND (LINUX OR WINDOWS))
	option(WITH_QPID "Compiling with support for QPID" ON)
endif()

if (WITH_QPID)

    # we only support proper.
	# I don't know how we check versions for native search of qpid-qmf? header check? program_exec?
	find_multiple( "qpidcommon;qpidclient;qmf;qmfengine;qmfconsole;qpidtypes;qpidmessaging;qmf2" QPID_FOUND)
	find_program ( QMF_GEN qmf-gen )

endif()

# only enable if qpid and qmf bits are all present.
if (QPID_FOUND AND NOT "${QMF_GEN}" MATCHES "-NOTFOUND" )
	# you will need to update config.h.cmake with your var.
	message (STATUS "external configured QPID_FOUND=(${QPID_FOUND}) QMF_GEN=(${QMF_GEN})")
	set( HAVE_EXT_QPID ON PARENT_SCOPE )
	set (QPID_FOUND ${QPID_FOUND} PARENT_SCOPE)
	set (QMF_GEN ${QMF_GEN} PARENT_SCOPE)
else()

	message (STATUS "external skipped (qpid)")

endif()
