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
#include "MyString.h"
#include "string_list.h"
#include "condor_arglist.h"
#include "my_popen.h"
#include "XXX_startd_factory_attrs.h"
#include "condor_partition.h"
#include "condor_partition_mgr.h"
#include "condor_workload.h"
#include "condor_workload_mgr.h"
#include "condor_classad.h"
#include "condor_uid.h"

WorkloadManager::WorkloadManager()
{
}

WorkloadManager::~WorkloadManager()
{
}

void WorkloadManager::query_workloads(char *script)
{
	FILE *fin = NULL;
	char *delim = " \t";
	int idx;
	ClassAd *ad = NULL;
	int eof, error, empty;
	char *classad_delimitor = "---\n";
	ArgList args;
	priv_state priv;

	dprintf(D_ALWAYS, "Querying workloads with: %s\n", script);

	args.AppendArg(script);

	priv = set_root_priv();
	fin = my_popen(args, "r", TRUE);

	if (fin == NULL) {
		EXCEPT("Can't execute %s\n", script);
	}

	// wipe out whatever was there initially.
	for (idx = 0; idx < m_wklds.getsize(); idx++)
	{
		ad = m_wklds[idx].detach();
		delete ad;
	}

	m_wklds.truncate(0);

	// read a series of classads which represent the workloads this daemon
	// must try to allocate/deallocate partitions to match.
	idx = 0;
	ad = new ClassAd(fin,classad_delimitor,eof,error,empty);
	while(!empty)
	{
		m_wklds[idx].attach(ad);

		// get ready for the next one.
		idx++;

		ad = new ClassAd(fin,classad_delimitor,eof,error,empty);
	}

	// The last ad is empty, doesn't get recorded anywhere, and is deleted.
	delete(ad);

	my_pclose(fin);
	set_priv(priv);
}

void WorkloadManager::total_idle(int &smp, int &dual, int &vn)
{
	int idx;

	smp = 0;
	dual = 0;
	vn = 0;

	for (idx = 0; idx < m_wklds.length(); idx++) {
		smp += m_wklds[idx].get_smp_idle();
		dual += m_wklds[idx].get_dual_idle();
		vn += m_wklds[idx].get_vn_idle();
	}
}

void WorkloadManager::dump(int flags)
{
	int idx;

	for (idx = 0; idx < m_wklds.length(); idx++) {
		m_wklds[idx].dump(flags);
	}
}
