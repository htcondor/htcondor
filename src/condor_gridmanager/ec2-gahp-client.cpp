#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "gahp-client.h"

// Utility function.
void addStringListToRequestLine( StringList & sl, std::string & reqLine ) {
	const char * text;
	char * escapedText;

	sl.rewind();
	int count = 0;
	if( sl.number() > 0 ) {
		while( (text = sl.next()) ) {
			escapedText = strdup( escapeGahpString( text ) );
			formatstr_cat( reqLine, " %s", escapedText );
			free( escapedText );
			++count;
		}
	}
	ASSERT( count == sl.number() );

	formatstr_cat( reqLine, " %s", NULLSTRING );
}

//  Start VM
int GahpClient::ec2_vm_start( std::string service_url,
							  std::string publickeyfile,
							  std::string privatekeyfile,
							  std::string ami_id,
							  std::string keypair,
							  std::string user_data,
							  std::string user_data_file,
							  std::string instance_type,
							  std::string availability_zone,
							  std::string vpc_subnet,
							  std::string vpc_ip,
							  std::string client_token,
							  std::string block_device_mapping,
							  std::string iam_profile_arn,
							  std::string iam_profile_name,
							  StringList & groupnames,
							  StringList & groupids,
							  StringList & parametersAndValues,
							  std::string &instance_id,
							  std::string &error_code)
{
	// command line looks like:
	// EC2_COMMAND_VM_START <req_id> <publickeyfile> <privatekeyfile> <ami-id> <keypair> <groupname> <groupname> ...
	static const char* command = "EC2_VM_START";

	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	// check the input arguments
	if ( service_url.empty() ||
		 publickeyfile.empty() ||
		 privatekeyfile.empty() ||
		 ami_id.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	// Generate request line

	if ( keypair.empty() ) keypair = NULLSTRING;
	if ( user_data.empty() ) user_data = NULLSTRING;
	if ( user_data_file.empty() ) user_data_file = NULLSTRING;
	if ( instance_type.empty() ) instance_type = NULLSTRING;
	if ( availability_zone.empty() ) availability_zone = NULLSTRING;
	if ( vpc_subnet.empty() ) vpc_subnet = NULLSTRING;
	if ( vpc_ip.empty() ) vpc_ip = NULLSTRING;
	if ( client_token.empty() ) client_token = NULLSTRING;
	if ( block_device_mapping.empty() ) block_device_mapping = NULLSTRING;
	if ( iam_profile_arn.empty() ) iam_profile_arn = NULLSTRING;
	if ( iam_profile_name.empty() ) iam_profile_name = NULLSTRING;

	std::string reqline;

	char* esc1 = strdup( escapeGahpString(service_url) );
	char* esc2 = strdup( escapeGahpString(publickeyfile) );
	char* esc3 = strdup( escapeGahpString(privatekeyfile) );
	char* esc4 = strdup( escapeGahpString(ami_id) );
	char* esc5 = strdup( escapeGahpString(keypair) );
	char* esc6 = strdup( escapeGahpString(user_data) );
	char* esc7 = strdup( escapeGahpString(user_data_file) );
	char* esc8 = strdup( escapeGahpString(instance_type) );
	char* esc9 = strdup( escapeGahpString(availability_zone) );
	char* esc10 = strdup( escapeGahpString(vpc_subnet) );
	char* esc11 = strdup( escapeGahpString(vpc_ip) );
	char* esc12 = strdup( escapeGahpString(client_token) );
	char* esc13 = strdup( escapeGahpString(block_device_mapping) );
	char* esc14 = strdup( escapeGahpString(iam_profile_arn) );
	char* esc15 = strdup( escapeGahpString(iam_profile_name) );

	int x = formatstr(reqline, "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s", esc1, esc2, esc3, esc4, esc5, esc6, esc7, esc8, esc9, esc10, esc11, esc12, esc13, esc14, esc15 );

	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );
	free( esc5 );
	free( esc6 );
	free( esc7 );
	free( esc8 );
	free( esc9 );
	free( esc10 );
	free( esc11 );
	free( esc12 );
	free( esc13 );
	free( esc14 );
	free( esc15 );
	ASSERT( x > 0 );

	addStringListToRequestLine( groupnames, reqline );
	addStringListToRequestLine( groupids, reqline );
	addStringListToRequestLine( parametersAndValues, reqline );

	const char *buf = reqline.c_str();
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy, low_prio);
	}

	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);

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
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}


// Stop VM
int GahpClient::ec2_vm_stop( std::string service_url,
							 std::string publickeyfile,
							 std::string privatekeyfile,
							 std::string instance_id,
							 std::string & error_code )
{
	// command line looks like:
	// EC2_COMMAND_VM_STOP <req_id> <publickeyfile> <privatekeyfile> <instance-id>
	static const char* command = "EC2_VM_STOP";

	// check input arguments
	if ( service_url.empty() ||
		 publickeyfile.empty() ||
		 privatekeyfile.empty() ||
		 instance_id.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	Gahp_Args * result = NULL;
	std::vector< YourString > arguments;
	arguments.push_back( service_url );
	arguments.push_back( publickeyfile );
	arguments.push_back( privatekeyfile );
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

int GahpClient::ec2_gahp_statistics( StringList & returnStatistics ) {
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


// ...
int GahpClient::ec2_vm_status_all( std::string service_url,
                                   std::string publickeyfile,
                                   std::string privatekeyfile,
                                   StringList & returnStatus,
                                   std::string & error_code )
{
    static const char * command = "EC2_VM_STATUS_ALL";

	// Generate request line
	char* esc1 = strdup( escapeGahpString(service_url) );
	char* esc2 = strdup( escapeGahpString(publickeyfile) );
	char* esc3 = strdup( escapeGahpString(privatekeyfile) );
	
	std::string reqline;
	formatstr(reqline, "%s %s %s", esc1, esc2, esc3 );
	const char *buf = reqline.c_str();
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy, high_prio);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
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
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
} 


// Check VM status
int GahpClient::ec2_vm_status( std::string service_url,
							   std::string publickeyfile,
							   std::string privatekeyfile,
							   std::string instance_id,
							   StringList &returnStatus,
							   std::string & error_code )
{	
	// command line looks like:
	// EC2_COMMAND_VM_STATUS <return 0;"EC2_VM_STATUS";
	static const char* command = "EC2_VM_STATUS";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( service_url.empty() ||
		 publickeyfile.empty() ||
		 privatekeyfile.empty() ||
		 instance_id.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// Generate request line
	std::string reqline;
	
	char* esc1 = strdup( escapeGahpString(service_url) );
	char* esc2 = strdup( escapeGahpString(publickeyfile) );
	char* esc3 = strdup( escapeGahpString(privatekeyfile) );
	char* esc4 = strdup( escapeGahpString(instance_id) );
	
	int x = formatstr(reqline, "%s %s %s %s", esc1, esc2, esc3, esc4 );
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );
	ASSERT( x > 0 );
	
	const char *buf = reqline.c_str();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy, high_prio);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	// we expect the following return:
	//		seq_id 0 <instance_id> <status> <ami_id> <state_reason_code> <public_dns> <private_dns> <keypairname> <group> <group> <group> ... 
	//		seq_id 0
	//		seq_id 1 error_code error_string
	//		seq_id 1
	// We use "NULL" to replace the empty items. and there at least has one group.
	
	if ( result ) {
		// command completed.
		int rc = 0;
		
		if (result->argc == 2) {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				error_string = "";
			}
		} 
		else if (result->argc == 4) {
			rc = atoi( result->argv[1] );
			error_code = result->argv[2];
			error_string = result->argv[3];
		}
		else if (result->argc == 6)
		{
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				EXCEPT( "Bad %s result", command );
			}
			else {
				if ( strcmp(result->argv[3], "running") == 0) {
					rc = 1;
				}
				else
				{
					// get the status info
					for (int i=2; i<result->argc; i++) {
						returnStatus.append( result->argv[i] );
					}
				}
				returnStatus.rewind();
			}				
		} 
		else if (result->argc < 10) {
			EXCEPT( "Bad %s result", command );
		} else {
			rc = atoi(result->argv[1]);
			if (rc == 1) {
				EXCEPT( "Bad %s result", command );
			}
			else {
				if ( (strcmp(result->argv[3], "pending")!=0) && 
					 (strcmp(result->argv[3], "running")!=0) ) {
					rc = 1;
				}
				else
				{
					// get the status info
					for (int i=2; i<result->argc; i++) {
						returnStatus.append( result->argv[i] );
					}
				}
				returnStatus.rewind();
			}
		}
		
		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}



// Ping to check if the server is alive
int GahpClient::ec2_ping(std::string service_url,
						 std::string publickeyfile,
						 std::string privatekeyfile,
						 std::string & error_code )
{
	// we can use "Status All" command to make sure EC2 Server is alive.
	static const char* command = "EC2_VM_STATUS_ALL";
	
	// Generate request line
	char* esc1 = strdup( escapeGahpString(service_url) );
	char* esc2 = strdup( escapeGahpString(publickeyfile) );
	char* esc3 = strdup( escapeGahpString(privatekeyfile) );
	
	std::string reqline;
	formatstr(reqline, "%s %s %s", esc1, esc2, esc3 );
	const char *buf = reqline.c_str();
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy, high_prio);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
	if ( result ) {
		int rc = atoi(result->argv[1]);
		
		if( result->argc == 4 ) {
		    error_code = result->argv[2];
		    error_string = result->argv[3];
		}
		
		delete result;
		return rc;
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// Determine what implementation of EC2 we're talking to
int GahpClient::ec2_vm_server_type(std::string service_url,
								   std::string publickeyfile,
								   std::string privatekeyfile,
								   std::string & server_type,
								   std::string & error_code )
{
	// we can use "Status All" command to make sure EC2 Server is alive.
	static const char* command = "EC2_VM_SERVER_TYPE";

	// Generate request line
	char* esc1 = strdup( escapeGahpString(service_url) );
	char* esc2 = strdup( escapeGahpString(publickeyfile) );
	char* esc3 = strdup( escapeGahpString(privatekeyfile) );

	std::string reqline;
	formatstr( reqline, "%s %s %s", esc1, esc2, esc3 );
	const char *buf = reqline.c_str();

	free( esc1 );
	free( esc2 );
	free( esc3 );

	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy, high_prio);
	}

	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);

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
	}

	// Now check if pending command timed out.
	if ( check_pending_timeout(command,buf) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}


// Create and register SSH keypair
int GahpClient::ec2_vm_create_keypair( std::string service_url,
									   std::string publickeyfile,
									   std::string privatekeyfile,
									   std::string keyname,
									   std::string outputfile,
									   std::string & error_code)
{
	// command line looks like:
	// EC2_COMMAND_VM_CREATE_KEYPAIR <req_id> <publickeyfile> <privatekeyfile> <groupname> <outputfile> 
	static const char* command = "EC2_VM_CREATE_KEYPAIR";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( service_url.empty() ||
		 publickeyfile.empty() ||
		 privatekeyfile.empty() ||
		 keyname.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	if ( outputfile.empty() ) {
		outputfile = NULL_FILE;
	}
	
	// construct command line
	std::string reqline;
	
	char* esc1 = strdup( escapeGahpString(service_url) );
	char* esc2 = strdup( escapeGahpString(publickeyfile) );
	char* esc3 = strdup( escapeGahpString(privatekeyfile) );
	char* esc4 = strdup( escapeGahpString(keyname) );
	char* esc5 = strdup( escapeGahpString(outputfile) );
	
	int x = formatstr(reqline, "%s %s %s %s %s", esc1, esc2, esc3, esc4, esc5);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );
	free( esc5 );

	ASSERT( x > 0 );
	
	const char *buf = reqline.c_str();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy, medium_prio);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
	
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
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}	



// The destroy keypair function will delete the name of keypair, it will not touch the output file of 
// keypair. So in EC2 Job, we should delete keypair output file manually. We don't need to care about
// the keypair name/output file in EC2, it will be removed automatically.
int GahpClient::ec2_vm_destroy_keypair( std::string service_url,
										std::string publickeyfile,
										std::string privatekeyfile,
										std::string keyname,
										std::string & error_code )
{
	// command line looks like:
	// EC2_COMMAND_VM_DESTROY_KEYPAIR <req_id> <publickeyfile> <privatekeyfile> <groupname> 
	static const char* command = "EC2_VM_DESTROY_KEYPAIR";
	
	// check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// check input arguments
	if ( service_url.empty() ||
		 publickeyfile.empty() ||
		 privatekeyfile.empty() ||
		 keyname.empty() ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}
	
	// construct command line
	std::string reqline;
	
	char* esc1 = strdup( escapeGahpString(service_url) );
	char* esc2 = strdup( escapeGahpString(publickeyfile) );
	char* esc3 = strdup( escapeGahpString(privatekeyfile) );
	char* esc4 = strdup( escapeGahpString(keyname) );
	
	int x = formatstr(reqline, "%s %s %s %s", esc1, esc2, esc3, esc4);
	
	free( esc1 );
	free( esc2 );
	free( esc3 );
	free( esc4 );

	ASSERT( x > 0 );
	
	const char *buf = reqline.c_str();
		
	// Check if this request is currently pending. If not, make it the pending request.
	if ( !is_pending(command,buf) ) {
		// Command is not pending, so go ahead and submit a new one if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending(command, buf, deleg_proxy, medium_prio);
	}
	
	// If we made it here, command is pending.

	// Check first if command completed.
	Gahp_Args* result = get_pending_result(command, buf);
		
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
	}
	
	// Now check if pending command timed out.
	if ( check_pending_timeout(command, buf) ) 
	{
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;	
}

int GahpClient::ec2_associate_address(std::string service_url,
                                      std::string publickeyfile,
                                      std::string privatekeyfile,
                                      std::string instance_id, 
                                      std::string elastic_ip,
                                      StringList & returnStatus,
                                      std::string & error_code )
{

    static const char* command = "EC2_VM_ASSOCIATE_ADDRESS";

    int rc=0;
    
    // check if this command is supported
    if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    // check input arguments
    if ( service_url.empty() ||
		 publickeyfile.empty() ||
		 privatekeyfile.empty() ||
		 instance_id.empty() ||
		 elastic_ip.empty() ) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    // Generate request line
    std::string reqline;

    char* esc1 = strdup( escapeGahpString(service_url) );
    char* esc2 = strdup( escapeGahpString(publickeyfile) );
    char* esc3 = strdup( escapeGahpString(privatekeyfile) );
    char* esc4 = strdup( escapeGahpString(instance_id) );
    char* esc5 = strdup( escapeGahpString(elastic_ip) );
    
    int x = formatstr(reqline, "%s %s %s %s %s", esc1, esc2, esc3, esc4, esc5 );
    
    free( esc1 );
    free( esc2 );
    free( esc3 );
    free( esc4 );
    free( esc5 );
    ASSERT( x > 0 );
    
    const char *buf = reqline.c_str();
        
    // Check if this request is currently pending. If not, make it the pending request.
    if ( !is_pending(command,buf) ) {
        // Command is not pending, so go ahead and submit a new one if our command mode permits.
        if ( m_mode == results_only ) {
            return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
        }
        now_pending(command, buf, deleg_proxy, medium_prio);
    }
    
    // If we made it here, command is pending.

    // Check first if command completed.
    Gahp_Args* result = get_pending_result(command, buf);

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
        
    }
    
    return rc;

}


int
GahpClient::ec2_create_tags(std::string service_url,
							std::string publickeyfile,
							std::string privatekeyfile,
							std::string instance_id, 
							StringList &tags,
							StringList &returnStatus,
							std::string &error_code)
{
    static const char* command = "EC2_VM_CREATE_TAGS";

    int rc = 0;

    // check if this command is supported
    if  (!server->m_commands_supported->contains_anycase(command)) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    // check input arguments
    if (service_url.empty() ||
		publickeyfile.empty() ||
		privatekeyfile.empty() ||
		instance_id.empty()) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    // Generate request line
    std::string reqline;

    char *esc1 = strdup(escapeGahpString(service_url));
    char *esc2 = strdup(escapeGahpString(publickeyfile));
    char *esc3 = strdup(escapeGahpString(privatekeyfile));
    char *esc4 = strdup(escapeGahpString(instance_id));
    
    int x = formatstr(reqline, "%s %s %s %s", esc1, esc2, esc3, esc4);
    
    free(esc1);
    free(esc2);
    free(esc3);
    free(esc4);
    ASSERT(x > 0);

	const char *tag;
	int count = 0;
	tags.rewind();
	if (tags.number() > 0) {
		while ((tag = tags.next())) {
			char *esc_tag = strdup(escapeGahpString(tag));
			formatstr_cat(reqline, " %s", esc_tag);
			count++;
			free(esc_tag);
		}
	}
	ASSERT(count == tags.number());
    
    const char *buf = reqline.c_str();
        
    // Check if this request is currently pending. If not, make it the pending request.
    if (!is_pending(command, buf)) {
        // Command is not pending, so go ahead and submit a new one if our command mode permits.
        if (m_mode == results_only) {
            return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
        }
        now_pending(command, buf, deleg_proxy, medium_prio);
    }
    
    // If we made it here, command is pending.

    // Check first if command completed.
    Gahp_Args* result = get_pending_result(command, buf);

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
    }
    return rc;
}

int GahpClient::ec2_attach_volume(std::string service_url,
                              std::string publickeyfile,
                              std::string privatekeyfile,
                              std::string volume_id,
							  std::string instance_id, 
                              std::string device_id,
                              StringList & returnStatus,
                              std::string & error_code )
{
    static const char* command = "EC2_VM_ATTACH_VOLUME";

    int rc=0;
    
    // check if this command is supported
    if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    // check input arguments
    if ( service_url.empty() ||
		 publickeyfile.empty() ||
		 privatekeyfile.empty() ||
		 instance_id.empty() ||
		 volume_id.empty() ||
		 device_id.empty() ){
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    // Generate request line
    std::string reqline;

    char* esc1 = strdup( escapeGahpString(service_url) );
    char* esc2 = strdup( escapeGahpString(publickeyfile) );
    char* esc3 = strdup( escapeGahpString(privatekeyfile) );
    char* esc4 = strdup( escapeGahpString(volume_id) );
	char* esc5 = strdup( escapeGahpString(instance_id) );
    char* esc6 = strdup( escapeGahpString(device_id) );
    
    int x = formatstr(reqline, "%s %s %s %s %s %s", esc1, esc2, esc3, esc4, esc5, esc6 );
    
    free( esc1 );
    free( esc2 );
    free( esc3 );
    free( esc4 );
    free( esc5 );
	free( esc6 );
    ASSERT( x > 0 );
    
    const char *buf = reqline.c_str();
        
    
    if ( m_mode == results_only ) 
	{
		return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
	}
	else
	{
        now_pending(command, buf, deleg_proxy, medium_prio);
	}
    
    // Check first if command completed.
    Gahp_Args* result = get_pending_result(command, buf);

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
        
    }
    
    return rc;

}

//
// Spot instance support.
//
int GahpClient::ec2_spot_start( std::string service_url,
                                std::string publickeyfile,
                                std::string privatekeyfile,
                                std::string ami_id,
                                std::string spot_price,
                                std::string keypair,
                                std::string user_data,
                                std::string user_data_file,
                                std::string instance_type,
                                std::string availability_zone,
                                std::string vpc_subnet,
                                std::string vpc_ip,
                                std::string client_token,
                                std::string iam_profile_arn,
                                std::string iam_profile_name,
                                StringList & groupnames,
                                std::string & request_id,
                                std::string & error_code )
{
    static const char * command = "EC2_VM_START_SPOT";

    if( server->m_commands_supported->contains_anycase( command ) == FALSE ) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    if( service_url.empty()
     || publickeyfile.empty()
     || privatekeyfile.empty()
     || ami_id.empty()
     || spot_price.empty() ) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    if( keypair.empty() ) { keypair = NULLSTRING; }
    if( user_data.empty() ) { user_data = NULLSTRING; }
    if( user_data_file.empty() ) { user_data_file = NULLSTRING; }
    if( instance_type.empty() ) { instance_type = NULLSTRING; }
    if( availability_zone.empty() ) { availability_zone = NULLSTRING; }
    if( vpc_subnet.empty() ) { vpc_subnet = NULLSTRING; }
    if( vpc_ip.empty() ) { vpc_ip = NULLSTRING; }
    if( client_token.empty() ) { client_token = NULLSTRING; }
    if ( iam_profile_arn.empty() ) iam_profile_arn = NULLSTRING;
    if ( iam_profile_name.empty() ) iam_profile_name = NULLSTRING;

    std::string space = " ";
    std::string requestLine;
    requestLine += escapeGahpString( service_url ) + space;
    requestLine += escapeGahpString( publickeyfile ) + space;
    requestLine += escapeGahpString( privatekeyfile ) + space;
    requestLine += escapeGahpString( ami_id ) + space;
    requestLine += escapeGahpString( spot_price ) + space;
    requestLine += escapeGahpString( keypair ) + space;
    requestLine += escapeGahpString( user_data ) + space;
    requestLine += escapeGahpString( user_data_file ) + space;
    requestLine += escapeGahpString( instance_type ) + space;
    requestLine += escapeGahpString( availability_zone ) + space;
    requestLine += escapeGahpString( vpc_subnet ) + space;
    requestLine += escapeGahpString( vpc_ip ) + space;
    requestLine += escapeGahpString( client_token ) + space;
    requestLine += escapeGahpString( iam_profile_arn ) + space;
    requestLine += escapeGahpString( iam_profile_name );

    char * groups = groupnames.print_to_delimed_string( " " );
    if( groups != NULL ) {
        requestLine += space + groups;
    }
    free( groups );

    const char * arguments = requestLine.c_str();
    if( ! is_pending( command, arguments ) ) {
        if( m_mode == results_only ) {
            return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
        }
        now_pending( command, arguments, deleg_proxy, low_prio );
    }

    Gahp_Args * result = get_pending_result( command, arguments );
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
    }

    if( check_pending_timeout( command, arguments ) ) {
		formatstr( error_string, "%s timed out", command );
        return GAHPCLIENT_COMMAND_TIMED_OUT;
    }

    return GAHPCLIENT_COMMAND_PENDING;
}

int GahpClient::ec2_spot_stop(  std::string service_url,
                                std::string publickeyfile,
                                std::string privatekeyfile,
                                std::string request_id,
                                std::string & error_code )
{
    static const char * command = "EC2_VM_STOP_SPOT";
    
    if( server->m_commands_supported->contains_anycase( command ) == FALSE ) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    if( service_url.empty()
     || publickeyfile.empty()
     || privatekeyfile.empty()
     || request_id.empty() ) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    std::string space = " ";
    std::string requestLine;
    requestLine += escapeGahpString( service_url ) + space;
    requestLine += escapeGahpString( publickeyfile ) + space;
    requestLine += escapeGahpString( privatekeyfile ) + space;
    requestLine += escapeGahpString( request_id );

    const char * arguments = requestLine.c_str();
    if( ! is_pending( command, arguments ) ) {
        if( m_mode == results_only ) {
            return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
        }
        now_pending( command, arguments, deleg_proxy, medium_prio );
    }

    Gahp_Args * result = get_pending_result( command, arguments );        
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
    }

    if( check_pending_timeout( command, arguments ) ) {
		formatstr( error_string, "%s timed out", command );
        return GAHPCLIENT_COMMAND_TIMED_OUT;
    }
    
    return GAHPCLIENT_COMMAND_PENDING;
}

int GahpClient::ec2_spot_status(    std::string service_url,
                                    std::string publickeyfile,
                                    std::string privatekeyfile,
                                    std::string request_id,
                                    StringList & returnStatus,
                                    std::string & error_code )
{
    static const char * command = "EC2_VM_STATUS_SPOT";

    if( server->m_commands_supported->contains_anycase( command ) == FALSE ) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    if( service_url.empty()
     || publickeyfile.empty()
     || privatekeyfile.empty()
     || request_id.empty() ) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    std::string space = " ";
    std::string requestLine;
    requestLine += escapeGahpString( service_url ) + space;
    requestLine += escapeGahpString( publickeyfile ) + space;
    requestLine += escapeGahpString( privatekeyfile ) + space;
    requestLine += escapeGahpString( request_id );
    
    const char * arguments = requestLine.c_str();
    if( ! is_pending( command, arguments ) ) {
        if( m_mode == results_only ) {
            return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
        }
        now_pending( command, arguments, deleg_proxy, high_prio );
    }

    Gahp_Args * result = get_pending_result( command, arguments );        
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
    }

    if( check_pending_timeout( command, arguments ) ) {
		formatstr( error_string, "%s timed out", command );
        return GAHPCLIENT_COMMAND_TIMED_OUT;
    }
    
    return GAHPCLIENT_COMMAND_PENDING;
}

int GahpClient::ec2_spot_status_all(    std::string service_url,
                                        std::string publickeyfile,
                                        std::string privatekeyfile,
                                        StringList & returnStatus,
                                        std::string & error_code )
{
    static const char * command = "EC2_VM_STATUS_ALL_SPOT";

    if( server->m_commands_supported->contains_anycase( command ) == FALSE ) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    if( service_url.empty()
     || publickeyfile.empty()
     || privatekeyfile.empty() ) {
        return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
    }

    std::string space = " ";
    std::string requestLine;
    requestLine += escapeGahpString( service_url ) + space;
    requestLine += escapeGahpString( publickeyfile ) + space;
    requestLine += escapeGahpString( privatekeyfile );
    
    const char * arguments = requestLine.c_str();
    if( ! is_pending( command, arguments ) ) {
        if( m_mode == results_only ) {
            return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
        }
        now_pending( command, arguments, deleg_proxy, high_prio );
    }

    Gahp_Args * result = get_pending_result( command, arguments );        
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
    }
    
    if( check_pending_timeout( command, arguments ) ) {
		formatstr( error_string, "%s timed out", command );
        return GAHPCLIENT_COMMAND_TIMED_OUT;
    }
    
    return GAHPCLIENT_COMMAND_PENDING;    
}
