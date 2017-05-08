#ifndef _CONDOR_BULKREQUEST_H
#define _CONDOR_BULKREQUEST_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "BulkRequest.h"

class BulkRequest : public Functor {
	public:
		BulkRequest( ClassAd * r, EC2GahpClient * egc, ClassAd * s,
			const std::string & su, const std::string & pkf,
			const std::string & skf, ClassAdCollection * c,
			const std::string & commandID, const std::string & annexID );
		virtual ~BulkRequest() { }

		virtual int operator() ();
		virtual int rollback();

		bool validateAndStore( ClassAd const * command, std::string & validationError );
		void log();


	protected:
		EC2GahpClient * gahp;
		ClassAd * reply;
		ClassAd * scratchpad;

		std::string service_url, public_key_file, secret_key_file;
		std::string client_token, spot_price, target_capacity;
		std::string iam_fleet_role, allocation_strategy, valid_until;

		std::vector< std::string > launch_specifications;

		std::string commandID;
		std::string bulkRequestID;
		ClassAdCollection * commandState;
};

#endif /* _CONDOR_BULKREQUEST_H */

