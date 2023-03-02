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

#ifndef _CONDOR_CRON_PARAM_H
#define _CONDOR_CRON_PARAM_H

#include "condor_common.h"

class CronParamBase
{
  public:
	CronParamBase( const char &base );
	virtual ~CronParamBase( void ) { };
	char * Lookup( const char * ) const;

	// Virtual member functions
	bool Lookup( const char *name, std::string &s ) const;
	bool Lookup( const char *name, bool &value ) const;
	bool Lookup( const char	*item,
				 double		&value,
				 double		 default_value,
				 double		 min_value,
				 double		 max_value ) const;

  protected:
	const char			&m_base;
	mutable char		 m_name_buf[128];

	// Virtual Protected member functions
	virtual const char *GetParamName( const char *item ) const;
	virtual char *GetDefault( const char * /*item*/ ) const;
	virtual void GetDefault( const char * /*item*/, double & /*dv*/ ) const;
};

#endif /* _CONDOR_CRON_PARAM_H */
