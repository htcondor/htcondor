
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

	bool submit( const char *callback_contact );
	bool cancel();
	bool probe();

	const char *errorString();

	PROC_ID procID;
	char *jobContact;
	int jobState;
	char *RSL;
	char *rmContact;
	int errorCode;
	char *userLogFile;
	bool updateSchedd;
};

#endif
