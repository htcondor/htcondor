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
#include "MyString.h"
#include "condor_workload.h"
#include "condor_debug.h"
#include "condor_classad.h"
#include "string_list.h"
#include "XXX_startd_factory_attrs.h"

Workload::Workload()
{
	m_data = NULL;
	m_initialized = false;
}

Workload::~Workload()
{
	delete m_data;

	m_initialized = false;
}

void Workload::attach(ClassAd *ad)
{
	// if anything was already here, get rid of it
	if (m_data != NULL) {
		delete m_data;
	}

	m_data = ad;

	m_initialized = true;
}

// give ownership of the pointer back to the caller.
ClassAd* Workload::detach(void)
{
	ClassAd *ad = NULL;

	m_initialized = false;

	ad = m_data;

	m_data = NULL;

	return ad;
}

void Workload::set_name(MyString name)
{
	m_data->Assign(ATTR_WKLD_NAME, name);
}

MyString Workload::get_name(void)
{
	MyString name;

	m_data->LookupString(ATTR_WKLD_NAME, name);

	return name;
}

void Workload::set_smp_idle(int num)
{
	m_data->Assign(ATTR_WKLD_SMP_IDLE, num);
}

int Workload::get_smp_idle(void)
{
	int num;

	m_data->LookupInteger(ATTR_WKLD_SMP_IDLE, num); 

	return num;
}

void Workload::set_smp_running(int num)
{
	m_data->Assign(ATTR_WKLD_SMP_RUNNING, num);
}

int Workload::get_smp_running(void)
{
	int num;

	m_data->LookupInteger(ATTR_WKLD_SMP_RUNNING, num); 

	return num;
}


void Workload::set_dual_idle(int num)
{
	m_data->Assign(ATTR_WKLD_DUAL_IDLE, num);
}

int Workload::get_dual_idle(void)
{
	int num;

	m_data->LookupInteger(ATTR_WKLD_DUAL_IDLE, num); 

	return num;
}

void Workload::set_dual_running(int num)
{
	m_data->Assign(ATTR_WKLD_DUAL_RUNNING, num);
}

int Workload::get_dual_running(void)
{
	int num;

	m_data->LookupInteger(ATTR_WKLD_DUAL_RUNNING, num); 

	return num;
}


void Workload::set_vn_idle(int num)
{
	m_data->Assign(ATTR_WKLD_VN_IDLE, num);
}

int Workload::get_vn_idle(void)
{
	int num;

	m_data->LookupInteger(ATTR_WKLD_VN_IDLE, num); 

	return num;
}

void Workload::set_vn_running(int num)
{
	m_data->Assign(ATTR_WKLD_VN_RUNNING, num);
}

int Workload::get_vn_running(void)
{
	int num;

	m_data->LookupInteger(ATTR_WKLD_VN_RUNNING, num); 

	return num;
}

void Workload::dump(int flags)
{
	StringList sl;
	MyString parts;

	if (m_initialized == false) {
		dprintf(flags, "Workload is not initialized for printing!\n");
		return;
	}

	dprintf(flags, "Workload: %s\n", get_name().Value());
	dprintf(flags, "\tSMP Running: %d\n", get_smp_running());
	dprintf(flags, "\tSMP Idle: %d\n", get_smp_idle());
	dprintf(flags, "\tDUAL Running: %d\n", get_dual_running());
	dprintf(flags, "\tDUAL Idle: %d\n", get_dual_idle());
	dprintf(flags, "\tVN Running: %d\n", get_vn_running());
	dprintf(flags, "\tVN Idle: %d\n", get_vn_idle());
}





