/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef NORDUGRIDJOB_H
#define NORDUGRIDJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "classad_hashtable.h"

#include "ftp_lite.h"

#include "basejob.h"
#include "nordugridresource.h"

void NordugridJobInit();
void NordugridJobReconfig();
bool NordugridJobAdMustExpand( const ClassAd *jobad );
BaseJob *NordugridJobCreate( ClassAd *jobad );
extern const char *NordugridJobAdConst;

class NordugridResource;

class NordugridJob : public BaseJob
{
 public:

	NordugridJob( ClassAd *classad );

	~NordugridJob();

	void Reconfig();
	int doEvaluateState();
	BaseResource *GetResource();

	static int probeInterval;
	static int submitInterval;

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }
	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }

	int gmState;
	time_t lastProbeTime;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;

	MyString errorString;
	char *resourceManagerString;

	char *remoteJobId;
	int remoteJobState;

	ftp_lite_server *ftp_srvr;
	NordugridResource *myResource;

		// Used by doStageIn() and doStageOut()
	StringList *stage_list;

		// These get set before file stage out, but don't get handed
		// to JobTerminated() until after file stage out succeeds.
	int exitCode;
	bool normalExit;

	int doSubmit( char *&job_id );
	int doStatus( int &new_remote_state );
	int doRemove();
	int doStageIn();
	int doStageOut();
	int doExitInfo();
	int doStageInQuery( bool &need_stage_in );
	int doList( const char *dir_name, StringList *&dir_list );

	MyString *buildSubmitRSL();

 protected:
};

#endif

