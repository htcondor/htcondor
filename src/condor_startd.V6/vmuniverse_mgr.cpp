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
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_environ.h"
#include "startd.h"
#include "my_popen.h"
#include "daemon.h"
#include "daemon_types.h"
#include "directory.h"
#include "condor_environ.h"
#include "vmuniverse_mgr.h"
#include "condor_vm_universe_types.h"
#include "vm_univ_utils.h"
#include "setenv.h"
#include <algorithm>

extern ResMgr* resmgr;

static unsigned long get_image_size(procInfo& pi)
{
#if defined(WIN32)
	return pi.rssize;
#else
	return pi.imgsize;
#endif
}

VMStarterInfo::VMStarterInfo()
{
	m_pid = 0;
	m_memory = 0;
	m_vcpus = 1;
	m_vm_pid = 0;
	memset(&m_vm_exited_pinfo, 0, sizeof(m_vm_exited_pinfo));
	memset(&m_vm_alive_pinfo, 0, sizeof(m_vm_alive_pinfo));
}

bool
VMStarterInfo::updateUsageOfVM(void)
{
	if( m_vm_pid == 0) {
		return false;
	}

	int proc_status = PROCAPI_OK;

	piPTR pi = NULL;
	if( ProcAPI::getProcInfo(m_vm_pid, pi, proc_status) == 
			PROCAPI_SUCCESS ) {
		memcpy(&m_vm_alive_pinfo, pi, sizeof(struct procInfo));
		if( IsDebugVerbose(D_LOAD) ) {
			dprintf(D_LOAD,"Usage of process[%d] for a VM is updated\n", 
					m_vm_pid);
			dprintf(D_LOAD,"sys_time=%lu, user_time=%lu, image_size=%lu\n", 
					pi->sys_time, pi->user_time, get_image_size(*pi));
		}
		delete pi;
		return true;
	}
	if (pi) delete pi;
	return false;
}

void
VMStarterInfo::getUsageOfVM(ProcFamilyUsage &usage)
{
	bool updated = updateUsageOfVM();

	usage.user_cpu_time = m_vm_exited_pinfo.user_time + m_vm_alive_pinfo.user_time;
	usage.sys_cpu_time = m_vm_exited_pinfo.sys_time + m_vm_alive_pinfo.sys_time;

	if( updated ) {
		usage.percent_cpu = m_vm_alive_pinfo.cpuusage;
	}else
		usage.percent_cpu = 0;

	unsigned long exited_max_image = get_image_size(m_vm_exited_pinfo);
	unsigned long alive_max_image = get_image_size(m_vm_alive_pinfo);

	usage.max_image_size = (exited_max_image > alive_max_image ) ? 
				exited_max_image : alive_max_image;

	if( updated ) {
		usage.total_image_size = m_vm_alive_pinfo.imgsize;
		usage.total_resident_set_size = m_vm_alive_pinfo.rssize;
#if HAVE_PSS
		usage.total_proportional_set_size = m_vm_alive_pinfo.pssize;
#endif
	}else {
		usage.total_image_size = 0;
        usage.total_resident_set_size = 0;
#if HAVE_PSS
		usage.total_proportional_set_size = 0;
		usage.total_proportional_set_size = false;
#endif
	}

	if( IsDebugVerbose(D_LOAD) ) {
		dprintf( D_LOAD,
				"VMStarterInfo::getUsageOfVM(): Percent CPU usage "
				"for VM process with pid %u is: %f\n",
				m_vm_pid,
				usage.percent_cpu );
	}
}

void
VMStarterInfo::addProcessForVM(pid_t vm_pid)
{
	if( vm_pid < 0 ) {
		return;
	}

	if( vm_pid == m_vm_pid ) {
		// vm_pid doesn't change
		return;
	}

	// Add the old usage to m_vm_exited_pinfo
	m_vm_exited_pinfo.sys_time += m_vm_alive_pinfo.sys_time;
	m_vm_exited_pinfo.user_time += m_vm_alive_pinfo.user_time;
	if( m_vm_alive_pinfo.rssize > m_vm_exited_pinfo.rssize ) {
		m_vm_exited_pinfo.rssize = m_vm_alive_pinfo.rssize;
	}
	if( m_vm_alive_pinfo.imgsize > m_vm_exited_pinfo.imgsize ) {
		m_vm_exited_pinfo.imgsize = m_vm_alive_pinfo.imgsize;
	}
#if HAVE_PSS
	if( m_vm_alive_pinfo.pssize_available && m_vm_alive_pinfo.pssize > m_vm_exited_pinfo.pssize ) {
		m_vm_exited_pinfo.pssize_available = true;
		m_vm_exited_pinfo.pssize = m_vm_alive_pinfo.pssize;
	}
#endif

	// Reset usage of the current process for VM
	memset(&m_vm_alive_pinfo, 0, sizeof(m_vm_alive_pinfo));
	m_vm_pid = vm_pid;
}

pid_t
VMStarterInfo::getProcessForVM(void) const
{
	return m_vm_pid;
}

void
VMStarterInfo::addMACForVM(const char* mac)
{
	m_vm_mac = mac;
}

const char *
VMStarterInfo::getMACForVM(void)
{
	return m_vm_mac.c_str();
}

void
VMStarterInfo::addIPForVM(const char* ip)
{
	m_vm_ip = ip;
}

const char *
VMStarterInfo::getIPForVM(void)
{
	return m_vm_ip.c_str();
}

void 
VMStarterInfo::publishVMInfo(ClassAd* ad)
{
	if( !ad ) {
		return;
	}
	if (m_vm_mac.length() == 0) {
		ad->Assign(ATTR_VM_GUEST_MAC, m_vm_mac);
	}
	if( m_vm_ip.length() == 0 ) {
		ad->Assign(ATTR_VM_GUEST_IP, m_vm_ip);
	}
	if( m_memory > 0 ) {
		ad->Assign(ATTR_VM_GUEST_MEM, m_memory); 
	}
	ad->Assign(ATTR_JOB_VM_VCPUS, m_vcpus);
}

VMUniverseMgr::VMUniverseMgr()
{
	m_needCheck = false;
	m_starter_has_vmcode = false;
	m_vm_max_memory = 0;
	m_vm_used_memory = 0;
	m_vm_max_num = 0;	// no limit
	m_vm_networking = false;
	m_check_tid = -1;
	m_check_interval = 0;
}

VMUniverseMgr::~VMUniverseMgr()
{
	if( m_check_tid != -1 ) {
		daemonCore->Cancel_Timer(m_check_tid);
		m_check_tid = -1;
	}
}

bool
VMUniverseMgr::init( void )
{
	std::string vmtype;
	std::string vmgahppath;

	m_vm_type = "";

	if( !m_starter_has_vmcode ) {
		dprintf( D_ALWAYS, "VM universe will be disabled, "
				"because Starter doesn't support vm universe\n");
		return false;
	}

	char *tmp = NULL;
	tmp = param( "VM_TYPE" );
	if( !tmp ) {
		return false;
	}

	dprintf( D_FULLDEBUG, "VM_TYPE(%s) is defined in condor config file\n", tmp);
	vmtype = tmp;
	free(tmp);

	tmp = param( "VM_GAHP_SERVER" );
	if( !tmp ) {
		dprintf( D_ALWAYS, "To support vm universe, '%s' must be defined "
				"in condor config file\n", "VM_GAHP_SERVER"); 
		return false;
	}

	if( access(tmp,X_OK) < 0) {
		// make sure that vmgahp is executable
		dprintf( D_ALWAYS, "To support vm universe, '%s' must be executable\n", tmp); 
		free(tmp);
		return false;
	}
	vmgahppath = tmp;
	free(tmp);

	m_vm_max_num = param_integer("VM_MAX_NUMBER", 0, 0);

	// now, we've got a path to a vmgahp server.  
	// try to test it with given vmtype 
	// and grab the output (whose format should be a classad type), 
	// and set the appropriate values for vm universe
	if( testVMGahp(vmgahppath.c_str(), vmtype.c_str()) == false ) {
		dprintf( D_ALWAYS, "Test of vmgahp for VM_TYPE('%s') failed. "
				"So we disabled VM Universe\n", vmtype.c_str());
		m_vm_type = "";
		return false;
	}

	return true;
}

void
VMUniverseMgr::printVMGahpInfo( int debug_level )
{
	dprintf( debug_level, "........VMGAHP info........\n");
	dPrintAd(debug_level, m_vmgahp_info);
	dprintf( debug_level, "\n");
}

void
VMUniverseMgr::publish( ClassAd* ad)
{
	if( !ad ) {
		return;
	}
	if( !m_starter_has_vmcode || ( m_vm_type.length() == 0 )) {
		ad->Assign(ATTR_HAS_VM, false);
		return;
	}

	ad->Assign(ATTR_HAS_VM, true);

	// publish the number of still executable Virtual machines
	if( m_vm_max_num > 0 ) {
		int avail_vm_num = m_vm_max_num - numOfRunningVM();
		ad->Assign(ATTR_VM_AVAIL_NUM, avail_vm_num);
	}else {
		// no limit of the number of executable VM
		ad->Assign(ATTR_VM_AVAIL_NUM, VM_AVAIL_UNLIMITED_NUM);
	}

	// we will publish all information provided by vmgahp server

	ExprTree* expr = NULL;
	const char *attr_name = NULL;
	for( auto itr = m_vmgahp_info.begin(); itr != m_vmgahp_info.end(); itr++ ) {
		attr_name = itr->first.c_str();
		expr = itr->second;
		// we need to adjust available vm memory
		if( strcasecmp(attr_name, ATTR_VM_MEMORY) == MATCH ) {
			int freemem = getFreeVMMemSize();
			ad->Assign(ATTR_VM_MEMORY, freemem);
		}else if( strcasecmp(attr_name, ATTR_VM_NETWORKING) == MATCH ) {
			ad->Assign(ATTR_VM_NETWORKING, m_vm_networking); 
		}else {
			ExprTree * pTree =  expr->Copy();
			ad->Insert(attr_name, pTree);
		}
	}

	// Now, we will publish mac and ip addresses of all guest VMs.
	std::string all_macs;
	std::string all_ips;
	const char* guest_ip = NULL;
	const char* guest_mac = NULL;

	for (VMStarterInfo *info: m_vm_starter_list) {
		guest_ip = info->getIPForVM();
		if( guest_ip ) {
			if( all_ips.length() == 0 ) {
				all_ips += ",";
			}
			all_ips += guest_ip;
		}

		guest_mac = info->getMACForVM();
		if( guest_mac ) {
			if (all_macs.length() == 0) {
				all_macs += ",";
			}
			all_macs += guest_mac;
		}
	}
	if( all_ips.length() == 0 ) {
		ad->Assign(ATTR_VM_ALL_GUEST_IPS, all_ips);
	}
	if( all_macs.length() == 0 ) {
		ad->Assign(ATTR_VM_ALL_GUEST_MACS, all_macs);
	}
}

bool
VMUniverseMgr::testVMGahp(const char* gahppath, const char* vmtype)
{
	m_needCheck = false;

	if( !m_starter_has_vmcode ) {
		return false;
	}

	if( !gahppath || !vmtype ) {
		return false;
	}

#if defined(WIN32)
		// On Windows machine, the option that Starter log file includes 
		// logs from vmgahp causes deadlock even if the option works well 
		// on Linux machine. I guess that is due to Windows Pipes but 
		// I don't know the exact reason.
		// Until the problem is solved, 
		// this option will be disabled on Windows machine.
	char *need_log_file = param("VM_GAHP_LOG");
	if( need_log_file ) {
		free(need_log_file);
	}else {
		dprintf( D_ALWAYS, "To support vm universe, '%s' must be defined "
				"in condor config file, which is a log file for vmgahp.\n", 
				"VM_GAHP_LOG"); 
		return false;
	}
#endif

	// vmgahp is daemonCore, so we need to add -f -t options of daemonCore.
	// Then, try to execute vmgahp with 
	// vmtype <vmtype>"
	// and grab the output as a ClassAd
	ArgList systemcmd;
	systemcmd.AppendArg(gahppath);
	systemcmd.AppendArg("-f");
	char *gahp_log_file = param("VM_GAHP_LOG");
	if( gahp_log_file ) {
		free(gahp_log_file);
	}else {
		systemcmd.AppendArg("-t");
	}
	systemcmd.AppendArg("-M");
	systemcmd.AppendArg(std::to_string(VMGAHP_TEST_MODE));
	systemcmd.AppendArg("vmtype");
	systemcmd.AppendArg(vmtype);

#if !defined(WIN32)
	if( can_switch_ids() ) {
		std::string tmp_str;
		formatstr(tmp_str,"%d", (int)get_condor_uid());
		SetEnv("VMGAHP_USER_UID", tmp_str.c_str());
	}
#endif

	priv_state prev_priv;
	if( (strcasecmp(vmtype, CONDOR_VM_UNIVERSE_XEN) == MATCH) || (strcasecmp(vmtype, CONDOR_VM_UNIVERSE_KVM) == MATCH) ) {
		// Xen requires root privilege
		prev_priv = set_root_priv();
	}else {
		prev_priv = set_condor_priv();

	}
	FILE* fp = NULL;
	fp = my_popen(systemcmd, "r", FALSE );
	set_priv(prev_priv);

	if( !fp ) {
		dprintf( D_ALWAYS, "Failed to execute %s, ignoring\n", gahppath );
		return false;
	}

	bool read_something = false;
	char buf[2048];

	m_vmgahp_info.Clear();
	while( fgets(buf, 2048, fp) ) {
		if( !m_vmgahp_info.Insert(buf) ) {
			dprintf( D_ALWAYS, "Failed to insert \"%s\" into VMInfo, "
					 "ignoring invalid parameter\n", buf );
			continue;
		}
		read_something = true;
	}
	my_pclose( fp );
	if( !read_something ) {
		std::string args_string;
		systemcmd.GetArgsStringForDisplay(args_string,0);
		dprintf( D_ALWAYS, 
				 "Warning: '%s' did not produce any valid output.\n", 
				 args_string.c_str());
		if( (strcasecmp(vmtype, CONDOR_VM_UNIVERSE_XEN) == 0) ) {
			std::string err_msg;
			err_msg += "\n#######################################################\n";
			err_msg += "##### Make sure the followings ";
			err_msg += "to use VM universe for Xen\n";
			err_msg += "### - The owner of script progrm like ";
			err_msg += "'condor_vm_xen.sh' must be root\n";
			err_msg += "### - The script program must be executable\n";
			err_msg += "### - Other writable bit for the above files is ";
			err_msg += "not allowed.\n";
			err_msg += "#########################################################\n";
			dprintf( D_ALWAYS, "%s", err_msg.c_str());
		} else if( (strcasecmp(vmtype, CONDOR_VM_UNIVERSE_KVM) == 0)) {
		        std::string err_msg;
			err_msg += "\n#######################################################\n";
			err_msg += "##### Make sure the followings ";
			err_msg += "to use VM universe for KVM\n";
			err_msg += "### - The owner of script progrm like ";
			err_msg += "'condor_vm_xen.sh' must be root\n";
			err_msg += "### - The script program must be executable\n";
			err_msg += "### - Other writable bit for the above files is ";
			err_msg += "not allowed.\n";
			err_msg += "#########################################################\n";
			dprintf( D_ALWAYS, "%s", err_msg.c_str());
		}else if( strcasecmp(vmtype, CONDOR_VM_UNIVERSE_VMWARE ) == 0 ) {
			dprintf(D_ALWAYS, "A VM_TYPE of vmware is no longer supported\n");
		}
		return false;
	}

	// For debug
	printVMGahpInfo(D_ALWAYS);

	// Read vm_type
	std::string tmp_vmtype;
	if( m_vmgahp_info.LookupString( ATTR_VM_TYPE, tmp_vmtype) != 1 ) {
		dprintf( D_ALWAYS, "There is no %s in the output of vmgahp. "
				"So VM Universe will be disabled\n", ATTR_VM_TYPE);
		return false;
	}
	if( strcasecmp(tmp_vmtype.c_str(), vmtype) != 0 ) {
		dprintf( D_ALWAYS, "The vmgahp(%s) doesn't support this vmtype(%s)\n",
				gahppath, vmtype);
		return false;
	}
	dprintf( D_ALWAYS, "VMType('%s') is supported\n", vmtype);

	// Read vm_memory
	if( m_vmgahp_info.LookupInteger(ATTR_VM_MEMORY, m_vm_max_memory) != 1 ) {
		dprintf( D_ALWAYS, "There is no %s in the output of vmgahp\n",ATTR_VM_MEMORY);
		return false;
	}
	if( m_vm_max_memory == 0 ) {
		dprintf( D_ALWAYS, "There is no sufficient memory for virtual machines\n");
		return false;
	}

	dprintf( D_ALWAYS, "The maximum available memory for vm universe is "
			"set to %d MB\n", m_vm_max_memory);

	// Read vm_networking
	bool tmp_networking = false;
	std::string tmp_networking_types;

	m_vmgahp_info.LookupBool(ATTR_VM_NETWORKING, tmp_networking);
	if( tmp_networking ) {
		if( m_vmgahp_info.LookupString( ATTR_VM_NETWORKING_TYPES, 
					tmp_networking_types) != 1 ) {
			tmp_networking = false;
			m_vmgahp_info.Assign(ATTR_VM_NETWORKING, false);
		}
	}

	m_vm_networking = param_boolean("VM_NETWORKING",false);
	if( m_vm_networking ) {
		if( !tmp_networking ) {
			dprintf( D_ALWAYS, "Even if VM_NETWORKING is TRUE in condor config,"
					" VM_NETWORKING is disabled because vmgahp doesn't "
					"support VM_NETWORKING\n");
			m_vm_networking = false;
		}
	}
	if( m_vm_networking == false ) {
		dprintf( D_ALWAYS, "VM networking is disabled\n");
	}else {
		dprintf( D_ALWAYS, "VM networking is enabled\n");
		dprintf( D_ALWAYS, "Supported networking types are %s\n", 
				tmp_networking_types.c_str());
	}
			
	// Now, we received correct information from vmgahp
	m_vm_type = tmp_vmtype;
	m_vmgahp_server = gahppath;

	return true;
}

int
VMUniverseMgr::getFreeVMMemSize() const
{
	return (m_vm_max_memory - m_vm_used_memory);
}

bool
VMUniverseMgr::canCreateVM(ClassAd *jobAd)
{
	if( !m_starter_has_vmcode || ( m_vm_type.length() == 0 )) {
		return false;
	}

	if( ( m_vm_max_num > 0 ) && 
		( numOfRunningVM() >= m_vm_max_num) ) {
		dprintf(D_ALWAYS, "Current number(%d) of running VM reaches to "
				"maximum number(%d)\n", numOfRunningVM(), m_vm_max_num);
		return false;
	}

	if( !jobAd ) {
		return true;
	}

	// check free memory for VM
	int int_value = 0;
	if( (jobAd->LookupInteger(ATTR_JOB_VM_MEMORY, int_value) != 1) &&
		(jobAd->LookupInteger(ATTR_REQUEST_MEMORY, int_value) != 1) ) {
		dprintf(D_ALWAYS, "Can't find VM memory in Job ClassAd\n");
		return false;
	}
	if( !int_value || ( int_value > getFreeVMMemSize() )) {
		dprintf(D_ALWAYS, "Not enough memory for VM: Requested mem=%d(MB),"
			   " Freemem=%d(MB)\n", int_value, getFreeVMMemSize()); 
		return false;
	}

	// check if the MAC address for checkpointed VM
	// is conflict with running VMs.
	std::string string_value;
	if( jobAd->LookupString(ATTR_VM_CKPT_MAC, string_value) == 1 ) {
		if( findVMStarterInfoWithMac(string_value.c_str()) ) {
			dprintf(D_ALWAYS, "MAC address[%s] for VM is already being used "
					"by other VM\n", string_value.c_str());
			return false;
		}
	}

	return true;
}

bool
VMUniverseMgr::allocVM(pid_t s_pid, ClassAd &ad, char const *execute_dir)
{
	if( canCreateVM(&ad) == false ) {
		return false;
	}

	// Find memory for VM
	int vm_mem = 0;
	if( (ad.LookupInteger(ATTR_JOB_VM_MEMORY, vm_mem) != 1) &&
	    (ad.LookupInteger(ATTR_REQUEST_MEMORY, vm_mem) != 1) ) {
		dprintf(D_ALWAYS, "Can't find VM memory in Job ClassAd\n");
		return false;
	}

	int vcpus = 0;
	if( (ad.LookupInteger(ATTR_JOB_VM_VCPUS, vcpus) != 1) &&
	    (ad.LookupInteger(ATTR_REQUEST_CPUS, vcpus) != 1) )
	  {
	    dprintf(D_FULLDEBUG, "Defaulting to one CPU\n");
	    vcpus = 1;
	  }

	// check whether this pid already exists
	VMStarterInfo *oldinfo = findVMStarterInfoWithStarterPid(s_pid);
	if( oldinfo ) {
		freeVM(s_pid);
		// oldinfo is freed 
		oldinfo = NULL;
	}
	
	VMStarterInfo *newinfo = new VMStarterInfo;
	ASSERT(newinfo);

	m_vm_used_memory += vm_mem;

	newinfo->m_pid = s_pid;
	newinfo->m_memory = vm_mem;
	newinfo->m_job_ad = ad; 
	newinfo->m_execute_dir = execute_dir;
	newinfo->m_vcpus = vcpus;

	// If there exists MAC or IP address for a checkpointed VM,
	// we use them as initial values.
	std::string string_value;
	if( ad.LookupString(ATTR_VM_CKPT_MAC, string_value) == 1 ) {
		newinfo->m_vm_mac = string_value;
	}
	/*
	string_value = "";
	if( ad.LookupString(ATTR_VM_CKPT_IP, string_value) == 1 ) {
		newinfo->m_vm_ip = string_value;
	}
	*/

	m_vm_starter_list.push_back(newinfo);
	return true;
}

void
VMUniverseMgr::freeVM(pid_t s_pid)
{
	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);
	if( !info ) {
		return;
	}

	std::string pid_dir;
	Directory execute_dir(info->m_execute_dir.c_str(), PRIV_ROOT);
	formatstr(pid_dir,"dir_%ld", (long)s_pid);

	if( execute_dir.Find_Named_Entry( pid_dir.c_str() ) ) {
		// starter didn't exit cleanly,
		// maybe it seems to be killed by startd.
		// So we need to make sure that VM is really destroyed. 
		killVM(info);
	}

	m_vm_used_memory -= info->m_memory;
	m_vm_starter_list.erase(std::find(m_vm_starter_list.begin(), m_vm_starter_list.end(), info));
	delete info;

	if( !m_vm_starter_list.size() && m_needCheck ) { 
		// the last vm job is just finished
		// if m_needCheck is true, we need to call docheckVMUniverse
		docheckVMUniverse();
	}
}

VMStarterInfo *
VMUniverseMgr::findVMStarterInfoWithStarterPid(pid_t s_pid)
{
	if( s_pid <= 0 ) {
		return nullptr;
	}

	for (VMStarterInfo *info: m_vm_starter_list) {
		if( info->m_pid == s_pid ) {
			return info;
		}
	}
	return nullptr;
}

VMStarterInfo *
VMUniverseMgr::findVMStarterInfoWithVMPid(pid_t vm_pid)
{
	if( vm_pid <= 0 ) {
		return NULL;
	}

	for (VMStarterInfo *info: m_vm_starter_list) {
		if( info->m_vm_pid == vm_pid ) {
			return info;
		}
	}
	return nullptr;
}

VMStarterInfo *
VMUniverseMgr::findVMStarterInfoWithMac(const char* mac)
{
	if( !mac ) {
		return NULL;
	}

	for (VMStarterInfo *info: m_vm_starter_list) {
		if( !strcasecmp(info->m_vm_mac.c_str(), mac) ) {
			return info;
		}
	}
	return nullptr;
}

VMStarterInfo *
VMUniverseMgr::findVMStarterInfoWithIP(const char* ip)
{
	if( !ip ) {
		return nullptr;
	}

	for (VMStarterInfo *info: m_vm_starter_list) {
		if( !strcasecmp(info->m_vm_ip.c_str(), ip) ) {
			return info;
		}
	}
	return nullptr;
}

void
VMUniverseMgr::checkVMUniverse( bool warn )
{
	// This might be better as detect, where detect never sets m_needCheck
	// because it should be an error for there to be running VMs on startup.
	if( warn ) {
		dprintf( D_ALWAYS, "VM-gahp server reported an internal error\n");
	}

	if( numOfRunningVM() == 0 ) {
		// There is no running VM job.
		// So test vm universe right now.
		docheckVMUniverse();
		return;
	}

	// There are running VM jobs.
	// We need to wait for all jobs to be finished
	// When all jobs are finished, we will call docheckVMUniverse function
	m_needCheck = true;
}

void 
VMUniverseMgr::docheckVMUniverse( int /* timerID */ )
{
	char *vm_type = param( "VM_TYPE" );
	dprintf( D_ALWAYS, "VM universe will be tested "
			"to check if it is available\n");
	if( init() == false && vm_type ) {
		// VM universe is desired, but not available

		// In VMware, some errors may be transient.
		// For example, when VMware fails to start a new VM 
		// due to an incorrect config file, we are unable to 
		// run 'vmrun' command for a while.
		// But after some time, we are able to run it again.
		// So we register a timer to call this function later.
		m_check_interval = param_integer("VM_RECHECK_INTERVAL", 600);
		if ( m_check_tid >= 0 ) {
			daemonCore->Reset_Timer( m_check_tid, m_check_interval );
		} else {
			m_check_tid = daemonCore->Register_Timer(m_check_interval,
				(TimerHandlercpp)&VMUniverseMgr::docheckVMUniverse,
				"VMUniverseMgr::docheckVMUniverse", this);
		}
		dprintf( D_ALWAYS, "Started a timer to test VM universe after "
			   "%d(seconds)\n", m_check_interval);
	} else {
		if ( m_check_tid >= 0 ) {
			daemonCore->Cancel_Timer( m_check_tid );
			m_check_tid = -1;

			// in the case where we had to use the timer,
			// make certain we publish our changes.  
			if( resmgr ) {
				resmgr->eval_and_update_all();
			}	
		}
	}
	free( vm_type );
}

void 
VMUniverseMgr::setStarterAbility(bool has_vmcode)
{
	m_starter_has_vmcode = has_vmcode;
}

int
VMUniverseMgr::numOfRunningVM(void)
{
	return (int) m_vm_starter_list.size();
}

void
VMUniverseMgr::killVM(const char *matchstring)
{
	if ( !matchstring ) {
		return;
	}
	if( !m_vm_type.length() || !m_vmgahp_server.length() ) {
		return;
	}

	// vmgahp is daemonCore, so we need to add -f -t options of daemonCore.
	// Then, try to execute vmgahp with 
	// vmtype <vmtype> match <string>"
	ArgList systemcmd;
	systemcmd.AppendArg(m_vmgahp_server);
	systemcmd.AppendArg("-f");
	char *gahp_log_file = param("VM_GAHP_LOG");
	if( gahp_log_file ) {
		free(gahp_log_file);
	}else {
		systemcmd.AppendArg("-t");
	}
	systemcmd.AppendArg("-M");
	systemcmd.AppendArg(std::to_string(VMGAHP_KILL_MODE));
	systemcmd.AppendArg("vmtype");
	systemcmd.AppendArg(m_vm_type);
	systemcmd.AppendArg("match");
	systemcmd.AppendArg(matchstring);

#if !defined(WIN32)
	if( can_switch_ids() ) {
		std::string tmp_str;
		formatstr(tmp_str,"%d", (int)get_condor_uid());
		SetEnv("VMGAHP_USER_UID", tmp_str.c_str());
	}
#endif

	// execute vmgahp with root privilege
	priv_state priv = set_root_priv();
	int ret = my_system(systemcmd);
	// restore privilege
	set_priv(priv);

	if( ret == 0 ) {
		dprintf( D_ALWAYS, "VMUniverseMgr::killVM() is called with "
						"'%s'\n", matchstring );
	}else {
		dprintf( D_ALWAYS, "VMUniverseMgr::killVM() failed!\n");
	}

	return;
}

void
VMUniverseMgr::killVM(VMStarterInfo *info)
{
	if( !info ) {
		return;
	}
	if( !m_vm_type.length() || !m_vmgahp_server.length() ) {
		return;
	}

	if( info->m_vm_pid > 0 ) {
		dprintf( D_ALWAYS, "In VMUniverseMgr::killVM(), "
				"Sending SIGKILL to Process[%d]\n", (int)info->m_vm_pid);
		daemonCore->Send_Signal(info->m_vm_pid, SIGKILL);
	}

	std::string matchstring;

	if( (strcasecmp(m_vm_type.c_str(), CONDOR_VM_UNIVERSE_XEN ) == MATCH) || (strcasecmp(m_vm_type.c_str(), CONDOR_VM_UNIVERSE_KVM) == 0)) {
		if( create_name_for_VM(&info->m_job_ad, matchstring) == false ) {
			dprintf(D_ALWAYS, "VMUniverseMgr::killVM() : "
					"cannot make the name of VM\n");
			return;
		}
	}else {
		// Except Xen, we need the path of working directory of Starter
		// in order to destroy VM.
		formatstr(matchstring, "%s%cdir_%ld", info->m_execute_dir.c_str(),
	                   DIR_DELIM_CHAR, (long)info->m_pid);
	}

	killVM( matchstring.c_str() );
}

bool 
VMUniverseMgr::isStarterForVM(pid_t s_pid)
{
	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);

	if(info) {
		return true;
	}else {
		return false;
	}
}

bool
VMUniverseMgr::getUsageForVM(pid_t s_pid, ProcFamilyUsage &usage)
{
	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);
	if( !info ) {
		return false;
	}

	ProcFamilyUsage vm_usage;
	info->getUsageOfVM(vm_usage);

	usage.user_cpu_time += vm_usage.user_cpu_time;
	usage.sys_cpu_time += vm_usage.sys_cpu_time;
	usage.percent_cpu += vm_usage.percent_cpu;
	if( usage.max_image_size < vm_usage.max_image_size ) {
		usage.max_image_size = vm_usage.max_image_size;
	}
	usage.total_image_size += vm_usage.total_image_size;
	usage.total_resident_set_size += vm_usage.total_resident_set_size;
#if HAVE_PSS
	if( vm_usage.total_proportional_set_size_available ) {
		usage.total_proportional_set_size_available = true;
		usage.total_proportional_set_size += vm_usage.total_proportional_set_size;
	}
#endif
	return true;
}

bool
VMUniverseMgr::addProcessForVM(pid_t s_pid, pid_t vm_pid)
{
	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);
	if( !info ) {
		return false;
	}

	info->addProcessForVM(vm_pid);
	dprintf(D_ALWAYS, "In VMUniverseMgr::addProcessForVM, process[%d] is "
			"added to Starter[%d]\n", (int)vm_pid, (int)s_pid);
	return true;
}

pid_t
VMUniverseMgr::getProcessForVM(pid_t s_pid)
{
	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);
	if( !info ) {
		return 0;
	}

	return info->getProcessForVM();
}

void
VMUniverseMgr::addMACForVM(pid_t s_pid, const char *mac)
{
	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);
	if( !info ) {
		return;
	}

	info->addMACForVM(mac);
	dprintf(D_FULLDEBUG, "In VMUniverseMgr::addMACForVM, mac[%s] is "
			"added to Starter[%d]\n", mac, (int)s_pid);
}

const char*
VMUniverseMgr::getMACForVM(pid_t s_pid)
{
	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);
	if( !info ) {
		return NULL;
	}

	return info->getMACForVM();
}

void
VMUniverseMgr::addIPForVM(pid_t s_pid, const char *ip)
{
	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);
	if( !info ) {
		return;
	}

	info->addIPForVM(ip);
	dprintf(D_FULLDEBUG, "In VMUniverseMgr::addIPForVM, ip[%s] is "
			"added to Starter[%d]\n", ip, (int)s_pid);
}

const char*
VMUniverseMgr::getIPForVM(pid_t s_pid)
{
	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);
	if( !info ) {
		return NULL;
	}

	return info->getIPForVM();
}

void
VMUniverseMgr::publishVMInfo(pid_t s_pid, ClassAd* ad)
{
	if( !ad ) {
		return;
	}

	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);
	if( !info ) {
		return;
	}

	info->publishVMInfo(ad);
	return;
}
