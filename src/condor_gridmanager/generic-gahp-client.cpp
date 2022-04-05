#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "gahp-client.h"

int
GenericGahpClient::callGahpFunction(
	const char * command,
	const std::vector< YourString > & arguments,
	Gahp_Args * & result,
	PrioLevel priority
) {
	// check if this command is supported
	if( server->m_commands_supported->contains_anycase( command ) == FALSE ) {
		return GAHPCLIENT_COMMAND_NOT_SUPPORTED;
	}

	// don't check the input arguments, we don't know anything about them.
	// in the future, we could register check functions for each command.

	// generate request line - do a single 1K allocation.
	std::string reqline( 1024, '\0' );
	// if this causes a reallocation I will be very upset.
	reqline.clear();
	for( unsigned i = 0; i < arguments.size(); ++i ) {
		const char * ptr = arguments[i].ptr();
		// Arguably, we should EXCEPT() on a NULL ptr.
		if( ptr == NULL || ptr[0] == '\0' ) {
			reqline += NULLSTRING;
		} else {
			reqline += escapeGahpString( ptr );
		}
		reqline += " ";
	}
	reqline.erase( reqline.size() - 1 );

	// If the command is not currently pending, make it the pending request.
	const char * buf = reqline.c_str();
	if( ! is_pending( command, buf ) ) {
		if( m_mode == results_only ) {
			return GAHPCLIENT_COMMAND_NOT_SUBMITTED;
		}
		now_pending( command, buf, deleg_proxy, priority );
	}

	// Has the command completed?
	result = get_pending_result( command, buf );
	if( result ) {
		return 0;
	}

	// Did the pending command time out?
	if( check_pending_timeout( command, buf ) ) {
		formatstr( error_string, "%s timed out", command );
		return GAHPCLIENT_COMMAND_TIMED_OUT;
	}

	// If we made it here, the command is still pending.
	return GAHPCLIENT_COMMAND_PENDING;
}

