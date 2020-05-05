#ifndef _CONDOR_GCE_INSERT_INSTANCE_H
#define _CONDOR_GCE_INSERT_INSTANCE_H

// #include "Functor.h"

class GCEInstanceInsert : public Functor {
	public:
		GCEInstanceInsert( GahpClient * gceGahpClient,
		    ClassAd * reply, ClassAd * scratchpad,
		    const std::array< std::string, 11 > & theMany,
		    const std::vector< std::pair< std::string, std::string > > & labels );
		virtual ~GCEInstanceInsert() { }

		virtual int operator() ();
		virtual int rollback();

	protected:
		ClassAd * reply;
		ClassAd * scratchpad;
		GahpClient * gahpClient;

        std::string serviceURL, authFile, account, project, zone;
        std::string instanceName, machineType, imageName;
        std::string metadata, metadataFile, jsonFile;
        std::vector< std::pair< std::string, std::string > > labels;
};

#endif /* _CONDOR_GCE_INSERT_INSTANCE_H */
