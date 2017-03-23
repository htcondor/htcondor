#ifndef _CONDOR_SETUP_REPLY_H
#define _CONDOR_SETUP_REPLY_H

// #include "condor_common.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "SetupReply.h"

class SetupReply : public Functor {
	public:
		SetupReply( ClassAd * r, GahpClient * g, ClassAd * s, Stream * rs, ClassAdCollection * cac, const std::string & cid ) :
			reply(r), replyStream(rs), cfGahp(g), scratchpad(s), commandState(cac), commandID(cid) { }
		virtual ~SetupReply() { }

		virtual int operator() ();
		virtual int rollback();

	private:
		ClassAd * reply;
		Stream * replyStream;
		GahpClient * cfGahp;
		ClassAd * scratchpad;
		GahpClient * eventsGahp;
		ClassAdCollection * commandState;
		std::string commandID;
};

#endif /* _CONDOR_SETUP_REPLY_H */
