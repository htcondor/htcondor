/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
