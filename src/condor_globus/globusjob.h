
#ifndef GLOBUSJOB_H
#define GLOBUSJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "globus_utils.h"

#define JM_COMMIT_TIMEOUT	300

class GlobusJob
{
 public:

	GlobusJob( GlobusJob& copy );
	GlobusJob( ClassAd *classad );

	~GlobusJob();

	bool start();
	bool commit();
	bool stop_job_manager();
	bool callback( int state = 0, int error = 0 );
	bool cancel();
	bool probe();
	bool callback_register();
	bool restart();

	const char *errorString();

	char *buildRSL( ClassAd *classad );

	PROC_ID procID;
	char *jobContact;
	int jobState;	// this is the Globus status, not Condor job status
	char *RSL;
	char *rmContact;
	char *localOutput;
	char *localError;
	int errorCode;
	int jmFailureCode;
	char *userLogFile;
	bool removedByUser;
	int exitValue;
	bool executeLogged;
	bool exitLogged;
	bool stateChanged;
	bool newJM;		// This means a jobmanager that supports restart
					// and two-phase commit
	bool restartingJM;
	time_t restartWhen;
	bool durocRequest;
	int shadow_birthday;

 protected:
	bool callbackRegistered;
	bool ignore_callbacks;
	//Classad *ad;
};

#endif
