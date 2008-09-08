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

#ifndef _HIBERNATER_WIN32_H_
#define _HIBERNATER_WIN32_H_

#include "hibernator.h"

/***************************************************************
 * MsWindowsHibernator class
 ***************************************************************/

#if defined ( WIN32 )

class MsWindowsHibernator : public HibernatorBase
{

public:

	MsWindowsHibernator () throw ();
	virtual ~MsWindowsHibernator () throw ();

protected:

	/* Enter the given sleep state.  Can be any of S[1-5],
	   but only S[3-5] truly exist on Windows */
	bool enterState ( SLEEP_STATE state, bool force ) const;

private:

	/* Auxiliary function to shutdown the machine */
	bool tryShutdown ( bool force ) const;

	/* Discover supported sleep states */
	virtual void initStates ();

};

#endif // WIN32

#endif // _HIBERNATER_WIN32_H_
