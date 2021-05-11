#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "gahp-client.h"
#include "classad_collection.h"
#include "stat_wrapper.h"

#include "annex.h"
#include "annex-setup.h"
#include "generate-id.h"
#include "user-config-dir.h"

#include "Functor.h"

#include "CreateKeyPair.h"
#include "CheckForStack.h"
#include "CreateStack.h"
#include "FunctorSequence.h"
#include "GenerateConfigFile.h"
#include "SetupReply.h"
#include "WaitForStack.h"

extern ClassAdCollection * commandState;

void
checkOneParameter( const char * pName, int & rv, std::string & pValue ) {
	param( pValue, pName );
	if( pValue.empty() ) {
		if( rv == 0 ) { fprintf( stderr, "Your setup is incomplete:\n" ); }
		fprintf( stderr, "\t%s is unset.\n", pName );
		rv = 1;
	}
}

void
checkOneParameter( const char * pName, int & rv ) {
	std::string pValue;
	checkOneParameter( pName, rv, pValue );
}

int
check_account_setup( const std::string & publicKeyFile, const std::string & privateKeyFile, const std::string & cfURL, const std::string & ec2URL ) {
	ClassAd * reply = new ClassAd();
	ClassAd * scratchpad = new ClassAd();

	std::string commandID;
	generateCommandID( commandID );

	Stream * replyStream = NULL;

	EC2GahpClient * cfGahp = startOneGahpClient( publicKeyFile, cfURL );
	EC2GahpClient * ec2Gahp = startOneGahpClient( publicKeyFile, ec2URL );

	std::string bucketStackName = "HTCondorAnnex-ConfigurationBucket";
	std::string bucketStackDescription = "configuration bucket";
	CheckForStack * bucketCFS = new CheckForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		bucketStackName, bucketStackDescription,
		commandState, commandID );

	std::string lfStackName = "HTCondorAnnex-LambdaFunctions";
	std::string lfStackDescription = "Lambda functions";
	CheckForStack * lfCFS = new CheckForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		lfStackName, lfStackDescription,
		commandState, commandID );

	std::string rStackName = "HTCondorAnnex-InstanceProfile";
	std::string rStackDescription = "instance profile";
	CheckForStack * rCFS = new CheckForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		rStackName, rStackDescription,
		commandState, commandID );

	std::string sgStackName = "HTCondorAnnex-SecurityGroup";
	std::string sgStackDescription = "security group";
	CheckForStack * sgCFS = new CheckForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		sgStackName, sgStackDescription,
		commandState, commandID );

	SetupReply * sr = new SetupReply( reply, cfGahp, ec2Gahp, "Your setup looks OK.", scratchpad,
		replyStream, commandState, commandID );

	FunctorSequence * fs = new FunctorSequence(
		{ bucketCFS, lfCFS, rCFS, sgCFS }, sr,
		commandState, commandID, scratchpad );

	int setupTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
		 (void (Service::*)()) & FunctorSequence::operator(),
		 "CheckForStacks", fs );
	cfGahp->setNotificationTimerId( setupTimer );
	ec2Gahp->setNotificationTimerId( setupTimer );

	return 0;
}

int
check_setup( const char * cloudFormationURL, const char * serviceURL ) {
	int rv = 0;

	std::string cfURL = cloudFormationURL ? cloudFormationURL : "";
	if( cfURL.empty() ) {
		param( cfURL, "ANNEX_DEFAULT_CF_URL" );
	}
	if( cfURL.empty() ) {
		fprintf( stderr, "No CloudFormation URL specified on command-line and ANNEX_DEFAULT_CF_URL is not set or empty in configuration.\n" );
		return 1;
	}

	std::string ec2URL = serviceURL ? serviceURL : "";
	if( ec2URL.empty() ) {
		param( ec2URL, "ANNEX_DEFAULT_EC2_URL" );
	}
	if( ec2URL.empty() ) {
		fprintf( stderr, "No EC2 URL specified on command-line and ANNEX_DEFAULT_EC2_URL is not set or empty in configuration.\n" );
		return 1;
	}

	std::string accessKeyFile, secretKeyFile;
	checkOneParameter( "ANNEX_DEFAULT_ACCESS_KEY_FILE", rv, accessKeyFile );
	checkOneParameter( "ANNEX_DEFAULT_SECRET_KEY_FILE", rv, secretKeyFile );

	checkOneParameter( "ANNEX_DEFAULT_S3_BUCKET", rv );
	checkOneParameter( "ANNEX_DEFAULT_ODI_SECURITY_GROUP_IDS", rv );
	checkOneParameter( "ANNEX_DEFAULT_ODI_LEASE_FUNCTION_ARN", rv );
	checkOneParameter( "ANNEX_DEFAULT_SFR_LEASE_FUNCTION_ARN", rv );
	checkOneParameter( "ANNEX_DEFAULT_ODI_INSTANCE_PROFILE_ARN", rv );

	checkOneParameter( "COLLECTOR_HOST", rv );
	checkOneParameter( "SEC_PASSWORD_FILE", rv );

	if( secretKeyFile != USE_INSTANCE_ROLE_MAGIC_STRING ) {
		StatWrapper sw( secretKeyFile.c_str() );
		mode_t mode = sw.GetBuf()->st_mode;
		if( mode & S_IRWXG || mode & S_IRWXO || getuid() != sw.GetBuf()->st_uid ) {
			fprintf( stderr, "Secret key file must be accessible only by owner.  Please verify that your user owns the file and that the file permissons are restricted to the owner.\n" );
			rv = 1;
		}
	}

	if( rv != 0 ) {
		return rv;
	} else {
		std::string secPasswordFile;
		param( secPasswordFile, "SEC_PASSWORD_FILE" );
		int fd = open( secPasswordFile.c_str(), O_RDONLY );
		if( fd == -1 ) {
			fprintf( stderr, "Unable to open SEC_PASSWORD_FILE '%s': %s (%d).  Either create this file (run 'condor_store_cred -c add -f %s') or correct the value of SEC_PASSWORD_FILE (which you can locate by running 'condor_config_val -v SEC_PASSWORD_FILE').\n",
				secPasswordFile.c_str(), strerror(errno), errno, secPasswordFile.c_str() );
			return 1;
		} else {
			close( fd );
		}

		// Is the PASSWORD method available?
		fprintf( stderr, "Checking security configuration... " );
		Daemon * daemon = new Daemon( DT_COLLECTOR, NULL );
		if(! daemon->locate(Daemon::LOCATE_FOR_LOOKUP)) {
			fprintf( stderr, "Unable to locate collector.  Make sure COLLECTOR_HOST is set correctly.\n" );
			delete daemon;
			return 1;
		} else {
			CondorError errorStack;
			ReliSock * sock = (ReliSock *) daemon->makeConnectedSocket(
				Stream::reli_sock, 0, 0, & errorStack );
			if( sock == NULL ) {
				fprintf( stderr, "Failed to connect to to collector at '%s'.  Make sure COLLECTOR_HOST is set correctly.  If it is, ask your administrator about firewalls.", daemon->addr() );
				fprintf( stderr, "  Because firewalls are machine-specific, we will continue to check your set-up.  However, if your instances fail to report to your pool, you may want to address this problem and try again.\n" );
				fprintf( stderr, "\n" );
			} else {
				if(! daemon->startSubCommand( DC_SEC_QUERY, UPDATE_STARTD_AD, sock, 0, & errorStack )) {
					fprintf( stderr, "We were not authorized to advertise to the collector.  Run 'condor_ping -verbose -type COLLECTOR UPDATE_STARTD_AD' for more information." );
					fprintf( stderr, "  Although it is possible to configure PASSWORD -based security in a host-specific way, it would be unusual to do so, so we recommend you investigate this problem.  We will continue to check your set-up.\n" );
					fprintf( stderr, "\n" );
				} else {
					fprintf( stderr, "OK.\n" );
				}
				delete sock;
			}
		}
		delete daemon;

		return check_account_setup( accessKeyFile, secretKeyFile, cfURL, ec2URL );
	}
}

void
setup_usage() {
	fprintf( stdout,
		"\n"
		"To do the one-time setup for an AWS account:\n"
		"\tcondor_annex [options] -setup\n"
		"... if you're on an EC2 instance (with a privileged IAM role):\n"
		"\tcondor_annex [-aws-region <region>] -setup FROM INSTANCE [CloudFormation URL]\n"
		"\n"
		"To specify the files for the access (public) key and secret (private) keys\n"
		"[or, for experts, the CloudFormation URL]:\n"
		"\tcondor_annex [options] -setup\n"
		"\t\t<path/to/access-key-file>\n"
		"\t\t<path/to/private-key-file>\n"
		"\t\t[<https://cloudformation.<region>.amazonaws.com/]\n"
		"\n"
		"For how to specify a region, and other options:\n"
		"\tcondor_annex -help\n"
	);
}

int
setup( const char * region, const char * pukf, const char * prkf, const char * cloudFormationURL, const char * serviceURL ) {
	std::string publicKeyFile, privateKeyFile;
	if( pukf != NULL && prkf != NULL ) {
		publicKeyFile = pukf;
		privateKeyFile = prkf;
	} else {
		std::string userDirectory;
		if(! createUserConfigDir( userDirectory )) {
			fprintf( stderr, "You must therefore specify the public and private key files on the command-line.\n" );
			setup_usage();
			return 1;
		} else {
			publicKeyFile = userDirectory + "/publicKeyFile";
			privateKeyFile = userDirectory + "/privateKeyFile";
		}
	}

	if( MATCH == strcasecmp( publicKeyFile.c_str(), "FROM" )
	  && MATCH == strcasecmp( privateKeyFile.c_str(), "INSTANCE" ) ) {
		publicKeyFile = USE_INSTANCE_ROLE_MAGIC_STRING;
		privateKeyFile = USE_INSTANCE_ROLE_MAGIC_STRING;
	}

	if( publicKeyFile != USE_INSTANCE_ROLE_MAGIC_STRING ) {
		int fd = safe_open_no_create_follow( publicKeyFile.c_str(), O_RDONLY );
		if( fd == -1 ) {
			fprintf( stderr, "Unable to open public key file '%s': '%s' (%d).\n",
				publicKeyFile.c_str(), strerror( errno ), errno );
			if( pukf == NULL ) {
				fprintf( stderr, "Trying FROM INSTANCE, instead.\n" );
				publicKeyFile = USE_INSTANCE_ROLE_MAGIC_STRING;
				privateKeyFile = USE_INSTANCE_ROLE_MAGIC_STRING;
			} else {
				setup_usage();
				return 1;
			}
		} else {
			close( fd );
		}
	}

	if( privateKeyFile != USE_INSTANCE_ROLE_MAGIC_STRING ) {
		int fd = safe_open_no_create_follow( privateKeyFile.c_str(), O_RDONLY );
		if( fd == -1 ) {
			fprintf( stderr, "Unable to open private key file '%s': '%s' (%d).\n",
				privateKeyFile.c_str(), strerror( errno ), errno );
			if( prkf == NULL ) {
				fprintf( stderr, "Trying FROM INSTANCE, instead.\n" );
				publicKeyFile = USE_INSTANCE_ROLE_MAGIC_STRING;
				privateKeyFile = USE_INSTANCE_ROLE_MAGIC_STRING;
			} else {
				setup_usage();
				return 1;
			}
		} else {
			close( fd );
		}
	}


	std::string cfURL = cloudFormationURL ? cloudFormationURL : "";
	if( cfURL.empty() ) {
		param( cfURL, "ANNEX_DEFAULT_CF_URL" );
	}
	if( cfURL.empty() ) {
		fprintf( stderr, "No CloudFormation URL specified on command-line and ANNEX_DEFAULT_CF_URL is not set or empty in configuration.\n" );
		return 1;
	}

	std::string ec2URL = serviceURL ? serviceURL : "";
	if( ec2URL.empty() ) {
		param( ec2URL, "ANNEX_DEFAULT_EC2_URL" );
	}
	if( ec2URL.empty() ) {
		fprintf( stderr, "No EC2 URL specified on command-line and ANNEX_DEFAULT_EC2_URL is not set or empty in configuration.\n" );
		return 1;
	}

	ClassAd * reply = new ClassAd();
	ClassAd * scratchpad = new ClassAd();

	// This 100% redundant, but it moves all our config-file generation
	// code to the same place, so it's worth it.
	scratchpad->Assign( "AccessKeyFile", publicKeyFile );
	scratchpad->Assign( "SecretKeyFile", privateKeyFile );

	std::string commandID;
	generateCommandID( commandID );

	Stream * replyStream = NULL;

	EC2GahpClient * cfGahp = startOneGahpClient( publicKeyFile, cfURL );
	EC2GahpClient * ec2Gahp = startOneGahpClient( publicKeyFile, ec2URL );

	// FIXME: Do something cleverer for versioning.
	std::string bucketStackURL = "https://s3.amazonaws.com/condor-annex/bucket-10.json";
	std::string bucketStackName = "HTCondorAnnex-ConfigurationBucket";
	std::string bucketStackDescription = "configuration bucket (this takes less than a minute)";
	std::map< std::string, std::string > bucketParameters;
	CreateStack * bucketCS = new CreateStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		bucketStackName, bucketStackURL, bucketParameters,
		commandState, commandID );
	WaitForStack * bucketWFS = new WaitForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		bucketStackName, bucketStackDescription,
		commandState, commandID );

	// FIXME: Do something cleverer for versioning.
	std::string lfStackURL = "https://s3.amazonaws.com/condor-annex/template-10.json";
	std::string lfStackName = "HTCondorAnnex-LambdaFunctions";
	std::string lfStackDescription = "Lambda functions (this takes about a minute)";
	std::map< std::string, std::string > lfParameters;
	lfParameters[ "S3BucketName" ] = "<scratchpad>";
	CreateStack * lfCS = new CreateStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		lfStackName, lfStackURL, lfParameters,
		commandState, commandID );
	WaitForStack * lfWFS = new WaitForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		lfStackName, lfStackDescription,
		commandState, commandID );

	// FIXME: Do something cleverer for versioning.
	std::string rStackURL = "https://s3.amazonaws.com/condor-annex/role-10.json";
	std::string rStackName = "HTCondorAnnex-InstanceProfile";
	std::string rStackDescription = "instance profile (this takes about two minutes)";
	std::map< std::string, std::string > rParameters;
	rParameters[ "S3BucketName" ] = "<scratchpad>";
	CreateStack * rCS = new CreateStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		rStackName, rStackURL, rParameters,
		commandState, commandID );
	WaitForStack * rWFS = new WaitForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		rStackName, rStackDescription,
		commandState, commandID );

	// FIXME: Do something cleverer for versioning.
	std::string sgStackURL = "https://s3.amazonaws.com/condor-annex/security-group-10.json";
	std::string sgStackName = "HTCondorAnnex-SecurityGroup";
	std::string sgStackDescription = "security group (this takes less than a minute)";
	std::map< std::string, std::string > sgParameters;
	CreateStack * sgCS = new CreateStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		sgStackName, sgStackURL, sgParameters,
		commandState, commandID );
	WaitForStack * sgWFS = new WaitForStack( reply, cfGahp, scratchpad,
		cfURL, publicKeyFile, privateKeyFile,
		sgStackName, sgStackDescription,
		commandState, commandID );

	CreateKeyPair * ckp = new CreateKeyPair( reply, ec2Gahp, scratchpad,
		ec2URL, publicKeyFile, privateKeyFile,
		commandState, commandID );

	GenerateConfigFile * gcf = new GenerateConfigFile( cfGahp, region, scratchpad );

	SetupReply * sr = new SetupReply( reply, cfGahp, ec2Gahp, "Setup successful.", scratchpad,
		replyStream, commandState, commandID );

	FunctorSequence * fs = new FunctorSequence(
		{ bucketCS, bucketWFS, lfCS, lfWFS, rCS, rWFS, sgCS, sgWFS, ckp, gcf }, sr,
		commandState, commandID, scratchpad );


	int setupTimer = daemonCore->Register_Timer( 0, TIMER_NEVER,
		 (void (Service::*)()) & FunctorSequence::operator(),
		 "CreateStack, DescribeStacks, WriteConfigFile", fs );
	cfGahp->setNotificationTimerId( setupTimer );
	ec2Gahp->setNotificationTimerId( setupTimer );

	return 0;
}
