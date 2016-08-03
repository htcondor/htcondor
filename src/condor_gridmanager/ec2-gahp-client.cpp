#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "gahp-client.h"

EC2GahpClient::EC2GahpClient( const char * id, const char * path, const ArgList * args ) :
	GahpClient( id, path, args ) { }

EC2GahpClient::~EC2GahpClient() { }

// Utility function.
void
pushStringListBack( std::vector< YourString > & v, StringList & sl ) {
	const char * text = NULL;

	sl.rewind();
	int count = 0;
	if( sl.number() > 0 ) {
		while( (text = sl.next()) ) {
			v.push_back( text );
			++count;
		}
	}
	ASSERT( count == sl.number() );

	v.push_back( NULLSTRING );
}

#define CHECK_COMMON_ARGUMENTS if( service_url.empty() || publickeyfile.empty() || privatekeyfile.empty() ) { return GAHPCLIENT_COMMAND_NOT_SUPPORTED; }
#define PUSH_COMMON_ARGUMENTS arguments.push_back( service_url ); arguments.push_back( publickeyfile ); arguments.push_back( privatekeyfile );

int EC2GahpClient::ec2_vm_start( const std::string & service_url,
								 const std::string & publickeyfile,
								 const std::string & privatekeyfile,
								 const std::string & ami_id,
								 const std::string & keypair,
								 const std::string & user_data,
								 const std::string & user_data_file,
								 const std::string & instance_type,
								 const std::string & availability_zone,
								 const std::string & vpc_subnet,
								 const std::string & vpc_ip,
								 const std::string & client_token,
								 const std::string & block_device_mapping,
								 const std::string & iam_profile_arn,
								 const std::string & iam_profile_name,
								 StringList & groupnames,
								 StringList & groupids,
								 StringList & parametersAndValues,
								 std::string &instance_id,
								 std::string &error_code)
{
	// command line looks like:
	// EC2_COMMAND_VM_START <req_id> <publickeyfile> <privatekeyfile> <ami-id> <keypair> <groupname> <groupname> ...
	static const char* command = "EC2_VM_START";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;
	if( ami_id.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.push_back( ami_id );
	arguments.push_back( keypair );
	arguments.push_back( user_data );
	arguments.push_back( user_data_file );
	arguments.push_back( instance_type );
	arguments.push_back( availability_zone );
	arguments.push_back( vpc_subnet );
	arguments.push_back( vpc_ip );
	arguments.push_back( client_token );
	arguments.push_back( block_device_mapping );
	arguments.push_back( iam_profile_arn );
	arguments.push_back( iam_profile_name );
	pushStringListBack( arguments, groupnames );
	pushStringListBack( arguments, groupids );
	pushStringListBack( arguments, parametersAndValues );
	int cgf = callGahpFunction( command, arguments, result, low_prio );
	if( cgf != 0 ) { return cgf; }

	// we expect the following return:
	//		seq_id 0 instance_id
	//		seq_id 1 error_code error_string
	//		seq_id 1

	if ( result ) {
		// command completed.
		int rc = 0;
		if ( result->argc == 2 ) {
			rc = atoi(result->argv[1]);
			if ( rc == 0 ) {
				EXCEPT( "Bad %s result", command );
				rc = 1;
			} else {
				error_string = "";
			}
		} else if ( result->argc == 3 ) {
			rc = atoi(result->argv[1]);
			instance_id = result->argv[2];
		} else if ( result->argc == 4 ) {
			// get the error code
			rc = atoi( result->argv[1] );
 			error_code = result->argv[2];
 			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

int EC2GahpClient::ec2_vm_stop(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,
								const std::string & instance_id,
								std::string & error_code )
{
	// command line looks like:
	// EC2_COMMAND_VM_STOP <req_id> <publickeyfile> <privatekeyfile> <instance-id>
	static const char* command = "EC2_VM_STOP";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;
	if( instance_id.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.push_back( instance_id );
	int cgf = callGahpFunction( command, arguments, result, medium_prio );
	if( cgf != 0 ) { return cgf; }

	if ( result ) {
		// command completed.
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) error_string = "";
		} else if ( result->argc == 4 ) {
			// get the error code
			rc = atoi( result->argv[1] );
			error_code = result->argv[2];
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

int EC2GahpClient::ec2_gahp_statistics( StringList & returnStatistics ) {
	server->write_line( "STATISTICS" );
	Gahp_Args result;
	server->read_argv( result );

	// How do we normally handle this?
	if( strcmp( result.argv[0], "S" ) != 0 ) {
		return 1;
	}

	// For now, don't bother to check how many statistics came back.
	for( int i = 1; i < result.argc; ++i ) {
		returnStatistics.append( result.argv[i] );
	}

	return 0;
}

int EC2GahpClient::ec2_vm_status_all( const std::string & service_url,
                                      const std::string & publickeyfile,
                                      const std::string & privatekeyfile,
                                      StringList & returnStatus,
                                      std::string & error_code )
{
    static const char * command = "EC2_VM_STATUS_ALL";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

	if ( result ) {
		int rc = atoi(result->argv[1]);

		switch( result->argc ) {
		    case 2:
		        if( rc != 0 ) { EXCEPT( "Bad %s result", command ); }
		        break;

		    case 4:
		        if( rc == 0 ) { EXCEPT( "Bad %s result", command ); }
    		    error_code = result->argv[2];
	    	    error_string = result->argv[3];
                break;

            default:
                if( (result->argc - 2) % 6 != 0 ) { EXCEPT( "Bad %s result", command ); }
                for( int i = 2; i < result->argc; ++i ) {
                    returnStatus.append( result->argv[i] );
                }
                returnStatus.rewind();
                break;
        }

		delete result;
		return rc;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

int EC2GahpClient::ec2_ping(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,
								std::string & error_code )
{
	// we can use "Status All" command to make sure EC2 Server is alive.
	static const char* command = "EC2_VM_STATUS_ALL";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

	if ( result ) {
		int rc = atoi(result->argv[1]);

		if( result->argc == 4 ) {
		    error_code = result->argv[2];
		    error_string = result->argv[3];
		}

		delete result;
		return rc;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

int EC2GahpClient::ec2_vm_server_type(	const std::string & service_url,
										const std::string & publickeyfile,
										const std::string & privatekeyfile,
										std::string & server_type,
										std::string & error_code )
{
	// we can use "Status All" command to make sure EC2 Server is alive.
	static const char* command = "EC2_VM_SERVER_TYPE";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

	// The result should look like:
	//		seq_id 0 server_type
	//		seq_id 1
	//		seq_id error_code error_string
	if ( result ) {
		int rc = 0;
		if ( result->argc == 2 ) {
			rc = atoi(result->argv[1]);
			if ( rc == 0 ) {
				EXCEPT( "Bad %s result", command );
				rc = 1;
			} else {
				error_string = "";
			}
		} else if ( result->argc == 3 ) {
			rc = atoi(result->argv[1]);
			server_type = result->argv[2];
		} else if ( result->argc == 4 ) {
			// get the error code
			rc = atoi( result->argv[1] );
			error_code = result->argv[2];
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

int EC2GahpClient::ec2_vm_create_keypair(	const std::string & service_url,
											const std::string & publickeyfile,
											const std::string & privatekeyfile,
											const std::string & keyname,
											const std::string & outputfile,
											std::string & error_code)
{
	// command line looks like:
	// EC2_COMMAND_VM_CREATE_KEYPAIR <req_id> <publickeyfile> <privatekeyfile> <groupname> <outputfile> 
	static const char* command = "EC2_VM_CREATE_KEYPAIR";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	if( keyname.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.push_back( keyname );
	if ( outputfile.empty() ) {
		arguments.push_back( NULL_FILE );
	} else {
		arguments.push_back( outputfile );
	}
	int cgf = callGahpFunction( command, arguments, result, medium_prio );
	if( cgf != 0 ) { return cgf; }

	// The result should look like:
	//		seq_id 0
	//		seq_id 1
	//		seq_id error_code error_string

	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		}
		else if ( result->argc == 4 ) {
			rc = atoi( result->argv[1] );
			error_code = result->argv[2];
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

int EC2GahpClient::ec2_vm_destroy_keypair(	const std::string & service_url,
											const std::string & publickeyfile,
											const std::string & privatekeyfile,
											const std::string & keyname,
											std::string & error_code )
{
	// command line looks like:
	// EC2_COMMAND_VM_DESTROY_KEYPAIR <req_id> <publickeyfile> <privatekeyfile> <groupname> 
	static const char* command = "EC2_VM_DESTROY_KEYPAIR";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	if( keyname.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.push_back( keyname );
	int cgf = callGahpFunction( command, arguments, result, medium_prio );
	if( cgf != 0 ) { return cgf; }

	// The result should look like:
	//		seq_id 0
	//		seq_id 1
	//		seq_id error_code error_string

	if ( result ) {
		// command completed
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		}
		else if ( result->argc == 4 ) {
			rc = atoi( result->argv[1] );
			error_code = result->argv[2];
			error_string = result->argv[3];
		} else {
			EXCEPT( "Bad %s result", command );
		}

		delete result;
		return rc;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

// Not sure why this function always returns 0.
int EC2GahpClient::ec2_associate_address( const std::string & service_url,
                                          const std::string & publickeyfile,
                                          const std::string & privatekeyfile,
                                          const std::string & instance_id,
                                          const std::string & elastic_ip,
                                          StringList & returnStatus,
                                          std::string & error_code )
{

    static const char* command = "EC2_VM_ASSOCIATE_ADDRESS";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;
	if( instance_id.empty() || elastic_ip.empty() ) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.push_back( instance_id );
	arguments.push_back( elastic_ip );
	int cgf = callGahpFunction( command, arguments, result, medium_prio );
	if( cgf != 0 ) { return 0; }

    if ( result ) {
        // command completed and the return value looks like:
        int return_code = atoi(result->argv[1]);

        if (return_code == 1) {

            if (result->argc == 2) {
                error_string = "";
            } else if (result->argc == 4) {
                error_code = result->argv[2];
                error_string = result->argv[3];
            } else {
                EXCEPT("Bad %s Result",command);
            }

        } else {    // return_code == 0

            if ( ( (result->argc-2) % 2) != 0 ) {
                EXCEPT("Bad %s Result",command);
            } else {
                // get the status info
                for (int i=2; i<result->argc; i++) {
                    returnStatus.append( result->argv[i] );
                }
                returnStatus.rewind();
            }
        }

        delete result;
		return 0;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

// Not sure why this function always returns 0.
int EC2GahpClient::ec2_create_tags(	const std::string & service_url,
									const std::string & publickeyfile,
									const std::string & privatekeyfile,
									const std::string & instance_id,
									StringList &tags,
									StringList &returnStatus,
									std::string &error_code)
{
    static const char* command = "EC2_VM_CREATE_TAGS";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;
	if( instance_id.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.push_back( instance_id );
	pushStringListBack( arguments, tags );
	int cgf = callGahpFunction( command, arguments, result, medium_prio );
	if( cgf != 0 ) { return 0; }

    if (result) {
        // command completed and the return value looks like:
        int return_code = atoi(result->argv[1]);

        if (return_code == 1) {
            if (result->argc == 2) {
                error_string = "";
            } else if (result->argc == 4) {
                error_code = result->argv[2];
                error_string = result->argv[3];
            } else {
                EXCEPT("Bad %s Result",command);
            }
        } else {    // return_code == 0
            if (((result->argc-2) % 2) != 0) {
                EXCEPT("Bad %s Result", command);
            } else {
                // get the status info
                for (int i=2; i<result->argc; i++) {
                    returnStatus.append(result->argv[i]);
                }
                returnStatus.rewind();
            }
        }
        delete result;
        return 0;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

// Not sure why this function always returns 0.
int EC2GahpClient::ec2_attach_volume( const std::string & service_url,
                                      const std::string & publickeyfile,
                                      const std::string & privatekeyfile,
                                      const std::string & volume_id,
                                      const std::string & instance_id,
                                      const std::string & device_id,
                                      StringList & returnStatus,
                                      std::string & error_code )
{
    static const char* command = "EC2_VM_ATTACH_VOLUME";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;
	if( volume_id.empty() || instance_id.empty() || device_id.empty() ) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.push_back( volume_id );
	arguments.push_back( instance_id );
	arguments.push_back( device_id );
	int cgf = callGahpFunction( command, arguments, result, medium_prio );
	if( cgf != 0 ) { return 0; }

    if ( result ) {
        // command completed and the return value looks like:
        int result_code = atoi(result->argv[1]);

        if (result_code == 1) {

            if (result->argc == 2) {
                error_string = "";
            } else if (result->argc == 4) {
                error_code = result->argv[2];
                error_string = result->argv[3];
            } else {
                EXCEPT("Bad %s Result",command);
            }

        } else {    // result_code == 0

            if ( ( (result->argc-2) % 2) != 0 ) {
                EXCEPT("Bad %s Result",command);
            } else {
                // get the status info
                for (int i=2; i<result->argc; i++) {
                    returnStatus.append( result->argv[i] );
                }
                returnStatus.rewind();
            }
        }

        delete result;
		return 0;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

//
// Spot instance support.
//
int EC2GahpClient::ec2_spot_start( const std::string & service_url,
                                   const std::string & publickeyfile,
                                   const std::string & privatekeyfile,
                                   const std::string & ami_id,
                                   const std::string & spot_price,
                                   const std::string & keypair,
                                   const std::string & user_data,
                                   const std::string & user_data_file,
                                   const std::string & instance_type,
                                   const std::string & availability_zone,
                                   const std::string & vpc_subnet,
                                   const std::string & vpc_ip,
                                   const std::string & client_token,
                                   const std::string & iam_profile_arn,
                                   const std::string & iam_profile_name,
                                   StringList & groupnames,
                                   StringList & groupids,
                                   std::string & request_id,
                                   std::string & error_code )
{
    static const char * command = "EC2_VM_START_SPOT";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;
	if( ami_id.empty() || spot_price.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.push_back( ami_id );
	arguments.push_back( spot_price );
	arguments.push_back( keypair );
	arguments.push_back( user_data );
	arguments.push_back( user_data_file );
	arguments.push_back( instance_type );
	arguments.push_back( availability_zone );
	arguments.push_back( vpc_subnet );
	arguments.push_back( vpc_ip );
	arguments.push_back( client_token );
	arguments.push_back( iam_profile_arn );
	arguments.push_back( iam_profile_name );
	pushStringListBack( arguments, groupnames );
	pushStringListBack( arguments, groupids );
	int cgf = callGahpFunction( command, arguments, result, low_prio );
	if( cgf != 0 ) { return cgf; }

    if( result ) {
        int rc = 0;
        switch( result->argc ) {
            case 2:
                rc = atoi( result->argv[1] );
                if( rc == 0 ) { EXCEPT( "Bad %s result", command ); }
                else { error_string = ""; }
                break;

            case 3:
                rc = atoi( result->argv[1] );
                request_id = result->argv[2];
                break;

            case 4:
                rc = atoi( result->argv[1] );
                error_code = result->argv[2];
                error_string = result->argv[3];
                break;

            default:
                EXCEPT( "Bad %s result", command );
                break;
        }
        delete result;
        return rc;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

int EC2GahpClient::ec2_spot_stop( const std::string & service_url,
                                  const std::string & publickeyfile,
                                  const std::string & privatekeyfile,
                                  const std::string & request_id,
                                  std::string & error_code )
{
    static const char * command = "EC2_VM_STOP_SPOT";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;
	if( request_id.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.push_back( request_id );
	int cgf = callGahpFunction( command, arguments, result, medium_prio );
	if( cgf != 0 ) { return cgf; }

    if( result ) {
        // We expect results of the form
        //      <request ID> 0
        //      <request ID> 1
        //      <request ID> 1 <error code> <error string>

        if( result->argc < 2 ) { EXCEPT( "Bad %s result", command ); }
        int rc = atoi( result->argv[1] );

        switch( result->argc ) {
            case 2:
                if( rc != 0 ) { error_string = ""; }
                break;

            case 4:
                if( rc != 0 ) {
                    error_code = result->argv[2];
                    error_string = result->argv[3];
                } else {
                    EXCEPT( "Bad %s result", command );
                }
                break;

            default:
                EXCEPT( "Bad %s result", command );
                break;
        }

        delete result;
        return rc;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

int EC2GahpClient::ec2_spot_status_all( const std::string & service_url,
                                        const std::string & publickeyfile,
                                        const std::string & privatekeyfile,
                                        StringList & returnStatus,
                                        std::string & error_code )
{
    static const char * command = "EC2_VM_STATUS_ALL_SPOT";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

    if( result ) {
        // We expect results of the form
        //      <request ID> 0
        //      <request ID> 0 (<SIR ID> <status> <ami ID> <instance ID|NULL> <status code|NULL>)+
        //      <request ID> 1
        //      <request ID> 1 <error code> <error string>
        if( result->argc < 2 ) { EXCEPT( "Bad %s result", command ); }

        int rc = atoi( result->argv[1] );
        if( result->argc == 2 ) {
            if( rc == 1 ) { error_string = ""; }
        } else if( result->argc == 4 ) {
            if( rc != 1 ) { EXCEPT( "Bad %s result", command ); }
            error_code = result->argv[2];
            error_string = result->argv[3];
        } else if( (result->argc - 2) % 5 == 0 ) {
            for( int i = 2; i < result->argc; ++i ) {
                if( strcmp( result->argv[i], NULLSTRING ) ) {
                    returnStatus.append( result->argv[i] );
                } else {
                    returnStatus.append( "" );
                }
            }
        } else {
            EXCEPT( "Bad %s result", command );
        }

        delete result;
        return rc;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}
