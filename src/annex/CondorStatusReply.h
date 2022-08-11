#ifndef _CONDOR_CONDOR_STATUS_REPLY_H
#define _CONDOR_CONDOR_STATUS_REPLY_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "StatusReply.h"

class CondorStatusReply : public Functor {
	public:
		CondorStatusReply( ClassAd * r, ClassAd *,
			EC2GahpClient * g, ClassAd * s,
			ClassAdCollection *, const std::string & cid,
			int _argc, char ** _argv, int sci ) :
			reply( r ),
			gahp( g ), scratchpad( s ),
			commandID( cid ),
			argc( _argc ), argv( _argv ), subCommandIndex( sci )
		{ }

		virtual ~CondorStatusReply() { }

		virtual int operator() ();
		virtual int rollback();

	private:
		ClassAd * reply;
		EC2GahpClient * gahp;
		ClassAd * scratchpad;

		std::string commandID;

		int argc;
		char ** argv;
		unsigned subCommandIndex;
};

#endif /* _CONDOR_CONDOR_STATUS_REPLY_H */
