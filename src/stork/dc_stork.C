#include "condor_common.h"
#include "condor_config.h"
#include "dc_stork.h"
#include "dap_client_interface.h"

#define WANT_NAMESPACES
#include "classad_distribution.h"

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

	return stork_submit (request,
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
