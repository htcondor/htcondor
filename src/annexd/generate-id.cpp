#include <string>
#include "generate-id.h"
#include <uuid/uuid.h>
#include "stl_string_utils.h"

// Stolen from EC2Job::build_client_token in condor_gridmanager/ec2job.cpp.
void
generateUUID( std::string & commandID ) {
	char uuid_str[37];
	uuid_t uuid;
	uuid_generate( uuid );
	uuid_unparse( uuid, uuid_str );
	uuid_str[36] = '\0';
	commandID.assign( uuid_str );
}

void
generateCommandID( std::string & commandID ) {
	generateUUID( commandID );
}

void
generateAnnexID( std::string & annexID ) {
	generateUUID( annexID );
	annexID.resize( 27 );
}

void
generateClientToken(	const std::string & annexID,
						std::string & clientToken ) {
	//
	// Client tokens must be no more than 64 (ASCII) characters long, and
	// they must be unique across daemon restarts.  Rather than use a
	// counter (or whatever) and add more hard state, we just use a UUID.
	//
	// FIXME: enforce the 27-character limit at the command-line.  Otherwise,
	// we can't rely on the annex IDs being unique (on a per-user basis).
	//
	// Our current set-up uses one rule per annex, so with the default
	// limits, each user account can support fifty annexes.  Additional
	// cleverness may allow us to add up to four more targets per rule
	// (but it's unclear how we'd consistently map a target annex name
	// to the same rule).
	//
	// It shouldn't be too hard to convert the lease function to accept
	// a list of annex-ID / expiration-date pairs, but updating the
	// argument list in the target seems like an even better way than
	// the preceding to have race conditions.
	//
	// We insert the '_' in the middle so that we can reliably identify the
	// beginning of the UUID (the last place an underscore appears), which
	// is necessarily the end of the annex ID, which we want to use in
	// the instance to select a config tarball to download.
	//
	generateUUID( clientToken );
	formatstr( clientToken, "%.27s_%.36s", annexID.c_str(), clientToken.c_str() );
}
