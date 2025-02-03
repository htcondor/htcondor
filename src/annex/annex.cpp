#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include "gahp-client.h"
#include "compat_classad.h"
#include "classad_command_util.h"
#include "classad_collection.h"

#include "match_prefix.h"
#include "daemon.h"
#include "dc_annexd.h"
#include "condor_base64.h"
#include "my_username.h"
#include "shortfile.h"

#include <queue>
#include <iostream>

#include "annex.h"
#include "annex-setup.h"
#include "annex-update.h"
#include "annex-create.h"
#include "annex-status.h"
#include "annex-condor-status.h"
#include "annex-condor-off.h"
#include "user-config-dir.h"

// Why don't c-style timer callbacks have context pointers?
ClassAd * command = NULL;

// Although the annex daemon uses a GAHP, it doesn't have a schedd managing
// its hard state in an existing job ad; it has to do that job on its own.
ClassAdCollection * commandState;

// Required by GahpServer::Startup().
char * GridmanagerScratchDir = NULL;

// Start a GAHP client.  Each GAHP client can only have one request outstanding
// at a time, and it distinguishes between them based only on the name of the
// command.  As a result, we want each annex (or bulk request) to have its own
// GAHP client, to make sure that the requests don't collide.  If two GAHP
// clients shared a 'name' parameter, they will share a GAHP server process;
// to make the RequestLimitExceeded stuff work right, we start a GAHP server
// for each {public key file, service URL} tuple.
EC2GahpClient * startOneGahpClient( const std::string & publicKeyFile, const std::string & /* serviceURL */ ) {
	std::string gahpName;

	// This makes me sad, now that the annexd needs to use three endpoints, so
	// let's ignore the technical limitation about RequestLimitExceeded for
	// now, since we shouldn't encounter it and the only consequence is, I
	// think, backoffs for endpoints that don't need them.  (If this becomes
	// a problem, we should probably change this to configuration knob,
	// because that means we probably want to split the load up among multiple
	// GAHPs anyway.)
	// formatstr( gahpName, "annex-%s@%s", publicKeyFile.c_str(), serviceURL.c_str() );

	// The unfixable technical restriction is actually that all credentials
	// passed to the same GAHP must be accessible by it.  Since the GAHP
	// (will) run as a particular user for precisely this reason (not just
	// isolation but also network filesystems), we could instead name the
	// GAHP after the key into the authorized user map we used.  For now,
	// since people aren't likely to have that many different credentials,
	// just use the name of the credentials.
	formatstr( gahpName, "annex-%s", publicKeyFile.c_str() );

	ArgList args;

	// A GAHP used by the annex daemon is configured as an annex GAHP,
	// regardless of which GAHP it actually is.  This sets the subsystem.
	args.AppendArg( "-s" );
	args.AppendArg( "ANNEX_GAHP" );

	std::string logDirectory;
	param( logDirectory, "LOG" );
	if(! logDirectory.empty()) {
		args.AppendArg( "-l" );
		args.AppendArg( logDirectory.c_str() );
	}

	args.AppendArg( "-w" );
	int minWorkerCount = param_integer( "ANNEX_GAHP_WORKER_MIN_NUM", 1 );
	args.AppendArg( std::to_string(minWorkerCount) );

	args.AppendArg( "-m" );
	int maxWorkerCount = param_integer( "ANNEX_GAHP_WORKER_MAX_NUM", 1 );
	args.AppendArg( std::to_string(maxWorkerCount) );

	args.AppendArg( "-d" );
	char * gahp_debug = param( "ANNEX_GAHP_DEBUG" );
	if( gahp_debug == NULL ) {
		args.AppendArg( "D_ALWAYS" );
	} else {
		args.AppendArg( gahp_debug );
		free( gahp_debug );
	}

	char * gahp_path = param( "ANNEX_GAHP" );
	if( gahp_path == NULL ) {
		EXCEPT( "ANNEX_GAHP must be defined." );
	}

	EC2GahpClient * gahp = new EC2GahpClient( gahpName.c_str(), gahp_path, & args );
	free( gahp_path );

	gahp->setMode( GahpClient::normal );

	int gct = param_integer( "ANNEX_GAHP_CALL_TIMEOUT", 10 * 60 );
	gahp->setTimeout( gct );

	if( gahp->Startup() == false ) {
		EXCEPT( "Failed to start GAHP." );
	}

	return gahp;
}

void
InsertOrUpdateAd( const std::string & id, ClassAd * command,
  ClassAdCollection * log ) {
	// We could call ListNewAdsInTransaction() and check to see if we've
	// already inserted or updated an ad with this id, but this should work
	// in either case, so let's keep things simple for now.
	ClassAd * existing = NULL;
	if( log->LookupClassAd( id, existing ) && existing != NULL ) {
		log->DestroyClassAd( id );
	}

	log->NewClassAd( id, command );
}

bool
validateLease( time_t endOfLease, std::string & validationError ) {
	time_t now = time( NULL );
	if( endOfLease < now ) {
		validationError = "The lease must end in the future.";
		return false;
	}
	return true;
}

std::string commandStateFile;

void
main_config() {
	commandStateFile = param( "ANNEX_COMMAND_STATE" );
}

bool
createConfigTarball(	const char * configDir,
						const char * annexName,
						const char * owner,
						long unclaimedTimeout,
						std::string & tarballPath,
						std::string & tarballError,
						std::string & instanceID ) {
	char * cwd = get_current_dir_name();

	int rv = chdir( configDir );
	if( rv != 0 ) {
		formatstr( tarballError, "unable to change to config dir '%s' (%d): '%s'",
			configDir, errno, strerror( errno ) );
		free(cwd);
		return false;
	}

	// Must be readable by the 'condor' user on the instance, but it
	// will be owned by root.
	int fd = safe_open_wrapper_follow( "00ec2-dynamic.config",
		O_WRONLY | O_CREAT | O_TRUNC, 0644 );
	if( fd < 0 ) {
		formatstr( tarballError, "failed to open config file '%s' for writing: '%s' (%d)",
			"00ec2-dynamic.config", strerror( errno ), errno );
		free(cwd);
		return false;
	}


	std::string passwordFile = "password_file.pl";

	std::string collectorHost;
	param( collectorHost, "COLLECTOR_HOST" );
	if( collectorHost.empty() ) {
		formatstr( tarballError, "COLLECTOR_HOST empty or undefined" );
		free(cwd);
		close(fd);
		return false;
	}

	Daemon collector( DT_COLLECTOR, collectorHost.c_str() );
	if(! collector.locate()) {
		formatstr( tarballError, "unable to find collector defined by COLLECTOR_HOST (%s)", collectorHost.c_str() );
		free(cwd);
		return false;
	}
	if(! collector.getInstanceID( instanceID )) {
		formatstr( tarballError, "unable to get collector's instance ID" );
		free(cwd);
		return false;
	}

	std::string startExpression = "START = MayUseAWS == TRUE\n";
	if( owner != NULL ) {
		formatstr( startExpression,
			"START = (MayUseAWS == TRUE) && stringListMember( Owner, \"%s\" )\n"
			"\n",
			owner );
	}

	std::string contents;
	formatstr( contents,
		"use security:host_based\n"
		"LOCAL_HOSTS = $(FULL_HOSTNAME) $(IP_ADDRESS) 127.0.0.1 $(TCP_FORWARDING_HOST)\n"
		"CONDOR_HOST = condor_pool@*/* $(LOCAL_HOSTS)\n"
		"COLLECTOR_HOST = %s\n"
		"\n"
		"SEC_DEFAULT_AUTHENTICATION = REQUIRED\n"
		"SEC_DEFAULT_AUTHENTICATION_METHODS = FS, PASSWORD\n"
		"SEC_DEFAULT_INTEGRITY = REQUIRED\n"
		"SEC_DEFAULT_ENCRYPTION = REQUIRED\n"
		"\n"
		"SEC_PASSWORD_FILE = /etc/condor/config.d/%s\n"
		"ALLOW_WRITE = condor_pool@*/* $(LOCAL_HOSTS)\n"
		"\n"
		"AnnexName = \"%s\"\n"
		"IsAnnex = TRUE\n"
		"STARTD_ATTRS = $(STARTD_ATTRS) AnnexName IsAnnex\n"
		"MASTER_ATTRS = $(MASTER_ATTRS) AnnexName IsAnnex\n"
		"\n"
		"STARTD_NOCLAIM_SHUTDOWN = %ld\n"
		"\n"
		"%s",
		collectorHost.c_str(), passwordFile.c_str(),
		annexName, unclaimedTimeout, startExpression.c_str()
	);

	rv = write( fd, contents.c_str(), contents.size() );
	if( rv == -1 ) {
		formatstr( tarballError, "error writing to '%s': '%s' (%d)",
			"00ec2-dynamic.config", strerror( errno ), errno );
		free(cwd);
		return false;
	} else if( rv != (int)contents.size() ) {
		formatstr( tarballError, "short write to '%s': '%s' (%d)",
			"00ec2-dynamic.config", strerror( errno ), errno );
		free(cwd);
		return false;
	}
	close( fd );


	std::string localPasswordFile;
	param( localPasswordFile, "SEC_PASSWORD_FILE" );
	if( localPasswordFile.empty() ) {
		formatstr( tarballError, "SEC_PASSWORD_FILE empty or undefined" );
		free(cwd);
		return false;
	}

	fd = open( localPasswordFile.c_str(), O_RDONLY );
	if( fd == -1 ) {
		formatstr( tarballError, "Unable to open SEC_PASSWORD_FILE '%s': %s (%d)",
			localPasswordFile.c_str(), strerror(errno), errno );
		free(cwd);
		return false;
	} else {
		close( fd );
	}

	// FIXME: Rewrite without system().
	std::string cpCommand;
	formatstr( cpCommand, "cp '%s' '%s'", localPasswordFile.c_str(), passwordFile.c_str() );
	int status = system( cpCommand.c_str() );
	if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
		formatstr( tarballError, "failed to copy '%s' to '%s'",
			localPasswordFile.c_str(), passwordFile.c_str() );
		free(cwd);
		return false;
	}


	std::string tbn;
	formatstr( tbn, "%s/temporary.tar.gz.XXXXXX", cwd );
	char * tarballName = strdup( tbn.c_str() );
	// Some version of mkstemp() create files with 0666, instead of 0600,
	// which would be bad.  I claim without checking that we don't compile
	// on platforms whose version of mkstemp() is broken.
	int tfd = mkstemp( tarballName );
	if( tfd == -1 ) {
		formatstr( tarballError, "failed to create temporary filename for tarball" );
		free(cwd);
		free(tarballName);
		return false;
	}
	close( tfd );

	// FIXME: Rewrite without system().
	std::string command;
	formatstr( command, "tar -z -c -f '%s' *", tarballName );
	status = system( command.c_str() );
	if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
		formatstr( tarballError, "failed to create tarball" );
		free(cwd);
		free(tarballName);
		return false;
	}
	tarballPath = tarballName;
	free( tarballName );

	rv = chdir( cwd );
	if( rv != 0 ) {
		formatstr( tarballError, "unable to change back to working dir '%s' (%d): '%s'",
			cwd, errno, strerror( errno ) );
		free(cwd);
		return false;
	}
	free( cwd );

	return true;
}

// Modelled on BulkRequest::validateAndStore() from annexd/BulkRequest.cpp.
bool
assignUserData( ClassAd & command, const char * ud, bool replace, std::string & validationError ) {
	ExprTree * launchConfigurationsTree = command.Lookup( "LaunchSpecifications" );
	if(! launchConfigurationsTree) {
		validationError = "Attribute 'LaunchSpecifications' missing.";
		return false;
	}
	classad::ExprList * launchConfigurations = dynamic_cast<classad::ExprList *>( launchConfigurationsTree );
	if(! launchConfigurations) {
		validationError = "Attribute 'LaunchSpecifications' not a list.";
		return false;
	}

	auto lcIterator = launchConfigurations->begin();
	for( ; lcIterator != launchConfigurations->end(); ++lcIterator ) {
		classad::ClassAd * ca = dynamic_cast<classad::ClassAd *>( * lcIterator );
		if( ca == NULL ) {
			validationError = "'LaunchSpecifications[x]' not a ClassAd.";
			return false;
		}
		ClassAd launchConfiguration( * ca );

		std::string userData;
		launchConfiguration.LookupString( "UserData", userData );
		if( userData.empty() || replace ) {
			ca->InsertAttr( "UserData", ud );
		}
	}

	return true;
}

void
help( const char * argv0 ) {
	fprintf( stdout, "To create an annex:\n"
		"%s -annex-name <annex-name> -count|-slots <number>\n"
		"\n"
		"For on-demand instances:\n"
		"\t[-aws-on-demand]\n"
		"\t-count <integer number of instances>\n"
		"\n"
		"For spot fleet instances:\n"
		"\t[-aws-spot-fleet]\n"
		"\t-slots <integer number of weighted capacity units>\n"
		"\n"
		"To set the lease or idle durations:\n"
		"\t[-duration <lease duration in decimal hours>]\n"
		"\t[-idle <idle duration in decimal hours>]\n"
		"\n"
		"To start the annex automatically without a yes/no approval prompt:\n"
		"\t[-yes]"
		"To customize the annex's HTCondor configuration:\n"
		"\t[-config-dir </full/path/to/config.d>]\n"
		"\n"
		"To tag the instances (repeat -tag for each tag):\n"
		"\t[-tag tag-name tag-value]\n"
		"\n"
		"To set the instances' user data:\n"
		"\t[-aws-[default-]user-data[-file] <data|/full/path/to/file> ]\n"
		"\n"
		"To customize an AWS on-demand annex:\n"
		"\t[-aws-on-demand-instance-type <instance-type>]\n"
		"\t[-aws-on-demand-ami-id <AMI-ID>]\n"
		"\t[-aws-on-demand-security-group-ids <group-ID[,groupID]*>]\n"
		"\t[-aws-on-demand-key-name <key-name>]\n"
		"\n"
		"To customize an AWS Spot Fleet annex, specify a JSON config file:\n"
		"\t[-aws-spot-fleet-config-file </full/path/to/config.json>]\n"
		"\n"
		"To specify who may use your annex:\n"
		"\t[-owner <username[, username]*>]\n"
		"To specify that anyone may use your annex:\n"
		"\t[-no-owner]\n"
		"\n"
		"To specify the AWS region in which to create your annex:\n"
		"\t[-aws-region <region>]\n"
		"\n"
		"To reprint this help:\n"
		"\t[-help]\n"
		"\n"
		"Expert mode (specify account):\n"
		"\t[-aws-access-key-file </full/path/to/access-key-file>]\n"
		"\t[-aws-secret-key-file </full/path/to/secret-key-file>]\n"
		"\t[-aws-ec2-url https://ec2.<region>.amazonaws.com]\n"
		"\t[-aws-events-url https://events.<region>.amazonaws.com]\n"
		"\t[-aws-lambda-url https://lambda.<region>.amazonaws.com]\n"
		"\t[-aws-s3-url https://s3.<region>.amazonaws.com]\n"
		"\n"
		"Expert mode (specify implementation details):\n"
		"\t[-aws-spot-fleet-lease-function-arn <sfr-lease-function-arn>]\n"
		"\t[-aws-on-demand-lease-function-arn <odi-lease-function-arn>]\n"
		"\t[-aws-on-demand-instance-profile-arn <instance-profile-arn>]\n"
		"\n"
		"OR, to do the one-time setup for an AWS region:\n"
		"%s [-aws-region <region>] -setup [</full/path/to/access-key-file> </full/path/to/secret-key-file> [<CloudFormation URL>]]\n"
		"... if you're on an EC2 instance (with a privileged IAM role):\n"
		"%s [-aws-region <region>] -setup FROM INSTANCE [CloudFormation URL]\n"
		"\n"
		"OR, to check if the one-time setup has been done:\n"
		"%s [-aws-region <region>] -check-setup\n"
		"\n"
		"OR, to check the status of your annex(es) in a region:\n"
		"%s [-aws-region <region>] -status [-annex[-name] <annex-name>] [-classad[s]]\n"
		"\n"
		"OR, to reset the lease on an existing annex:\n"
		"%s [-aws-region <region>] -annex[-name] <annex-name> -duration <lease duration in decimal hours>\n"
		"\n"
		, argv0, argv0, argv0, argv0, argv0, argv0 );
}


// Part of emulating the original client-server architecture.
void prepareCommandAd( const ClassAd & commandArguments, int commandInt ) {
	command = new ClassAd( commandArguments );
	command->Assign( ATTR_COMMAND, getCommandString( commandInt ) );
	command->Assign( "RequestVersion", 1 );

	// We don't trust the client the supply the command ID or annex ID.
	command->AssignExpr( "CommandID", "Undefined" );
	command->AssignExpr( "AnnexID", "Undefined" );
}

int create_annex( ClassAd & commandArguments ) {
	prepareCommandAd( commandArguments, CA_BULK_REQUEST );
	daemonCore->Register_Timer( 0, TIMER_NEVER,
		& callCreateOneAnnex, "callCreateOneAnnex()" );
	return 0;
}

int update_annex( ClassAd & commandArguments ) {
	// In the original client-server architecture, this would have been
	// CA_BULK_UPDATE, but since we're doing our own dispatch, don't bother.
	prepareCommandAd( commandArguments, CA_BULK_REQUEST );
	daemonCore->Register_Timer( 0, TIMER_NEVER,
		& callUpdateOneAnnex, "callUpdateOneAnnex()" );
	return 0;
}

void prepareTarballForUpload(	ClassAd & commandArguments,
								const char * configDir,
								char * owner, bool ownerSpecified,
								bool noOwner,
								long int unclaimedTimeout ) {
	std::string annexName;
	commandArguments.LookupString( ATTR_ANNEX_NAME, annexName );
	ASSERT(! annexName.empty());

	// Create a temporary directory.  If a config directory was specified,
	// copy its contents into the temporary directory.  Create (or copy)
	// the dynamic configuration files into the temporary directory.  Create
	// a tarball of the temporary directory's contents in the cwd.  Delete
	// the temporary directory (and all of its contents).  While the tool
	// and the daemon are joined at the hip, just delete the tarball after
	// uploading it; otherwise, we'll probably do file transfer and have
	// the tool and the daemon clean up after themselves.
	char dirNameTemplate[] = ".condor_annex.XXXXXX";
	const char * tempDir = mkdtemp( dirNameTemplate );
	if( tempDir == NULL ) {
		fprintf( stderr, "Failed to create temporary directory (%d): '%s'.\n", errno, strerror( errno ) );
		exit( 2 );
	}

	// FIXME: Rewrite without system().
	if( configDir != NULL ) {
		std::string command;
		formatstr( command, "cp -a '%s'/* '%s'", configDir, tempDir );
		int status = system( command.c_str() );
		if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
			fprintf( stderr, "Failed to copy '%s' to '%s', aborting.\n",
				configDir, tempDir );
			exit( 2 );
		}
	}

	//
	// A job's Owner attribute is almost aways set by condor_submit to be
	// the return of my_username(), and this value is checked by the schedd.
	//
	// However, if the user submitted their jobs with +Owner = undefined,
	// then what the mapfile says goes.  Rather than have this variable
	// be configurable (because we may end up where condor_annex doesn't
	// have per-user configuration), just require users in this rare
	// situation to run condor_ping -type schedd WRITE and use the
	// answer in a config directory (or command-line argument, if that
	// becomes a thing) for setting the START expression.
	//

	std::string tarballPath, tarballError, instanceID;
	if(! ownerSpecified) { owner = my_username(); }
	bool createdTarball = createConfigTarball( tempDir, annexName.c_str(),
		noOwner ? NULL : owner, unclaimedTimeout, tarballPath, tarballError,
		instanceID );
	if(! ownerSpecified) { free( owner ); }
	commandArguments.Assign( "CollectorInstanceID", instanceID );

	// FIXME: Rewrite without system().
	std::string cmd;
	formatstr( cmd, "rm -fr '%s'", tempDir );
	int status = system( cmd.c_str() );
	if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
		fprintf( stderr, "Failed to delete temporary directory '%s'.\n",
			tempDir );
		exit( 2 );
	}

	if(! createdTarball) {
		fprintf( stderr, "Failed to create config tarball (%s); aborting.\n", tarballError.c_str() );
		exit( 3 );
	}

	commandArguments.Assign( "UploadFrom", tarballPath );
}

void handleUserData(	ClassAd & commandArguments,
						bool clUserDataWins, std::string & userData,
						const char * userDataFileName ) {
	if( userDataFileName != NULL ) {
		if(! htcondor::readShortFile( userDataFileName, userData ) ) {
			exit( 4 );
		}
	}

	if(! userData.empty()) {
		std::string validationError;
		char * base64Encoded = condor_base64_encode( (const unsigned char *)userData.c_str(), userData.length(), false );
		if(! assignUserData( commandArguments, base64Encoded, clUserDataWins, validationError )) {
			fprintf( stderr, "Failed to set user data in request ad (%s).\n", validationError.c_str() );
			exit( 5 );
		}
		free( base64Encoded );
	}
}

bool saidYes( const std::string & response ) {
	if( strcasecmp( response.c_str(), "yes" ) == 0 ) { return true; }
	if( strcasecmp( response.c_str(), "y" ) == 0 ) { return true; }
	return false;
}

void getSFRApproval(	ClassAd & commandArguments, const char * sfrConfigFile,
						bool leaseDurationSpecified, bool unclaimedTimeoutSpecified,
						long int count, long int leaseDuration, long int unclaimedTimeout, bool skipYesNoPrompt ) {
	FILE * file = NULL;
	bool closeFile = true;
	if( strcmp( sfrConfigFile, "-" ) == 0 ) {
		file = stdin;
		closeFile = false;
	} else {
		file = safe_fopen_wrapper_follow( sfrConfigFile, "r" );
		if( file == NULL ) {
			fprintf( stderr, "Unable to open annex specification file '%s'.\n", sfrConfigFile );
			exit( 1 );
		}
	}

	CondorClassAdFileIterator ccafi;
	if(! ccafi.begin( file, closeFile, CondorClassAdFileParseHelper::Parse_json )) {
		fprintf( stderr, "Failed to start parsing spot fleet request.\n" );
		exit( 2 );
	} else {
		ClassAd spotFleetRequest;
		int numAttrs = ccafi.next( spotFleetRequest );
		if( numAttrs <= 0 ) {
			fprintf( stderr, "Failed to parse spot fleet request, found no attributes.\n" );
			exit( 2 );
		} else if( numAttrs > 11 ) {
			fprintf( stderr, "Failed to parse spot fleet request, found too many attributes.\n" );
			exit( 2 );
		}

		// The command-line arguments beat the json file.
		spotFleetRequest.Update( commandArguments );
		commandArguments = spotFleetRequest;
	}

	std::string response;

	fprintf( stdout,
		"Will request %ld spot slot%s for %.2f hours.  "
		"Each instance will terminate after being idle for %.2f hours.\n",
		count,
		count == 1 ? "" : "s",
		leaseDuration / 3600.0,
		unclaimedTimeout / 3600.0
	);

	if(! skipYesNoPrompt) {
		fprintf( stdout, "Is that OK?  (Type 'yes' or 'no'): " );
		std::getline( std::cin, response );
	}
	else {
		response = "yes";
	}

	if(! saidYes( response )) {
		fprintf( stdout, "Not starting an annex; user did not confirm.\n\n" );

		if(! leaseDurationSpecified) {
			fprintf( stdout, "To change the lease duration, use the -duration flag.\n" );
		}
		if(! unclaimedTimeoutSpecified) {
			fprintf( stdout, "To change how long an idle instance will wait before terminating itself, use the -idle flag.\n" );
		}

		exit( 1 );
	} else {
		fprintf( stdout, "Starting annex...\n" );
		commandArguments.Assign( "ExpectedDelay", "  It will take about three minutes for the new machines to join the pool." );
	}
}

void getODIApproval(	ClassAd & commandArguments,
						const char * odiInstanceType, bool odiInstanceTypeSpecified,
						bool leaseDurationSpecified, bool unclaimedTimeoutSpecified,
						long int count, long int leaseDuration, long int unclaimedTimeout, bool skipYesNoPrompt ) {
	fprintf( stdout,
		"Will request %ld %s on-demand instance%s for %.2f hours.  "
		"Each instance will terminate after being idle for %.2f hours.\n",
		count, odiInstanceType, count == 1 ? "" : "s",
		leaseDuration / 3600.0, unclaimedTimeout / 3600.0
	);

	std::string response;
	if(! skipYesNoPrompt) {
		fprintf( stdout, "Is that OK?  (Type 'yes' or 'no'): " );
		std::getline( std::cin, response );
	}
	else {
		response = "yes";
	}

	if(! saidYes( response )) {
		fprintf( stdout, "Not starting an annex; user did not confirm.\n\n" );

		if(! odiInstanceTypeSpecified) {
			fprintf( stdout, "To change the instance type, use the -aws-on-demand-instance-type flag.\n" );
		}

		if(! leaseDurationSpecified) {
			fprintf( stdout, "To change the lease duration, use the -duration flag.\n" );
		}
		if(! unclaimedTimeoutSpecified) {
			fprintf( stdout, "To change how long an idle instance will wait before terminating itself, use the -idle flag.\n" );
		}

		exit( 1 );
	} else {
		fprintf( stdout, "Starting annex...\n" );
		commandArguments.Assign( "ExpectedDelay", "  It will take about three minutes for the new machines to join the pool." );
	}
}

void assignWithDefault(	ClassAd & commandArguments, const char * & value,
						const char * paramName, const char * argName ) {
	if(! value) {
		value = param( paramName );
	}
	if( value ) {
		commandArguments.Assign( argName, value );
	}
}

void dumpParam( const char * attribute ) {
	std::string value;
	param( value, attribute );
	dprintf( D_AUDIT | D_NOHEADER, "%s = %s\n", attribute, value.c_str() );
}

void dumpParam( const char * attribute, int defaultValue ) {
	int value = param_integer( attribute, defaultValue );
	dprintf( D_AUDIT | D_NOHEADER, "%s = %d\n", attribute, value );
}

int _argc;
char ** _argv;

int
annex_main( int argc, char ** argv ) {
	//
	// Parsing.
	//

	argc = _argc;
	argv = _argv;

	bool wantClassAds = false;
	int udSpecifications = 0;
	const char * sfrConfigFile = NULL;
	const char * annexName = NULL;
	const char * configDir = NULL;
	const char * region = NULL;
	const char * serviceURL = NULL;
	const char * eventsURL = NULL;
	const char * lambdaURL = NULL;
	const char * s3URL = NULL;
	const char * publicKeyFile = NULL;
	const char * secretKeyFile = NULL;
	const char * cloudFormationURL = NULL;
	const char * sfrLeaseFunctionARN = NULL;
	const char * odiLeaseFunctionARN = NULL;
	const char * odiInstanceType = NULL;
	bool odiInstanceTypeSpecified = false;
	const char * odiImageID = NULL;
	const char * odiInstanceProfileARN = NULL;
	const char * s3ConfigPath = NULL;
	const char * odiKeyName = NULL;
	const char * odiSecurityGroupIDs = NULL;
	bool clUserDataWins = true;
	std::string userData;
	const char * userDataFileName = NULL;
	std::vector< std::pair< std::string, std::string > > tags;
	bool skipYesNoPrompt = false;

	enum annex_t {
		at_none = 0,
		at_sfr = 1,
		at_odi = 2
	};
	annex_t annexType = at_none;

	unsigned subCommandIndex = 0;
	long int unclaimedTimeout = 0;
	bool unclaimedTimeoutSpecified = false;
	long int leaseDuration = 0;
	bool leaseDurationSpecified = false;
	long int count = 0;
	char * owner = NULL;
	bool ownerSpecified = false;
	bool noOwner = false;
	enum command_t {
		ct_setup = 2,
		ct_status = 5,
		ct_default = 0,
		ct_condor_off = 7,
		ct_check_setup = 3,
		ct_create_annex = 1,
		ct_update_annex = 4,
		ct_condor_status = 6
	};
	command_t theCommand = ct_default;
	for( int i = 1; i < argc; ++i ) {
		if( strcmp( argv[i], "status" ) == 0 ) {
			theCommand = ct_condor_status;
			subCommandIndex = i;
			break;
		} else if( strcmp( argv[i], "off" ) == 0 ) {
			theCommand = ct_condor_off;
			subCommandIndex = i;
			break;
		} else if( is_dash_arg_prefix( argv[i], "aws-region", 10 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				region = argv[i];
			} else {
				fprintf( stderr, "%s: -aws-region requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-ec2-url", 11 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				serviceURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-ec2-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-events-url", 14 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				eventsURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-events-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-lambda-url", 14 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				lambdaURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-lambda-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-s3-url", 10 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				s3URL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-s3-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-user-data-file", 14 ) ) {
			++i; ++udSpecifications;
			if( i < argc && argv[i] != NULL ) {
				userDataFileName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-user-data-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-user-data", 13 ) ) {
			++i; ++udSpecifications;
			if( i < argc && argv[i] != NULL ) {
				userData = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-user-data requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-default-user-data-file", 22 ) ) {
			++i; ++udSpecifications;
			if( i < argc && argv[i] != NULL ) {
				clUserDataWins = false;
				userDataFileName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-user-data-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-default-user-data", 21 ) ) {
			++i; ++udSpecifications;
			if( i < argc && argv[i] != NULL ) {
				clUserDataWins = false;
				userData = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-user-data requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "annex-name", 5 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				annexName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -annex-name requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "config-dir", 10 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				configDir = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -config-dir requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "owner", 5 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				owner = argv[i];
				ownerSpecified = true;
				continue;
			} else {
				fprintf( stderr, "%s: -owner requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "no-owner", 7 ) ) {
			++i;
			noOwner = true;
		} else if( is_dash_arg_prefix( argv[i], "aws-spot-fleet-config-file", 22 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				sfrConfigFile = argv[i];
				if( annexType == at_none ) {
					annexType = at_sfr;
				}
				if( annexType == at_odi ) {
					fprintf( stderr, "%s: unwilling to ignore -aws-spot-fleet-config-file being set for an on-demand annex.\n", argv[0] );
					return 1;
				}
				continue;
			} else {
				fprintf( stderr, "%s: -aws-spot-fleet-config-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-public-key-file", 19 ) ||
				   is_dash_arg_prefix( argv[i], "aws-access-key-file", 19 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				publicKeyFile = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-access-key-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-secret-key-file", 19 ) ||
				   is_dash_arg_prefix( argv[i], "aws-private-key-file", 20 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				secretKeyFile = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-secret-key-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-spot-fleet-lease-function-arn", 22 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				sfrLeaseFunctionARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-spot-fleet-lease-function-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-lease-function-arn", 21 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				odiLeaseFunctionARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-lease-function-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-instance-type", 24 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				odiInstanceType = argv[i];
				odiInstanceTypeSpecified = true;
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-instance-type requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-key-name", 17 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				odiKeyName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-key-name requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-security-group-ids", 31 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				odiSecurityGroupIDs = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-security-group-ids requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-ami-id", 20 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				odiImageID = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-ami-id requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand-instance-profile-arn", 22 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				odiInstanceProfileARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-on-demand-instance-profile-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-s3-config-path", 18 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				s3ConfigPath = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -aws-s3-config-path requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "tag", 3 ) ) {
			++i;
			char * tagName = NULL, * tagValue = NULL;
			if( i < argc && argv[i] != NULL ) {
				tagName = argv[i];

				++i;
				if( i < argc && argv[i] != NULL ) {
					tagValue = argv[i];
				}
			}
			if( tagName == NULL || tagValue == NULL ) {
				fprintf( stderr, "%s: -tag requires two arguments.\n", argv[0] );
				return 1;
			} else {
				tags.push_back( std::make_pair( tagName, tagValue ) );
				continue;
			}
		} else if( is_dash_arg_prefix( argv[i], "duration", 8 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ld = argv[i];
				double fractionHours = strtod( ld, & endptr );
				leaseDuration = fractionHours * 60 * 60;
				leaseDurationSpecified = true;
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -duration accepts a decimal number of hours.\n", argv[0] );
					return 1;
				}
				if( leaseDuration <= 0 ) {
					fprintf( stderr, "%s: the duration must be greater than zero.\n", argv[0] );
					return 1;
				}
				continue;
			} else {
				fprintf( stderr, "%s: -duration requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "idle", 4 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ut = argv[i];
				double fractionalHours = strtod( ut, & endptr );
				unclaimedTimeout = fractionalHours * 60 * 60;
				unclaimedTimeoutSpecified = true;
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -idle accepts a decimal number of hours.\n", argv[0] );
					return 1;
				}
				if( unclaimedTimeout <= 0 ) {
					fprintf( stderr, "%s: the idle time must be greater than zero.\n", argv[0] );
					return 1;
				}
				continue;
			} else {
				fprintf( stderr, "%s: -idle requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "slots", 5 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ld = argv[i];
				count = strtol( ld, & endptr, 0 );
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -slots requires an integer argument.\n", argv[0] );
					return 1;
				}
				if( count <= 0 ) {
					fprintf( stderr, "%s: the number of requested slots must be greater than zero.\n", argv[0] );
					return 1;
				}
				if( annexType == at_none ) {
					annexType = at_sfr;
				}
				if( annexType == at_odi ) {
					fprintf( stderr, "%s: you must specify -count for on-demand annexes.\n", argv[0] );
					return 1;
				}
				continue;
			} else {
				fprintf( stderr, "%s: -slots requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "count", 5 ) ) {
			++i;
			if( i < argc && argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ld = argv[i];
				count = strtol( ld, & endptr, 0 );
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -count requires an integer argument.\n", argv[0] );
					return 1;
				}
				if( count <= 0 ) {
					fprintf( stderr, "%s: the number of requested instances must be greater than zero.\n", argv[0] );
					return 1;
				}
				if( annexType == at_none ) {
					annexType = at_odi;
				}
				if( annexType == at_sfr ) {
					fprintf( stderr, "%s: you must specify -slots for a Spot Fleet annex.\n", argv[0] );
					return 1;
				}
				continue;
			} else {
				fprintf( stderr, "%s: -count requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "aws-spot-fleet", 14 ) ) {
			if( annexType == at_none ) {
				annexType = at_sfr;
			}
			if( annexType == at_odi ) {
				fprintf( stderr, "%s: -aws-spot-fleet conflicts with another command-line option, aborting.\n", argv[0] );
				return 1;
			}
			continue;
		} else if( is_dash_arg_prefix( argv[i], "aws-on-demand", 13 ) ) {
			if( annexType == at_none ) {
				annexType = at_odi;
			}
			if( annexType == at_sfr ) {
				fprintf( stderr, "%s: -aws-on-demand conflicts with another command-line option, aborting.\n", argv[0] );
				return 1;
			}
			continue;
		} else if( is_dash_arg_prefix( argv[i], "help", 1 ) ) {
			help( argv[0] );
			return 1;
		} else if( is_dash_arg_prefix( argv[i], "check-setup", 11 ) ) {
			theCommand = ct_check_setup;
		} else if( is_dash_arg_prefix( argv[i], "setup", 5 ) ) {
			theCommand = ct_setup;
			++i; if( i < argc ) { publicKeyFile = argv[i]; }
			++i; if( i < argc ) { secretKeyFile = argv[i]; }
			++i; if( i < argc ) { cloudFormationURL = argv[i]; }
		} else if( is_dash_arg_prefix( argv[i], "status", 6 ) ) {
			theCommand = ct_status;
		} else if( is_dash_arg_prefix( argv[i], "classads", 7 ) ) {
			wantClassAds = true;
		} else if( is_dash_arg_prefix( argv[i], "yes", 3 ) ) {
			skipYesNoPrompt = true;
		} else if( argv[i][0] == '-' && argv[i][1] != '\0' ) {
			fprintf( stderr, "%s: unrecognized option (%s).\n", argv[0], argv[i] );
			return 1;
		}
	}

	//
	// Validation.
	//

	if( udSpecifications > 1 ) {
		fprintf( stderr, "%s: you may specify no more than one of -aws-[default-]user-data[-file].\n", argv[0] );
		return 1;
	}

	//
	// Set attributes common to all commands.
	//
	ClassAd commandArguments;

	if( publicKeyFile != NULL ) {
		commandArguments.Assign( "PublicKeyFile", publicKeyFile );
	}

	if( secretKeyFile != NULL ) {
		commandArguments.Assign( "SecretKeyFile", secretKeyFile );
	}

	std::string defaultRegion;
	if(! region) {
		param( defaultRegion, "ANNEX_DEFAULT_AWS_REGION" );
		if(! defaultRegion.empty()) {
			region = defaultRegion.c_str();
		}
	}

	if( region ) {
		char *safeRegion(strdup(region));

		for( unsigned i = 0; i < strlen( region ); ++i ) {
			if( ('a' <= region[i] && region[i] <= 'z') ||
			    ('A' <= region[i] && region[i] <= 'Z') ||
			    ('0' <= region[i] && region[i] <= '9') ||
			    strchr( "_./", region[i] ) != NULL )
			{
				continue;
			} else {
				safeRegion[i] = '_';
			}
		}
		get_mySubSystem()->setLocalName( safeRegion );
		free(safeRegion);

		std::string buffer;

		formatstr( buffer, "https://ec2.%s.amazonaws.com", region );
		serviceURL = strdup( buffer.c_str() );
		assert( serviceURL != NULL );

		formatstr( buffer, "https://s3.%s.amazonaws.com", region );
		s3URL = strdup( buffer.c_str() );
		assert( s3URL != NULL );

		formatstr( buffer, "https://events.%s.amazonaws.com", region );
		eventsURL = strdup( buffer.c_str() );
		assert( eventsURL != NULL );

		formatstr( buffer, "https://lambda.%s.amazonaws.com", region );
		lambdaURL = strdup( buffer.c_str() );
		assert( lambdaURL != NULL );
	}

	std::string sURLy;
	if( serviceURL != NULL ) {
		sURLy = serviceURL;
		commandArguments.Assign( "ServiceURL", serviceURL );
		if( region ) { free( const_cast<char *>(serviceURL) ); }
	}

	if( eventsURL != NULL ) {
		commandArguments.Assign( "EventsURL", eventsURL );
		if( region ) { free( const_cast<char *>(eventsURL) ); }
	}

	if( lambdaURL != NULL ) {
		commandArguments.Assign( "LambdaURL", lambdaURL );
		if( region ) { free( const_cast<char *>(lambdaURL) ); }
	}

	if( s3URL != NULL ) {
		commandArguments.Assign( "S3URL", s3URL );
		if( region ) { free( const_cast<char *>(s3URL) ); }
	}

	if( leaseDuration == 0 ) {
		leaseDuration = param_integer( "ANNEX_DEFAULT_LEASE_DURATION", 3000 );
	}

	if( unclaimedTimeout == 0 ) {
		unclaimedTimeout = param_integer( "ANNEX_DEFAULT_UNCLAIMED_TIMEOUT", 900 );
	}

	commandArguments.Assign( "TargetCapacity", count );

	time_t now = time( NULL );
	commandArguments.Assign( "EndOfLease", now + leaseDuration );

	//
	// Determine the command, then do command-specific things.
	//
	if( annexType == at_none ) {
		annexType = at_odi;
	}

	if( theCommand == ct_default ) {
		theCommand = ct_create_annex;

		if( count == 0 ) {
			if( leaseDurationSpecified ) {
				if(! unclaimedTimeoutSpecified) {
					theCommand = ct_update_annex;
				} else {
					fprintf( stderr, "You may not change the idle duration of existing instances.  If you'd like to add new instances with a particular idle duration, you must specify %s.\n",
						annexType == at_odi ? "-count" : "-slots" );
					return 1;
				}
			} else {
				fprintf( stderr, "To change the lease duration of an existing annex, use -duration.  To start a new annex, use %s.\n",
					annexType == at_odi ? "-count" : "-slots" );
				return 1;
			}
		}
	}

	switch( theCommand ) {
		case ct_create_annex:
		case ct_update_annex:
			if( annexName == NULL ) {
				fprintf( stderr, "%s: you must specify -annex-name.\n", argv[0] );
				return 1;
			}
			commandArguments.Assign( ATTR_ANNEX_NAME, annexName );
			break;

		default:
			break;
	}

	switch( theCommand ) {
		case ct_create_annex:
		case ct_update_annex: {
			std::string tagNames;
			std::string attrName;
			for( unsigned i = 0; i < tags.size(); ++i ) {
				if (!tagNames.empty()) tagNames += ',';
				tagNames += tags[i].first;
				formatstr( attrName, "%s%s", ATTR_EC2_TAG_PREFIX, tags[i].first.c_str() );
				commandArguments.Assign( attrName, tags[i].second );
			}

			commandArguments.Assign( ATTR_EC2_TAG_NAMES, tagNames );
			} break;

		default:
			break;
	}

	switch( theCommand ) {
		case ct_create_annex:
			// Validate the spot fleet request config file.
			if( annexType == at_sfr && sfrConfigFile == NULL ) {
				sfrConfigFile = param( "ANNEX_DEFAULT_SFR_CONFIG_FILE" );
				if( sfrConfigFile == NULL ) {
					fprintf( stderr, "Spot Fleet Requests require the -aws-spot-fleet-config-file flag.\n" );
					return 1;
				}
			}

			// Determine where to upload the config tarball.
			{ std::string tarballTarget;
			param( tarballTarget, "ANNEX_DEFAULT_S3_BUCKET" );
			if( s3ConfigPath != NULL ) {
				tarballTarget = s3ConfigPath;
			} else {
				formatstr( tarballTarget, "%s/config-%s.tar.gz",
				tarballTarget.c_str(), annexName );
			}
			if( tarballTarget.empty() ) {
				fprintf( stderr, "If you don't specify -aws-s3-config-path, ANNEX_DEFAULT_S3_BUCKET must be set in configuration.\n" );
				return 1;
			}
			commandArguments.Assign( "UploadTo", tarballTarget ); }

			// Deliberate fall-through to common code.
            //@fallthrough@

		case ct_update_annex:
			// Set AnnexType and LeaseFunctionARN.
			if( annexType == at_sfr ) {
				commandArguments.Assign( "AnnexType", "sfr" );
				if(! sfrLeaseFunctionARN) {
					sfrLeaseFunctionARN = param( "ANNEX_DEFAULT_SFR_LEASE_FUNCTION_ARN" );
				}
			commandArguments.Assign( "LeaseFunctionARN", sfrLeaseFunctionARN );
			}

			if( annexType == at_odi ) {
				commandArguments.Assign( "AnnexType", "odi" );
				if(! odiLeaseFunctionARN) {
					odiLeaseFunctionARN = param( "ANNEX_DEFAULT_ODI_LEASE_FUNCTION_ARN" );
				}
			commandArguments.Assign( "LeaseFunctionARN", odiLeaseFunctionARN );
			}
			break;

		default:
			break;
	}

	std::string arguments = argv[0]; arguments += " ";
	for( int i = 1; i < argc; ++i ) {
		formatstr( arguments, "%s%s ", arguments.c_str(), argv[i] );
	}
	arguments.erase( arguments.length() - 1 );
	dprintf( D_AUDIT | D_IDENT | D_PID, getuid(), "%s\n", arguments.c_str() );

	// Dump the relevant param table entires, if anyone's interested.
	if( IsDebugCatAndVerbosity( (D_AUDIT | D_VERBOSE) ) ) {
		dumpParam( "ANNEX_PROVISIONING_DELAY", 5 * 60 );
		dumpParam( "COLLECTOR_HOST" );
		dumpParam( "USER_CONFIG_FILE" );
		dumpParam( "ANNEX_DEFAULT_AWS_REGION" );
		dumpParam( "ANNEX_DEFAULT_EC2_URL" );
		dumpParam( "ANNEX_DEFAULT_CWE_URL" );
		dumpParam( "ANNEX_DEFAULT_LAMBDA_URL" );
		dumpParam( "ANNEX_DEFAULT_S3_URL" );
		dumpParam( "ANNEX_DEFAULT_CF_URL" );
		dumpParam( "ANNEX_DEFAULT_CWE_URL" );
		dumpParam( "ANNEX_DEFAULT_ACCESS_KEY_FILE" );
		dumpParam( "ANNEX_DEFAULT_SECRET_KEY_FILE" );
		dumpParam( "ANNEX_DEFAULT_CONNECTIVITY_FUNCTION_ARN" );
		dumpParam( "ANNEX_GAHP_WORKER_MIN_NUM", 1 );
		dumpParam( "ANNEX_GAHP_WORKER_MAX_NUM", 1 );
		dumpParam( "ANNEX_GAHP_DEBUG");
		dumpParam( "ANNEX_GAHP" );
		dumpParam( "ANNEX_GAHP_CALL_TIMEOUT", 10 * 60 );
		dumpParam( "ANNEX_COMMAND_STATE" );
		dumpParam( "SEC_PASSWORD_FILE" );
		dumpParam( "ANNEX_DEFAULT_LEASE_DURATION", 3000 );
		dumpParam( "ANNEX_DEFAULT_UNCLAIMED_TIMEOUT", 900 );
		dumpParam( "ANNEX_DEFAULT_SFR_CONFIG_FILE" );
		dumpParam( "ANNEX_DEFAULT_S3_BUCKET" );
		dumpParam( "ANNEX_DEFAULT_SFR_LEASE_FUNCTION_ARN" );
		dumpParam( "ANNEX_DEFAULT_ODI_LEASE_FUNCTION_ARN" );
	}

	switch( theCommand ) {
		case ct_condor_off:
			return condor_off( annexName, argc, argv, subCommandIndex );

		case ct_condor_status:
			// Obviously the wrong command int, but it no longer matters.
			prepareCommandAd( commandArguments, CA_BULK_REQUEST );
			return condor_status( annexName, sURLy.c_str(),
				argc, argv, subCommandIndex );

		case ct_status:
			// Obviously the wrong command int, but it no longer matters.
			prepareCommandAd( commandArguments, CA_BULK_REQUEST );
			return status( annexName, wantClassAds, sURLy.c_str() );

		case ct_setup: {
			std::string cfURL;
			if( cloudFormationURL != NULL ) {
				cfURL = cloudFormationURL;
			} else {
				fprintf( stdout, "Will do setup in region '%s'.\n", region );
				formatstr( cfURL, "https://cloudformation.%s.amazonaws.com", region );
			}

			return setup( region, publicKeyFile, secretKeyFile, cfURL.c_str(), sURLy.c_str() );
		}

		case ct_check_setup: {
			std::string cfURL;
			if( cloudFormationURL != NULL ) {
				cfURL = cloudFormationURL;
			} else {
				fprintf( stdout, "Will check setup in region '%s'.\n", region );
				formatstr( cfURL, "https://cloudformation.%s.amazonaws.com", region );
			}

			return check_setup( cfURL.c_str(), sURLy.c_str() );
		}

		case ct_create_annex:
			switch( annexType ) {
				case at_sfr:
					getSFRApproval(	commandArguments, sfrConfigFile,
									leaseDurationSpecified, unclaimedTimeoutSpecified,
									count, leaseDuration, unclaimedTimeout, skipYesNoPrompt );
					break;

				case at_odi:
					assignWithDefault( commandArguments, odiInstanceType,
						"ANNEX_DEFAULT_ODI_INSTANCE_TYPE", "InstanceType" );
					assignWithDefault( commandArguments, odiImageID,
						"ANNEX_DEFAULT_ODI_IMAGE_ID", "ImageID" );
					assignWithDefault( commandArguments, odiInstanceProfileARN,
						"ANNEX_DEFAULT_ODI_INSTANCE_PROFILE_ARN",
						"InstanceProfileARN" );
					assignWithDefault( commandArguments, odiKeyName,
						"ANNEX_DEFAULT_ODI_KEY_NAME", "KeyName" );
					assignWithDefault( commandArguments, odiSecurityGroupIDs,
						"ANNEX_DEFAULT_ODI_SECURITY_GROUP_IDS", "SecurityGroupIDs" );

					getODIApproval( commandArguments,
									odiInstanceType, odiInstanceTypeSpecified,
									leaseDurationSpecified, unclaimedTimeoutSpecified,
									count, leaseDuration, unclaimedTimeout, skipYesNoPrompt );
					break;

				default:
					ASSERT( false );
			}

			prepareTarballForUpload( commandArguments, configDir, owner, ownerSpecified, noOwner, unclaimedTimeout );
			handleUserData( commandArguments, clUserDataWins, userData, userDataFileName );
			return create_annex( commandArguments );

		case ct_update_annex:
			return update_annex( commandArguments );

		default:
			ASSERT( false );
			return 1;
	}

	return -1;
}


void
main_init( int argc, char ** argv ) {
	// Make sure that, e.g., updateInterval is set.
	main_config();

	commandState = new ClassAdCollection( NULL );
	if( !commandState->InitLogFile(commandStateFile.c_str()) ) {
		DC_Exit( 1 );
	}

	int rv = annex_main( argc, argv );
	if( rv != 0 ) { DC_Exit( rv ); }
}

void
main_shutdown_fast() {
	DC_Exit( 0 );
}

void
main_shutdown_graceful() {
	DC_Exit( 0 );
}

void
main_pre_dc_init( int /* argc */, char ** /* argv */ ) {
}

void
main_pre_command_sock_init() {
}

int
main( int argc, char ** argv ) {
	set_mySubSystem( "ANNEX", true, SUBSYSTEM_TYPE_DAEMON );

	// This is dumb, but easier than fighting daemon core about parsing.
	_argc = argc;
	_argv = (char **)malloc( argc * sizeof( char * ) );
	for( int i = 0; i < argc; ++i ) {
		_argv[i] = strdup( argv[i] );
	}

	// This is also dumb, but less dangerous than (a) reaching into daemon
	// core to set a flag and (b) hoping that my command-line arguments and
	// its command-line arguments don't conflict.
	char ** dcArgv = (char **)malloc( 7 * sizeof( char * ) );
	dcArgv[0] = argv[0];
	// Force daemon core to run in the foreground.
	dcArgv[1] = strdup( "-f" );
	// Disable the daemon core command socket.
	dcArgv[2] = strdup( "-p" );
	dcArgv[3] = strdup(  "0" );

	// We need to spin up the param system to find the user config directory,
	// to which we want to log.
	config_ex( CONFIG_OPT_WANT_META );
	std::string userDir;
	dcArgv[4] = strdup( "-log" );
	if(! createUserConfigDir( userDir )) { return 1; }
	dcArgv[5] = strdup( userDir.c_str() );
	dcArgv[6] = NULL;

	argc = 6;
	argv = dcArgv;

	dc_main_init = & main_init;
	dc_main_config = & main_config;
	dc_main_shutdown_fast = & main_shutdown_fast;
	dc_main_shutdown_graceful = & main_shutdown_graceful;
	dc_main_pre_dc_init = & main_pre_dc_init;
	dc_main_pre_command_sock_init = & main_pre_command_sock_init;

	return dc_main( argc, argv );
}
