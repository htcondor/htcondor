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
			v.emplace_back(text );
			++count;
		}
	}
	ASSERT( count == sl.number() );

	v.emplace_back(NULLSTRING );
}

void
pushVectorBack( std::vector< YourString > & arguments, const std::vector< std::string > & v ) {
	for( unsigned i = 0; i < v.size(); ++i ) {
		arguments.emplace_back(v[i] );
	}
	arguments.emplace_back(NULLSTRING );
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
								 unsigned int maxCount,
								 StringList & groupnames,
								 StringList & groupids,
								 StringList & parametersAndValues,
								 std::vector< std::string > &instance_ids,
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
	arguments.emplace_back(ami_id );
	arguments.emplace_back(keypair );
	arguments.emplace_back(user_data );
	arguments.emplace_back(user_data_file );
	arguments.emplace_back(instance_type );
	arguments.emplace_back(availability_zone );
	arguments.emplace_back(vpc_subnet );
	arguments.emplace_back(vpc_ip );
	arguments.emplace_back(client_token );
	arguments.emplace_back(block_device_mapping );
	arguments.emplace_back(iam_profile_arn );
	arguments.emplace_back(iam_profile_name );

	std::string maxCountString;
	formatstr( maxCountString, "%u", maxCount );
	arguments.emplace_back(maxCountString );

	pushStringListBack( arguments, groupnames );
	pushStringListBack( arguments, groupids );
	pushStringListBack( arguments, parametersAndValues );
	int cgf = callGahpFunction( command, arguments, result, low_prio );
	if( cgf != 0 ) { return cgf; }

	// we expect the following return:
	//		seq_id 0 instance_id+
	//		seq_id 1 error_code error_string
	//		seq_id 1

	if ( result ) {
		if( result->argc < 2 ) {
			EXCEPT( "Bad %s result", command );
		}

		int rc = atoi( result->argv[1] );
		if( rc == 1 ) {
			switch( result->argc ) {
				case 4:
		 			error_code = result->argv[2];
 					error_string = result->argv[3];
 					break;
				case 2:
					error_code = "";
					error_string = "";
					break;
				default:
					EXCEPT( "Bad %s result", command );
			}
		} else if( rc == 0 ) {
			for( int i = 2; i < result->argc; ++i ) {
				instance_ids.emplace_back(result->argv[i] );
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
	arguments.emplace_back(instance_id );
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

// The previous polymorpheme could be implemented in terms of this function
// pretty easily, but to avoid risking breakage, we won't bother yet.
int EC2GahpClient::ec2_vm_stop(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,
								const std::vector< std::string > & instance_ids,
								std::string & error_code )
{
	// command line looks like:
	// EC2_COMMAND_VM_STOP <req_id> <publickeyfile> <privatekeyfile> <instance-id>+
	static const char* command = "EC2_VM_STOP";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;
	if( instance_ids.size() == 0 ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	// Assumes we have fewer than 1000 instances.
	for( size_t i = 0; i < instance_ids.size(); ++i ) {
		arguments.emplace_back(instance_ids[i] );
	}
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
                if( (result->argc - 2) % 8 != 0 ) { EXCEPT( "Bad %s result", command ); }
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

int EC2GahpClient::ec2_vm_status_all( const std::string & service_url,
                                      const std::string & publickeyfile,
                                      const std::string & privatekeyfile,
                                      const std::string & filterName,
                                      const std::string & filerValue,
                                      StringList & returnStatus,
                                      std::string & error_code )
{
    static const char * command = "EC2_VM_STATUS_ALL";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(filterName );
	arguments.emplace_back(filerValue );
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
                if( (result->argc - 2) % 8 != 0 ) { EXCEPT( "Bad %s result", command ); }
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
	arguments.emplace_back(keyname );
	if ( outputfile.empty() ) {
		arguments.emplace_back(NULL_FILE );
	} else {
		arguments.emplace_back(outputfile );
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
	arguments.emplace_back(keyname );
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
	arguments.emplace_back(instance_id );
	arguments.emplace_back(elastic_ip );
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
	arguments.emplace_back(instance_id );
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
	arguments.emplace_back(volume_id );
	arguments.emplace_back(instance_id );
	arguments.emplace_back(device_id );
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
	arguments.emplace_back(ami_id );
	arguments.emplace_back(spot_price );
	arguments.emplace_back(keypair );
	arguments.emplace_back(user_data );
	arguments.emplace_back(user_data_file );
	arguments.emplace_back(instance_type );
	arguments.emplace_back(availability_zone );
	arguments.emplace_back(vpc_subnet );
	arguments.emplace_back(vpc_ip );
	arguments.emplace_back(client_token );
	arguments.emplace_back(iam_profile_arn );
	arguments.emplace_back(iam_profile_name );
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
	arguments.emplace_back(request_id );
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


void
setAttribute(	std::string & s,
				const YourString & attribute,
				const YourString & value,
				bool addTrailingComma = true  ) {
	if( attribute.ptr() == NULL || value.ptr() == NULL ) { return; }
	s.append( "\"" ).append( attribute ).append( "\" : " );
	s.append( "\"" ).append( value ).append( "\"" );
	if( addTrailingComma ) {
		s.append( ", " );
	}
}

void
setLastAttribute(	std::string & s,
					const YourString & attribute,
					const YourString & value ) {
	setAttribute( s, attribute, value, false );
}

void EC2GahpClient::LaunchConfiguration::convertToJSON( std::string & s ) const {
	s.append( "{ " );
	setAttribute( s, "ImageId", ami_id );
	setAttribute( s, "SpotPrice", spot_price );
	setAttribute( s, "KeyName", keypair );

	// The GAHP will base64-encode the user data.
	setAttribute( s, "UserData", user_data );
	setAttribute( s, "InstanceType", instance_type );
	setAttribute( s, "SubnetId", vpc_subnet );

	// Actually part of the 'Placement' subhash.
	setAttribute( s, "AvailabilityZone", availability_zone );

	// Actually part of the 'BlockDeviceMappings' subhash.
	setAttribute( s, "BlockDeviceMapping", block_device_mapping );

	// Actually part of the 'IamInstanceProfile' subhash.
	setAttribute( s, "IAMProfileARN", iam_profile_arn );
	setAttribute( s, "IAMProfileName", iam_profile_name );

	// Actually part of the 'SecurityGroup's subhash.
	char * tmp = groupnames->print_to_delimed_string( ", " );
	setAttribute( s, "SecurityGroupNames", tmp );
	free( tmp );
	tmp = groupids->print_to_delimed_string( ", " );
	setAttribute( s, "SecurityGroupIDs", tmp );
	free( tmp );

	setLastAttribute( s, "WeightedCapacity", weighted_capacity );
	s.append( " }" );
}

int EC2GahpClient::bulk_start(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,

								const std::string & client_token,
								const std::string & spot_price,
								const std::string & target_capacity,
								const std::string & iam_fleet_role,
								const std::string & allocation_strategy,
								const std::string & valid_until,

								const std::vector< LaunchConfiguration > & launch_configurations,

								std::string & bulkRequestID,
								std::string & error_code ) {
	// One big copy is cheaper than a bunch of small reallocations,
	// so convert the launch configuration(s) to JSON in the large buffer
	// and copy it over to the vector (since YourString doesn't own the
	// JSON, some local in this function must instead).
	std::string buffer( 1024, '\0' );
	std::vector< std::string > lcStrings( launch_configurations.size() );
	if( launch_configurations.size() <= 0 ) { return GAHPCLIENT_COMMAND_NOT_SUPPORTED; }
	for( unsigned i = 0; i < launch_configurations.size(); ++i ) {
		buffer.clear();
		launch_configurations[i].convertToJSON( buffer );
		lcStrings[i] = buffer;
	}

	return bulk_start(	service_url, publickeyfile, privatekeyfile,
						client_token, spot_price, target_capacity,
						iam_fleet_role, allocation_strategy, valid_until,
						lcStrings, bulkRequestID, error_code );
}

int EC2GahpClient::bulk_start(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,

								const std::string & client_token,
								const std::string & spot_price,
								const std::string & target_capacity,
								const std::string & iam_fleet_role,
								const std::string & allocation_strategy,
								const std::string & valid_until,

								const std::vector< std::string > & launch_configurations,

								std::string & bulkRequestID,
								std::string & error_code ) {
	static const char * command = "EC2_BULK_START";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(client_token );
	arguments.emplace_back(spot_price );
	arguments.emplace_back(target_capacity );
	arguments.emplace_back(iam_fleet_role );
	arguments.emplace_back(allocation_strategy );
	arguments.emplace_back(valid_until );
	pushVectorBack( arguments, launch_configurations );

	int cgf = callGahpFunction( command, arguments, result, low_prio );
	if( cgf != 0 ) { return cgf; }

	if( result ) {
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
			bulkRequestID = result->argv[2];
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

int EC2GahpClient::bulk_stop(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,
								const std::string & bulkRequestID,
								std::string & error_code ) {
	static const char * command = "EC2_BULK_STOP";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(bulkRequestID );

	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

	if( result ) {
		int rc = 0;
		if ( result->argc == 2 ) {
			rc = atoi(result->argv[1]);
            if( rc == 1 ) { error_string = ""; }
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

int EC2GahpClient::bulk_query(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,
								StringList & returnStatus,
								std::string & error_code ) {
	static const char * command = "EC2_BULK_QUERY";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;

	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

	if( result ) {
		int rc = 0;
		if ( result->argc == 2 ) {
			rc = atoi(result->argv[1]);
            if( rc == 1 ) { error_string = ""; }
		} else if ( result->argc == 4 ) {
			// get the error code
			rc = atoi( result->argv[1] );
 			error_code = result->argv[2];
 			error_string = result->argv[3];
		} else {
			if( (result->argc - 2) % 3 != 0 ) { EXCEPT( "Bad %s result", command ); }

			rc = atoi( result->argv[1] );
			for( int i = 2; i < result->argc; ++i ) {
				returnStatus.append( result->argv[i] );
			}
			returnStatus.rewind();
		}

		delete result;
		return rc;
	} else {
		EXCEPT( "callGahpFunction() succeeded but result was NULL." );
	}
}

int EC2GahpClient::put_rule(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,
								const std::string & ruleName,
								const std::string & scheduleExpression,
								const std::string & state,
								std::string & ruleARN,
								std::string & error_code ) {
	static const char * command = "CWE_PUT_RULE";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(ruleName );
	arguments.emplace_back(scheduleExpression );
	arguments.emplace_back(state );

	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

	if( result ) {
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
			ruleARN = result->argv[2];
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

int EC2GahpClient::delete_rule(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,
								const std::string & ruleName,
								std::string & error_code ) {
	static const char * command = "CWE_DELETE_RULE";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(ruleName );

	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

	if( result ) {
		int rc = 0;
		if ( result->argc == 2 ) {
			rc = atoi(result->argv[1]);
            if( rc == 1 ) { error_string = ""; }
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

int EC2GahpClient::get_function(	const std::string & service_url,
									const std::string & publickeyfile,
									const std::string & privatekeyfile,
									const std::string & functionARN,
									std::string & functionHash,
									std::string & error_code ) {
	static const char * command = "AWS_GET_FUNCTION";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(functionARN );

	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

	if( result ) {
		int rc = 0;
		if ( result->argc == 2 ) {
			rc = atoi(result->argv[1]);
			if( rc == 1 ) { error_string = ""; }
		} else if ( result->argc == 3 ) {
			rc = atoi(result->argv[1]);
			functionHash = result->argv[2];
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

int EC2GahpClient::put_targets(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,
								const std::string & ruleName,
								const std::string & id,
								const std::string & arn,
								const std::string & input,
								std::string & error_code ) {
	static const char * command = "CWE_PUT_TARGETS";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(ruleName );
	arguments.emplace_back(id );
	arguments.emplace_back(arn );
	arguments.emplace_back(input );

	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

	if( result ) {
		int rc = 0;
		if ( result->argc == 2 ) {
			rc = atoi(result->argv[1]);
            if( rc == 1 ) { error_string = ""; }
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

int EC2GahpClient::remove_targets(	const std::string & service_url,
									const std::string & publickeyfile,
									const std::string & privatekeyfile,
									const std::string & ruleName,
									const std::string & id,
									std::string & error_code ) {
	static const char * command = "CWE_REMOVE_TARGETS";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(ruleName );
	arguments.emplace_back(id );

	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

	if( result ) {
		int rc = 0;
		if ( result->argc == 2 ) {
			rc = atoi(result->argv[1]);
            if( rc == 1 ) { error_string = ""; }
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

int EC2GahpClient::s3_upload(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,

								const std::string & bucketName,
								const std::string & fileName,
								const std::string & path,

								std::string & error_code )
{
	// command line looks like:
	// S3_UPLOAD <req_id> <publickeyfile> <privatekeyfile> <bucketName> <fileName> <path>
	static const char* command = "S3_UPLOAD";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;
	if( bucketName.empty() || fileName.empty() || path.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(bucketName );
	arguments.emplace_back(fileName );
	arguments.emplace_back(path );
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

int EC2GahpClient::describe_stacks(  const std::string & service_url,
	const std::string & publickeyfile,
	const std::string & privatekeyfile,

	const std::string & stackName,

	std::string & stackStatus,
	std::map< std::string, std::string > & outputs,

	std::string & error_code ) {
	// command line looks like:
	// CF_DESCRIBE_STACKS <req_id> <serviceurl> <accesskeyfile> <secretkeyfile> <stackName>
	static const char* command = "CF_DESCRIBE_STACKS";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;
	if( stackName.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(stackName );
	int cgf = callGahpFunction( command, arguments, result, medium_prio );
	if( cgf != 0 ) { return cgf; }

	if ( result ) {
		// command completed.
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) error_string = "";
		} else if ( result->argc == 3 ) {
			// If we saw no outputs from this stack.
			rc = atoi(result->argv[1]);
			stackStatus = result->argv[2];
		} else if ( result->argc == 4 ) {
			// get the error code
			rc = atoi( result->argv[1] );
			error_code = result->argv[2];
			error_string = result->argv[3];
		} else if ( result->argc >= 5 ) {
			rc = atoi(result->argv[1]);
			stackStatus = result->argv[2];

			for( int i = 3; i + 1 < result->argc; i +=2 ) {
				outputs[ result->argv[i] ] = result->argv[i + 1];
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

int EC2GahpClient::create_stack(
	const std::string & service_url,
	const std::string & publickeyfile,
	const std::string & privatekeyfile,

	const std::string & stackName,
	const std::string & templateURL,
	const std::string & capability,
	const std::map< std::string, std::string > & parameters,

	std::string & stackID,
	std::string & error_code )
{
	// command line looks like:
	// CF_CREATE_STACK <req_id> <serviceurl> <accesskeyfile> <secretkeyfile> <stackName> <templateURL> <capability> (<parameters-name> <parameter-value>)* <NULLSTRING>
	static const char* command = "CF_CREATE_STACK";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;
	if( stackName.empty() || templateURL.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(stackName );
	arguments.emplace_back(templateURL );
	arguments.emplace_back(capability );
	std::vector< std::string > plist;
	for( auto i = parameters.begin(); i != parameters.end(); ++i ) {
		plist.push_back( i->first );
		plist.push_back( i->second );
	}
	pushVectorBack( arguments, plist );

	int cgf = callGahpFunction( command, arguments, result, medium_prio );
	if( cgf != 0 ) { return cgf; }

	if ( result ) {
		// command completed.
		int rc = 0;
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) error_string = "";
		} else if ( result->argc == 3 ) {
			rc = atoi(result->argv[1]);
			stackID = result->argv[2];
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

int EC2GahpClient::call_function(	const std::string & service_url,
									const std::string & publickeyfile,
									const std::string & privatekeyfile,
									const std::string & functionARN,
									const std::string & argumentBlob,
									std::string & returnBlob,
									std::string & error_code ) {
	static const char * command = "AWS_CALL_FUNCTION";

	// callGahpFunction() checks if this command is supported.
	CHECK_COMMON_ARGUMENTS;

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	PUSH_COMMON_ARGUMENTS;
	arguments.emplace_back(functionARN );
	arguments.emplace_back(argumentBlob );

	int cgf = callGahpFunction( command, arguments, result, high_prio );
	if( cgf != 0 ) { return cgf; }

	if( result ) {
		int rc = 0;
		if ( result->argc == 2 ) {
			rc = atoi(result->argv[1]);
			if( rc == 1 ) { error_string = ""; }
		} else if ( result->argc == 3 ) {
			rc = atoi(result->argv[1]);
			returnBlob = result->argv[2];
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
