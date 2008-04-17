#include "condor_common.h"
#include "condor_debug.h"
#include "MyString.h"
#include "HashTable.h"
#include "condor_partition.h"
#include "condor_partition_mgr.h"
#include "condor_classad.h"
#include "condor_arglist.h"
#include "my_popen.h"
#include "XXX_bgd_attrs.h"

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

	/* load a description of the available partitions */

	dprintf(D_ALWAYS, "Finding available partitions with: %s\n", script);

	args.AppendArg(script);

	fin = my_popen(args, "r", TRUE);

	if (fin == NULL) {
		EXCEPT("Can't execute %s\n", script);
	}

	read_partitions(fin);

	my_pclose(fin);
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

void PartitionManager::schedule_partitions(WorkloadManager &wkld_mgr,
	char *boot_script, char *back_script)
{
	int idx;
	int total_smp_idle;
	int total_dual_idle;
	int total_vn_idle;
	MyString name;
	bool val = true;

	// figure out the big picture workload to satisfy
	wkld_mgr.total_idle(total_smp_idle, total_dual_idle, total_vn_idle);

	dprintf(D_ALWAYS, "Total Idle: SMP: %d, DUAL: %d, VN: %d\n",
		total_smp_idle, total_dual_idle, total_vn_idle);

	// This is a crappy algorithm which will boot (if needed) and then
	// back a single partition for each kind of idle job.

	SCHED_SMP:
	if (total_smp_idle > 0) {
		// find a booted one first if possible
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == BOOTED) {
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
	}

	SCHED_DUAL:
	if (total_dual_idle > 0) {
		// find a booted one first if possible
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == BOOTED) {
				dprintf(D_ALWAYS, "Backing DUAL partition: %s %d\n",
					m_parts[idx].get_name().Value(),
					m_parts[idx].get_size());
				m_parts[idx].back(back_script);
				name = m_parts[idx].get_name();
				m_assigned.insert(name,val);
				goto SCHED_VN;
			}
		}

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
	}

	SCHED_VN:
	if (total_vn_idle > 0) {
		// find a booted one first if possible
		for (idx = 0; idx < m_parts.length(); idx++) {
			if (m_parts[idx].get_pstate() == BOOTED) {
				dprintf(D_ALWAYS, "Backing VN partition: %s %d\n",
					m_parts[idx].get_name().Value(),
					m_parts[idx].get_size());
				m_parts[idx].back(back_script);
				name = m_parts[idx].get_name();
				m_assigned.insert(name,val);
				goto DONE;
			}
		}

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
	}

	DONE:
		;
}

void PartitionManager::dump(int flags)
{
	int idx;

	for (idx = 0; idx < m_parts.length(); idx++) {
		m_parts[idx].dump(flags);
	}
}

