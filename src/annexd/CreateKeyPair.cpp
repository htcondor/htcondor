#include "condor_common.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "CreateKeyPair.h"

#include "condor_config.h"
#include "filename_tools.h"
#include "directory.h"

bool
createUserConfigDir( std::string & directory ) {
	std::string userConfigName;
	MyString userConfigSource;
	param( userConfigName, "USER_CONFIG_FILE" );
	if(! userConfigName.empty()) {
		find_user_file( userConfigSource, userConfigName.c_str(), false );
		if(! userConfigSource.empty()) {
			// Create the containing directory if necessary, and only the
			// containing directory -- don't do anything stupid if the
			// user configuration directory is misconfigured.
			std::string dir, file;
			filename_split( userConfigSource.c_str(), dir, file );
			if(! IsDirectory( dir.c_str() )) {
				mkdir( dir.c_str(), 0755 );
			}

			directory = dir;
			return true;
		} else {
			fprintf( stderr, "Unable to locate your user configuration file.  " );
			return false;
		}
	} else {
		fprintf( stderr, "Your HTCondor installation is configured to ignore user configuration files.  Contact your system administrator.  " );
		return false;
	}
}

int
CreateKeyPair::operator() () {
	dprintf( D_FULLDEBUG, "CreateKeyPair::operator()\n" );
	std::string keyName = "HTCondorAnnex-KeyPair";


	std::string path;
	if(! createUserConfigDir( path )) {
		char * cdn = get_current_dir_name();
		path = cdn;
		free( cdn );

		fprintf( stderr, "Will write private SSH key to '%s', instead.\n", path.c_str() );
	}
	std::string keypairFile = path + "/" + keyName + ".pem";

	std::string errorCode;
	int rc = gahp->ec2_vm_create_keypair(
				service_url, public_key_file, secret_key_file,

				keyName, keypairFile,

				errorCode );

	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	}

	if( rc == 0 ) {
		scratchpad->Assign( "KeyName", keyName );
		scratchpad->Assign( "KeyPath", keypairFile );

		reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
		rc = PASS_STREAM;
	} else {
		std::string message;
		formatstr( message, "Failed to create SSH key pair: '%s' (%d): '%s'.",
			errorCode.c_str(), rc, gahp->getErrorString() );
		dprintf( D_ALWAYS, "%s\n", message.c_str() );

		reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
		reply->Assign( ATTR_ERROR_STRING, message );
		rc = FALSE;
	}

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return rc;
}


int
CreateKeyPair::rollback() {
	dprintf( D_FULLDEBUG, "CreateKeyPair::rollback()\n" );
	std::string keyName = "HTCondorAnnex-KeyPair";

	std::string errorCode;
	int rc = gahp->ec2_vm_destroy_keypair(
				service_url, public_key_file, secret_key_file,

				keyName,

				errorCode );

	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	}

	if( rc != 0 ) {
		dprintf( D_ALWAYS, "Failed to destroy keypair '%s' ('%s').\n", keyName.c_str(), errorCode.c_str() );
	}

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
