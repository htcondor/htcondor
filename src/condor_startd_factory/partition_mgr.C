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
#include "HashTable.h"
#include "condor_partition.h"
#include "condor_partition_mgr.h"
#include "condor_classad.h"
#include "condor_arglist.h"
#include "my_popen.h"
#include "condor_uid.h"
#include "XXX_startd_factory_attrs.h"

PartitionManager::PartitionManager() :
	m_assigned(2000, hashFuncMyString)
{
}

PartitionManager::~PartitionManager()
{
	int idx;
	ClassAd *ad = NULL;

	for (idx = 0; idx < m_parts.length(); idx++)
	{
		ad = m_parts[idx].detach();
		if (ad != NULL) {
			delete ad;
		}
	}
}

void PartitionManager::query_available_partitions(char *script)
{
	FILE *fin = NULL;
	ArgList args;
	priv_state priv;

	/* load a description of the available partitions which could be 
		backed, booted, or generatable. */

	dprintf(D_ALWAYS, "Finding available partitions with: %s\n", script);

	args.AppendArg(script);

	priv = set_root_priv();
	fin = my_popen(args, "r", TRUE);

	if (fin == NULL) {
		EXCEPT("Can't execute %s\n", script);
	}

	read_partitions(fin);

	my_pclose(fin);
	set_priv(priv);
}

void PartitionManager::read_partitions(FILE *fin)
{
	int idx;
	ClassAd *ad = NULL;
	int eof, error, empty;
	char *classad_delimitor = "---\n";
	MyString tmp_str;
	bool tmp_bool = true;

	// XXX This function will trash what is already known about the partitions.

	// wipe out whatever was there initially.
	for (idx = 0; idx < m_parts.getsize(); idx++)
	{
		ad = m_parts[idx].detach();
		delete ad;
	}
	m_parts.truncate(0);

	idx = 0;
	ad = new ClassAd(fin,classad_delimitor,eof,error,empty);
	while(!empty)
	{
		m_parts[idx].attach(ad);
		tmp_str = m_parts[idx].get_name();

		// If the partition is only booted, check to see if it is in the 
		// m_assigned hash. If so, then mark it assigned. If it is
		// backed, then remove it from the m_assigned table since the
		// assignment has been completed.
		switch(m_parts[idx].get_pstate()) {
			case BOOTED:
				if (m_assigned.lookup(tmp_str, tmp_bool) == 0) {
					m_parts[idx].set_pstate(ASSIGNED);
				}
				break;
			case BACKED:
				if (m_assigned.lookup(tmp_str, tmp_bool) == 0) {
					m_assigned.remove(tmp_str);
				}
				break;
			default:
				// do nothing
				break;
		}

		// get ready for the next one.
		idx++;

		ad = new ClassAd(fin,classad_delimitor,eof,error,empty);
	}

	// The last ad is empty, doesn't get recorded anywhere, and is deleted.
	delete(ad);
}

// return true is partition exists by this name
bool PartitionManager::partition_exists(MyString name)
{
	int idx;

	for (idx = 0; idx < m_parts.length(); idx++) {
		if (m_parts[idx].get_name() == name) {
			return true;
		}
	}

	return false;
}

void PartitionManager::set_partition_backer(MyString part_name, MyString backer)
{
	int idx;

	for (idx = 0; idx < m_parts.length(); idx++) {
		if (m_parts[idx].get_name() == part_name) {
			m_parts[idx].set_backer(backer);
			// if it is owned by a startd, then this is always true.
			m_parts[idx].set_pstate(BACKED);
			return;
		}
	}
}

MyString PartitionManager::get_partition_backer(MyString part_name)
{
	int idx;
	MyString none = "";

	for (idx = 0; idx < m_parts.length(); idx++) {
		if (m_parts[idx].get_name() == part_name) {
			return m_parts[idx].get_backer();
		}
	}

	return none;
}

// What is the maximum number of jobs that could potentially run on the
// backed partitions of each kind?
void PartitionManager::partition_realization(int &tot_smp, int &tot_dual, 
	int &tot_vn)
{
	int idx;

	tot_smp = 0;
	tot_dual = 0;
	tot_vn = 0;

	for (idx = 0; idx < m_parts.length(); idx++) {
		if (m_parts[idx].get_pstate() == BACKED) {
			switch(m_parts[idx].get_pkind()) {
				case SMP:
					tot_smp += m_parts[idx].get_size();
					break;

				case DUAL:
					tot_dual += m_parts[idx].get_size();
					break;

				case VN:
					tot_dual += m_parts[idx].get_size();
					break;

				default:
					EXCEPT("PartitionManager::total_backed_partitions(): "
						"Programmer error!");
			}
		}
	}
}

void PartitionManager::schedule_partitions(WorkloadManager &wkld_mgr,
	char *generate_script, char *boot_script, char *back_script, 
	char *shutdown_script, char *destroy_script)
{
	int idx;
	int total_smp_idle;
	int total_dual_idle;
	int total_vn_idle;
	MyString name;
	bool val = true;
	int smp_backed;
	int dual_backed;
	int vn_backed;

	// figure out the big picture workload to satisfy
	wkld_mgr.total_idle(total_smp_idle, total_dual_idle, total_vn_idle);

	// figure out the maximum number of jobs the currently backed 
	// partitions are able to realize
	partition_realization(smp_backed, dual_backed, vn_backed);

	dprintf(D_ALWAYS, "Total Idle: SMP: %d, DUAL: %d, VN: %d\n",
		total_smp_idle, total_dual_idle, total_vn_idle);

	// Tell the user if I have to grab more partitions of a various type.
	if (total_smp_idle > smp_backed) {
		dprintf(D_ALWAYS, "%d backed SMP jobs, %d total SMP idle, attempting "
			"to back an SMP partition.\n", smp_backed, total_smp_idle);
	} else {
		dprintf(D_ALWAYS, "%d backed SMP jobs satisfies %d idle SMP jobs.\n",
			smp_backed, total_smp_idle);
	}

	if (total_dual_idle > dual_backed) {
		dprintf(D_ALWAYS, "%d backed DUAL jobs, %d total DUAL idle, attempting "
			"to back a DUAL partition.\n", dual_backed, total_dual_idle);
	} else {
		dprintf(D_ALWAYS, "%d backed DUAL jobs satisfies %d idle DUAL jobs.\n",
			dual_backed, total_dual_idle);
	}

	if (total_vn_idle > vn_backed) {
		dprintf(D_ALWAYS, "%d backed VN jobs, %d total VN idle, attempting "
			"to back a DUAL partition.\n", vn_backed, total_vn_idle);
	} else {
		dprintf(D_ALWAYS, "%d backed VN jobs satisfies %d idle VN jobs.\n",
			dual_backed, total_vn_idle);
	}

	// This is a crappy algorithm which will boot and then
	// back a single partition for each kind of idle job.

	// Only try to back a partition of there are more jobs than 
	// the current backed partitions can realize. Otherwise if a
	// single job in one schedd doesn't run for a while, eventually
	// all available partitions will be allocated to it. So, stop
	// when we know there are enough compute elements of a certain kind 
	// to satisfy the workloads.

	// ********* SCHEDULE FOR SMP JOBS **************
	SCHED_SMP:
	if (total_smp_idle > smp_backed) {

		// find a booted one first if possible
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == BOOTED &&
				m_parts[idx].get_pkind() == SMP)
			{
				dprintf(D_ALWAYS, "Backing SMP partition: %s %d\n",
					m_parts[idx].get_name().Value(),
					m_parts[idx].get_size());
				m_parts[idx].back(back_script);
				name = m_parts[idx].get_name();
				m_assigned.insert(name,val);
				goto SCHED_DUAL;
			}
		}

		// if not, we'll use a generated one
		SCHED_SMP_GENERATE:
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == GENERATED) {
				dprintf(D_ALWAYS, "Booting SMP partition: %s %d\n",
					m_parts[idx].get_name().Value(),
					m_parts[idx].get_size());
				m_parts[idx].boot(boot_script, SMP);
				// we'll find this booted partition and then back it.
				goto SCHED_SMP;
			}
		}

		// We don't implement the next section of code, so skip it, but
		// leave it there so compilation checks it at least.
		dprintf(D_ALWAYS, 
			"WARNING: No suitable partitions found for SMP jobs.\n");
		goto SMP_NOT_IMPL;

		// if there are no generated partitions, we have to generate one
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == NOT_GENERATED) {
				dprintf(D_ALWAYS, 
					"Generating a partition for DUAL jobs: 32 nodes\n");
				if (m_parts[idx].generate(generate_script, 32) == true) {
					// we'll find this generated partition and then boot it.
					goto SCHED_SMP_GENERATE;
				}
				// if the generation fails, we fall through...
			}
		}
		SMP_NOT_IMPL: ;
	}

	// ********* SCHEDULE FOR DUAL JOBS **************
	SCHED_DUAL:
	if (total_dual_idle > dual_backed) {

		// find a booted one first if possible
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == BOOTED &&
				m_parts[idx].get_pkind() == DUAL)
			{
				dprintf(D_ALWAYS, "Backing DUAL partition: %s %d\n",
					m_parts[idx].get_name().Value(),
					m_parts[idx].get_size());
				m_parts[idx].back(back_script);
				name = m_parts[idx].get_name();
				m_assigned.insert(name,val);
				goto SCHED_VN;
			}
		}

		// if not, we'll use a generated one
		SCHED_DUAL_GENERATE:
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == GENERATED) {
				dprintf(D_ALWAYS, "Booting DUAL partition: %s %d\n",
					m_parts[idx].get_name().Value(),
					m_parts[idx].get_size());
				m_parts[idx].boot(boot_script, DUAL);
				// we'll find this booted partition and then back it.
				goto SCHED_DUAL;
			}
		}

		// We don't implement the next section of code, so skip it, but
		// leave it there so compilation checks it at least.
		dprintf(D_ALWAYS, 
			"WARNING: No suitable partitions found for DUAL jobs.\n");
		goto DUAL_NOT_IMPL;
		// if there are no generated partitions, we have to generate one
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == NOT_GENERATED) {
				dprintf(D_ALWAYS, 
					"Generating a partition for DUAL jobs: 32 nodes\n");
				if (m_parts[idx].generate(generate_script, 32) == true) {
					// we'll find this generated partition and then boot it.
					goto SCHED_DUAL_GENERATE;
				}
				// if the generation fails, we fall through...
			}
		}
		DUAL_NOT_IMPL: ;
	}

	// ********* SCHEDULE FOR VN JOBS **************
	SCHED_VN:
	if (total_vn_idle > vn_backed) {
		// find a booted one first if possible
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == BOOTED &&
				m_parts[idx].get_pkind() == VN)
			{
				dprintf(D_ALWAYS, "Backing VN partition: %s %d\n",
					m_parts[idx].get_name().Value(),
					m_parts[idx].get_size());
				m_parts[idx].back(back_script);
				name = m_parts[idx].get_name();
				m_assigned.insert(name,val);
				goto DONE;
			}
		}

		// if not, we'll use a generated one
		SCHED_VN_GENERATE:
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == GENERATED) {
				dprintf(D_ALWAYS, "Booting VN partition: %s %d\n",
					m_parts[idx].get_name().Value(),
					m_parts[idx].get_size());
				m_parts[idx].boot(boot_script, VN);
				// we'll find this booted partition and then back it.
				goto SCHED_VN;
			}
		}

		dprintf(D_ALWAYS, 
			"WARNING: No suitable partitions found for DUAL jobs.\n");
		goto VN_NOT_IMPL;
		// if there are no generated partitions, we have to generate one
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == NOT_GENERATED) {
				dprintf(D_ALWAYS, 
					"Generating a partition for VN jobs: 32 nodes\n");
				if (m_parts[idx].generate(generate_script, 32) == true) {
					// we'll find this generated partition and then boot it.
					goto SCHED_VN_GENERATE;
				}
				// if the generation fails, we fall through...
			}
		}
		VN_NOT_IMPL: ;
	}

	DONE:
		;
	
	// Now that we've potentially backed partitions, let's take a look at the 
	// partitions again and see if we can evict any ones which are only booted.
	// This state represents a non-backed partition (due to a startd
	// killing itself due to lack of work) which we don't need.
	// XXX After getting rid of something, maybe we should try and restart
	// the algorithm to see if something would have gotten booted and
	// backed right away for a different type of HTC job. Otherwise we'll wait
	// an entire cycle beofre trying to adjust the partitions again.
	for (idx = 0; idx < m_parts.length(); idx++) {
		if (m_parts[idx].get_pstate() == BOOTED) {
			dprintf(D_ALWAYS, "Shutting down unused partition: %s %d %s\n",
				m_parts[idx].get_name().Value(),
				m_parts[idx].get_size(),
				pkind_xlate(m_parts[idx].get_pkind()).Value());
			m_parts[idx].shutdown(shutdown_script);
		}
	}
}

void PartitionManager::dump(int flags)
{
	int idx;

	for (idx = 0; idx < m_parts.length(); idx++) {
		m_parts[idx].dump(flags);
	}
}

