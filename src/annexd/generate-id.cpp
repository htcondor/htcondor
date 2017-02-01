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
	//
	// Each client token must be unique, even across daemon restarts.  We
	// therefore can't just use a counter, unless we want to add more hard
	// state.  However, the following obvious approach truncates eight of
	// the second UUID's sixteen bytes.  This seems unlikely to cause problems,
	// but if it does, we could compact the annex ID where we make it and
	// the client randomizer here by eliding the dashes in their ASCII.
	//
	// The preceding text is obsolete now that we've made the annex ID the
	// (user-supplied) annex name.  This simplifies identifying which of the
	// bits on EC2 belong to which annex, and potentially makes room for
	// more of the uniquifier we're adding.  (We could actually enforce
	// that annex names are only 25 characters long.)
	//
	// However, because of the cap on the number of rules, we'd like to
	// be able to use one rule per annex; the easiest way to do that is
	// to have the rule and target be identified by the annex in question.
	// This should mostly already just work, with the last annex command
	// to that name winning.  That would get us up to a number of annexes
	// equal to the number of permitted Rules (50, by default); additional
	// cleverness may allow us to use multiple Targets per rule (max 5?).
	//
	// We could even put a list in each target (with some rewriting) of
	// prefix/lease-expiration pairs, but race conditions there might be
	// a serious problem.  More investigation required.
	//
	// The more serious multi-annex usability issue relates to the config
	// tarball, but that's for another comment...
	//
	generateUUID( clientToken );
	clientToken = annexID + clientToken;
	clientToken.resize( 64 );
}

