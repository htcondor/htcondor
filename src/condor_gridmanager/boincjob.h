/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef BOINCJOB_H
#define BOINCJOB_H

#include "condor_common.h"
#include "condor_classad.h"

#include "basejob.h"
#include "boincresource.h"
#include "gahp-client.h"

class BoincResource;

void BoincJobInit();
void BoincJobReconfig();
BaseJob *BoincJobCreate( ClassAd *jobad );
bool BoincJobAdMatch( const ClassAd *job_ad );

class BoincJob : public BaseJob
{
 public:

	BoincJob( ClassAd *classad );

	~BoincJob();

	void Reconfig();
	void doEvaluateState();
	void NewBoincState( const char *new_state );
	BaseResource *GetResource();
	void SetRemoteBatchName( const char *batch_name );

	void BuildOutputInfo( std::string &iwd, std::string &std_err,
						  bool &transfer_all,
						  GahpClient::BoincOutputFiles &outputs );
	std::string GetAppName();
	std::string GetVar(const char *str);
	ArgList *GetArgs();
	void GetInputFilenames( std::vector<std::pair<std::string, std::string> > &files );

	static int gahpCallTimeout;
	static int maxConnectFailures;

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )
		{ maxConnectFailures = count; }

	// New variables
	int gmState;
	std::string remoteState;
	BoincResource *myResource;
	time_t enteredCurrentGmState;
	char *m_serviceUrl;
	int connectFailureCount;

	GahpClient *gahp;

	char *remoteBatchName;
	char *remoteJobName;
	std::string errorString;
	std::string gahpErrorString;

private:
};

#endif

