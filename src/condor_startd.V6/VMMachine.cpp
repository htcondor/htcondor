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
#include "startd.h"
#include "VMMachine.h"
#include "VMManager.h"
#include "vm_common.h"

/* Interfaces for class VMMachine */
VMMachine::VMMachine(VMManager *s, const char *addr)
{
	m_addr = strdup(addr);
	m_vmmanager = s;
	m_last_time = time(NULL);
	m_vmmanager->attach(this);
}

VMMachine::~VMMachine() 
{
	m_vmmanager->detach(this);
	free(m_addr);
}

void 
VMMachine::sendEventToVM(int cmd, void* data) 
{
	vmapi_sendCommand(m_addr, cmd, data);
}

bool 
VMMachine::match(const char *addr)
{
	if ( !strcmp(addr, m_addr) )
		return TRUE;
	else
		return FALSE;
}

void 
VMMachine::print(void) 
{
	dprintf(D_FULLDEBUG,"VMMachine(%s) is registered\n", m_addr);
	return;
}

void 
VMMachine::updateTimeStamp(void) 
{
	m_last_time = time(NULL);
	return;
}

int 
VMMachine::getTimeStamp(void) const 
{
	return m_last_time;
}

char* 
VMMachine::getVMSinful(void) 
{
	return m_addr;
}
