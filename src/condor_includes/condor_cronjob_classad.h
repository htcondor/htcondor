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

#ifndef _CONDOR_CRONJOB_CLASSAD_H
#define _CONDOR_CRONJOB_CLASSAD_H

#include "condor_cronjob.h"
#include "env.h"

// Define a "ClassAd" 'Cron' job
class ClassAdCronJob : public CronJobBase
{
  public:
	ClassAdCronJob( const char *mgrName, const char *jobName );
	virtual ~ClassAdCronJob( );
	int Initialize( void );

  private:
	virtual int ProcessOutput( const char *line );
	virtual int Publish( const char *name, ClassAd *ad ) = 0;

	ClassAd		*OutputAd;
	int			OutputAdCount;

	Env         classad_env;
	MyString	mgrNameUc;
};

#endif /* _CONDOR_CRONJOB_CLASSAD_H */
