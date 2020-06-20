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


set( CEDAR_UTIL_SRCS 
${CMAKE_CURRENT_SOURCE_DIR}/authentication.cpp
${CMAKE_CURRENT_SOURCE_DIR}/buffers.cpp
${CMAKE_CURRENT_SOURCE_DIR}/cedar_no_ckpt.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_auth_anonymous.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_auth_claim.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_auth.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_auth_fs.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_auth_kerberos.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_auth_munge.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_auth_passwd.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_auth_ssl.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_auth_sspi.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_auth_x509.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_crypt_3des.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_crypt_blowfish.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_crypt_aesgcm.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_crypt.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_ipverify.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_rw.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_secman.cpp
${CMAKE_CURRENT_SOURCE_DIR}/CryptKey.cpp
${CMAKE_CURRENT_SOURCE_DIR}/errno_num.cpp
${CMAKE_CURRENT_SOURCE_DIR}/open_flags.cpp
${CMAKE_CURRENT_SOURCE_DIR}/reli_sock.cpp
${CMAKE_CURRENT_SOURCE_DIR}/SafeMsg.cpp
${CMAKE_CURRENT_SOURCE_DIR}/safe_sock.cpp
${CMAKE_CURRENT_SOURCE_DIR}/shared_port_client.cpp
${CMAKE_CURRENT_SOURCE_DIR}/shared_port_endpoint.cpp
${CMAKE_CURRENT_SOURCE_DIR}/shared_port_server.cpp
${CMAKE_CURRENT_SOURCE_DIR}/sockCache.cpp
${CMAKE_CURRENT_SOURCE_DIR}/sock.cpp
${CMAKE_CURRENT_SOURCE_DIR}/stream.cpp
PARENT_SCOPE
)

if (NOT WINDOWS)
	condor_exe_test(cedar_test.exe "cedar.t.unix.cpp" "${CONDOR_TOOL_LIBS}")
endif()

