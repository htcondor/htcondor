#ifndef _CONDOR_REPLY_AND_CLEAN_H
#define _CONDOR_REPLY_AND_CLEAN_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "ClassAdReplyCleanup.h"

class ReplyAndClean : public Functor {
	public:
		ReplyAndClean( ClassAd * r, Stream * rs, GahpClient * g, ClassAd * s, GahpClient * eg ) :
			reply(r), replyStream(rs), gahp(g), scratchpad(s), eventsGahp(eg) { }
		virtual ~ReplyAndClean() { }

		virtual int operator() ();
		virtual int rollback();

	private:
		ClassAd * reply;
		Stream * replyStream;
		GahpClient * gahp;
		ClassAd * scratchpad;
		GahpClient * eventsGahp;
};

#endif /* _CONDOR_REPLY_AND_CLEAN_H */
