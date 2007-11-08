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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_environ.h"
#include "startd.h"
#include "my_popen.h"
#include "daemon.h"
#include "daemon_types.h"
#include "directory.h"
#include "condor_environ.h"
#include "../condor_privsep/condor_privsep.h"
#include "vmuniverse_mgr.h"
#include "condor_vm_universe_types.h"
#include "vm_univ_utils.h"

static unsigned long get_image_size(procInfo& pi)
{
#if defined(WIN32)
	return pi.rssize;
#else
	return pi.imgsize;
#endif
}

static bool check_vmgahp_permissions(const char* vmgahp, const char* gahpconfig, const char* vmtype)
{
	if( !vmgahp || vmgahp[0] == '\0' || !gahpconfig || gahpconfig[0] == '\0' || 
			!vmtype || vmtype[0] == '\0' ) {
		return false;
	}

#if !defined(WIN32) 
	bool vmgahp_has_setuid_root = false;
	struct stat vmgahp_sbuf;
	struct stat gahpconfig_sbuf;
	if( stat(vmgahp, &vmgahp_sbuf) < 0 )  {
		dprintf( D_ALWAYS, "ERROR: Failed to stat() for \"%s\":%s\n",
				vmgahp, strerror(errno));
		return false;
	}
	if( stat(gahpconfig, &gahpconfig_sbuf) < 0 )  {
		dprintf( D_ALWAYS, "ERROR: Failed to stat() for \"%s\":%s\n",
				gahpconfig, strerror(errno));
		return false;
	}

	// Does vmgahp have setuid-root ?
	if( (vmgahp_sbuf.st_uid == 0) && (vmgahp_sbuf.st_mode & S_ISUID)) { 
		vmgahp_has_setuid_root = true;
	}

	// Other writable bit
	if( vmgahp_sbuf.st_mode & S_IWOTH ) {
		dprintf( D_ALWAYS, "ERROR: other writable bit is not allowed for \"%s\" "
			   "due to security issues\n", vmgahp);
		return false;
	}
	if( gahpconfig_sbuf.st_mode & S_IWOTH ) {
		dprintf( D_ALWAYS, "ERROR: other writable bit is not allowed for \"%s\" "
			   "due to security issues\n", gahpconfig);
		return false;
	}

	if( can_switch_ids() ) {
		// Condor runs as root
		if( strcasecmp(vmtype, CONDOR_VM_UNIVERSE_XEN ) != MATCH ) {
			// check if vmgahp has setuid-root 
			// Running Condor as root with setuid-root vmgahp
			// causes problems.
			if( vmgahp_sbuf.st_mode & S_ISUID ) { 
				dprintf( D_ALWAYS, "ERROR: Condor is running as root. "
						"However, vmgahp(\"%s\") has also setuid. "
						"To run Condor as root with setuid vmgahp is not"
						"allowed\n", vmgahp);
				return false;
			}
		}
	}else {
		bool need_setuid_root = false;

		// Condor runs as user
		if( privsep_enabled() ) {

			/* 
			 * If we are using privsep, vmgahp must also have setuid-root.
			 *
			 * When we use privsep, we can kill processes with 
			 * the help of procd. 
			 * However, the process dealing with a VM may be unable to be 
			 * tracked by procd. (e.g.) a VM process created by VMware.
			 *
			 * For example, when Startd forcedly killed Starter and all its child 
			 * processes, VM process might still remain.
			 * 
			 * Therefore, vmgahp should deal with this situation. 
			 * So when we use privsep, vmgahp executed by Startd need to 
			 * have setuid-root. That is why vmgahp must have setuid-root.
			 */

			if( !vmgahp_has_setuid_root ) {
				need_setuid_root = true;
				dprintf( D_ALWAYS, "ERROR: In order to use vm universe "
						"with privsep enabled, vmgahp('%s') must also have "
						"setuid-root\n", vmgahp);
			}
		}

		if( vmgahp_has_setuid_root || need_setuid_root ) {
			// vmgahp has setuid-root
			if( gahpconfig_sbuf.st_uid != 0 ) {
				if( need_setuid_root ) {
					dprintf(D_ALWAYS, "ERROR: Because vmgahp(\"%s\") need to have "
							"setuid-root, the owner of both \"%s\" and script program "
							"must be root\n", vmgahp, gahpconfig);
				}else {
					dprintf(D_ALWAYS, "ERROR: Because vmgahp(\"%s\") has setuid-root, "
							"the owner of both \"%s\" and script program must be root\n",
							vmgahp, gahpconfig);
				}
				return false;
			}
		}

		if( need_setuid_root ) {
			return false;
		}
	}
#endif

	if( !strcasecmp(vmtype, CONDOR_VM_UNIVERSE_XEN)) {
#if !defined(LINUX)
		dprintf( D_ALWAYS, "ERROR: vmgahp for Xen is only supported on "
				"Linux machines\n");
		return false;
#else
		// Linux
		if( !can_switch_ids() ) {
			// Condor runs as user
			// So vmgahp for Xen needs setuid-root
			if( !vmgahp_has_setuid_root ) {
				// No setuid-root bit
				dprintf(D_ALWAYS, "ERROR: \"%s\" must have setuid-root bit\n", 
						vmgahp);
				return false;
			}
		}

		if( gahpconfig_sbuf.st_uid != 0 ) {
			dprintf(D_ALWAYS, "ERROR: For Xen vmgahp the owner of \"%s\" must be root\n", 
					gahpconfig);
			return false;
		}
#endif
	}else if( !strcasecmp(vmtype, CONDOR_VM_UNIVERSE_VMWARE)) {
#if !defined(WIN32) 
		if( can_switch_ids() || privsep_enabled() ) {
			// Condor runs as root
			if( !(vmgahp_sbuf.st_mode & S_IXOTH) ) {
				dprintf(D_ALWAYS, "ERROR: \"%s\" must be executable for anybody.\n", 
						vmgahp); 
				return false;
			}
			if( !(gahpconfig_sbuf.st_mode & S_IROTH) ) {
				dprintf(D_ALWAYS, "ERROR: \"%s\" must be readable for anybody.\n", 
						gahpconfig); 
				return false;
			}
		}
#endif
	}

	return true;
}


VMStarterInfo::VMStarterInfo()
{
	m_pid = 0;
	m_memory = 0;

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
	struct procInfo pinfo;
	memset(&pinfo, 0, sizeof(pinfo));

	piPTR pi = &pinfo;
	if( ProcAPI::getProcInfo(m_vm_pid, pi, proc_status) == 
			PROCAPI_SUCCESS ) {
		memcpy(&m_vm_alive_pinfo, &pinfo, sizeof(pinfo));
		if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_LOAD) ) {
			dprintf(D_FULLDEBUG,"Usage of process[%d] for a VM is updated\n", 
					m_vm_pid);
			dprintf(D_FULLDEBUG,"sys_time=%lu, user_time=%lu, image_size=%lu\n", 
					pinfo.sys_time, pinfo.user_time, get_image_size(pinfo));
		}
		return true;
	}
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
	}else {
		usage.total_image_size = 0;
	}

	if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_LOAD) ) {
		dprintf( D_FULLDEBUG,
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

	// Reset usage of the current process for VM
	memset(&m_vm_alive_pinfo, 0, sizeof(m_vm_alive_pinfo));
	m_vm_pid = vm_pid;
}

pid_t
VMStarterInfo::getProcessForVM(void)
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
	return m_vm_mac.GetCStr();
}

void
VMStarterInfo::addIPForVM(const char* ip)
{
	m_vm_ip = ip;
}

const char *
VMStarterInfo::getIPForVM(void)
{
	return m_vm_ip.GetCStr();
}

void 
VMStarterInfo::publishVMInfo(ClassAd* ad, amask_t mask )
{
	if( !ad ) {
		return;
	}
	if( m_vm_mac.IsEmpty() == false ) {
		ad->Assign(ATTR_VM_GUEST_MAC, m_vm_mac);
	}
	if( m_vm_ip.IsEmpty() == false ) {
		ad->Assign(ATTR_VM_GUEST_IP, m_vm_ip);
	}
	if( m_memory > 0 ) {
		ad->Assign(ATTR_VM_GUEST_MEM, m_memory); 
	}
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
	MyString vmtype;
	MyString vmgahppath;
	MyString vmgahpconfig;

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

	tmp = param( "VM_GAHP_CONFIG" );
	if( !tmp ) {
		dprintf( D_ALWAYS, "To support vm universe, '%s' must be defined "
				"in condor config file\n", "VM_GAHP_CONFIG"); 
		return false;
	}
	vmgahpconfig = tmp;
	free(tmp);

	m_vm_max_num = 0;
	tmp = param( "VM_MAX_NUMBER");
	if( tmp ) {
		int vmax = (int)strtol(tmp, (char **)NULL, 10);
		if( vmax > 0 ) {
			m_vm_max_num = vmax;
		}
		free(tmp);
	}

	// now, we've got a path to a vmgahp server.  
	// try to test it with given vmtype and gahp config file 
	// and grab the output (whose format should be a classad type), 
	// and set the appropriate values for vm universe
	if( testVMGahp(vmgahppath.Value(), vmgahpconfig.Value(), 
				vmtype.Value()) == false ) {
		dprintf( D_ALWAYS, "Test of vmgahp for VM_TYPE('%s') failed. "
				"So we disabled VM Universe\n", vmtype.Value());
		m_vm_type = "";
		return false;
	}
	return true;
}

void
VMUniverseMgr::printVMGahpInfo( int debug_level )
{
	dprintf( debug_level, "........VMGAHP info........\n");
	m_vmgahp_info.AttrList::dPrint(debug_level);
	dprintf( debug_level, "\n");
}

void
VMUniverseMgr::publish( ClassAd* ad, amask_t mask )
{
	if( !ad ) {
		return;
	}
	if( !m_starter_has_vmcode || ( m_vm_type.Length() == 0 )) {
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
	m_vmgahp_info.ResetExpr();

	MyString line;
	ExprTree* expr = NULL;
	char *attr_name = NULL;
	while((expr = m_vmgahp_info.NextExpr()) != NULL) {
		attr_name = ((Variable*)expr->LArg())->Name();

		// we need to adjust available vm memory
		if( stricmp(attr_name, ATTR_VM_MEMORY) == MATCH ) {
			int freemem = getFreeVMMemSize();
			ad->Assign(ATTR_VM_MEMORY, freemem);
		}else if( stricmp(attr_name, ATTR_VM_NETWORKING) == MATCH ) {
			ad->Assign(ATTR_VM_NETWORKING, m_vm_networking); 
		}else {
			expr->PrintToStr(line);
			ad->Insert(line.Value());
		}
	}

	// Now, we will publish mac and ip addresses of all guest VMs.
	MyString all_macs;
	MyString all_ips;
	VMStarterInfo *info = NULL;
	const char* guest_ip = NULL;
	const char* guest_mac = NULL;

	m_vm_starter_list.Rewind();
	while( m_vm_starter_list.Next(info) ) {
		guest_ip = info->getIPForVM();
		if( guest_ip ) {
			if( all_ips.IsEmpty() == false ) {
				all_ips += ",";
			}
			all_ips += guest_ip;
		}

		guest_mac = info->getMACForVM();
		if( guest_mac ) {
			if( all_macs.IsEmpty() == false ) {
				all_macs += ",";
			}
			all_macs += guest_mac;
		}
	}
	if( all_ips.IsEmpty() == false ) {
		ad->Assign(ATTR_VM_ALL_GUEST_IPS, all_ips);
	}
	if( all_macs.IsEmpty() == false ) {
		ad->Assign(ATTR_VM_ALL_GUEST_MACS, all_macs);
	}
}

bool
VMUniverseMgr::testVMGahp(const char* gahppath, const char* gahpconfigfile, const char* vmtype)
{
	m_needCheck = false;

	if( !m_starter_has_vmcode ) {
		return false;
	}

	if( !gahppath || !gahpconfigfile || !vmtype ) {
		return false;
	}

	if( !check_vmgahp_permissions(gahppath, gahpconfigfile, vmtype) ) {
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
	// "gahpconfig <gahpconfigfile> vmtype <vmtype>"
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
	systemcmd.AppendArg(VMGAHP_TEST_MODE);
	systemcmd.AppendArg("gahpconfig");
	systemcmd.AppendArg(gahpconfigfile);
	systemcmd.AppendArg("vmtype");
	systemcmd.AppendArg(vmtype);

#if !defined(WIN32)
	MyString oldValue;
	const char *envName = NULL;

	if( !can_switch_ids() && has_root_setuid(gahppath) ) {
		// Condor runs as user and vmgahp has setuid-root
		// We need to set CONDOR_IDS environment
		envName = EnvGetName(ENV_UG_IDS);
		if( envName ) {
			MyString tmp_str;
			tmp_str.sprintf("%d.%d", (int)get_condor_uid(),(int)get_condor_gid());
			set_temporary_env(envName, tmp_str.Value(), &oldValue);
		}
	}

	if( can_switch_ids() ) {
		MyString tmp_str;
		tmp_str.sprintf("%d", (int)get_condor_uid());
		set_temporary_env("VMGAHP_USER_UID", tmp_str.Value(), &oldValue);
	}
#endif

	priv_state prev_priv;
	if( strcasecmp(vmtype, CONDOR_VM_UNIVERSE_XEN) == MATCH ) {
		// Xen requires root privilege
		prev_priv = set_root_priv();
	}else {
		prev_priv = set_condor_priv();

	}
	FILE* fp = NULL;
	fp = my_popen(systemcmd, "r", FALSE );
	set_priv(prev_priv);

#if !defined(WIN32)
	if( envName ) {
		set_temporary_env(envName, oldValue.GetCStr(), NULL);
	}
#endif

	if( !fp ) {
		dprintf( D_ALWAYS, "Failed to execute %s, ignoring\n", gahppath );
		return false;
	}

	bool read_something = false;
	char buf[2048];

	m_vmgahp_info.clear();
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
		MyString args_string;
		systemcmd.GetArgsStringForDisplay(&args_string,0);
		dprintf( D_ALWAYS, 
				 "Warning: '%s' did not produce any valid output.\n", 
				 args_string.Value());
		if( strcasecmp(vmtype, CONDOR_VM_UNIVERSE_XEN) == 0 ) {
			MyString err_msg;
			err_msg += "\n#######################################################\n";
			err_msg += "##### Make sure the followings ";
			err_msg += "to use VM universe for Xen\n";
			err_msg += "### - The owner of script progrm like ";
			err_msg += "'condor_vm_xen.sh' must be root\n";
			err_msg += "### - The script program must be executable\n";
			err_msg += "### - Other writable bit for the above files is ";
			err_msg += "not allowed.\n";
			err_msg += "#########################################################\n";
			dprintf( D_ALWAYS, "%s", err_msg.Value());
		}else if( strcasecmp(vmtype, CONDOR_VM_UNIVERSE_VMWARE ) == 0 ) {
			MyString err_msg;
			MyString err_msg2;
			err_msg += "\n#######################################################\n";
			err_msg += "##### Make sure the followings ";
			err_msg += "to use VM universe for VMware\n";

			if( can_switch_ids() ) {
				// Condor runs as root
#if defined(WIN32)
				err_msg2.sprintf("### - \"%s\" must be readable for anybody.\n", 
						gahpconfigfile);
				err_msg += err_msg2;
#endif

				err_msg += "### - The script program like 'condor_vm_vmware.pl'";
				err_msg += " must be readable for anybody.\n";
			}

#if !defined(WIN32)
			err_msg += "### - Other writable bit for the vmgahp config file and ";
			err_msg += "script program is not allowed\n";
#endif

			err_msg += "### - Check the path of vmware-cmd, vmrun, and mkisofs ";
			err_msg += "in 'condor_vm_vmware.pl\n'";
			err_msg += "#########################################################\n";
			dprintf( D_ALWAYS, "%s", err_msg.Value());
		}
		return false;
	}

	// For debug
	printVMGahpInfo(D_ALWAYS);

	// Read vm_type
	MyString tmp_vmtype;
	if( m_vmgahp_info.LookupString( ATTR_VM_TYPE, tmp_vmtype) != 1 ) {
		dprintf( D_ALWAYS, "There is no %s in the output of vmgahp. "
				"So VM Universe will be disabled\n", ATTR_VM_TYPE);
		return false;
	}
	if( strcasecmp(tmp_vmtype.Value(), vmtype) != 0 ) {
		dprintf( D_ALWAYS, "The vmgahp(%s) doesn't support this vmtype(%s)\n",
				gahppath, vmtype);
		return false;
	}
	dprintf( D_ALWAYS, "VMType('%s') is supported\n", vmtype);

	// Read vm_memory
	int tmp_mem = 0;
	if( m_vmgahp_info.LookupInteger(ATTR_VM_MEMORY, tmp_mem) != 1 ) {
		dprintf( D_ALWAYS, "There is no %s in the output of vmgahp\n",ATTR_VM_MEMORY);
		return false;
	}
	if( tmp_mem == 0 ) {
		dprintf( D_ALWAYS, "There is no sufficient memory for virtual machines\n");
		return false;
	}

	// VM_MEMORY in condor config should be less than 
	// VM_MEMORY provided by vmgahp */
	char *vmtmp = NULL;
	vmtmp = param( "VM_MEMORY" );
	if(vmtmp) {
		int vmem = (int)strtol(vmtmp, (char **)NULL, 10);
		if( (vmem <= 0) || (vmem > tmp_mem)) {
			m_vm_max_memory = tmp_mem;
			dprintf( D_ALWAYS, "Warning: Even though '%s = %d' is defined "
					"in condor config file, the amount of memory for "
					"vm universe is set to %d MB, because vmgahp says "
					"(%d MB) as the maximum memory for vm universe\n", 
					"VM_MEMORY", vmem, tmp_mem, tmp_mem);
		}else {
			m_vm_max_memory = vmem;
		}
		free(vmtmp);
	} else {
		m_vm_max_memory = tmp_mem;
	}
	dprintf( D_ALWAYS, "The maximum available memory for vm universe is "
			"set to %d MB\n", m_vm_max_memory);

	// Read vm_networking
	bool tmp_networking = false;
	MyString tmp_networking_types;

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
				tmp_networking_types.Value());
	}
			
	// Now, we received correct information from vmgahp
	m_vm_type = tmp_vmtype;
	m_vmgahp_server = gahppath;
	m_vmgahp_config = gahpconfigfile;

	return true;
}

int
VMUniverseMgr::getFreeVMMemSize()
{
	return (m_vm_max_memory - m_vm_used_memory);
}

bool
VMUniverseMgr::canCreateVM(ClassAd *jobAd)
{
	if( !m_starter_has_vmcode || ( m_vm_type.Length() == 0 )) {
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
	if( jobAd->LookupInteger(ATTR_JOB_VM_MEMORY, int_value) != 1 ) {
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
	MyString string_value;
	if( jobAd->LookupString(ATTR_VM_CKPT_MAC, string_value) == 1 ) {
		if( findVMStarterInfoWithMac(string_value.GetCStr()) ) {
			dprintf(D_ALWAYS, "MAC address[%s] for VM is already being used "
					"by other VM\n", string_value.Value());
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
	if( ad.LookupInteger(ATTR_JOB_VM_MEMORY, vm_mem) != 1 ) {
		dprintf(D_ALWAYS, "Can't find VM memory in Job ClassAd\n");
		return false;
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

	// If there exists MAC or IP address for a checkpointed VM,
	// we use them as initial values.
	MyString string_value;
	if( ad.LookupString(ATTR_VM_CKPT_MAC, string_value) == 1 ) {
		newinfo->m_vm_mac = string_value;
	}
	/*
	string_value = "";
	if( ad.LookupString(ATTR_VM_CKPT_IP, string_value) == 1 ) {
		newinfo->m_vm_ip = string_value;
	}
	*/

	m_vm_starter_list.Append(newinfo);
	return true;
}

void
VMUniverseMgr::freeVM(pid_t s_pid)
{
	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);
	if( !info ) {
		return;
	}

	MyString pid_dir;
	Directory execute_dir(info->m_execute_dir.Value(), PRIV_ROOT);
	pid_dir.sprintf("dir_%ld", (long)s_pid);

	if( execute_dir.Find_Named_Entry( pid_dir.Value() ) ) {
		// starter didn't exit cleanly,
		// maybe it seems to be killed by startd.
		// So we need to make sure that VM is really destroyed. 
		killVM(info);
	}

	m_vm_used_memory -= info->m_memory;
	m_vm_starter_list.Delete(info);
	delete info;

	if( !m_vm_starter_list.Number() && m_needCheck ) { 
		// the last vm job is just finished
		// if m_needCheck is true, we need to call docheckVMUniverse
		docheckVMUniverse();
	}
}

VMStarterInfo *
VMUniverseMgr::findVMStarterInfoWithStarterPid(pid_t s_pid)
{
	if( s_pid <= 0 ) {
		return NULL;
	}

	VMStarterInfo *info = NULL;

	m_vm_starter_list.Rewind();
	while( m_vm_starter_list.Next(info) ) {
		if( info->m_pid == s_pid ) {
			return info;
		}
	}
	return NULL;
}

VMStarterInfo *
VMUniverseMgr::findVMStarterInfoWithVMPid(pid_t vm_pid)
{
	if( vm_pid <= 0 ) {
		return NULL;
	}

	VMStarterInfo *info = NULL;

	m_vm_starter_list.Rewind();
	while( m_vm_starter_list.Next(info) ) {
		if( info->m_vm_pid == vm_pid ) {
			return info;
		}
	}
	return NULL;
}

VMStarterInfo *
VMUniverseMgr::findVMStarterInfoWithMac(const char* mac)
{
	if( !mac ) {
		return NULL;
	}

	VMStarterInfo *info = NULL;

	m_vm_starter_list.Rewind();
	while( m_vm_starter_list.Next(info) ) {
		if( !strcasecmp(info->m_vm_mac.Value(), mac) ) {
			return info;
		}
	}
	return NULL;
}

VMStarterInfo *
VMUniverseMgr::findVMStarterInfoWithIP(const char* ip)
{
	if( !ip ) {
		return NULL;
	}

	VMStarterInfo *info = NULL;

	m_vm_starter_list.Rewind();
	while( m_vm_starter_list.Next(info) ) {
		if( !strcasecmp(info->m_vm_ip.Value(), ip) ) {
			return info;
		}
	}
	return NULL;
}

void 
VMUniverseMgr::checkVMUniverse(void)
{
	dprintf( D_ALWAYS, "VM-gahp server reported an internal error\n");

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
VMUniverseMgr::docheckVMUniverse(void)
{
	dprintf( D_ALWAYS, "VM universe will be tested "
			"to check if it is still available\n");
	if( init() == false ) {
		// VM universe is not available

		// In VMware, some errors may be transient.
		// For example, when VMware fails to start a new VM 
		// due to an incorrect config file, we are unable to 
		// run 'vmrun' command for a while.
		// But after some time, we are able to run it again.
		// So we register a timer to call this function later.
		m_check_interval = param_integer("VM_RECHECK_INTERVAL", 600);
		m_check_tid = daemonCore->Register_Timer(m_check_interval,
				(TimerHandlercpp)&VMUniverseMgr::init,
				"VMUniverseMgr::init", this);
		dprintf( D_ALWAYS, "Started a timer to test VM universe after "
			   "%d(seconds)\n", m_check_interval);
	}
}

void 
VMUniverseMgr::setStarterAbility(bool has_vmcode)
{
	m_starter_has_vmcode = has_vmcode;
}

int
VMUniverseMgr::numOfRunningVM(void)
{
	return m_vm_starter_list.Number();
}

void
VMUniverseMgr::killVM(VMStarterInfo *info)
{
	if( !info ) {
		return;
	}
	if( !m_vm_type.Length() || !m_vmgahp_server.Length() || 
			!m_vmgahp_config.Length() ) {
		return;
	}

	if( info->m_vm_pid > 0 ) {
		dprintf( D_ALWAYS, "In VMUniverseMgr::killVM(), "
				"Sending SIGKILL to Process[%d]\n", (int)info->m_vm_pid);
		daemonCore->Send_Signal(info->m_vm_pid, SIGKILL);
	}

	MyString matchstring;
	MyString workingdir;

	workingdir.sprintf("%s%cdir_%ld", info->m_execute_dir.Value(),
	                   DIR_DELIM_CHAR, (long)info->m_pid);

	if( strcasecmp(m_vm_type.Value(), CONDOR_VM_UNIVERSE_XEN ) == MATCH ) {
		if( create_name_for_VM(&info->m_job_ad, matchstring) == false ) {
			dprintf(D_ALWAYS, "VMUniverseMgr::killVM() : "
					"cannot make the name of VM\n");
			return;
		}
	}else {
		// Except Xen, we need the path of working directory of Starter
		// in order to destroy VM.
		matchstring = workingdir;
	}

	// vmgahp is daemonCore, so we need to add -f -t options of daemonCore.
	// Then, try to execute vmgahp with 
	// "gahpconfig <gahpconfigfile> vmtype <vmtype> match <string>"
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
	systemcmd.AppendArg(VMGAHP_KILL_MODE);
	systemcmd.AppendArg("gahpconfig");
	systemcmd.AppendArg(m_vmgahp_config);
	systemcmd.AppendArg("vmtype");
	systemcmd.AppendArg(m_vm_type);
	systemcmd.AppendArg("match");
	systemcmd.AppendArg(matchstring);

#if !defined(WIN32)
	MyString oldValue;
	const char *envName = NULL;

	if( !can_switch_ids() && has_root_setuid(m_vmgahp_server.Value()) ) {
		// Condor runs as user and vmgahp has setuid-root
		// We need to set CONDOR_IDS environment
		envName = EnvGetName(ENV_UG_IDS);
		if( envName ) {
			MyString tmp_str;
			tmp_str.sprintf("%d.%d", (int)get_condor_uid(),(int)get_condor_gid());
			set_temporary_env(envName, tmp_str.Value(), &oldValue);
		}
	}

	if( can_switch_ids() ) {
		MyString tmp_str;
		tmp_str.sprintf("%d", (int)get_condor_uid());
		set_temporary_env("VMGAHP_USER_UID", tmp_str.Value(), &oldValue);
	}
#endif

	// execute vmgahp with root privilege
	priv_state priv = set_root_priv();
	int ret = my_system(systemcmd);
	// restore privilege
	set_priv(priv);

#if !defined(WIN32)
	if( envName ) {
		set_temporary_env(envName, oldValue.GetCStr(), NULL);
	}
#endif

	if( ret == 0 ) {
		dprintf( D_ALWAYS, "VMUniverseMgr::killVM() is called with "
						"'%s'\n", matchstring.Value());
	}else {
		dprintf( D_ALWAYS, "VMUniverseMgr::killVM() failed!\n");
	}

	return;
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
VMUniverseMgr::publishVMInfo(pid_t s_pid, ClassAd* ad, amask_t mask )
{
	if( !ad ) {
		return;
	}

	VMStarterInfo *info = findVMStarterInfoWithStarterPid(s_pid);
	if( !info ) {
		return;
	}

	info->publishVMInfo(ad, mask);
	return;
}
