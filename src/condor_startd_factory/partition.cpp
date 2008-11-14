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
#include "condor_arglist.h"
#include "my_popen.h"
#include "condor_partition.h"
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_uid.h"
#include "XXX_startd_factory_attrs.h"

Partition::Partition() {
	m_data = NULL;
	m_initialized = false;
}

Partition::~Partition()
{
	delete m_data;
	m_initialized = false;
}

void Partition::attach(ClassAd *ad)
{
	// if we attach over and over, then assume the previous data is defunct and
	// get rid of it.

	if (m_data != NULL) {
		delete(m_data);
	}

	m_data = ad;
	m_initialized = true;
}

ClassAd* Partition::detach(void)
{
	ClassAd *ad = NULL;

	ad = m_data;

	m_initialized = false;

	m_data = NULL;

	return ad;
}

void Partition::set_name(MyString &name)
{
	m_data->Assign(ATTR_PARTITION_NAME, name);
}

MyString Partition::get_name(void)
{
	MyString name;

	m_data->LookupString(ATTR_PARTITION_NAME, name);

	return name;
}

void Partition::set_backer(MyString &name)
{
	m_data->Assign(ATTR_PARTITION_BACKER, name);
}

MyString Partition::get_backer(void)
{
	MyString name;

	m_data->LookupString(ATTR_PARTITION_BACKER, name);

	return name;
}

void Partition::set_size(size_t size)
{
	// typecast due to  classad integral types
	m_data->Assign(ATTR_PARTITION_SIZE, (int)size);
}

size_t Partition::get_size(void)
{
	int size;

	m_data->LookupInteger(ATTR_PARTITION_SIZE, size);

	// typecast due to  classad integral types
	return (size_t)size;
}

void Partition::set_pstate(PState pstate)
{
	// typecast due to  classad integral types

	switch(pstate) {
		case NOT_GENERATED:
			m_data->Assign(ATTR_PARTITION_STATE, "NOT_GENERATED");
			break;
		case GENERATED:
			m_data->Assign(ATTR_PARTITION_STATE, "GENERATED");
			break;
		case BOOTED:
			m_data->Assign(ATTR_PARTITION_STATE, "BOOTED");
			break;
		case ASSIGNED:
			m_data->Assign(ATTR_PARTITION_STATE, "ASSIGNED");
			break;
		case BACKED:
			m_data->Assign(ATTR_PARTITION_STATE, "BACKED");
			break;
		default:
			EXCEPT("Partition::set_pstate: Invalid pstate: %d\n", pstate);
			break;
	}
}

void Partition::set_pstate(MyString pstate)
{
	if (pstate != "NOT_GENERATED" && 
		pstate != "GENERATED" &&
		pstate != "BOOTED" &&
		pstate != "ASSIGNED" &&
		pstate != "BACKED")
	{
		EXCEPT("Partition::set_pstate(): Invalid assignment string: %s\n",
			pstate.Value());
	}

	m_data->Assign(ATTR_PARTITION_STATE, pstate);
}

PState Partition::get_pstate(void)
{
	MyString pstate;

	m_data->LookupString(ATTR_PARTITION_STATE, pstate);

	if (pstate == "NOT_GENERATED") {
		return NOT_GENERATED;
	}

	if (pstate == "GENERATED") {
		return GENERATED;
	}

	if (pstate == "BOOTED") {
		return BOOTED;
	}

	if (pstate == "ASSIGNED") {
		return ASSIGNED;
	}

	if (pstate == "BACKED") {
		return BACKED;
	}

	// should never happen, but in case it does...
	return PSTATE_ERROR;
}

void Partition::set_pkind(PKind pkind)
{
	// typecast due to  classad integral types

	switch(pkind) {
		case SMP:
			m_data->Assign(ATTR_PARTITION_KIND, "SMP");
			break;
		case DUAL:
			m_data->Assign(ATTR_PARTITION_KIND, "DUAL");
			break;
		case VN:
			m_data->Assign(ATTR_PARTITION_KIND, "VN");
			break;
		default:
			EXCEPT("Partition::set_pstate: Invalid pkind: %d\n", pkind);
			break;
	}
}

void Partition::set_pkind(MyString pkind)
{
	if (pkind != "SMP" && 
		pkind != "DUAL" &&
		pkind != "VN")
	{
		EXCEPT("Partition::set_pkind(): Invalid assignment string: %s\n",
			pkind.Value());
	}

	m_data->Assign(ATTR_PARTITION_KIND, pkind);
}

PKind Partition::get_pkind(void)
{
	MyString pkind;

	m_data->LookupString(ATTR_PARTITION_KIND, pkind);

	if (pkind == "SMP") {
		return SMP;
	}

	if (pkind == "DUAL") {
		return DUAL;
	}

	if (pkind == "VN") {
		return VN;
	}

	// should never happen, but in case it does...
	return PKIND_ERROR;
}

// This is a blocking call and must provide a fully generate partition when
// it returns. 
bool Partition::generate(char *script, int size)
{
	return true;
}

// This is a blocking call and must provide a fully booted partition when it
// returns. Otherwise, this partition could be overcommitted given the 
// nature of the use of this call.
// script partition_name size kind
void Partition::boot(char *script, PKind pkind)
{
	FILE *fin = NULL;
	ArgList args;
	MyString line;
	priv_state priv;

	// we're told what kind of partition this is going to be
	set_pkind(pkind);

	dprintf(D_ALWAYS, "\t%s %s %d %s\n",
		script,
		get_name().Value(),
		get_size(),
		pkind_xlate(get_pkind()).Value());

	args.AppendArg(script);
	args.AppendArg(get_name());
	args.AppendArg(get_size());
	args.AppendArg(pkind_xlate(get_pkind()).Value());

	priv = set_root_priv();
	fin = my_popen(args, "r", TRUE);
	line.readLine(fin); // read back OK or NOT_OK, XXX ignore
	my_pclose(fin);
	set_priv(priv);

	// Now that the script is done, mark it booted.
	set_pstate(BOOTED);
}

// This is a blocking call and must provide a fully shutdown partition when it
// returns.
// script partition_name 
void Partition::shutdown(char *script)
{
	FILE *fin = NULL;
	ArgList args;
	MyString line;
	priv_state priv;

	dprintf(D_ALWAYS, "\t%s %s\n",
		script,
		get_name().Value());

	args.AppendArg(script);
	args.AppendArg(get_name());

	priv = set_root_priv();
	fin = my_popen(args, "r", TRUE);
	line.readLine(fin); // read back OK or NOT_OK, XXX ignore
	my_pclose(fin);
	set_priv(priv);

	// Now that the script is done, mark it simply generated.
	set_pstate(GENERATED);
}

void Partition::back(char *script)
{
	FILE *fin = NULL;
	ArgList args;
	MyString line;
	priv_state priv;

	dprintf(D_ALWAYS, "\t%s %s %d %s\n",
		script,
		get_name().Value(),
		get_size(),
		pkind_xlate(get_pkind()).Value());

	args.AppendArg(script);
	args.AppendArg(get_name());
	args.AppendArg(get_size());
	args.AppendArg(pkind_xlate(get_pkind()).Value());

	priv = set_root_priv();
	fin = my_popen(args, "r", TRUE);
	line.readLine(fin); // read back OK or NOT_OK, XXX ignore
	my_pclose(fin);
	set_priv(priv);

	// we don't know it is backed until the 
	// STARTD_FACTORY_SCRIPT_AVAILABLE_PARTITIONS
	// tells us it is actually backed. This prevents overcommit of a
	// partition to multiple startds.
	set_pstate(ASSIGNED);
}


void Partition::dump(int flags)
{
	MyString backer;

	if (m_initialized == false) {
		dprintf(flags, "Partition is not initialized!\n");
		return;
	}

	dprintf(flags, "Partition: %s\n", get_name().Value());

	backer = get_backer();
	if (backer == "") {
		dprintf(flags, "\tBacked by: [NONE]\n");
	} else {
		dprintf(flags, "\tBacked by: %s\n", backer.Value());
	}

	dprintf(flags, "\tSize: %d\n", get_size());
	dprintf(flags, "\tPState: %s\n", pstate_xlate(get_pstate()).Value());

	switch(get_pstate()) {
		case BOOTED:
		case ASSIGNED:
		case BACKED:
			dprintf(flags, "\tPKind: %s\n", pkind_xlate(get_pkind()).Value());
			break;
		default:
			dprintf(flags, "\tPKind: N/A\n");
			break;
	}
}



MyString pkind_xlate(PKind pkind)
{
	MyString p;

	switch(pkind) {
		case PKIND_ERROR:
			p = "PKIND_ERROR";
			break;

		case SMP:
			p = "SMP";
			break;

		case DUAL:
			p = "DUAL";
			break;

		case VN:
			p = "VN";
			break;

		case PKIND_END:
			p = "PKIND_END";
			break;

		default:
			p = "PKIND_ERROR";
			break;
	}

	return p;
}

PKind pkind_xlate(MyString pkind)
{
	if (pkind == "PKIND_ERROR") {
		return PKIND_ERROR;
	}

	if (pkind == "SMP") {
		return SMP;
	}

	if (pkind == "DUAL") {
		return DUAL;
	}

	if (pkind == "VN") {
		return VN;
	}

	if (pkind == "PKIND_END") {
		return PKIND_END;
	}

	return PKIND_ERROR;
}


MyString pstate_xlate(PState pstate)
{
	MyString p;

	switch(pstate) {
		case PSTATE_ERROR:
			p = "PSTATE_ERROR";
			break;

		case NOT_GENERATED:
			p = "NOT_GENERATED";
			break;

		case GENERATED:
			p = "GENERATED";
			break;

		case BOOTED:
			p = "BOOTED";
			break;

		case ASSIGNED:
			p = "ASSIGNED";
			break;

		case BACKED:
			p = "BACKED";
			break;

		case PSTATE_END:
			p = "PSTATE_END";
			break;

		default:
			p = "PKIND_ERROR";
			break;
	}

	return p;
}

PState pstate_xlate(MyString pstate)
{
	if (pstate == "PSTATE_ERROR") {
		return PSTATE_ERROR;
	}

	if (pstate == "NOT_GENERATED") {
		return NOT_GENERATED;
	}

	if (pstate == "GENERATED") {
		return GENERATED;
	}

	if (pstate == "BOOTED") {
		return BOOTED;
	}

	if (pstate == "ASSIGNED") {
		return ASSIGNED;
	}

	if (pstate == "BACKED") {
		return BACKED;
	}

	if (pstate == "PSTATE_END") {
		return PSTATE_END;
	}

	return PSTATE_ERROR;
}




