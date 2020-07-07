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

#ifndef _HIBERNATER_WINDOWS_H_
#define _HIBERNATER_WINDOWS_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include "hibernator.h"

/***************************************************************
 * MsWindowsHibernator class
 ***************************************************************/

class MsWindowsHibernator : public HibernatorBase
{

public:

	MsWindowsHibernator (void) noexcept;
	virtual ~MsWindowsHibernator (void) noexcept;

	/* Discover supported sleep states */
	virtual bool initialize( void );

protected:

	/* Override this to enter the given sleep state on a
	   particular OS */
	HibernatorBase::SLEEP_STATE enterStateStandBy(   bool force ) const;
	HibernatorBase::SLEEP_STATE enterStateSuspend(   bool force ) const;
	HibernatorBase::SLEEP_STATE enterStateHibernate( bool force ) const;
	HibernatorBase::SLEEP_STATE enterStatePowerOff(  bool force ) const;

private:

	/* Auxiliary function to shutdown the machine */
	bool tryShutdown ( bool force ) const;

};

#define HIBERNATOR_TYPE_DEFINED	1
typedef MsWindowsHibernator	RealHibernator;

#endif // _HIBERNATER_WINDOWS_H_
