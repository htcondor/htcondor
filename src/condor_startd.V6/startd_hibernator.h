/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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

#ifndef _STARTD_HIBERNATER_H_
#define _STARTD_HIBERNATER_H_

#include "hibernator.h"


/***************************************************************
 * Startd Hibernator class
 ***************************************************************/

class StartdHibernator : public HibernatorBase
{
  public:

	StartdHibernator(void) noexcept;
	virtual ~StartdHibernator(void) noexcept;

	/* Discover supported sleep states */
	bool initialize( void );

	const char *getMethod(void) const { return "STARTD"; };
	bool update( void );

	// Private methods
  private:

	/* Override this to enter the given sleep state on a
	   particular OS */
	HibernatorBase::SLEEP_STATE enterStateStandBy(   bool force ) const;
	HibernatorBase::SLEEP_STATE enterStateSuspend(   bool force ) const;
	HibernatorBase::SLEEP_STATE enterStateHibernate( bool force ) const;
	HibernatorBase::SLEEP_STATE enterStatePowerOff(  bool force ) const;
	HibernatorBase::SLEEP_STATE enterState( SLEEP_STATE state, bool force )
		const;

	// Private data
  private:
	std::string	 m_plugin_path;
	std::vector<std::string> m_plugin_args;
	ClassAd		 m_ad;
	
};

#define HIBERNATOR_TYPE_DEFINED	1

#endif // _STARTD_HIBERNATER_H_
