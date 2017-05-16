#ifndef _CONDOR_GET_SFRS_H
#define _CONDOR_GET_SFRS_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "GetSFRs.h"

class GetSFRs : public Functor {
	public:
		GetSFRs( ClassAd * r,
			EC2GahpClient * g, ClassAd * s,
				const std::string & su, const std::string & pkf, const std::string & skf,
			ClassAdCollection * c, const std::string & cid ) :
			reply( r ),
			gahp( g ), scratchpad( s ),
				service_url( su ), public_key_file( pkf ), secret_key_file( skf ),
			commandState( c ), commandID( cid )
		{ }

		virtual ~GetSFRs() { }

		virtual int operator() ();
		virtual int rollback();

	private:
		ClassAd * reply;
		EC2GahpClient * gahp;
		ClassAd * scratchpad;

		std::string service_url, public_key_file, secret_key_file;

		ClassAdCollection * commandState;
		std::string commandID;
};

#endif /* _CONDOR_GET_SFRS_H */
