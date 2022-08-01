#ifndef _CONDOR_STATUS_REPLY_H
#define _CONDOR_STATUS_REPLY_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "StatusReply.h"

class StatusReply : public Functor {
	public:
		StatusReply( ClassAd * r, ClassAd * cad,
			EC2GahpClient * g, ClassAd * s, bool w,
			ClassAdCollection *, const std::string & cid ) :
			reply( r ), command( cad ),
			gahp( g ), scratchpad( s ), wantClassAds( w ),
			commandID( cid )
		{ }

		virtual ~StatusReply() { }

		virtual int operator() ();
		virtual int rollback();

	private:
		ClassAd * reply;
		ClassAd * command;
		EC2GahpClient * gahp;
		ClassAd * scratchpad;

		bool wantClassAds;

		std::string commandID;
};

#endif /* _CONDOR_STATUS_REPLY_H */
