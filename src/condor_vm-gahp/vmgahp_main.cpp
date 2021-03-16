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
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "vmgahp_common.h"
#include "vmgahp.h"
#include "vmware_type.h"
#if defined (HAVE_EXT_LIBVIRT) && !defined(VMWARE_ONLY)
#  include "xen_type.linux.h"
#endif
#include "subsystem_info.h"

const char *vmgahp_version = "$VMGahpVersion " CONDOR_VMGAHP_VERSION " May 1 2007 Condor\\ VMGAHP $";

VMGahp *vmgahp = NULL;
int vmgahp_stdout_pipe = -1;
int vmgahp_stderr_pipe = -1;
int vmgahp_mode = VMGAHP_TEST_MODE;

PBuffer vmgahp_stderr_buffer;
int vmgahp_stderr_tid = -1;
#ifndef vmprintf
int	oriDebugFlags = 0;
#endif

std::string workingdir;

// This variables come from vmgahp_common.C
extern std::string caller_name;
extern std::string job_user_name;
extern uid_t caller_uid;
extern uid_t caller_gid;
extern uid_t job_user_uid;
extern uid_t job_user_gid;

void
vm_cleanup(void)
{
	if(vmgahp) {
		delete vmgahp;
		vmgahp = NULL;
		sleep(2);
	}
	return;
}

void Init() {}
void Register() {}
void Reconfig()
{
	// DaemonCore library changes current working directory to
	// LOG directory defined in Condor config file.
	// However, because vmgahp server is usually executed by starter
	// we will change current working directory to job directory
	if( !workingdir.empty() ) {
		if( chdir(workingdir.c_str()) < 0 ) {
			DC_Exit(1);
		}
	}

	// If we use Termlog,
	// we don't want logs from DaemonCore
#ifdef vmprintf
	if (dprintf_to_term_check()) {
		set_debug_flags(NULL, D_NOHEADER);
	}
#else // the old way
	oriDebugFlags = DebugFlags;
	if( dprintf_to_term_check() ) {
		DebugFlags = 0;
	}

	if( (vmgahp_mode != VMGAHP_TEST_MODE) &&
			(vmgahp_mode != VMGAHP_KILL_MODE) ) {
		char *gahp_log_file = param("VM_GAHP_LOG");
		if( gahp_log_file ) {
			if( !strcmp(gahp_log_file, NULL_FILE) ) {
				// We don't want log from vm-gahp
				oriDebugFlags = 0;
				DebugFlags = 0;
			}
			free(gahp_log_file);
		}
	}
#endif
}

void
main_config()
{
	Reconfig();
}

void
main_shutdown_fast()
{
	vmprintf(D_ALWAYS, "Received Signal for shutdown fast\n");
	vm_cleanup();
	DC_Exit(0);
}

void
main_shutdown_graceful()
{
	vmprintf(D_ALWAYS, "Received Signal for shutdown gracefully\n");
	vm_cleanup();
	DC_Exit(0);
}

void
init_uids()
{
#if !defined(WIN32)
	bool SwitchUid = can_switch_ids();

	caller_uid = getuid();
	caller_gid = getgid();

	// Set user uid/gid
	char *val;
	std::string user_uid;
	std::string user_gid;
	val = getenv("VMGAHP_USER_UID");
	user_uid = val ? val : "";
	if( user_uid.empty() == false ) {
		int env_uid = (int)strtol(user_uid.c_str(), (char **)NULL, 10);
		if( env_uid > 0 ) {
			job_user_uid = env_uid;

			// Try to read user_gid
			val = getenv("VMGAHP_USER_GID");
			user_gid = val ? val : "";
			if( user_gid.empty() == false ) {
				int env_gid = (int)strtol(user_gid.c_str(), (char **)NULL, 10);
				if( env_gid > 0 ) {
					job_user_gid = env_gid;
				}
			}
			if( job_user_gid == ROOT_UID ) {
				job_user_gid = job_user_uid;
			}
		}
	}

	if( !SwitchUid ) {
		// We cannot switch uids
		// a job user uid is set to caller uid
		job_user_uid = caller_uid;
		job_user_gid = caller_gid;
	}else {
		// We can switch uids
		if( job_user_uid == ROOT_UID ) {
			// a job user uid is not set yet.
			if( caller_uid != ROOT_UID ) {
				job_user_uid = caller_uid;
				if( caller_gid != ROOT_UID ) {
					job_user_gid = caller_gid;
				}else {
					job_user_gid = caller_uid;
				}
			}else {
				fprintf(stderr, "\nERROR: Please set environment variable "
						"'VMGAHP_USER_UID=<uid>'\n");
				exit(1);
			}
		}
	}

	// find the user name calling this program
	char *user_name = NULL;
	passwd_cache* p_cache = pcache();
	if( p_cache->get_user_name(caller_uid, user_name) == true ) {
		caller_name = user_name;
		free(user_name);
	}

	if( job_user_uid == caller_uid ) {
		job_user_name = caller_name;
	}

	if( SwitchUid ) {
		set_user_ids(job_user_uid, job_user_gid);
		set_user_priv();

		// Try to get the name of a job user
		// If failed, it is harmless.
		if( job_user_uid != caller_uid ) {
			if( p_cache->get_user_name(job_user_uid, user_name) == true ) {
				job_user_name = user_name;
				free(user_name);
			}
		}
	}
#endif
}

void
main_pre_command_sock_init()
{
	daemonCore->WantSendChildAlive( false );
}

static void
usage( char *name)
{
	vmprintf(D_ALWAYS,
			"Usage: \n"
			"\tTestMode: %s -f -t -M 0 vmtype <vmtype> \n"
			"\tStandAlone: %s -f -t -M 3\n"
			"\tKillMode: %s -f -t -M 4 vmtype <vmtype> match <matchstring>\n",
			name, name, name);
	DC_Exit(1);

}

void main_init(int argc, char *argv[])
{
	init_uids();

	char *val;
	std::string vmtype;
	std::string matchstring;

#ifdef vmprintf
	// use D_PID to prefix log lines with (pid:NNN), note that vmprintf output "VMGAHP[NNN]" instead
	//set_debug_flags(NULL, D_PID);
#else
	// save DebugFlags for vmprintf
	oriDebugFlags = DebugFlags;
#endif

	// handle specific command line args
	if( argc < 3 ) {
		usage(argv[0]);
	}

	// Read the first argument
	if( strcmp(argv[1], "-M")) {
		usage(argv[0]);
	}

	vmgahp_mode = atoi(argv[2]);
	if( vmgahp_mode == VMGAHP_IO_MODE ) {
		vmgahp_mode = VMGAHP_STANDALONE_MODE;
	} else if( vmgahp_mode >= VMGAHP_MODE_MAX || vmgahp_mode < 0 ||
			   vmgahp_mode == VMGAHP_WORKER_MODE ) {
		usage(argv[0]);
	}

	vmgahp_stdout_pipe = daemonCore->Inherit_Pipe(fileno(stdout),
			true,		//write pipe
			false,		//nonregisterable
			false);		//blocking

	if( vmgahp_stdout_pipe == -1 ) {
			vmprintf(D_ALWAYS,"Can't get stdout pipe");
			DC_Exit(1);
	}

	if( dprintf_to_term_check() && (vmgahp_mode != VMGAHP_TEST_MODE ) &&
			(vmgahp_mode != VMGAHP_KILL_MODE )) {
		// Initialize pipe for stderr
		vmgahp_stderr_pipe = daemonCore->Inherit_Pipe(fileno(stderr),
				true,		//write pipe
				false,		//nonregisterable
				true);		//non-blocking

		if( vmgahp_stderr_pipe == -1 ) {
			vmprintf(D_ALWAYS,"Can't get stderr pipe");
			DC_Exit(1);
		}
		vmgahp_stderr_buffer.setPipeEnd(vmgahp_stderr_pipe);

		vmgahp_stderr_tid = daemonCore->Register_Timer(2, 2,
				write_stderr_to_pipe,
				"write_stderr_to_pipe");
		if( vmgahp_stderr_tid == -1 ) {
			vmprintf(D_ALWAYS,"Can't register stderr timer");
			DC_Exit(1);
		}
	}else {
		vmgahp_stderr_pipe = -1;
		vmgahp_stderr_tid = -1;
	}

	vmprintf(D_ALWAYS, "VM-GAHP initialized with run-mode %d\n", vmgahp_mode);

	if( (vmgahp_mode == VMGAHP_TEST_MODE) || (vmgahp_mode == VMGAHP_KILL_MODE )) {

		char _vmtype[] = "vmtype";
		char _match[] = "match";

		// This program is called in test mode
		char *opt = NULL, *arg = NULL;
		int opt_len = 0;
		char **tmp = argv + 2;

		for( tmp++ ; *tmp; tmp++ ) {
			opt = tmp[0];
			arg = tmp[1];
			opt_len = strlen(opt);

			if( ! strncmp(opt, _vmtype, opt_len) ) {
				// vm type
				if( !arg ) {
					usage(argv[0]);
				}
				vmtype = arg;
				tmp++; //consume the arg so we don't get confused
				continue;
			}

			if( ! strncmp(opt, _match, opt_len) ) {
				// match string
				if( !arg ) {
					usage(argv[0]);
				}
				matchstring = arg;
				tmp++; //consume the arg so we don't get confused
				continue;
			}

			usage(argv[0]);
		}

		if( vmgahp_mode == VMGAHP_TEST_MODE ) {
			if( vmtype.length() == 0 ) {
				usage(argv[0]);
			}
		}else if( vmgahp_mode == VMGAHP_KILL_MODE ) {
			if( ( vmtype.length() == 0 ) ||
			    ( matchstring.length() == 0 ) )
			{
				usage(argv[0]);
			}
		}
	}else {
		val = getenv("VMGAHP_VMTYPE");
		vmtype = val ? val : "";
		if( vmtype.empty() ) {
			vmprintf(D_ALWAYS, "cannot find vmtype\n");
			DC_Exit(1);
		}

		char *val = getenv("VMGAHP_WORKING_DIR");
		workingdir = val ? val : "";
		if( workingdir.empty() ) {
			vmprintf(D_ALWAYS, "cannot find vmgahp working dir\n");
			DC_Exit(1);
		}
	}

	Init();
	Register();
	Reconfig();

	// change vmtype to lowercase
	lower_case(vmtype);

	// check whether vmtype is supported by this gahp server
	if( verify_vm_type(vmtype.c_str()) == false ) {
		DC_Exit(1);
	}

	initialize_uids();

#if defined (HAVE_EXT_LIBVIRT) && !defined(VMWARE_ONLY)
	if( (strcasecmp(vmtype.c_str(), CONDOR_VM_UNIVERSE_XEN) == 0) || (strcasecmp(vmtype.c_str(), CONDOR_VM_UNIVERSE_KVM) == 0)) {
		// Xen requires root priviledge
		if( !canSwitchUid() ) {
			vmprintf(D_ALWAYS, "VMGahp server for Xen or KVM requires "
					"root privilege\n");
			DC_Exit(1);
		}
	}
#endif

	// create VMGahpConfig and fill it with vmgahp config parameters
	VMGahpConfig *gahpconfig = new VMGahpConfig;
	ASSERT(gahpconfig);
	set_root_priv();
	if( gahpconfig->init(vmtype.c_str()) == false ) {
		DC_Exit(1);
	}

	set_condor_priv();

	// Check if vm specific paramaters are valid in config file
#if defined (HAVE_EXT_LIBVIRT) && !defined(VMWARE_ONLY)
	// The calls to checkXenParams() were moved here because each
	// call is specific to the subclass type that is calling it.
	// These methods are static, so dynamic dispatch cannot be
	// used, and the testXen method belongs in the superclass.
	// Therefore, there was only one place where this could have
	// gone...
	if( (strcasecmp(vmtype.c_str(), CONDOR_VM_UNIVERSE_XEN) == 0)) {
		priv_state priv = set_root_priv();
		if( XenType::checkXenParams(gahpconfig) == false ) {
			DC_Exit(1);
		}
		set_priv(priv);
	}else if ((strcasecmp(vmtype.c_str(), CONDOR_VM_UNIVERSE_KVM) == 0)) {
                priv_state priv = set_root_priv();
		if( KVMType::checkXenParams(gahpconfig) == false ) {
			DC_Exit(1);
		}
		set_priv(priv);
	} else
#endif
	if( strcasecmp(vmtype.c_str(), CONDOR_VM_UNIVERSE_VMWARE) == 0 ) {
		priv_state priv = set_user_priv();
		if( VMwareType::checkVMwareParams(gahpconfig) == false ) {
			DC_Exit(1);
		}
		set_priv(priv);
	}

	if( vmgahp_mode == VMGAHP_TEST_MODE ) {
		// Try to test
#if defined (HAVE_EXT_LIBVIRT) && !defined(VMWARE_ONLY)
		if( (strcasecmp(vmtype.c_str(), CONDOR_VM_UNIVERSE_XEN) == 0)) {
			priv_state priv = set_root_priv();

			if( XenType::checkXenParams(gahpconfig) == false ) {
				vmprintf(D_ALWAYS, "\nERROR: the vm_type('%s') cannot "
						"be used.\n", vmtype.c_str());
				DC_Exit(0);
			}
			set_priv(priv);
		} else if ( (strcasecmp(vmtype.c_str(), CONDOR_VM_UNIVERSE_KVM) == 0)) {
			priv_state priv = set_root_priv();

			if( KVMType::checkXenParams(gahpconfig) == false ) {
				vmprintf(D_ALWAYS, "\nERROR: the vm_type('%s') cannot "
						"be used.\n", vmtype.c_str());
				DC_Exit(0);
			}
			set_priv(priv);

		} else
#endif
		if( strcasecmp(vmtype.c_str(), CONDOR_VM_UNIVERSE_VMWARE) == 0 ) {
			priv_state priv = set_user_priv();
			if( VMwareType::testVMware(gahpconfig) == false ) {
				vmprintf(D_ALWAYS, "\nERROR: the vm_type('%s') cannot "
						"be used.\n", vmtype.c_str());
				DC_Exit(0);
			}
			set_priv(priv);
		}

		// print information to stdout
		write_to_daemoncore_pipe("VM_GAHP_VERSION = \"%s\"\n", CONDOR_VMGAHP_VERSION);
		write_to_daemoncore_pipe("%s = \"%s\"\n", ATTR_VM_TYPE,
				gahpconfig->m_vm_type.c_str());
		write_to_daemoncore_pipe("%s = %d\n", ATTR_VM_MEMORY,
				gahpconfig->m_vm_max_memory);
		write_to_daemoncore_pipe("%s = %s\n", ATTR_VM_NETWORKING,
				gahpconfig->m_vm_networking? "TRUE":"FALSE");
		if( gahpconfig->m_vm_networking ) {
			char *tmp = gahpconfig->m_vm_networking_types.print_to_string();
			write_to_daemoncore_pipe("%s = \"%s\"\n", ATTR_VM_NETWORKING_TYPES,
									 tmp ? tmp : "");
			free(tmp);
		}
		if( gahpconfig->m_vm_hardware_vt ) {
			write_to_daemoncore_pipe("%s = TRUE\n", ATTR_VM_HARDWARE_VT);
		}
		DC_Exit(0);
	}

	if( vmgahp_mode == VMGAHP_KILL_MODE ) {
		if( matchstring.empty() ) {
			DC_Exit(0);
		}

		// With root privilege,
		// we will try to kill the VM that matches with given "matchstring".
		set_root_priv();

#if defined (HAVE_EXT_LIBVIRT) && !defined(VMWARE_ONLY)
		if( strcasecmp(vmtype.c_str(), CONDOR_VM_UNIVERSE_XEN) == 0 ) {
			XenType::killVMFast(matchstring.c_str());
		}else
		if( strcasecmp(vmtype.c_str(), CONDOR_VM_UNIVERSE_KVM) == 0 ) {
			KVMType::killVMFast(matchstring.c_str());
		}else
#endif
		if( strcasecmp(vmtype.c_str(), CONDOR_VM_UNIVERSE_VMWARE ) == 0 ) {
			VMwareType::killVMFast(gahpconfig->m_prog_for_script.c_str(),
					gahpconfig->m_vm_script.c_str(), matchstring.c_str(), true);
		}
		DC_Exit(0);
	}

	vmgahp = new VMGahp(gahpconfig, workingdir.c_str());
	ASSERT(vmgahp);

	/* Wait for command */
	vmgahp->startUp();

	write_to_daemoncore_pipe("%s\n", vmgahp_version);
}

int
main( int argc, char **argv )
{
	set_mySubSystem( "VM_GAHP", SUBSYSTEM_TYPE_GAHP );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	dc_main_pre_command_sock_init = main_pre_command_sock_init;
	return dc_main( argc, argv );
}
