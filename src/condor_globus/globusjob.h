
#ifndef GLOBUSJOB_H
#define GLOBUSJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "globus_utils.h"


class GlobusJob
{
 public:

	GlobusJob( GlobusJob& copy );
	GlobusJob( ClassAd *classad );

	~GlobusJob();

	bool start();
	bool callback( int state = 0, int error = 0 );
	bool cancel();
	bool probe();
	bool callback_register();

	const char *errorString();

	char *buildRSL( ClassAd *classad );

	PROC_ID procID;
	char *jobContact;
	int jobState;
	char *RSL;
	char *rmContact;
	int errorCode;
	char *userLogFile;

 protected:
	bool callback_already_registered;
};

#endif
