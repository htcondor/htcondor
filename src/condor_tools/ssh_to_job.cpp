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
#include "condor_config.h"
#include "condor_distribution.h"
#include "dc_schedd.h"
#include "dc_starter.h"
#include "MyString.h"
#include "sig_install.h"
#include "match_prefix.h"
#include "condor_claimid_parser.h"
#include "condor_attributes.h"
#include "authentication.h"
#include "condor_arglist.h"
#include "directory.h"
#include "basename.h"
#include "socket_proxy.h"


class SSHToJob {
public:
	SSHToJob();
	~SSHToJob();

	bool parseArgs(int argc,char **argv);

		// do whatever is appropriate for the args that were parsed
	bool execute();

		// try to create an ssh session
		// if the attempt fails and it makes sense to keep trying
		// and the user requested auto retries, then keep looping
	bool execute_ssh_retry();

		// make one attempt to create an ssh session
	bool execute_ssh();

		// act as a proxy between stdin/stdout and a socket
	bool execute_proxy();

		// get the exit status of ssh (encoded as for waitpid())
	int getSSHExitStatus();

private:
	PROC_ID m_jobid;
	int m_subproc;
	MyString m_schedd_name;
	MyString m_pool_name;
	MyString m_ssh_options;
	int m_proxy_fd;
	MyString m_program_name;
	int m_ssh_exit_status;
	ArgList m_ssh_args_from_command_line;
	MyString m_session_dir;
	bool m_debug;
	MyString m_shells; // comma-separated list of shells to try
	bool m_retry_sensible;
	bool m_auto_retry;
	int m_retry_delay;
	MyString m_ssh_keygen_args;

	void logError(char const *fmt,...) CHECK_PRINTF_FORMAT(2,3);
	void printUsage();
};

SSHToJob::SSHToJob():
	m_subproc(-1),
	m_ssh_options("ssh"),
	m_proxy_fd(-1),
	m_ssh_exit_status(-1),
	m_debug(false),
	m_retry_sensible(false),
	m_auto_retry(false),
	m_retry_delay(30)
{
}

SSHToJob::~SSHToJob()
{
	if( !m_session_dir.IsEmpty() ) {
		Directory dir(m_session_dir.Value());
		dir.Remove_Full_Path(m_session_dir.Value());
	}
}

int SSHToJob::getSSHExitStatus()
{
	return m_ssh_exit_status;
}

void SSHToJob::logError(char const *fmt,...)
{
	va_list args;
	va_start( args, fmt );
	vfprintf( stderr, fmt, args );
	va_end( args );
}

void SSHToJob::printUsage()
{
	fprintf(stderr, "Usage: %s [OPTIONS] { cluster | cluster.proc } [command]\n",
			m_program_name.Value());
	fprintf(stderr,"\n");
	fprintf(stderr,"OPTIONS:\n");
	fprintf(stderr," -debug\n");
	fprintf(stderr," -name schedd-name\n");
	fprintf(stderr," -pool pool-name\n");
	fprintf(stderr," -auto-retry               (if job not running yet)\n");
	fprintf(stderr," -shells shell1,shell2,... (shells to try)\n");
	fprintf(stderr," -ssh <alt ssh command>    (e.g. sftp or scp)\n");
	fprintf(stderr," -keygen-options <keygen options>\n");
}

bool SSHToJob::parseArgs(int argc,char **argv)
{
	if( argc > 0 ) {
		m_program_name = argv[0];
	}

		// default shell to request
	m_shells = getenv("SHELL");

	int nextarg;
	for( nextarg=1; nextarg<argc; nextarg++ ) {
		bool missing_arg = false;

		if( match_prefix( argv[nextarg], "-debug" ) ) {
				// dprintf to console
			Termlog = 1;
			dprintf_config( "TOOL" );
			DebugFlags |= D_FULLDEBUG;
			m_debug = true;
		} else if( match_prefix( argv[nextarg], "-help" ) ) {
			printUsage();
			return false;
		} else if( match_prefix( argv[nextarg], "-name" ) ) {
			if( argv[nextarg + 1] ) {
				m_schedd_name = argv[++nextarg];
			} else {
				missing_arg=true;
			}
		} else if( match_prefix( argv[nextarg], "-pool" ) ) {
			if( argv[nextarg + 1] ) {
				m_pool_name = argv[++nextarg];
			} else {
				missing_arg=true;
			}
		} else if( match_prefix( argv[nextarg], "-ssh") ) {
			if( argv[nextarg + 1] ) {
				m_ssh_options = argv[++nextarg];
			} else {
				missing_arg=true;
			}
		} else if( match_prefix( argv[nextarg], "-keygen-options") ) {
			if( argv[nextarg + 1] ) {
				m_ssh_keygen_args = argv[++nextarg];
			} else {
				missing_arg=true;
			}
		} else if( match_prefix( argv[nextarg], "-auto-retry") ) {
			m_auto_retry = true;
		} else if( match_prefix( argv[nextarg], "-shells") ) {
			if( argv[nextarg + 1] ) {
				m_shells = argv[++nextarg];
			} else {
				missing_arg=true;
			}
		} else if( match_prefix( argv[nextarg], "-proxy") ) {
			if( argv[nextarg + 1] ) {
				m_proxy_fd = strtol(argv[++nextarg],NULL,10);
			} else {
				missing_arg=true;
			}
		} else if( !strcmp( argv[nextarg], "--" ) ) {
			++nextarg;
			break;
		} else if( match_prefix( argv[nextarg], "-" ) ) {
			logError("Unrecognized argument: %s\n",argv[nextarg]);
			return false;
		} else {
			break; // non-option argument
		}

		if( missing_arg ) {
			logError("%s requires an argument",argv[nextarg]);
			return false;
		}
	}

	if( m_proxy_fd != -1 ) {
		return true;
	}

	if( argc <= nextarg ) {
		logError("ERROR: You must specify a job!\n\n");
		return false;
	}

	char *end=NULL;
	m_jobid.cluster = strtol(argv[nextarg],&end,10);
	if( end && *end == '.' ) {
		m_jobid.proc = strtol(end+1,&end,10);
	}
	else {
		m_jobid.proc = 0;
	}
	if( end && *end == '.' ) {
		m_subproc = strtol(end+1,&end,10);
	}
	else {
		m_subproc = -1;
	}
	if( !end || *end != '\0' ) {
		logError("Invalid job id: %s\n", argv[nextarg]);
		return false;
	}
	nextarg++;

	for(; nextarg < argc; nextarg++) {
		m_ssh_args_from_command_line.AppendArg(argv[nextarg]);
	}

	return true;
}

bool SSHToJob::execute()
{
	if( m_proxy_fd != -1 ) {
		return execute_proxy();
	}
	else {
		return execute_ssh_retry();
	}
}

bool SSHToJob::execute_proxy()
{
		// Our job is to act as a proxy between the ssh client
		// (on stdin/stdout) and sshd (on m_proxy_fd)

	dprintf(D_FULLDEBUG,"Setting up ssh proxy on file descriptor %d\n",
			m_proxy_fd);

	SocketProxy proxy;
	proxy.addSocketPair(m_proxy_fd,1);
	proxy.addSocketPair(0,m_proxy_fd);
	if( !proxy.getErrorMsg() ) {
		proxy.execute();
	}

	if( proxy.getErrorMsg() ) {
		logError("ssh proxy communication failure: %s\n",proxy.getErrorMsg());
		return false;
	}

	return true;
}

bool SSHToJob::execute_ssh_retry()
{
	int attempt = 0;
	for(attempt=0; attempt==0 || m_auto_retry; attempt++) {
		if( attempt > 0 ) {
			fprintf(stderr,"Will try again in %d seconds.\n",m_retry_delay);
			sleep(m_retry_delay);
		}
		if( execute_ssh() ) {
			return true;
		}
		if( !m_retry_sensible ) {
			return false;
		}
	}

	if( !m_auto_retry ) {
		fprintf(stderr,"(Use -auto-retry to keep trying periodically.)\n");
	}
	return false;
}

bool SSHToJob::execute_ssh()
{
	MyString error_msg;

	m_retry_sensible = false;

	DCSchedd schedd(m_schedd_name.IsEmpty() ? NULL : m_schedd_name.Value(),
					m_pool_name.IsEmpty() ? NULL   : m_pool_name.Value());
	if( schedd.locate() == false ) {
		if( m_schedd_name.IsEmpty() ) {
			logError("ERROR: Can't find address of local schedd\n");
			return false;
		}

		if( m_pool_name.IsEmpty() ) {
			logError("No such schedd named %s in local pool\n",
					 m_schedd_name.Value() );
		} else {
			logError("No such schedd named %s in pool %s\n",
					 m_schedd_name.Value(), m_pool_name.Value() );
		}
		return false;
	}


	MyString starter_addr;
	MyString starter_claim_id;
	MyString starter_version;
	MyString slot_name;
	CondorError error_stack;
	int timeout = 300;

		// We want encryption because we will be transferring an ssh key.
		// must be in format expected by SecMan::ImportSecSessionInfo()
	char const *session_info = "[Encryption=\"YES\";Integrity=\"YES\";]";

	bool success = schedd.getJobConnectInfo(m_jobid,m_subproc,session_info,timeout,&error_stack,starter_addr,starter_claim_id,starter_version,slot_name,error_msg,m_retry_sensible);

		// turn the ssh claim id into a security session so we can use it
		// to authenticate ourselves to the starter
	SecMan secman;
	char const *ssh_session_id;
	ClaimIdParser cidp(starter_claim_id.Value());
	if( success ) {
		success = secman.CreateNonNegotiatedSecuritySession(
					DAEMON,
					cidp.secSessionId(),
					cidp.secSessionKey(),
					cidp.secSessionInfo(),
					EXECUTE_SIDE_MATCHSESSION_FQU,
					starter_addr.Value(),
					0 );
		if( !success ) {
			error_msg = "Failed to create security session to connect to starter.";
		}
		else {
			ssh_session_id = cidp.secSessionId();
		}
	}

	if( !success ) {
		logError("%s\n",error_msg.Value());
		return false;
	}

	dprintf(D_FULLDEBUG,"Got connect info for starter %s\n",
			starter_addr.Value());

	ClassAd starter_ad;
	starter_ad.Assign(ATTR_STARTER_IP_ADDR,starter_addr);
	starter_ad.Assign(ATTR_VERSION,starter_version);

	DCStarter starter;
	if( !starter.initFromClassAd(&starter_ad) ) {
		logError("Failed to initialize starter object.\n");
		return false;
	}

	MyString remote_host;
	char const *at_pos = strrchr(slot_name.Value(),'@');
	if( at_pos ) {
		remote_host = at_pos + 1;
	}
	else {
		remote_host = slot_name;
	}

	char const *local_username = my_username();
	if( !local_username ) {
		local_username = "unknown";
	}

	char const *temp_dir = getenv("TMP");
	if( !temp_dir ) {
		temp_dir = "/tmp";
	}

	unsigned int num = 1;
	for(num=1;num<2000;num++) {
		unsigned int r = get_random_uint();
		m_session_dir.sprintf("%s%c%s.condor_ssh_to_job_%x",
							  temp_dir,DIR_DELIM_CHAR,local_username,r);
		if( mkdir(m_session_dir.Value(),0700)==0 ) {
			break;
		}
		m_session_dir = "";
		if( errno == EEXIST ) {
			continue;
		}
		logError("Failed to create ssh session dir %s: %s\n",
				m_session_dir.Value(),strerror(errno));
		return false;
	}
	if( m_session_dir.IsEmpty() ) {
		logError("Failed to create ssh session dir in %u tries.\n",num);
		return false;
	}


	MyString known_hosts_file;
	known_hosts_file.sprintf("%s%cknown_hosts",m_session_dir.Value(),DIR_DELIM_CHAR);
	MyString private_client_key_file;
	private_client_key_file.sprintf("%s%cssh_key",m_session_dir.Value(),DIR_DELIM_CHAR);

	ReliSock sock;
	MyString remote_user; // this will be filled in with the remote user name
	bool start_sshd = starter.startSSHD(
		known_hosts_file.Value(),
		private_client_key_file.Value(),
		m_shells.Value(),
		slot_name.Value(),
		m_ssh_keygen_args.Value(),
		sock,
		timeout,
		ssh_session_id,
		remote_user,
		error_msg,
		m_retry_sensible);

	if( !start_sshd ) {
		logError("%s\n",error_msg.Value());
		return false;
	}


	MyString proxy_command;
	ArgList proxy_arglist;
	proxy_arglist.AppendArg(m_program_name.Value());
	if( m_debug ) {
		proxy_arglist.AppendArg("-debug");
	}
	proxy_arglist.AppendArg("-proxy");
	proxy_arglist.AppendArg(sock.get_file_desc());
	if( !proxy_arglist.GetArgsStringSystem(&proxy_command,0,&error_msg) ) {
		logError("Failed to produce proxy command: %s\n",error_msg.Value());
		return false;
	}

	ArgList ssh_options_arglist;
	if(!ssh_options_arglist.AppendArgsV2Raw(m_ssh_options.Value(),&error_msg)
	   || ssh_options_arglist.Count() < 1 )
	{
		logError("Failed to parse ssh options (%s): %s\n",m_ssh_options.Value(),error_msg.Value());
		return false;
	}

		// We now look up the auto-generated ssh options and merge
		// those together.
	MyString m_ssh_basename;
	m_ssh_basename = condor_basename(ssh_options_arglist.GetArg(0));
	bool is_scp = (m_ssh_basename == "scp");

		// Build the ssh command.
		// The following macros are supported:
		// %% --> %, %x --> proxy command, %h --> remote host, %u --> user
		// %i --> ssh key, %k --> known hosts file
		// Note that use of remote host is just for clarity; since we
		// are proxying the connection for ssh, the name of the host
		// can be anything.  I have observed versions of ssh that
		// are hard-coded to check keys against a global known_hosts
		// file, no matter what options we set, so it is recommended
		// not to give ssh a real hostname.  That's why we prefix
		// the hostname with "condor-job" in the default options.
	MyString ssh_cmd;
	ArgList ssh_arglist;
	MyString param_name;
	param_name.sprintf("SSH_TO_JOB_%s_CMD",m_ssh_basename.Value());
	MyString default_ssh_cmd;
	default_ssh_cmd.sprintf("\"%s -oUser=%%u -oIdentityFile=%%i -oStrictHostKeyChecking=yes -oUserKnownHostsFile=%%k -oGlobalKnownHostsFile=%%k -oProxyCommand=%%x%s\"",
							ssh_options_arglist.GetArg(0),
							is_scp ? "" : " condor-job.%h");
	param(ssh_cmd,param_name.Value(),default_ssh_cmd.Value());

	if( !ssh_arglist.AppendArgsV2Quoted(ssh_cmd.Value(),&error_msg) ) {
		logError("Error parsing configuration %s: %s\n",
				param_name.Value(), error_msg.Value());
		return false;
	}

		// now insert any extra ssh options provided by -ssh
	int insert_arg;
	for(insert_arg=1; insert_arg<ssh_options_arglist.Count(); insert_arg++) {
		ssh_arglist.InsertArg(ssh_options_arglist.GetArg(insert_arg),insert_arg);
	}

	char **argarray = ssh_arglist.GetStringArray();
	ssh_arglist.Clear();
	for(int i=0; argarray[i]; i++) {
		char const *ptr;
		MyString new_arg;
		for(ptr=argarray[i]; *ptr; ptr++) {
			if( *ptr == '%' ) {
				ptr += 1;
				if( *ptr == '%' ) {
					new_arg += '%';
				}
				else if( *ptr == 'x' ) {
					new_arg += proxy_command;
				}
				else if( *ptr == 'u' ) {
					new_arg += remote_user;
				}
				else if( *ptr == 'h' ) {
					new_arg += remote_host;
				}
				else if( *ptr == 'i' ) {
					new_arg += private_client_key_file;
				}
				else if( *ptr == 'k' ) {
					new_arg += known_hosts_file;
				}
				else {
					logError("Unexpected %%%c in ssh command: %s\n",
							 *ptr ? *ptr : ' ', ssh_cmd.Value());
					return false;
				}
			}
			else {
				new_arg += *ptr;
			}
		}
		ssh_arglist.AppendArg(new_arg.Value());
	}
	deleteStringArray(argarray);
	argarray = NULL;

	ssh_arglist.AppendArgsFromArgList(m_ssh_args_from_command_line);

	MyString ssh_command;
	if( !ssh_arglist.GetArgsStringSystem(&ssh_command,0,&error_msg) ) {
		logError("Error producing ssh command string: %s\n",
				 error_msg.Value());
		return false;
	}

	dprintf(D_FULLDEBUG,"Executing ssh with command: %s\n",
			ssh_command.Value());

	m_ssh_exit_status = system(ssh_command.Value());

	return true;
}


int
main(int argc, char *argv[])
{


	myDistro->Init( argc, argv );
	config();

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	SSHToJob ssh_to_job;
	if( !ssh_to_job.parseArgs(argc,argv) ) {
		return 2;
	}
	if( !ssh_to_job.execute() ) {
		return 1;
	}

	int rc = ssh_to_job.getSSHExitStatus();

	if( WIFEXITED(rc) ) {
		return WEXITSTATUS(rc);
	}
	else if( rc ) { // exited due to signal
		return 1;
	}

	return 0;
}

#include "daemon_core_stubs.h"
