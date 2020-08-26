#ifndef _CONDOR_REPLY_AND_CLEAN_H
#define _CONDOR_REPLY_AND_CLEAN_H

// #include "condor_common.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "ReplyAndClean.h"

class ReplyAndClean : public Functor {
	public:
		ReplyAndClean( ClassAd * r, Stream * rs, GahpClient * g, ClassAd * s, GahpClient * eg, ClassAdCollection * cac, const std::string & cid, GahpClient * lg, GahpClient * s3g ) :
			reply(r), replyStream(rs), gahp(g), scratchpad(s), eventsGahp(eg), commandState(cac), commandID(cid), lambdaGahp(lg), s3Gahp(s3g) { }
		virtual ~ReplyAndClean() { }

		virtual int operator() ();
		virtual int rollback();

	private:
		ClassAd * reply;
		Stream * replyStream;
		GahpClient * gahp;
		ClassAd * scratchpad;
		GahpClient * eventsGahp;
		ClassAdCollection * commandState;
		std::string commandID;
		GahpClient * lambdaGahp;
		GahpClient * s3Gahp;
};

#endif /* _CONDOR_REPLY_AND_CLEAN_H */
