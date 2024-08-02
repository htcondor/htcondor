/***************************************************************
 *
 * Copyright (C) 1990-2022, Condor Team, Computer Sciences Department,
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
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "condor_io.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "basename.h"

#include "starter.h"
#include "java_detect.h"

#include "jic_shadow.h"
#include "jic_local_config.h"
#include "jic_local_file.h"
#include "jic_local_schedd.h"
#include "vm_proc.h"
#include "docker_proc.h"
#include "condor_getcwd.h"
#include "singularity.h"


extern "C" int exception_cleanup(int,int,const char*);	/* Our function called by EXCEPT */
JobInfoCommunicator* parseArgs( int argc, char* argv [] );

static Starter StarterObj;
Starter *Starter = &StarterObj;

extern int Foreground;	// from daemoncore
static bool is_gridshell = false;

// this appears at the bottom of this file:
int display_dprintf_header(char **buf,int *bufpos,int *buflen);
static char* dprintf_header = NULL;

int my_argc;
char** my_argv;

static int starter_stdin_fd = -1;
static int starter_stdout_fd = -1;
static int starter_stderr_fd = -1;

[[noreturn]]
static void PREFAST_NORETURN
usage()
{
	dprintf(D_ALWAYS, "argc = %d\n", my_argc);
	for( int i=0; i < my_argc; i++ ) {
		dprintf( D_ALWAYS, "argv[%d] = %s\n", i, my_argv[i] );
	}
	dprintf(D_ALWAYS, "usage: condor_starter initiating_host\n");
	dprintf(D_ALWAYS, "   or: condor_starter -job-keyword keyword\n");
	dprintf(D_ALWAYS, "                      -job-input-ad path\n");
	dprintf(D_ALWAYS, "                      -job-cluster number\n");
	dprintf(D_ALWAYS, "                      -job-proc    number\n");
	dprintf(D_ALWAYS, "                      -job-subproc number\n");
	DC_Exit(1);
}

/* DaemonCore interface implementation */


void
printClassAd( void )
{
	printf( "%s = \"%s\"\n", ATTR_VERSION, CondorVersion() );
	printf( "%s = True\n", ATTR_IS_DAEMON_CORE );
	printf( "%s = True\n", ATTR_HAS_FILE_TRANSFER );
	if(param_boolean("ENABLE_URL_TRANSFERS", true)) {
		printf( "%s = True\n", ATTR_HAS_JOB_TRANSFER_PLUGINS );	 // job supplied transfer plugins
	}
	printf( "%s = True\n", ATTR_HAS_PER_FILE_ENCRYPTION );
	printf( "%s = True\n", ATTR_HAS_RECONNECT );
	printf( "%s = True\n", ATTR_HAS_MPI );
	printf( "%s = True\n", ATTR_HAS_TDP );
	printf( "%s = True\n", ATTR_HAS_JOB_DEFERRAL );
    printf( "%s = True\n", ATTR_HAS_TRANSFER_INPUT_REMAPS );
    printf( "%s = True\n", ATTR_HAS_SELF_CHECKPOINT_TRANSFERS );

		/*
		  Attributes describing what kinds of Job Info Communicators
		  this starter has.  This is mostly for COD, but someday might
		  be useful to other people, too.  There's no need to
		  advertise the fact we've got a JICShadow, since all starters
		  always have and will be able to communicate with a shadow...
		*/

	printf( "%s = True\n", ATTR_HAS_JIC_LOCAL_CONFIG );
	printf( "%s = True\n", ATTR_HAS_JIC_LOCAL_STDIN );

	ClassAd *ad = java_detect();
	if(ad) {
		int gotone=0;
		char *str = 0;

		if(ad->LookupString(ATTR_JAVA_VENDOR,&str)) {
			printf("%s = \"%s\"\n",ATTR_JAVA_VENDOR,str);
			free(str);
			str = 0;
			gotone++;
		}
		if(ad->LookupString(ATTR_JAVA_VERSION,&str)) {
			printf("%s = \"%s\"\n",ATTR_JAVA_VERSION,str);
			free(str);
			str = 0;
			gotone++;
		}
		if(ad->LookupString(ATTR_JAVA_SPECIFICATION_VERSION,&str)) {
			printf("%s = \"%s\"\n",ATTR_JAVA_SPECIFICATION_VERSION,str);
			free(str);
			str = 0;
			gotone++;
		}
		if(gotone>0) printf( "%s = True\n",ATTR_HAS_JAVA);		
		delete ad;
	}

	// VM universe stuff
	if( VMProc::vm_univ_detect() ) {
		// This doesn't mean that vm universe is really available.
		// This just means that starter has codes for vm universe.
		// Actual testing for vm universe will be
		//  done by vmuniverse manager in startd.
		// ATTR_HAS_VM may be overwritten by vmuniverse manager in startd
		printf( "%s = True\n",ATTR_HAS_VM );
	}

	// Docker "universe."
	if( DockerProc::Detect() ) {
		printf( "%s = True\n", ATTR_HAS_DOCKER );
		printf( "%s = True\n", ATTR_HAS_DOCKER_URL);
		printf( "%s = True\n", ATTR_HAS_CONTAINER);

		std::string dockerVersion;
		if( DockerProc::Version( dockerVersion ) ) {
			printf( "%s = \"%s\"\n", ATTR_DOCKER_VERSION, dockerVersion.c_str() );
			if (dockerVersion.find("20.10.4,") != std::string::npos) {
				dprintf(D_ALWAYS, "Docker Version 20.10.4 detected.  This version cannot work with HTCondor.  Please upgrade docker to get Docker universe support\n");
				printf( "%s = False\n", ATTR_HAS_DOCKER );
			}
		}
	}

	// Singularity support
	if (htcondor::Singularity::enabled()) {

		bool can_run_sandbox = false;
		bool can_use_pidnamespaces = true;
		if (htcondor::Singularity::canRunSandbox(can_use_pidnamespaces))  {
			can_run_sandbox = true;
		}
		bool can_run_sif = false;
		if (htcondor::Singularity::canRunSIF())  {
			can_run_sif = true;
		}

		// To consider Singularity operational, we needed it to pass
		// running something... either sandbox or sif...
		if (can_run_sandbox || can_run_sif) {
			printf("%s = True\n", ATTR_HAS_SINGULARITY);
			printf("%s = True\n", ATTR_HAS_CONTAINER);
			// We can't test a download, so assume we can...
			printf("%s = True\n", ATTR_HAS_DOCKER_URL);
			if (can_run_sandbox) {
				printf("%s = True\n", ATTR_HAS_SANDBOX_IMAGE);
			}
			if (can_run_sif) {
				printf("%s = True\n", ATTR_HAS_SIF);
			}
			// So canRunSandbox() determined if this Singularity install can use pid namespaces.
			// Use the result to set ATTR_HAS_SINGULARITY_PIDNAMESPACES explicitly to True or False for
			// insertion into the slot ad, as subsequent invocations of the condor_starter
			// will lookup this slot attribute to determine if pid namesapces are available.
			printf("%s = %s\n", ATTR_HAS_SINGULARITY_PIDNAMESPACES, can_use_pidnamespaces ? "True" : "False");
		}
		else {
			// If we made it here, we cannot run either sif or sandbox images.
			// In this case, return HasSingularity=False, which means it is
			// present on the EP but broken.
			// In this case, add in SingularityOfflineReason attr to help explain why,
			// just like we do for Docker.
			printf("%s = False\n", ATTR_HAS_SINGULARITY);
			std::string offline_reason = "Both SIF and Sandbox tests on startup failed";
			if (!htcondor::Singularity::m_lastSingularityErrorLine.empty()) {
				offline_reason += " : " + htcondor::Singularity::m_lastSingularityErrorLine;
			}
			printf("SingularityOfflineReason = \"%s\"\n",
				EscapeChars(offline_reason.c_str(), "\"", '\\').c_str() // escape quotes in classad str
			);
		}
		printf("%s = \"%s\"\n", ATTR_SINGULARITY_VERSION, htcondor::Singularity::version());
	}


	// Detect ability to encrypt execute directory
	if (param_boolean("DISABLE_EXECUTE_DIRECTORY_ENCRYPTION", false)) {
	#ifdef LINUX
		// TODO: Replace with VolumeManager::DetectVolumeGroup()
		if (param_boolean("STARTD_ENFORCE_DISK_LIMITS", false) || FilesystemRemap::EncryptedMappingDetect() ) {
			printf( "%s = True\n", ATTR_HAS_ENCRYPT_EXECUTE_DIRECTORY );
		}
	#endif
	#ifdef WIN32
		printf( "%s = True\n", ATTR_HAS_ENCRYPT_EXECUTE_DIRECTORY );
	#endif
	}

	// Advertise which file transfer plugins are supported
	FileTransfer ft;
	CondorError e;
	std::string method_list = ft.GetSupportedMethods(e);
	if (!method_list.empty()) {
		printf("%s = \"%s\"\n", ATTR_HAS_FILE_TRANSFER_PLUGIN_METHODS, method_list.c_str());
	}

	// even if we have no transfer plugin methods, we may want to
	// advertise plugin extra attributes
	if ( ! ft.getPluginQueryAds().empty()) {
		// publish plugin extra attributes, GetSupportedMethods will collect plugin ads
		// when it queries the plugins for their '-classad' 
		// We will incorporate into the starter classad any attributes from the plugins
		// that are marked as SlotAttrs or StartdAttrs
		ClassAd extraAd;
		classad::References failed_methods;
		classad::References startd_attrs;
		// when SlotAttrs are enabled, a plugin choose whether to put an attribute
		// into the daemon ad, or into the slot ads. otherwise all attributes go into all ads
	   #ifdef ENABLE_PLUGIN_SLOT_ATTRS
		classad::References slot_attrs;
	   #endif
		std::string listval;

		for (auto & ad : ft.getPluginQueryAds()) {
			classad::References has_attrs;
			if (ad.LookupString("FailedMethods", listval)) {
				for (auto & meth : StringTokenIterator(listval)) { failed_methods.insert(meth); }
			}
			if (ad.LookupString("StartdAttrs", listval)) {
				for (auto & attr : StringTokenIterator(listval)) {
					classad::Value val;
					if (ad.EvaluateAttr(attr, val)) { // this returns only SAFE values, so no lists or classad
						extraAd.Insert(attr, classad::Literal::MakeLiteral(val));
						startd_attrs.emplace(attr);
						has_attrs.emplace(attr);
					}
				}
			}
		   #ifdef ENABLE_PLUGIN_SLOT_ATTRS
			if (ad.LookupString("SlotAttrs", listval)) {
				for (auto & attr : StringTokenIterator(listval.c_str())) {
					if ( ! has_attrs.count(attr)) {
						classad::Value val;
						if ( ! ad.EvaluateAttr(attr, val)) { // this returns only SAFE values, so no lists or classad
							continue;
						} 
						extraAd.Insert(attr, classad::Literal::MakeLiteral(val));
					}
					slot_attrs.emplace(attr);
				}
			}
		   #endif
		}
	   #ifdef ENABLE_PLUGIN_SLOT_ATTRS
		if ( ! slot_attrs.empty()) {
			listval = JoinAttrNames(slot_attrs, " ");
			extraAd.Assign("SlotAttrs", listval);
		}
		if ( ! startd_attrs.empty()) {
			listval = JoinAttrNames(startd_attrs, " ");
			extraAd.Assign("StartdAttrs", listval);
		}
	   #endif

		// print the offline transfer plugin methods
		if ( ! failed_methods.empty()) {
			listval = JoinAttrNames(failed_methods, ",");
			printf("OfflineFileTransferPluginMethods = \"%s\"\n", listval.c_str());
		}

		// print the extra attributes as part of the starter ad
		if (extraAd.size() > 0) {
			listval.clear();
			formatAd(listval, extraAd, nullptr, nullptr, false);
			chomp(listval);
			if ( ! listval.empty()) {
				puts(listval.c_str());
			}
		}
	}
	if (e.code()) {
		dprintf(D_ALWAYS, "WARNING: Initializing plugins returned: %s\n", e.getFullText().c_str());
	}


    // Advertise the proxy/cache used by each file transfer plug-in.
    auto proxy_by_method = ft.GetProxyByMethodMap();
    for( auto & [method, proxy] : proxy_by_method ) {
        printf( "%s_proxy = \"%s\"\n", method.c_str(), proxy.c_str() );
    }


#if defined(WIN32)
		// Advertise our ability to run jobs as the submitting user
	printf("%s = True\n", ATTR_HAS_WIN_RUN_AS_OWNER);
#endif

#ifndef WIN32
	// Many site intentionally remove /usr/sbin/ssh, which will
	// break condor_ssh_to_job.  Let's advertise if it exists
	// as if it doesn't, there's no hope of running condor_ssh_to_job

	std::string sshd;
	bool hasSshd = false;

	param(sshd,"SSH_TO_JOB_SSHD", "/usr/sbin/sshd");
	if (!sshd.empty()) {
		int rc = access(sshd.c_str(), X_OK);
		if (rc == 0) {
			hasSshd = true;
		}

	}
	printf("HasSshd = %s\n", hasSshd ? "true" : "false");
#endif
}


static char* orig_cwd = NULL;

void
main_pre_dc_init( int argc, char* argv[] )
{	
		// figure out what get_mySubSystem() should be based on argv[0], or
		// if we see "-gridshell" anywhere on the command-line
	const char* base = condor_basename(argv[0]);
	char const *tmp;
	tmp = strrchr(base, '_' );
	if( tmp && strncasecmp(tmp, "_gridshell", 10) == MATCH ) {
		get_mySubSystem()->setName( "GRIDSHELL" );
		is_gridshell = true;
	} else { 
		int i, len;
		for( i=1; i<argc; i++ ) {
			len = strlen(argv[i]);
			if( len < 3 ) {
					// ambiguous, bail out, since we don't want to get
					// confused with just "-" or something
				continue;
			}
			if( strncasecmp(argv[i], "-gridshell", MIN(len,10)) == MATCH ) {
				get_mySubSystem()->setName( "GRIDSHELL" );
				is_gridshell = true;
				break;
			}
		}
	}
	if( ! is_gridshell ) {
		get_mySubSystem()->setName( "STARTER" );
	}

		// if we were passed "-classad", just print our classad and
		// exit, without going back to daemoncore or anything.  we
		// need to do this *after* we set get_mySubSystem(), since this ends
		// up calling functions that rely on it being defined...  
	if( argc == 2 && strncasecmp(argv[1],"-cla",4) == MATCH ) {
			// needed for Java stuff
		config_ex(CONFIG_OPT_WANT_QUIET);

			// Would like to log errors to stderr if stderr is not
			// /dev/null to make it easier for users to debug, but not
			// sure how to do that on windows.  On Windows, when
			// condor_starter is run by the startd, setting Termlog=1
			// causes a dprintf to abort with an error if any calls to
			// dprintf() are made in a debug level that is turned on.
			// I have not found a way to detect when stderr is in this
			// state, so I am just leaving Termlog turned off in all
			// cases.

		// (Re)name the log file StarterLog.testing to make it clear
		// to admins that this is not the StarterLog they are looking for.
		std::string baseLog;
		param(baseLog, "STARTER_LOG", "");
		if (!baseLog.empty()) {
			std::string newLog = baseLog + ".testing";
			config_insert("STARTER_LOG", newLog.c_str());
		}
		dprintf_config(get_mySubSystem()->getName());

		printClassAd();
		exit(0);
	}

		// if we're still here, stash the cwd for future reference
	std::string cwd;
	if( ! condor_getcwd(cwd)) {
		dprintf( D_ALWAYS, "ERROR calling getcwd(): %s (errno %d)\n", 
				 strerror(errno), errno );
	} else {
		orig_cwd = strdup(cwd.c_str());
	}

		// if we're the gridshell, assume a "-f" option.  all that
		// does in DaemonCore-land is set a global variable, so we'll
		// just do that here, ourselves...
	if( is_gridshell ) {
		Foreground = 1;
	}

		// finally, dup() our standard file streams, so we can pass
		// those onto the actual user job if requested.
	starter_stdin_fd = dup( 0 );
	starter_stdout_fd = dup( 1 );
	starter_stderr_fd = dup( 2 );
}


void
main_init(int argc, char *argv[])
{
	my_argc = argc;
	my_argv = argv;

#ifdef WIN32
	// On NT, we need to make certain we have a console.
	// This is because we send soft kill via CTRL_C or CTRL_BREAK,
	// and this is done via the console, so we must have one.
	// ... and we do have one, cuz Daemon Core created one for us
	// at startup time.
	// Now we need to do a null call to SetConsoleCtrlHandler.
	// For some reason, sending CTRL_C's does not work until we
	// do this (seems like an NT bug).
	SetConsoleCtrlHandler(NULL, FALSE);
#endif

	// register a cleanup routine to kill our kids in case we EXCEPT
	_EXCEPT_Cleanup = exception_cleanup;

	JobInfoCommunicator* jic = NULL;

		// now, based on the command line args, figure out what kind
		// of JIC we need...
	if( argc < 2 ) {
		usage();
	}

	jic = parseArgs( argc, argv );

	if( ! jic ) {
			// we couldn't figure out what to do...
		usage();
	}

	if( !Starter->Init(jic, orig_cwd, is_gridshell, starter_stdin_fd,
					   starter_stdout_fd, starter_stderr_fd) ) {
		dprintf(D_ALWAYS, "Unable to start job.\n");
		DC_Exit(1);
	}
}


void
invalid( char* opt )
{
	dprintf( D_ALWAYS, "Command-line option '%s' is invalid\n", opt ); 
	usage();
}


void
ambiguous( char* opt )
{
	dprintf( D_ALWAYS, "Command-line option '%s' is ambiguous\n", opt ); 
	usage();
}


void PREFAST_NORETURN
another( char* opt )
{
	dprintf( D_ALWAYS, 
			 "Command-line option '%s' requires another argument\n", opt ); 
	usage();
}


JobInfoCommunicator*
parseArgs( int argc, char* argv [] )
{
	JobInfoCommunicator* jic = NULL;
	char* job_input_ad = NULL; 
	char* job_output_ad = NULL;
	char* job_update_ad = NULL;
	char* job_keyword = NULL; 
	int job_cluster = -1;
	int job_proc = -1;
	int job_subproc = -1;
	char* shadow_host = NULL;
	char* job_stdin = NULL;
	char* job_stdout = NULL;
	char* job_stderr = NULL;
	char* schedd_addr = NULL;

	bool warn_multi_keyword = false;
	bool warn_multi_input_ad = false;
	bool warn_multi_output_ad = false;
	bool warn_multi_update_ad = false;
	bool warn_multi_cluster = false;
	bool warn_multi_proc = false;
	bool warn_multi_subproc = false;
	bool warn_multi_stdin = false;
	bool warn_multi_stdout = false;
	bool warn_multi_stderr = false;

	char *opt, *arg;
	int opt_len;

	char _jobinputad[] = "-job-input-ad";
	char _joboutputad[] = "-job-output-ad";
	char _jobupdatead[] = "-job-update-ad";
	char _jobkeyword[] = "-job-keyword";
	char _jobcluster[] = "-job-cluster";
	char _jobproc[] = "-job-proc";
	char _jobsubproc[] = "-job-subproc";
	char _jobstdin[] = "-job-stdin";
	char _jobstdout[] = "-job-stdout";
	char _jobstderr[] = "-job-stderr";
	char _header[] = "-header";
	char _gridshell[] = "-gridshell";
	char _schedd_addr[] = "-schedd-addr";
	char _slot_name[] = "-slot-name";
	char* target = NULL;

	ASSERT( argc >= 2 );
	
	char** tmp = argv;
	for( tmp++; *tmp; tmp++ ) {
		target = NULL;
		opt = tmp[0];
		arg = tmp[1];
		opt_len = strlen( opt );

		if( opt[0] != '-' ) {
				// this must be a hostname...
			free( shadow_host );
			shadow_host = strdup( opt );
			continue;
		}

		if( ! strncmp(opt, _header, opt_len) ) { 
			if( ! arg ) {
				another( _header );
			}
			if (dprintf_header) {
				free(dprintf_header);
			}
			dprintf_header = strdup( arg );
			DebugId = display_dprintf_header;
			tmp++;	// consume the arg so we don't get confused 
			continue;
		}

		if( ! strncmp(opt, _gridshell, opt_len) ) { 
				// just skip this one, we already processed this in
				// main_pre_dc_init()  
			ASSERT( is_gridshell );
			continue;
		}

		if( ! strncmp(opt, _schedd_addr, opt_len) ) { 
			if( ! arg ) {
				another( _schedd_addr );
			}
			free( schedd_addr );
			schedd_addr = strdup( arg );
			tmp++;	// consume the arg so we don't get confused 
			continue;
		}

		if (opt_len >=4 && ! strncmp(opt, _slot_name, opt_len)) {
			if( ! arg ) {
				another( _slot_name );
			}
			config_insert( "STARTER_SLOT_NAME", arg );
			dprintf( D_ALWAYS, "STARTER_SLOT_NAME is %s\n", arg);
			tmp++; // consume the arg
			continue;
		}

		if( strncmp( "-job-", opt, MIN(opt_len,5)) ) {
			invalid( opt );
		}
		if( opt_len < 6 ) {
			ambiguous( opt );
		}
		switch( opt[5] ) {

		case 'c':
			if( strncmp(_jobcluster, opt, opt_len) ) {
				invalid( opt );
			} 
			target = _jobcluster;
			break;

		case 'k':
			if( strncmp(_jobkeyword, opt, opt_len) ) {
				invalid( opt );
			} 
			target = _jobkeyword;
			break;

		case 'i':
			if( strncmp(_jobinputad, opt, opt_len) ) {
				invalid( opt );
			} 
			target = _jobinputad;
			break;

		case 'o':
			if( strncmp(_joboutputad, opt, opt_len) ) {
				invalid( opt );
			} 
			target = _joboutputad;
			break;

		case 'u':
			if( strncmp(_jobupdatead, opt, opt_len) ) {
				invalid( opt );
			}
			target = _jobupdatead;
			break;

		case 'p':
			if( strncmp(_jobproc, opt, opt_len) ) {
				invalid( opt );
			} 
			target = _jobproc;
			break;

		case 's':
			if( !strncmp(_jobsubproc, opt, opt_len) ) {
				target = _jobsubproc;
				break;
			} else if( !strncmp(_jobstdin, opt, opt_len) ) {
				target = _jobstdin;
				break;
			} else if( !strncmp(_jobstdout, opt, opt_len) ) {
				target = _jobstdout;
				break;
			} else if( !strncmp(_jobstderr, opt, opt_len) ) {
				target = _jobstderr;
				break;
			}

			invalid( opt );
			break;

		default:
			invalid( opt );
			break;

		}
			// now, make sure we got the arg
		if( ! arg ) {
			another( target );
		} else {
				// consume it for the purposes of the for() loop
			tmp++;
		}
		if( target == _jobkeyword ) {
				// we can check like that, since we're setting target to
				// point to it, so we don't have to do a strcmp().
			if( job_keyword ) {
				warn_multi_keyword = true;
				free( job_keyword );
			}
			job_keyword = strdup( arg );
		} else if( target == _jobinputad ) {
			if( job_input_ad ) {
				warn_multi_input_ad = true;
				free( job_input_ad );
			}
			job_input_ad = strdup( arg );
		} else if( target == _joboutputad ) {
			if( job_output_ad ) {
				warn_multi_output_ad = true;
				free( job_output_ad );
			}
			job_output_ad = strdup( arg );
		} else if( target == _jobupdatead ) {
			if( job_update_ad ) {
				warn_multi_update_ad = true;
				free( job_update_ad );
			}
			job_update_ad = strdup( arg );
		} else if( target == _jobstdin ) {
			if( job_stdin ) {
				warn_multi_stdin = true;
				free( job_stdin );
			}
			job_stdin = strdup( arg );
		} else if( target == _jobstdout ) {
			if( job_stdout ) {
				warn_multi_stdout = true;
				free( job_stdout );
			}
			job_stdout = strdup( arg );
		} else if( target == _jobstderr ) {
			if( job_stderr ) {
				warn_multi_stderr = true;
				free( job_stderr );
			}
			job_stderr = strdup( arg );
		} else if( target == _jobcluster ) {
			if( job_cluster >= 0 ) {
				warn_multi_cluster = true;
			}
			job_cluster = atoi( arg );
			if( job_cluster < 0 ) {
				dprintf( D_ALWAYS, 
						 "ERROR: Invalid value for '%s': \"%s\"\n",
						 _jobcluster, arg );
				usage();
			}
		} else if( target == _jobproc ) {
			if( job_proc >= 0 ) {
				warn_multi_proc = true;
			}
			job_proc = atoi( arg );
			if( job_proc < 0 ) {
				dprintf( D_ALWAYS, 
						 "ERROR: Invalid value for '%s': \"%s\"\n",
						 _jobproc, arg );
				usage();
			}
		} else if( target == _jobsubproc ) {
			if( job_subproc >= 0 ) {
				warn_multi_subproc = true;
			}
			job_subproc = atoi( arg );
			if( job_subproc < 0 ) {
				dprintf( D_ALWAYS, 
						 "ERROR: Invalid value for '%s': \"%s\"\n",
						 _jobsubproc, arg );
				usage();
			}
		} else {
				// Should never get here, since we'll hit usage above
				// if we don't know what target option we're doing...
			EXCEPT( "Programmer error in parsing arguments" );
		}
	}

	if( job_stdin && job_stdin[0] == '-' && ! job_stdin[1] &&
		job_input_ad && job_input_ad[0] == '-' && ! job_input_ad[1] ) { 
		dprintf( D_ALWAYS, "ERROR: Cannot use starter's stdin for both "
				 "the job stdin (%s) and to define the job ClassAd (%s). "
				 "Please do not use '-' for one of these two flags and "
				 "try again.\n", _jobstdin, _jobinputad );
		usage();
	}

	if( job_output_ad && job_output_ad[0] == '-' && ! job_output_ad[1] &&
		job_stdout && job_stdout[0] == '-' && ! job_stdout[1] ) {
		dprintf( D_ALWAYS, "ERROR: Cannot use starter's stdout for both the "
				 "job stdout (%s) and to write the job's output ClassAd "
				 "(%s). Please do not use '-' for one of these two flags "
				 "and try again.\n", _jobstdout, _joboutputad );
		usage();
	}

	if( warn_multi_keyword ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%s\"\n",
				 _jobkeyword, job_keyword );
	}
	if( warn_multi_input_ad ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%s\"\n",
				 _jobinputad, job_input_ad );
	}
	if( warn_multi_output_ad ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%s\"\n",
				 _joboutputad, job_output_ad );
	}
	if( warn_multi_update_ad ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%s\"\n",
				 _jobupdatead, job_update_ad );
	}
	if( warn_multi_stdin ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%s\"\n",
				 _jobstdin, job_stdin );
	}
	if( warn_multi_stdout ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%s\"\n",
				 _jobstdout, job_stdout );
	}
	if( warn_multi_stderr ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%s\"\n",
				 _jobstderr, job_stderr );
	}
	if( warn_multi_cluster ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%d\"\n",
				 _jobcluster, job_cluster );
	}
	if( warn_multi_proc ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%d\"\n",
				 _jobproc, job_proc );
	}
	if( warn_multi_subproc ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%d\"\n",
				 _jobsubproc, job_subproc );
	}

	if( shadow_host ) {
		if( job_keyword ) {
			dprintf( D_ALWAYS, "You cannot use '%s' and specify a "
					 "shadow host\n", _jobkeyword );
			usage();
		}
		if( job_input_ad ) {
			dprintf( D_ALWAYS, "You cannot use '%s' and specify a "
					 "shadow host\n", _jobinputad );
			usage();
		}
		jic = new JICShadow( shadow_host );
		free( shadow_host );
		shadow_host = NULL;
		free( schedd_addr );
		free( job_output_ad );
		free( job_update_ad );
		free( job_stdin );
		free( job_stdout );
		free( job_stderr );
		if (job_keyword) { free(job_keyword); }
		return jic;
	}

	if( ! (job_keyword || job_input_ad) ) {
		dprintf( D_ALWAYS, "ERROR: You must specify either '%s' or '%s'\n",
				 _jobkeyword, _jobinputad ); 
		usage();
	}

		// If the user didn't specify it, use -1 for cluster and/or
		// proc, and the JIC subclasses will know they weren't on the
		// command-line.
	if( schedd_addr ) {
		if( ! job_input_ad ) {
			dprintf( D_ALWAYS, "ERROR: You must specify '%s' with '%s'\n",
					 _jobinputad, _schedd_addr ); 
			usage();
		}
		jic = new JICLocalSchedd( job_input_ad, schedd_addr,
								  job_cluster, job_proc, job_subproc );
	} else if( job_input_ad ) {
		if( job_keyword ) {
			jic = new JICLocalFile( job_input_ad, job_keyword, 
									job_cluster, job_proc, job_subproc );
		} else {
			jic = new JICLocalFile( job_input_ad, job_cluster, job_proc,
									job_subproc );
		}
	} else {
		ASSERT( job_keyword );
		jic = new JICLocalConfig( job_keyword, job_cluster, job_proc, 
								  job_subproc );
	}
	if( job_keyword ) {
		free( job_keyword );
	}
	if( job_input_ad ) {
		free( job_input_ad );
	}
	if( job_output_ad ) {
        jic->setOutputAdFile( job_output_ad );		
		free( job_output_ad );
	}
	if( job_update_ad ) {
		jic->setUpdateAdFile( job_update_ad );
		free( job_update_ad );
	}
	if( job_stdin ) {
        jic->setStdin( job_stdin );		
		free( job_stdin );
	}
	if( job_stdout ) {
        jic->setStdout( job_stdout );		
		free( job_stdout );
	}
	if( job_stderr ) {
        jic->setStderr( job_stderr );		
		free( job_stderr );
	}
	if( schedd_addr ) {
		free( schedd_addr );
	}
	return jic;
}


void
main_config()
{
	Starter->Config();
}


void
main_shutdown_fast()
{
	if ( Starter->RemoteShutdownFast(0) ) {
		// ShutdownFast says it is already finished, because there are
		// no jobs to shutdown.  No need to stick around.
		Starter->StarterExit(Starter->GetShutdownExitCode());
	}
}


void
main_shutdown_graceful()
{
	if ( Starter->RemoteShutdownGraceful(0) ) {
		// ShutdownGraceful says it is already finished, because
		// there are no jobs to shutdown.  No need to stick around.
		Starter->StarterExit(Starter->GetShutdownExitCode());
	}
}

extern "C" 
int exception_cleanup(int,int,const char*errmsg)
{
	_EXCEPT_Cleanup = NULL;
	Starter->jic->notifyStarterError(errmsg,true,0,0);
	Starter->RemoteShutdownFast(0);
	Starter->FinalCleanup();
	return 0;
}

int
main( int argc, char **argv )
{
	set_mySubSystem( NULL, true, SUBSYSTEM_TYPE_STARTER );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	dc_main_pre_dc_init = main_pre_dc_init;
	return dc_main( argc, argv );
}


int
display_dprintf_header(char **buf,int *bufpos,int *buflen)
{
	if( dprintf_header ) {
		return sprintf_realloc( buf, bufpos, buflen, "%s ", dprintf_header );
	}
	return 0;
}
