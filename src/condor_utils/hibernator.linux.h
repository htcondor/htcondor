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

#ifndef _HIBERNATER_LINUX_H_
#define _HIBERNATER_LINUX_H_

#include "hibernator.h"


/***************************************************************
 * Linux Hibernator class
 ***************************************************************/

/* Do we have the ability to test WOL? */
# if defined(LINUX)
#   define HAVE_LINUX_HIBERNATION	1
#   define HAVE_HIBERNATION			1
# endif

# if HAVE_LINUX_HIBERNATION

// Internal class to implement the various ways of interacting with the
// Linux hibernation methods
class BaseLinuxHibernator;

class LinuxHibernator : public HibernatorBase
{
  public:

	LinuxHibernator(void) noexcept;
	virtual ~LinuxHibernator(void) noexcept;

	/* Discover supported sleep states */
	bool initialize( void );

	bool setMethod( const char *method ) { m_method = method; return true; };
	const char *getMethod(void) const;

protected:

	/* Override this to enter the given sleep state on a
	   particular OS */
	HibernatorBase::SLEEP_STATE enterStateStandBy(   bool force ) const;
	HibernatorBase::SLEEP_STATE enterStateSuspend(   bool force ) const;
	HibernatorBase::SLEEP_STATE enterStateHibernate( bool force ) const;
	HibernatorBase::SLEEP_STATE enterStatePowerOff(  bool force ) const;

private:

	BaseLinuxHibernator	*m_real_hibernator;
	const char			*m_method;

};

#define HIBERNATOR_TYPE_DEFINED	1
typedef LinuxHibernator	RealHibernator;

#endif // HAVE_LINUX_HIBERNATION

#endif // _HIBERNATER_LINUX_H_
