#ifndef _CONDOR_UPLOAD_FILE_H
#define _CONDOR_UPLOAD_FILE_H

// #include "condor_common.h"
// #include "compat_classad.h"
// #include "classad_collection.h"
// #include "gahp-client.h"
// #include "Functor.h"
// #include "UploadFile.h"

class UploadFile : public Functor {
	public:
		UploadFile( const::std::string & f, const std::string & t,
			ClassAd * r, EC2GahpClient * g, ClassAd *,
			const std::string & su, const std::string & pkf, const std::string & skf,
			ClassAdCollection * c, const std::string & cid, const std::string & aid ) :
			uploadFrom( f ), uploadTo( t ),
			reply( r ), gahp( g ),
            service_url( su ), public_key_file( pkf ), secret_key_file( skf ),
            commandID( cid ), commandState( c ), annexID( aid )
		{ ASSERT( (!uploadFrom.empty()) && (!uploadTo.empty()) ); }

		virtual ~UploadFile() { }

		virtual int operator() ();
		virtual int rollback();

	private:
		std::string uploadFrom;
		std::string uploadTo;

		ClassAd * reply;
		EC2GahpClient * gahp;

		std::string service_url, public_key_file, secret_key_file;

		std::string commandID;
		ClassAdCollection * commandState;
		std::string annexID;
};

#endif /* _CONDOR_UPLOAD_FILE_H */
