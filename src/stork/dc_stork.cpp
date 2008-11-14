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

#include "condor_common.h"
#include "condor_config.h"
#include "dc_stork.h"
#include "dap_client_interface.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"

DCStork::
DCStork (const char * _host) {
	config (0);
	this->error_reason = NULL;
	this->sinful_string = get_stork_sinful_string (_host);
}

DCStork::
~DCStork () {
	if (this->error_reason)
		free (this->error_reason);
	
	if (this->sinful_string)
		free (this->sinful_string);
}

int DCStork::
SubmitJob (const classad::ClassAd * request,
					  const char * cred, 
					  const int cred_size, 
		              char *& id) {
			
	ClearError();

	return stork_submit (
						 NULL,
						 request,
						 this->sinful_string,
						 cred,
						 cred_size,
						 id,
						 this->error_reason);
}

int DCStork::
RemoveJob (const char * id) {

	ClearError();
	
	char * result = NULL;

	int rc= stork_rm (id,
					 this->sinful_string,
					 result,
					  this->error_reason);
	if (result)
		free(result);
	return rc;
}

int DCStork::
ListJobs (int & size, 
		  classad::ClassAd ** & result) {

	ClearError();
	return stork_list (this->sinful_string,
					   size,
					   result,
					   this->error_reason);
}

int DCStork::QueryJob (const char * id, 
	classad::ClassAd * & result) {

	ClearError();

	return stork_status (id,
						 this->sinful_string,
						 result,
						 error_reason);
}
	
const char * 
DCStork::GetLastError() const {
	return this->error_reason;
}

void DCStork::
ClearError() {
	if (this->error_reason)
		free (error_reason);
	error_reason = NULL;
}
