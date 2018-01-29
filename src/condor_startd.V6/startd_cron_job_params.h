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

#ifndef _STARTD_CRON_JOB_PARAMS_H
#define _STARTD_CRON_JOB_PARAMS_H

#include "classad_cron_job.h"
#include <list>

// Define a "ClassAd" cron job parameter object
class StartdCronJobParams : public ClassAdCronJobParams
{
  public:
	class Metric {
		public:
			typedef double (*Operator)(double a, double b);

			Metric() : _o(NULL), _t(NULL) { }
			Metric( Operator o, const char * t ) : _o(o), _t(t) { }
			double operator()( double a, double b ) { return (*_o)(a, b); }
			const char * c_str() { return _t; }
		private:
			Operator _o;
			const char * _t;
	};
  	typedef std::map< std::string, Metric > Metrics;

	StartdCronJobParams( const char			*job_name,
						 const CronJobMgr	&mgr );
	~StartdCronJobParams( void ) { };

	// Finish initialization
	bool Initialize( void );
	bool InSlotList( unsigned slot ) const;

	bool isMetric( const std::string & attributeName ) const;
	bool addMetric( const char * metricType, const char * resourceName );
	bool getMetric( const std::string & attributeName, Metric & m ) const;
	static bool getResourceNameFromAttributeName( const std::string & attributeName, std::string & resourceName );
	bool isResourceMonitor( void ) const { return metrics.size() > 0; }

  private:
  	Metrics metrics;

	std::list<unsigned>	m_slots;
};

#endif /* _STARTD_CRON_JOB_PARAMS_H */
