#ifndef _CONDOR_SETUP_REPLY_H
#define _CONDOR_SETUP_REPLY_H

// #include "condor_common.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "SetupReply.h"

class SetupReply : public Functor {
	public:
		SetupReply( ClassAd * r, GahpClient * g, GahpClient * h, const std::string & ss, ClassAd * s, Stream * rs, ClassAdCollection *, const std::string & cid ) :
			reply(r), replyStream(rs), cfGahp(g), ec2Gahp(h), successString(ss), scratchpad(s), commandID(cid) { }
		virtual ~SetupReply() { }

		virtual int operator() ();
		virtual int rollback();

	private:
		ClassAd * reply;
		Stream * replyStream;
		GahpClient * cfGahp;
		GahpClient * ec2Gahp;
		std::string successString;
		ClassAd * scratchpad;
		std::string commandID;
};

#endif /* _CONDOR_SETUP_REPLY_H */
