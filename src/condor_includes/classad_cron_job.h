/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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

#ifndef _CLASSAD_CRON_JOB_H
#define _CLASSAD_CRON_JOB_H

#include "condor_cron_job.h"
#include "condor_cron_job_params.h"
#include "env.h"

// Define a "ClassAd" cron job parameter object
class ClassAdCronJobParams : public CronJobParams
{
  public:
	ClassAdCronJobParams( const char		*job_name,
						  const CronJobMgr	&mgr );
	virtual ~ClassAdCronJobParams( void ) { };

	// Finish initialization
	virtual bool Initialize( void );

	// Get parameters
	const std::string &GetConfigValProg( void ) const {
		return m_config_val_prog;
	};
	const std::string &GetMgrNameUc( void ) const {
		return m_mgr_name_uc;
	};

  private:
	std::string m_config_val_prog;
	std::string m_mgr_name_uc;
};

// Define a "ClassAd" 'Cron' job
class ClassAdCronJob : public CronJob
{
  public:
	ClassAdCronJob( ClassAdCronJobParams *params, CronJobMgr &mgr );
	virtual ~ClassAdCronJob( );
	int Initialize( void );

  private:
	virtual int ProcessOutput( const char *line );
	virtual int ProcessOutputSep( const char *args );
	virtual int Publish( const char *name, const char *sep_args, ClassAd *ad ) = 0;
	virtual const ClassAdCronJobParams & Params( void ) const {
		return static_cast<ClassAdCronJobParams &>(*m_params);
	};
	virtual ClassAdCronJobParams & RwParams( void ) const {
		return static_cast<ClassAdCronJobParams &>(*m_params);
	};

	ClassAd		*m_output_ad;
	int 		 m_output_ad_count; // number of attributes inserted in to m_output_ad
	std::string	 m_output_ad_args;  // optional arguments from after the '-' that separates ads in the script output

	Env          m_classad_env;
};

#endif /* _CLASSAD_CRON_JOB_H */
