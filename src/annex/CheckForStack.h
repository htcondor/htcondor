#ifndef _CONDOR_CHECK_FOR_STACK_H
#define _CONDOR_CHECK_FOR_STACK_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "CheckForStack.h"

class CheckForStack : public Functor {
	public:
		CheckForStack( ClassAd * r, EC2GahpClient * g, ClassAd * s,
			const std::string & su, const std::string & pkf, const std::string & skf,
			const std::string & sn, const std::string & sd,
			ClassAdCollection *, const std::string & cid ) :
			reply( r ), cfGahp( g ), scratchpad( s ),
			service_url( su ), public_key_file( pkf ), secret_key_file( skf ),
			stackName( sn ), stackDescription( sd ), introduced( false ),
			commandID( cid )
		{ }
		virtual ~CheckForStack() { }

		virtual int operator() ();
		virtual int rollback();

	private:
		ClassAd * reply;
		EC2GahpClient * cfGahp;
		ClassAd * scratchpad;

		std::string service_url, public_key_file, secret_key_file;

		std::string stackName, stackDescription;
		bool introduced;

		std::string commandID;
};

#endif /* _CONDOR_CREATE_CHECK_FOR_STACK_H */
