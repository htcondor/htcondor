#include <string>
#include "generate-id.h"
#include <uuid/uuid.h>

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
}

void
generateClientToken(	const std::string & annexID,
						std::string & clientToken ) {
	// Each client token must be unique, even across daemon restarts.  We
	// therefore can't just use a counter, unless we want to add more hard
	// state.  However, the following obvious approach truncates eight of
	// the second UUID's eight bytes.  This seems unlikely to cause problems,
	// but if it does, we could compact the annex ID where we make it and
	// the client randomizer here by eliding the dashes in their ASCII.
	generateUUID( clientToken );
	clientToken = annexID + clientToken;
	clientToken.resize( 64 );
}

