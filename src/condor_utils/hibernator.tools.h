/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

#ifndef _HIBERNATER_USER_DEFINED_TOOLS_H_
#define _HIBERNATER_USER_DEFINED_TOOLS_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include "condor_common.h"
#include "condor_arglist.h"
#include "MyString.h"
#include <condor_daemon_core.h>
#include "hibernator.h"

/***************************************************************
 * UserDefinedToolsHibernator class
 ***************************************************************/

class UserDefinedToolsHibernator : public Service, public HibernatorBase
{

public:

	UserDefinedToolsHibernator ( const MyString &keyword ) noexcept;
	virtual ~UserDefinedToolsHibernator () noexcept;

	/** Get the name of the hibernation method used */
	const char* getMethod ( void ) const;

	

protected:

	/** Override this to enter the given sleep state on a 
		particular OS */
	HibernatorBase::SLEEP_STATE enterStateStandBy ( bool force ) const;
	HibernatorBase::SLEEP_STATE enterStateSuspend ( bool force ) const;
	HibernatorBase::SLEEP_STATE enterStateHibernate ( bool force ) const;
	HibernatorBase::SLEEP_STATE enterStatePowerOff ( bool force ) const;

	int userDefinedToolsHibernatorReaper ( int pid, int );

private:

	

	/** Forbid the use of a default ctor */
	UserDefinedToolsHibernator () noexcept;

	/** Configure supported sleep states */
	void configure ();

	/** Helper function called by all the above "enter" methods */
	HibernatorBase::SLEEP_STATE enterState ( HibernatorBase::SLEEP_STATE state ) const;
	
	/** Reaper that reaps the child; the caller expects no output */
	int hibernatorReaper ( int pid, int exit_status );

	/** User defined tool information */
	MyString m_keyword;
	char	 *m_tool_paths[11];
	ArgList	 m_tool_args[11];

	/** DC reaper ID. @see reaperOutput() */
	int m_reaper_id;

};

#endif // _HIBERNATER_USER_DEFINED_TOOLS_H_
