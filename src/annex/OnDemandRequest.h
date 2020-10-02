#ifndef _CONDOR_ONDEMANDREQUEST_H
#define _CONDOR_ONDEMANDREQUEST_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "OnDemandRequest.h"

class OnDemandRequest : public Functor {
	public:
		OnDemandRequest( ClassAd * r, EC2GahpClient * egc, ClassAd * s,
			const std::string & su, const std::string & pkf,
			const std::string & skf, ClassAdCollection * c,
			const std::string & commandID, const std::string & aid,
			const std::vector< std::pair< std::string, std::string > > & t );
		virtual ~OnDemandRequest() { }

		virtual int operator() ();
		virtual int rollback();

		bool validateAndStore( ClassAd const * command, std::string & validationError );
		void log();


	protected:
		EC2GahpClient * gahp;
		ClassAd * reply;
		ClassAd * scratchpad;

		std::string service_url, public_key_file, secret_key_file;

		int targetCapacity;
		std::string clientToken;
		std::string instanceType;
		std::string imageID;
		std::string instanceProfileARN;
		std::vector< std::string > instanceIDs;
		std::string keyName;
		std::string securityGroupIDs;

		std::string commandID;
		std::string bulkRequestID;
		ClassAdCollection * commandState;

		std::string annexID;

		std::vector< std::pair< std::string, std::string > > tags;
};

#endif /* _CONDOR_ONDEMANDREQUEST_H */

