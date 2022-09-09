#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "gahp_common.h"
#include "boincjob.h"

#include "gahp-client.h"

using std::set;
using std::vector;
using std::pair;
using std::string;

int GahpClient::boinc_submit( const char *batch_name,
							  const std::set<BoincJob *> &jobs )
{
	static const char* command = "BOINC_SUBMIT";

		// Check if this command is supported
	if  (server->m_commands_supported->contains_anycase(command)==FALSE) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	ASSERT( !jobs.empty() );
	int job_cnt = jobs.size();

		// Generate request line
	if (!batch_name) batch_name=NULLSTRING;
	std::string reqline;
	reqline = escapeGahpString( batch_name );
	formatstr_cat( reqline, " %s %d", escapeGahpString( (*jobs.begin())->GetAppName() ),
				   job_cnt );
	for (auto job : jobs) {
		ArgList *args_list = job->GetArgs();
		char **args = args_list->GetStringArray();
		int arg_cnt = args_list->Count();
		formatstr_cat( reqline, " %s %d", job->remoteJobName, arg_cnt );
		for ( int i = 0; i < arg_cnt; i++ ) {
			reqline += " ";
			reqline += escapeGahpString( args[i] );
		}
		deleteStringArray( args );
		delete args_list;

		vector<pair<string, string> > inputs;
		job->GetInputFilenames( inputs );
		formatstr_cat( reqline, " %d", (int)inputs.size() );
		for (auto & input : inputs) {
			reqline += " ";
			reqline += escapeGahpString( input.first );
			reqline += " ";
			reqline += escapeGahpString( input.second );
		}
	}


        if ( (*jobs.begin())->GetVar("rsc_fpops_est")  != "") { 
            reqline += " ";
            reqline += escapeGahpString( (*jobs.begin())->GetVar("rsc_fpops_est") );
        } else 
            reqline += " NULL";

        if ( (*jobs.begin())->GetVar("rsc_fpops_bound")  != "") { 
            reqline += " ";
            reqline += escapeGahpString( (*jobs.begin())->GetVar("rsc_fpops_bound") );
        } else 
            reqline += " NULL";

        if ( (*jobs.begin())->GetVar("rsc_memory_bound")  != "") { 
            reqline += " ";
            reqline += escapeGahpString( (*jobs.begin())->GetVar("rsc_memory_bound") );
        } else 
            reqline += " NULL";

        if ( (*jobs.begin())->GetVar("rsc_disk_bound")  != "") { 
            reqline += " ";
            reqline += escapeGahpString( (*jobs.begin())->GetVar("rsc_disk_bound") );
        } else 
            reqline += " NULL";

        if ( (*jobs.begin())->GetVar("delay_bound")  != "") { 
            reqline += " ";
            reqline += escapeGahpString( (*jobs.begin())->GetVar("delay_bound") );
        } else 
            reqline += " NULL";

        if ( (*jobs.begin())->GetVar("app_version_num")  != "") { 
            reqline += " ";
            reqline += escapeGahpString( (*jobs.begin())->GetVar("app_version_num") );
        } else 
            reqline += " NULL";

	const char *buf = reqline.c_str();

		// Check if this request is currently pending.  If not, make
		// it the pending request.
	if ( !is_pending( command, buf ) ) {
		// Command is not pending, so go ahead and submit a new one
		// if our command mode permits.
		if ( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending( command, buf );
	}

		// If we made it here, command is pending.
		
		// Check first if command completed.
	Gahp_Args* result = get_pending_result(command,buf);
	if ( result ) {
		// command completed.
		if ( result->argc != 2 ) {
			EXCEPT( "Bad %s Result", command );
		}
		int rc;
		if ( strcmp( result->argv[1], NULLSTRING ) == 0 ) {
			rc = 0;
		} else {
			rc = 1;
			error_string = result->argv[1];
		}

		delete result;
		return rc;
	}

		// Now check if pending command timed out.
	if ( check_pending_timeout( command, buf ) ) {
		// pending command timed out.
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

		// If we made it here, command is still pending...
	return GAHPCLIENT_COMMAND_PENDING;
}
