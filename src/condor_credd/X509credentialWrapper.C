#include "X509credentialWrapper.h"
#include "condor_common.h"

X509CredentialWrapper::X509CredentialWrapper (const classad::ClassAd& classad) : X509Credential (classad) {

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

