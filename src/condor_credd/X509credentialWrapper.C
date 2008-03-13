/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "X509credentialWrapper.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "classad/classad_distribution.h"

CredentialWrapper::CredentialWrapper() {
	cred = NULL;
}

CredentialWrapper::~CredentialWrapper() {
	if (cred) 
		delete cred;
}
const char * 
CredentialWrapper::GetStorageName() {
	return storage_name.Value();
}

void 
CredentialWrapper::SetStorageName (const char * name) {
	storage_name = name?name:"";
}

classad::ClassAd * 
CredentialWrapper::GetMetadata () {
	classad::ClassAd * class_ad = cred->GetMetadata(); 
	class_ad->InsertAttr (CREDATTR_STORAGE_NAME, storage_name.Value());
	return class_ad;
}

X509CredentialWrapper::X509CredentialWrapper (const classad::ClassAd& class_ad){
	cred = new X509Credential (class_ad);
	std::string _storage_name;
	if (class_ad.EvaluateAttrString (CREDATTR_STORAGE_NAME, _storage_name)) {
		storage_name = _storage_name.c_str();
	}
	
	init();
}

X509CredentialWrapper::~X509CredentialWrapper() {
	get_delegation_reset();
}

void
X509CredentialWrapper::init(void) {

	get_delegation_proc_start_time = GET_DELEGATION_START_TIME_NONE;
	get_delegation_pid = GET_DELEGATION_PID_NONE;
	get_delegation_err_fd = GET_DELEGATION_FD_NONE;
	get_delegation_err_filename = NULL;
	get_delegation_err_buff = NULL;
	get_delegation_password_pipe[0] = GET_DELEGATION_FD_NONE;
	get_delegation_password_pipe[1] = GET_DELEGATION_FD_NONE;
}

void
X509CredentialWrapper::get_delegation_reset(void) {
	if ( get_delegation_err_fd != GET_DELEGATION_FD_NONE ) {
		close( get_delegation_err_fd );
	}
	if ( get_delegation_err_filename != NULL ) {
		unlink ( get_delegation_err_filename );
		free ( get_delegation_err_filename );
	}
	if ( get_delegation_err_buff != NULL ) {
		free (get_delegation_err_buff );
	}
	if ( get_delegation_password_pipe[0] != GET_DELEGATION_FD_NONE ) {
		close ( get_delegation_password_pipe[0] ); 
	}
	if ( get_delegation_password_pipe[1] != GET_DELEGATION_FD_NONE ) {
		close ( get_delegation_password_pipe[1] ); 
	}

	init();
}

