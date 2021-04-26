#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "gahp-client.h"
#include "classad_collection.h"
#include "stat_wrapper.h"

#include "annex.h"
#include "annex-status.h"
#include "generate-id.h"

#include "Functor.h"
#include "FunctorSequence.h"
#include "GetInstances.h"
#include "CondorStatusReply.h"

extern ClassAd * command;
extern ClassAdCollection * commandState;

int
condor_status(	const char * annexName, const char * sURL,
				int argc, char ** argv, unsigned subCommandIndex ) {
	// This (and the other sequences like it) should probably be handled
	// by annex.cpp, in the argument-handling code.
	std::string serviceURL = sURL ? sURL : "";
	if( serviceURL.empty() ) {
		param( serviceURL, "ANNEX_DEFAULT_EC2_URL" );
	}
	if( serviceURL.empty() ) {
		fprintf( stderr, "No EC2 URL specified on command-line and ANNEX_DEFAULT_EC2_URL is not set or empty in configuration.\n" );
		return 1;
	}



	std::string commandID;
	generateCommandID( commandID );

	std::string publicKeyFile, secretKeyFile;

	param( publicKeyFile, "ANNEX_DEFAULT_ACCESS_KEY_FILE" );
	command->LookupString( "PublicKeyFile", publicKeyFile );

	param( secretKeyFile, "ANNEX_DEFAULT_SECRET_KEY_FILE" );
	command->LookupString( "SecretKeyFile", secretKeyFile );

	if( secretKeyFile != USE_INSTANCE_ROLE_MAGIC_STRING ) {
		StatWrapper sw( secretKeyFile.c_str() );
		mode_t mode = sw.GetBuf()->st_mode;
		if( mode & S_IRWXG || mode & S_IRWXO || getuid() != sw.GetBuf()->st_uid ) {
			fprintf( stderr, "Secret key file must be accessible only by owner.  Please verify that your user owns the file and that the file permissons are restricted to the owner.\n" );
			return 1;
		}
	}
	//
	// The standard incantations...
	ClassAd * reply = new ClassAd();
	ClassAd * scratchpad = new ClassAd();

	EC2GahpClient * ec2Gahp = startOneGahpClient( publicKeyFile, serviceURL );


	scratchpad->Assign( "AnnexID", annexName );

	GetInstances * getInstances = new GetInstances( reply,
		ec2Gahp, scratchpad, serviceURL, publicKeyFile, secretKeyFile,
		commandState, commandID );
	CondorStatusReply * statusReply = new CondorStatusReply( reply, command,
		ec2Gahp, scratchpad,
		commandState, commandID,
		argc, argv, subCommandIndex );

	FunctorSequence * fs = new FunctorSequence(
		{ getInstances }, statusReply,
		commandState, commandID, scratchpad );

	int timer = daemonCore->Register_Timer( 0, TIMER_NEVER,
		(void (Service::*)()) & FunctorSequence::operator(),
		"annex-status", fs );
	ec2Gahp->setNotificationTimerId( timer );


	return 0;
}
