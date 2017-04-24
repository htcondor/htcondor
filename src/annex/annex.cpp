#include "condor_common.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "match_prefix.h"
#include "daemon.h"
#include "dc_annexd.h"
#include "stat_wrapper.h"
#include "condor_base64.h"

void
// FIXME: The exit()s in here should probably be returns so that clean-up
// can be done, and this function has its own clean-up to do as well.
createConfigTarball(	const char * configDir,
						const char * annexName,
						std::string & tarballPath ) {
	char * cwd = get_current_dir_name();

	int rv = chdir( configDir );
	if( rv != 0 ) {
		fprintf( stderr, "Unable to change to config dir '%s' (%d): '%s'.\n",
			configDir, errno, strerror( errno ) );
		exit( 4 );
	}

	int fd = safe_open_wrapper_follow( "99ec2-dynamic.config",
		O_WRONLY | O_CREAT | O_TRUNC, 0600 );
	if( fd < 0 ) {
		fprintf( stderr, "Failed to open config file '%s' for writing: '%s' (%d).\n",
			"99ec2-dynamic.config", strerror( errno ), errno );
		exit( 4 );
	}


	std::string passwordFile = "password_file.pl";

	std::string collectorHost;
	param( collectorHost, "COLLECTOR_HOST" );
	if( collectorHost.empty() ) {
		fprintf( stderr, "COLLECTOR_HOST empty or undefined, aborting.\n" );
		exit( 5 );
	} else {
		fprintf( stdout, "Using COLLECTOR_HOST '%s'\n", collectorHost.c_str() );
	}

	std::string contents;
	formatstr( contents,
		"use security : host_based\n"
		"LOCAL_HOSTS = $(FULL_HOSTNAME) $(IP_ADDRESS) 127.0.0.1 $(TCP_FORWARDING_HOST)\n"
		"CONDOR_HOST = condor_pool@*/* $(LOCAL_HOSTS)\n"
		"COLLECTOR_HOST = %s\n"
		"\n"
		"SEC_DEFAULT_AUTHENTICATION = REQUIRED\n"
		"SEC_DEFAULT_AUTHENTICATION_METHODS = FS, PASSWORD\n"
		"\n"
		"SEC_PASSWORD_FILE = /etc/condor/config.d/%s\n"
		"ALLOW_WRITE = condor_pool@*/* $(LOCAL_HOSTS)\n"
		"\n"
		"AnnexName = \"%s\"\n"
		"STARTD_ATTRS = $(STARTD_ATTRS) AnnexName\n"
		"MASTER_ATTRS = $(MASTER_ATTRS) AnnexName\n"
		"\n",
		collectorHost.c_str(), passwordFile.c_str(), annexName );

	rv = write( fd, contents.c_str(), contents.size() );
	if( rv == -1 ) {
		fprintf( stderr, "Error writing to '%s': '%s' (%d).\n",
			"99ec2-dynamic.config", strerror( errno ), errno );
	} else if( rv != (int)contents.size() ) {
		fprintf( stderr, "Short write to '%s': '%s' (%d).\n",
			"99ec2-dynamic.config", strerror( errno ), errno );
		exit( 4 );
	}
	close( fd );


	std::string localPasswordFile;
	param( localPasswordFile, "SEC_PASSWORD_FILE" );
	if( passwordFile.empty() ) {
		fprintf( stderr, "SEC_PASSWORD_FILE empty or undefined, aborting.\n" );
		exit( 5 );
	} else {
		fprintf( stdout, "Using SEC_PASSWORD_FILE '%s'\n", localPasswordFile.c_str() );
	}

	// FIXME: Rewrite without system().
	std::string cpCommand;
	formatstr( cpCommand, "cp '%s' '%s'", localPasswordFile.c_str(), passwordFile.c_str() );
	int status = system( cpCommand.c_str() );
	if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
		fprintf( stderr, "Failed to copy '%s' to '%s', aborting.\n",
			localPasswordFile.c_str(), passwordFile.c_str() );
		exit( 5 );
	}


	// ...
	status = system( "tar --exclude config.tar.gz -z -c -f config.tar.gz *" );
	if(! (WIFEXITED( status ) && (WEXITSTATUS( status ) == 0))) {
		fprintf( stderr, "Failed to create tarball, aborting.\n" );
		exit( 5 );
	}


	rv = chdir( cwd );
	if( rv != 0 ) {
		fprintf( stderr, "Unable to change back to working dir '%s' (%d): '%s'.\n",
			cwd, errno, strerror( errno ) );
		exit( 4 );
	}
	free( cwd );

	formatstr( tarballPath, "%s/%s", configDir, "config.tar.gz" );
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

// Modelled on readShortFile() from ec2_gahp/amazonCommands.cpp.
bool
readShortFile( const char * fileName, std::string & contents ) {
    int fd = safe_open_wrapper_follow( fileName, O_RDONLY, 0600 );

    if( fd < 0 ) {
    	fprintf( stderr, "Failed to open file '%s' for reading: '%s' (%d).\n",
            fileName, strerror( errno ), errno );
        return false;
    }

    StatWrapper sw( fd );
    unsigned long fileSize = sw.GetBuf()->st_size;

    char * rawBuffer = (char *)malloc( fileSize + 1 );
    assert( rawBuffer != NULL );
    unsigned long totalRead = full_read( fd, rawBuffer, fileSize );
    close( fd );
    if( totalRead != fileSize ) {
    	fprintf( stderr, "Failed to completely read file '%s'; needed %lu but got %lu.\n",
            fileName, fileSize, totalRead );
        free( rawBuffer );
        return false;
    }
    contents.assign( rawBuffer, fileSize );
    free( rawBuffer );

    return true;
}

void
help( const char * argv0 ) {
	fprintf( stdout, "usage: %s\n"
		"\t-annex-name <annex-name>\n"
		"\t-duration <lease-duration>\n"
		"\t-slots <desired-capacity> | -count <desired-instances>\n"
		"\t[-odi | -sfr]\n"
		"\t[-access-key-file <access-key-file>]\n"
		"\t[-secret-key-file <secret-key-file>]\n"
		"\t[-config-dir <path>]\n"
/*		"\t[-pool <pool>] [-name <name>]\n"
*/		"\t[-service-url <service-url>]\n"
		"\t[-events-url <events-url>]\n"
		"\t[-lambda-url <lambda-url>]\n"
		"\t[-s3-url <lambda-url>]\n"
		"\t[-sfr-lease-function-arn <sfr-lease-function-arn>]\n"
		"\t[-odi-lease-function-arn <odi-lease-function-arn>]\n"
		"\t[-[default-]user-data[-file] <data|file> ]\n"
		"\t[-debug] [-help]\n"
		"\t[-sfr-config-file <spot-fleet-configuration-file>]\n"
		"\t[-odi-instance-type <instance-type>]\n"
		"\t[-odi-image-id <image-ID>\n"
		"\t[-odi-security-group-ids <group-ID[,groupID]*>\n"
		"\t[-odi-key-name <key-name>\n"
		"\t[-odi-instance-profile-arn <instance-profile-arn>]\n"
		, argv0 );
	fprintf( stdout, "%s defaults to On-Demand Instances (-odi).  "
		"For Spot Fleet Requests, use -sfr.  Specifying "
		"-sfr-config-file implies -sfr, as does -slots.  "
		"Specifying -count implies -odi.  You may not specify or imply "
		"-odi and -sfr in the same command.  Specifying -odi-* implies -odi."
		"\n", argv0 );
}

int
main( int argc, char ** argv ) {
	config();
	set_mySubSystem( "TOOL", SUBSYSTEM_TYPE_TOOL );

	bool clUserDataWins = true;
	std::string userData;
	const char * userDataFileName = NULL;

	bool quiet = false;
	int udSpecifications = 0;
	const char * pool = NULL;
	const char * name = NULL;
	const char * sfrConfigFile = NULL;
	const char * annexName = NULL;
	const char * configDir = NULL;
	const char * serviceURL = NULL;
	const char * eventsURL = NULL;
	const char * lambdaURL = NULL;
	const char * s3URL = NULL;
	const char * publicKeyFile = NULL;
	const char * secretKeyFile = NULL;
	const char * sfrLeaseFunctionARN = NULL;
	const char * odiLeaseFunctionARN = NULL;
	const char * odiInstanceType = NULL;
	const char * odiImageID = NULL;
	const char * odiInstanceProfileARN = NULL;
	const char * odiS3ConfigPath = NULL;
	const char * odiKeyName = NULL;
	const char * odiSecurityGroupIDs = NULL;
	bool annexTypeIsSFR = false;
	bool annexTypeIsODI = false;
	long int leaseDuration = 0;
	long int count = 0;
	for( int i = 1; i < argc; ++i ) {
		if( is_dash_arg_prefix( argv[i], "pool", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				pool = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -pool requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "name", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				name = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -name requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "service-url", 7 ) ) {
			++i;
			if( argv[i] != NULL ) {
				serviceURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -service-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "events-url", 6 ) ) {
			++i;
			if( argv[i] != NULL ) {
				eventsURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -events-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "lambda-url", 6 ) ) {
			++i;
			if( argv[i] != NULL ) {
				lambdaURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -lambda-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "s3-url", 2 ) ) {
			++i;
			if( argv[i] != NULL ) {
				s3URL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -s3-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "user-data-file", 10 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				userDataFileName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "user-data", 9 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				userData = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "default-user-data-file", 18 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				clUserDataWins = false;
				userDataFileName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "default-user-data", 17 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				clUserDataWins = false;
				userData = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "annex-name", 6 ) ) {
			++i;
			if( argv[i] != NULL ) {
				annexName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -annex-name requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "config-dir", 10 ) ) {
			++i;
			if( argv[i] != NULL ) {
				configDir = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -config-dir requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "sfr-config-file", 12 ) ) {
			++i;
			if( argv[i] != NULL ) {
				sfrConfigFile = argv[i];
				annexTypeIsSFR = true;
				continue;
			} else {
				fprintf( stderr, "%s: -sfr-config-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "public-key-file", 6 ) ||
					is_dash_arg_prefix( argv[1], "access-key-file", 6 ) ) {
			++i;
			if( argv[i] != NULL ) {
				publicKeyFile = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -{public|access}-key-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "secret-key-file", 6 ) ||
					is_dash_arg_prefix( argv[i], "private-key-file", 7 ) ) {
			++i;
			if( argv[i] != NULL ) {
				secretKeyFile = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -{secret|private}-key-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "sfr-lease-function-arn", 11 ) ) {
			++i;
			if( argv[i] != NULL ) {
				sfrLeaseFunctionARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -sfr-lease-function-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-lease-function-arn", 11 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiLeaseFunctionARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-lease-function-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-instance-type", 14 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiInstanceType = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-instance-type requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-key-name", 7 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiKeyName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-key-name requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-security-group-ids", 21 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiSecurityGroupIDs = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-security-group-ids requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-image-id", 11 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiImageID = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-image-id requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-instance-profile-arn", 22 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiInstanceProfileARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -lease-function-arn requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "odi-s3-config-path", 18 ) ) {
			++i;
			if( argv[i] != NULL ) {
				odiS3ConfigPath = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -odi-s3-config-path requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "duration", 8 ) ) {
			++i;
			if( argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ld = argv[i];
				leaseDuration = strtol( ld, & endptr, 0 );
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -lease-duration requires an integer argument.\n", argv[0] );
					return 1;
				}
				if( leaseDuration <= 0 ) {
					fprintf( stderr, "%s: the lease duration must be greater than zero.\n", argv[0] );
					return 1;
				}
				continue;
			} else {
				fprintf( stderr, "%s: -lease-duration requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "slots", 5 ) ) {
			++i;
			if( argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ld = argv[i];
				count = strtol( ld, & endptr, 0 );
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -slots requires an integer argument.\n", argv[0] );
					return 1;
				}
				if( count <= 0 ) {
					fprintf( stderr, "%s: the count must be greater than zero.\n", argv[0] );
					return 1;
				}
				annexTypeIsSFR = true;
				continue;
			} else {
				fprintf( stderr, "%s: -count requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "count", 5 ) ) {
			++i;
			if( argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ld = argv[i];
				count = strtol( ld, & endptr, 0 );
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -slots requires an integer argument.\n", argv[0] );
					return 1;
				}
				if( count <= 0 ) {
					fprintf( stderr, "%s: the count must be greater than zero.\n", argv[0] );
					return 1;
				}
				annexTypeIsODI = true;
				continue;
			} else {
				fprintf( stderr, "%s: -count requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "sfr", 3 ) ) {
			annexTypeIsSFR = true;
			continue;
		} else if( is_dash_arg_prefix( argv[i], "odi", 3 ) ) {
			annexTypeIsODI = true;
			continue;
		} else if( is_dash_arg_prefix( argv[i], "debug", 1 ) ) {
			dprintf_set_tool_debug( "TOOL", 0 );
			continue;
		} else if( is_dash_arg_prefix( argv[i], "help", 1 ) ) {
			help( argv[0] );
			return 1;
		} else if( argv[i][0] == '-' && argv[i][1] != '\0' ) {
			fprintf( stderr, "%s: unrecognized option (%s).\n", argv[0], argv[i] );
			return 1;
		}
	}

	if( udSpecifications > 1 ) {
		fprintf( stderr, "%s: you may specify no more than one of -[default-]user-data[-file].\n", argv[0] );
		return 1;
	}

	if( annexName == NULL ) {
		fprintf( stderr, "%s: you must specify -annex-name.\n", argv[0] );
		return 1;
	}

	if( count == 0 ) {
		fprintf( stderr, "%s: you must specify (a positive) -count.\n", argv[0] );
		return 1;
	}

	if( leaseDuration == 0 ) {
		fprintf( stderr, "%s: you must specify (a positive) -duration.\n", argv[0] );
		return 1;
	}

	if( annexTypeIsSFR && annexTypeIsODI ) {
		fprintf( stderr, "You must not specify more than one of -odi and -sfr.  If you specify -sfr-config-file, you must not specify -odi.  Specifying -count implies -odi and specifying -slots implies -sfr.\n" );
		return 1;
	}
	if(! (annexTypeIsSFR || annexTypeIsODI )) {
		annexTypeIsODI = true;
	}

	if( annexTypeIsSFR && sfrConfigFile == NULL ) {
		sfrConfigFile = param( "ANNEX_DEFAULT_SFR_CONFIG_FILE" );
		if( sfrConfigFile == NULL ) {
			fprintf( stderr, "Spot Fleet Requests require the -sfr-config-file flag.\n" );
			return 1;
		}
	}
	if( sfrConfigFile != NULL && ! annexTypeIsSFR ) {
		fprintf( stderr, "Unwilling to ignore -sfr-config-file flag being set for an ODI annex.\n" );
		return 1;
	}


	ClassAd spotFleetRequest;
	if( sfrConfigFile != NULL ) {
		FILE * file = NULL;
		bool closeFile = true;
		if( strcmp( sfrConfigFile, "-" ) == 0 ) {
			file = stdin;
			closeFile = false;
		} else {
			file = safe_fopen_wrapper_follow( sfrConfigFile, "r" );
			if( file == NULL ) {
				fprintf( stderr, "Unable to open annex specification file '%s'.\n", sfrConfigFile );
				return 1;
			}
		}

		CondorClassAdFileIterator ccafi;
		if(! ccafi.begin( file, closeFile, CondorClassAdFileParseHelper::Parse_json )) {
			fprintf( stderr, "Failed to start parsing spot fleet request.\n" );
			return 2;
		} else {
			int numAttrs = ccafi.next( spotFleetRequest );
			if( numAttrs <= 0 ) {
				fprintf( stderr, "Failed to parse spot fleet request, found no attributes.\n" );
				return 2;
			} else if( numAttrs > 11 ) {
				fprintf( stderr, "Failed to parse spot fleet reqeust, found too many attributes.\n" );
				return 2;
			}
		}
	} else {
		if(! odiInstanceType) {
			odiInstanceType = param( "ANNEX_DEFAULT_ODI_INSTANCE_TYPE" );
		}
		if( odiInstanceType ) {
			spotFleetRequest.Assign( "InstanceType", odiInstanceType );
		}

		if(! odiImageID) {
			odiImageID = param( "ANNEX_DEFAULT_ODI_IMAGE_ID" );
		}
		if( odiImageID ) {
			spotFleetRequest.Assign( "ImageID", odiImageID );
		}

		if(! odiInstanceProfileARN) {
			odiInstanceProfileARN = param( "ANNEX_DEFAULT_ODI_INSTANCE_PROFILE_ARN" );
		}
		if( odiInstanceProfileARN ) {
			spotFleetRequest.Assign( "InstanceProfileARN", odiInstanceProfileARN );
		}

		if(! odiKeyName) {
			odiKeyName = param( "ANNEX_DEFAULT_ODI_KEY_NAME" );
		}
		if( odiKeyName ) {
			spotFleetRequest.Assign( "KeyName", odiKeyName );
		}

		if(! odiSecurityGroupIDs) {
			odiSecurityGroupIDs = param( "ANNEX_DEFAULT_ODI_SECURITY_GROUP_IDS" );
		}
		if( odiSecurityGroupIDs ) {
			spotFleetRequest.Assign( "SecurityGroupIDs", odiSecurityGroupIDs );
		}
	}

	if( annexTypeIsSFR ) {
		spotFleetRequest.Assign( "AnnexType", "sfr" );
		if(! sfrLeaseFunctionARN) {
			sfrLeaseFunctionARN = param( "ANNEX_DEFAULT_SFR_LEASE_FUNCTION_ARN" );
		}
		spotFleetRequest.Assign( "LeaseFunctionARN", sfrLeaseFunctionARN );
	}
	if( annexTypeIsODI ) {
		spotFleetRequest.Assign( "AnnexType", "odi" );
		if(! odiLeaseFunctionARN) {
			odiLeaseFunctionARN = param( "ANNEX_DEFAULT_ODI_LEASE_FUNCTION_ARN" );
		}
		spotFleetRequest.Assign( "LeaseFunctionARN", odiLeaseFunctionARN );
	}

	spotFleetRequest.Assign( "AnnexName", annexName );
	spotFleetRequest.Assign( "TargetCapacity", count );
	time_t now = time( NULL );
	spotFleetRequest.Assign( "EndOfLease", now + leaseDuration );

	std::string tarballTarget;
	param( tarballTarget, "ANNEX_DEFAULT_ODI_S3_CONFIG_PATH" );
	if( odiS3ConfigPath != NULL ) {
		tarballTarget = odiS3ConfigPath;
	}
	if( tarballTarget.empty() ) {
		fprintf( stderr, "If you don't specify -odi-s3-config-path, ANNEX_DEFAULT_ODI_S3_CONFIG_PATH must be set in configuration.\n" );
		return 1;
	}
	spotFleetRequest.Assign( "UploadTo", tarballTarget );

	// Create the config tarball locally, then specify it.  That will
	// allow us to switch over to file transfer later if necessary.
	std::string tarballPath;
	if( configDir == NULL ) {
		char dirNameTemplate[] = ".condor_annex.XXXXXX";
		const char * tempDir = mkdtemp( dirNameTemplate );
		if( tempDir == NULL ) {
			fprintf( stderr, "Failed to create temporary directory (%d): '%s'.\n", errno, strerror( errno ) );
			return 2;
		}
		createConfigTarball( tempDir, annexName, tarballPath );
		// FIXME: copy the config tarball somewhere sane.
		rmdir( tempDir );
	} else {
		createConfigTarball( configDir, annexName, tarballPath );
	}
	spotFleetRequest.Assign( "UploadFrom", tarballPath );

	if( serviceURL != NULL ) {
		spotFleetRequest.Assign( "ServiceURL", serviceURL );
	}

	if( eventsURL != NULL ) {
		spotFleetRequest.Assign( "EventsURL", eventsURL );
	}

	if( lambdaURL != NULL ) {
		spotFleetRequest.Assign( "LambdaURL", lambdaURL );
	}

	if( s3URL != NULL ) {
		spotFleetRequest.Assign( "S3URL", s3URL );
	}

	if( publicKeyFile != NULL ) {
		spotFleetRequest.Assign( "PublicKeyFile", publicKeyFile );
	}

	if( secretKeyFile != NULL ) {
		spotFleetRequest.Assign( "SecretKeyFile", secretKeyFile );
	}

	// Handle user data.
	if( userDataFileName != NULL ) {
		if(! readShortFile( userDataFileName, userData ) ) {
			return 2;
		}
	}

	// condor_base64_encode() segfaults on the empty string.
	if(! userData.empty()) {
		std::string validationError;
		char * base64Encoded = condor_base64_encode( (const unsigned char *)userData.c_str(), userData.length() );
		if(! assignUserData( spotFleetRequest, base64Encoded, clUserDataWins, validationError )) {
			fprintf( stderr, "Failed to set user data in request ad (%s).\n", validationError.c_str() );
			return 6;
		}
		free( base64Encoded );
	}


	std::string annexDaemonName;
	param( annexDaemonName, "ANNEXD_NAME" );
	if( name == NULL ) { name = annexDaemonName.c_str(); }
	DCAnnexd annexd( name, pool );
	annexd.setSubsystem( "GENERIC" );

	if(! annexd.locate( Daemon::LOCATE_FOR_LOOKUP )) {
		char * error = annexd.error();
		if( error && error[0] != '\0' ) {
			fprintf( stderr, "%s: Can't locate annex daemon (%s).\n", argv[0], error );
		} else {
			fprintf( stderr, "%s: Can't locate annex daemon.\n", argv[0] );
		}
		return 3;
	}


	ClassAd reply;
	if(! quiet) { fprintf( stdout, "Sending bulk request command to daemon...\n" ); }
	if(! annexd.sendBulkRequest( & spotFleetRequest, & reply )) {
		char * error = annexd.error();
		if( error && error[0] != '\0' ) {
			fprintf( stderr, "%s\n", error );
		} else {
			fprintf( stderr, "Failed to send bulk request to daemon.\n" );
		}
		return 4;
	}

	int requestVersion = -1;
	reply.LookupInteger( "RequestVersion", requestVersion );
	if( requestVersion != 1 ) {
		fprintf( stderr, "Daemon's reply had missing or unknown RequestVersion (%d).\n", requestVersion );
		return 5;
	}

	std::string bulkRequestID;
	reply.LookupString( "BulkRequestID", bulkRequestID );
	if( bulkRequestID.empty() ) {
		fprintf( stderr, "Daemon's reply did not include bulk request ID.\n" );
		return 5;
	} else {
		fprintf( stdout, "%s\n", bulkRequestID.c_str() );
	}

	return 0;
}
