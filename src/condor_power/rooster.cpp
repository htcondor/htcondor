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


#include "condor_common.h"

#include "condor_debug.h"
#include "condor_fix_assert.h"
#include "condor_io.h"
#include "condor_constants.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_daemon_core.h"
#include "condor_config.h"
#include "rooster.h"

Rooster::Rooster():
	m_polling_interval(0),
	m_polling_timer(-1)
{
}

Rooster::~Rooster()
{
	stop();
}

void Rooster::init()
{
	config();
}

void Rooster::config()
{
	int old_polling_interval = m_polling_interval;
	m_polling_interval = param_integer("ROOSTER_INTERVAL",300);
	if( m_polling_interval < 0 ) {
		if( m_polling_timer != -1 ) {
			daemonCore->Cancel_Timer(m_polling_timer);
			m_polling_timer = -1;
		}
	}
	else if( m_polling_timer >= 0 ) {
		if( old_polling_interval != m_polling_interval ) {
			daemonCore->Reset_Timer(
				m_polling_timer,
				m_polling_interval,
				m_polling_interval);
		}
	}
	else {
		m_polling_timer = daemonCore->Register_Timer(
			m_polling_interval,
			m_polling_interval,
			(Eventcpp)&Rooster::poll,
			"Rooster::poll",
			this );
	}
}

void Rooster::stop()
{
	if( m_polling_timer != -1 ) {
		daemonCore->Cancel_Timer(m_polling_timer);
		m_polling_timer = -1;
	}
}

void Rooster::poll()
{
	dprintf(D_FULLDEBUG,"Cock-a-doodle-doo!\n");
}
