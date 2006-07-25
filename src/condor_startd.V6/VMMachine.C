/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "startd.h"
#include "VMMachine.h"
#include "vm_common.h"

/* Interfaces for class VMMachine */
VMMachine::VMMachine(VMManager *s, char *addr) 
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
VMMachine::match(char *addr) 
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
VMMachine::getTimeStamp(void) 
{
	return m_last_time;
}

char* 
VMMachine::getVMSinful(void) 
{
	return m_addr;
}
