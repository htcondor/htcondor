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
#include "sig_name.h"
#include "my_popen.h"
#include "match_prefix.h"
#include "condor_claimid_parser.h"
#include "condor_attributes.h"
#include "authentication.h"
#include "condor_arglist.h"
#include "directory.h"
#include "basename.h"
#include "socket_proxy.h"

// the following headers are for passing the ssh socket
// over a named socket
#include "fdpass.h"
#include "selector.h"
#include "condor_sockfunc.h"
#include <sys/un.h>

// For ssh-to-job to EC2.
#include "condor_qmgr.h"

// globals
char *g_jobid = NULL;	// the jobid as a string, for use in interrupt_handler()


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

		// return job id as malloced string; caller is responsible for calling free()
	char* getJobId();

		// should the job be removed on interrupt?
	bool get_remove_on_interrupt() { return m_remove_on_interrupt; };

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
	bool m_remove_on_interrupt;
	int m_retry_delay;
	MyString m_ssh_keygen_args;
	bool m_x_forwarding;

	bool m_could_be_ec2_job;

	void logError(char const *fmt,...) CHECK_PRINTF_FORMAT(2,3);
	void printUsage();
	int receiveSshConnection(char const *socket_name);
};

SSHToJob::SSHToJob():
	m_subproc(-1),
	m_ssh_options("ssh"),
	m_proxy_fd(-1),
	m_ssh_exit_status(-1),
	m_debug(false),
	m_retry_sensible(false),
	m_auto_retry(false),
	m_remove_on_interrupt(false),
	m_retry_delay(5),
	m_x_forwarding(false),
	m_could_be_ec2_job(true)
{
	m_jobid.cluster = m_jobid.proc = -1;
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

char* SSHToJob::getJobId()
{
	MyString temp;
	char *ret_val = NULL;

	temp.formatstr("%d.%d",m_jobid.cluster,m_jobid.proc);
	ret_val = strdup(temp.Value());
	return ret_val;
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
	fprintf(stderr," -remove-on-interrupt      (rm job on ctrl-c)\n");
	fprintf(stderr," -shells shell1,shell2,... (shells to try)\n");
	fprintf(stderr," -ssh <alt ssh command>    (e.g. sftp or scp)\n");
	fprintf(stderr," -keygen-options <keygen options>\n");
	fprintf(stderr," -X                        (enable X11 forwarding)\n");
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
			dprintf_set_tool_debug("TOOL", 0);
			set_debug_flags(NULL, D_FULLDEBUG);
			m_debug = true;
		} else if( match_prefix( argv[nextarg], "-help" ) ) {
			printUsage();
			exit(0);
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
		} else if( match_prefix( argv[nextarg], "-remove-on-interrupt") ) {
			m_remove_on_interrupt = true;
		} else if( match_prefix( argv[nextarg], "-shells") ) {
			if( argv[nextarg + 1] ) {
				m_shells = argv[++nextarg];
			} else {
				missing_arg=true;
			}
		} else if( match_prefix( argv[nextarg], "-proxy") ) {
			if( argv[nextarg + 1] ) {
				m_proxy_fd = receiveSshConnection(argv[++nextarg]);
				if( m_proxy_fd == -1 ) {
					return false;
				}
			} else {
				missing_arg=true;
			}
		} else if( !strcmp( argv[nextarg], "-X" ) ) {
			m_x_forwarding = true;
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

int SSHToJob::receiveSshConnection(char const *fdpass_sock_name)
{
	char *endp = NULL;
	int ssh_fd = strtol(fdpass_sock_name,&endp,10);
	if( endp && *endp == '\0' ) {
		return ssh_fd; // we were passed fd directly
	}

	struct sockaddr_un named_sock_addr;
	memset(&named_sock_addr, 0, sizeof(named_sock_addr));
	named_sock_addr.sun_family = AF_UNIX;
	strncpy(named_sock_addr.sun_path,fdpass_sock_name,sizeof(named_sock_addr.sun_path)-1);
	if( strcmp(named_sock_addr.sun_path,fdpass_sock_name) ) {
		logError("full socket name is too long: %s\n",
			 fdpass_sock_name);
		return -1;
	}

	int fdpass_sock_fd = socket(AF_UNIX,SOCK_STREAM,0);
	if( fdpass_sock_fd == -1 ) {
		logError("failed to created named socket %s: %s\n",
			 fdpass_sock_name,
			 strerror(errno));
		return -1;
	}

	// assign fdpass_sock_fd to a socket object, so closure
	// happens automatically when it goes out of scope
	ReliSock fdpass_sock;
	fdpass_sock.assign(fdpass_sock_fd);

	int connect_rc = connect(fdpass_sock_fd,(struct sockaddr *)&named_sock_addr, SUN_LEN(&named_sock_addr));
	if( connect_rc != 0 )
	{
		logError("failed to connect to %s: %s\n",
			 fdpass_sock_name,
			 strerror(errno));
		return -1;
	}

	ssh_fd = fdpass_recv(fdpass_sock_fd);
	if( ssh_fd == -1 ) {
		logError("failed to receive ssh fd: %s\n",
			 strerror(errno));
		return -1;
	}

	return ssh_fd;
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
			// try every 10 seconds for first 5 min, then every 30 seconds
			if ( m_retry_delay < 30 && ((m_retry_delay * attempt) >= 300) ) {
				m_retry_delay = 30;
			}
			//fprintf(stderr,"Will try again in %d seconds.\n",m_retry_delay);
			if (attempt==1) {
				fprintf(stderr,"Waiting for job to start...\n");
			}
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
	//
	// Handle EC2 jobs.
	//
	// We only contact the schedd twice the first time: once to determine
	// if the job is in EC2 job (and if so, what to do), and once when the
	// isn't, to get the job connect info.  Attempts for EC2 jobs never
	// try to connect twice, and after the first time a job isn't an EC2
	// job, m_could_be_ec2_job is false, skipping the superflous connection.
	//

	if( m_could_be_ec2_job ) {
		Qmgr_connection * q = ConnectQ( m_schedd_name.IsEmpty() ? NULL : m_schedd_name.Value(), 0, true );
		if( ! q ) {
			logError( "Can't connect to schedd\n" );
			return false;
		}
		ClassAd * jobAd = GetJobAd( m_jobid.cluster, m_jobid.proc );
		DisconnectQ( q );

		// We may have failed to fetch the ad, or the job may have left the q.
		if( jobAd == NULL ) {
			logError( "Unable to fetch job ad; is it still in the queue?\n" );
			return false;
		}

		std::string gridResource, vmName, kpName;
		bool gotGridResource = jobAd->EvaluateAttrString( ATTR_GRID_RESOURCE, gridResource );
		bool gotVMName = jobAd->EvaluateAttrString( ATTR_EC2_REMOTE_VM_NAME, vmName );
		bool gotKPName = jobAd->EvaluateAttrString( ATTR_EC2_KEY_PAIR_FILE, kpName );

		int jobStatus;
		if( ! jobAd->EvaluateAttrInt( ATTR_JOB_STATUS, jobStatus ) ) {
			// Something has gone terribly wrong.
			logError( "Can't get job status\n" );
			return false;
		}

		FreeJobAd( jobAd );

		if( gotGridResource ) {
			if( gridResource.substr( 0, 3 ) == "ec2" ) {
				if( gotVMName && gotKPName ) {
					int pid = fork();
					if( pid == 0 ) {
						// Because ssh has a command-line parser that doesn't
						// suck, only the order of the hostname vs the command
						// matters -- it will do the right thing with options
						// wherever they may occur.  Therefore, since we're
						// supplying the hostname, it doesn't matter what the
						// user supplies; we can just append it.
						ArgList sshArgList;
						sshArgList.AppendArg( "ssh" );
						sshArgList.AppendArg( "-i" );
						sshArgList.AppendArg( kpName.c_str() );
						sshArgList.AppendArg( vmName.c_str() );
						sshArgList.AppendArgsFromArgList( m_ssh_args_from_command_line );

						char ** argArray = sshArgList.GetStringArray();
						execvp( argArray[0], argArray );
						logError( "Error executing SSH: %s\n", strerror( errno ) );
						_exit( 1 );
					}

					if( pid < 0 ) {
						logError( "Error fork()ing to run SSH: %s\n", strerror( errno ) );
						return false;
					}

					while( true ) {
						int exitedPID = waitpid( pid, & m_ssh_exit_status, 0 );
						if( exitedPID == pid ) { break; }
						logError( "Unknown child (%d) exited while waiting for SSH.\n", exitedPID );
					}

					return true;
				} else {
					if( jobStatus != RUNNING ) {
						m_retry_sensible = true;
					} else {
						logError( "ERROR: You can not SSH to an EC2 job whose keypair HTCondor is not managing.\n" );
						m_retry_sensible = false;
					}
					return false;
				}
			}
		}
	}
	m_could_be_ec2_job = false;

	//
	// Handle non-EC2 jobs.
	//

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
		if ( !m_retry_sensible ) {
			logError("%s\n",error_msg.Value());
		}
		return false;
	}

	dprintf(D_FULLDEBUG,"Got connect info for starter %s\n",
			starter_addr.Value());

	ClassAd starter_ad;
	starter_ad.Assign(ATTR_STARTER_IP_ADDR,starter_addr);
	starter_ad.Assign(ATTR_VERSION,starter_version);

	DCStarter starter;
	if( !starter.initFromClassAd(&starter_ad) ) {
		if ( !m_retry_sensible ) {
			logError("Failed to initialize starter object.\n");
		}
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
		m_session_dir.formatstr("%s%c%s.condor_ssh_to_job_%x",
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
	known_hosts_file.formatstr("%s%cknown_hosts",m_session_dir.Value(),DIR_DELIM_CHAR);
	MyString private_client_key_file;
	private_client_key_file.formatstr("%s%cssh_key",m_session_dir.Value(),DIR_DELIM_CHAR);

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


	MyString fdpass_sock_name;
	fdpass_sock_name.formatstr("%s%cfdpass",m_session_dir.Value(),DIR_DELIM_CHAR);

	// because newer versions of openssh (e.g. 5.8) close
	// all file descriptors > 2, we have to pass the ssh connection
	// over a named socket rather letting the proxy command
	// inherit it from us via ssh

	struct sockaddr_un named_sock_addr;
	memset(&named_sock_addr, 0, sizeof(named_sock_addr));
	named_sock_addr.sun_family = AF_UNIX;
	strncpy(named_sock_addr.sun_path,fdpass_sock_name.Value(),sizeof(named_sock_addr.sun_path)-1);
	if( strcmp(named_sock_addr.sun_path,fdpass_sock_name.Value()) ) {
		logError("full socket name is too long: %s\n",
			fdpass_sock_name.Value());
		return false;
	}

	int fdpass_sock_fd = socket(AF_UNIX,SOCK_STREAM,0);
	if( fdpass_sock_fd == -1 ) {
		logError("failed to created named socket %s: %s\n",
			fdpass_sock_name.Value(),
			strerror(errno));
		return false;
	}

	// assign fdpass_sock_fd to a socket object, so closure
	// happens automatically when it goes out of scope
	ReliSock fdpass_sock;
	fdpass_sock.assign(fdpass_sock_fd);

	// we don't need/want fdpass_sock to be inherited by ssh,
	// so set close-on-exec, just to be safe
	int fd_flags = fcntl(fdpass_sock_fd,F_GETFD);
	if( fd_flags == -1 ) {
		logError("failed to get socket %s flags: %s\n",
			 fdpass_sock_name.Value(),
			 strerror(errno));
		return false;
	}
	fcntl(fdpass_sock_fd,F_SETFD,fd_flags | FD_CLOEXEC);

	// we don't need/want the ssh socket to be inherited by ssh,
	// so set close-on-exec, just to be safe
	fd_flags = fcntl(sock.get_file_desc(),F_GETFD);
	if( fd_flags == -1 ) {
		logError("failed to get ssh socket flags: %s\n",
			 strerror(errno));
		return false;
	}
	fcntl(sock.get_file_desc(),F_SETFD,fd_flags | FD_CLOEXEC);

		// Create fdpass named socket with no access to anybody but this user.
	mode_t old_umask = umask(077);

	int bind_rc = bind(
			   fdpass_sock_fd,
			   (struct sockaddr *)&named_sock_addr,
			   SUN_LEN(&named_sock_addr));

	if( bind_rc != 0 )
	{
		logError("failed to bind to %s: %s\n",
			fdpass_sock_name.Value(),
			strerror(errno));
		umask(old_umask);
		return false;
	}

	umask(old_umask);

	if( listen(fdpass_sock_fd,5) ) {
		logError("failed to listen on %s: %s\n",
			 fdpass_sock_name.Value(), strerror(errno));
		return false;
	}


	MyString proxy_command;
	ArgList proxy_arglist;
	proxy_arglist.AppendArg(m_program_name.Value());
	if( m_debug ) {
		proxy_arglist.AppendArg("-debug");
	}
	proxy_arglist.AppendArg("-proxy");
	proxy_arglist.AppendArg(fdpass_sock_name.Value());
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
	param_name.formatstr("SSH_TO_JOB_%s_CMD",m_ssh_basename.Value());
	MyString default_ssh_cmd;
	default_ssh_cmd.formatstr("\"%s -oUser=%%u -oIdentityFile=%%i -oStrictHostKeyChecking=yes -oUserKnownHostsFile=%%k -oGlobalKnownHostsFile=%%k -oProxyCommand=%%x%s\"",
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

	if( m_x_forwarding ) {
		ssh_arglist.InsertArg("-X",insert_arg);
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
	ssh_arglist.GetArgsStringForDisplay(&ssh_command);

	dprintf(D_FULLDEBUG,"Executing ssh command: %s\n",
			ssh_command.Value());

	int pid = fork();
	if( pid == 0 ) {
		// Some versions of ssh use whatever shell is
		// specified in SHELL to execute the proxy command.
		// If the shell is csh, it closes all file descriptors
		// except for stdio ones.  We used to pass the sshd fd
		// to the proxy command via inheritance, so this was a
		// problem.  We no longer rely on inheritance, because
		// some versions of ssh close all fds > 2.  But just
		// in case we ever do go back to relying on
		// inheritance, unset SHELL.

		unsetenv("SHELL");

		argarray = ssh_arglist.GetStringArray();
		execvp(argarray[0],argarray);
		logError("Error executing %s: %s\n",argarray[0],strerror(errno));
		_exit(1);
	}
	if( pid < 0 ) {
		logError("Error forking to execute ssh: %s\n",
			 strerror(errno));
		return false;
	}

	// now wait for ssh to exit or for the proxy command
	// to connect to the named socket

	Selector selector;
	selector.add_fd( fdpass_sock_fd, Selector::IO_READ );

	while( true ) {
		int exited_pid = waitpid(pid,&m_ssh_exit_status,fdpass_sock_fd == -1 ? 0 : WNOHANG);
		if( exited_pid == pid ) {
			break;
		}

		if( fdpass_sock_fd == -1 ) {
			continue;
		}

		selector.set_timeout(1);
		selector.execute();
		if( selector.fd_ready(fdpass_sock_fd,Selector::IO_READ) ) {
			condor_sockaddr accepted_addr;
			int accepted_fd = condor_accept( fdpass_sock_fd, accepted_addr );

			if( accepted_fd == -1 ) {
				logError("Error accepting connection on %s: %s\n",
					 fdpass_sock_name.Value(),
					 strerror(errno));
			}
			else {
				int fdpass_rc = fdpass_send(accepted_fd,sock.get_file_desc());
				if( fdpass_rc != 0 ) {
					logError("Error passing socket to proxy: %s\n",
						 strerror(errno));
				}
				else {
					dprintf(D_FULLDEBUG,"Passed ssh connection to ssh proxy.\n");
				}
				close(accepted_fd);
				accepted_fd = -1;
			}

			selector.delete_fd( fdpass_sock_fd, Selector::IO_READ );
			fdpass_sock.close();
			fdpass_sock_fd = -1;
		}
	}

	return true;
}


#if !defined(WIN32)
void interrupt_handler(int sig)
{

	fprintf(stderr,"Interrupted by signal %d (%s)",sig,signalName(sig));
	if (!g_jobid) {
		fprintf(stderr,"\n");
		exit(1);
	}

	// spawn off condor_rm to remove the job
	fprintf(stderr,", removing job %s...\n",g_jobid);
	ArgList args;
	args.AppendArg("condor_rm");
	args.AppendArg(g_jobid);
	int ret_val = my_system(args);
	if (ret_val == -1) {
		fprintf(stderr,"Failed to exec condor_rm, errno=%d (%s)\n",
				errno,strerror(errno));
	}
	exit(1);	// documentation says exit 1 = exited due to signal 
}

#endif

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

#if !defined(WIN32)
	if (ssh_to_job.get_remove_on_interrupt()) {
		// stash our job id in a buffer the signal handler can access
		g_jobid = ssh_to_job.getJobId();

		// install signal handlers
		install_sig_handler(SIGINT, interrupt_handler);
		install_sig_handler(SIGHUP, interrupt_handler);
		install_sig_handler(SIGTERM, interrupt_handler);
		install_sig_handler(SIGQUIT, interrupt_handler);
	}
#endif

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
