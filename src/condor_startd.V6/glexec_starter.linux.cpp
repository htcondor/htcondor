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
#include "condor_arglist.h"
#include "condor_auth_x509.h"
#include "basename.h"
#include "env.h"
#include "my_popen.h"
#include "MyString.h"
#include "setenv.h"
#include <condor_daemon_core.h>
#include "fdpass.h"

// data that needs to be preserved between calls to glexec_starter_prepare()
// and glexec_starter_handle_env()
//
static Env s_saved_env;
static int s_saved_sock_fds[2];
static int s_saved_starter_stdin;

bool
glexec_starter_prepare(const char* starter_path,
                       const char* proxy_file,
                       const ArgList& orig_args,
                       const Env* orig_env,
                       const int orig_std_fds[3],
                       ArgList& glexec_args,
                       Env& glexec_env,
                       int glexec_std_fds[3])
{
	// if GLEXEC_STARTER is set, use glexec to invoke the
	// starter (or fail if we can't). this involves:
	//   - verifying that we have a delegated proxy from
	//     the user stored, since we need to hand it to
	//     glexec so it can look up the UID/GID
	//   - invoking 'glexec_starter_setup.sh' via glexec to
	//     setup the starter's "private directory" for a copy
	//     of the job's proxy to go into, as well as the StarterLog
	//     and execute dir
	//   - adding the contents of the GLEXEC and config param
	//     and the path to 'condor_glexec_wrapper' to the front
	//     of the command line
	//   - setting up glexec's environment (setting the
	//     mode, handing off the proxy, etc.)
        //   - creating a UNIX-domain socket to use to communicate
	//     with our wrapper script, and munging the std_fds
	//     array

	// verify that we have a stored proxy
	if( proxy_file == NULL ) {
		dprintf( D_ALWAYS,
		         "cannot use glexec to spawn starter: no proxy "
		         "(is GLEXEC_STARTER set in the shadow?)\n" );
		return false;
	}

	// using the file name of the proxy that was stashed ealier, construct
	// the name of the starter's "private directory". the naming scheme is
	// (where XXXXXX is randomly generated via condor_mkstemp):
	//   - $(GLEXEC_USER_DIR)/startd-tmp-proxy-XXXXXX
	//       - startd's copy of the job's proxy
	//   - $(GLEXEC_USER_DIR)/starter-tmp-dir-XXXXXX
	//       - starter's private dir
	//
	std::string glexec_private_dir;
	char* dir_part = condor_dirname(proxy_file);
	ASSERT(dir_part != NULL);
	glexec_private_dir = dir_part;
	free(dir_part);
	glexec_private_dir += "/starter-tmp-dir-";
	const char* random_part = proxy_file;
	random_part += strlen(random_part) - 6;
	glexec_private_dir += random_part;
	dprintf(D_ALWAYS,
	        "GLEXEC: starter private dir is '%s'\n",
	        glexec_private_dir.c_str());

	// get the glexec command line prefix from config
	char* glexec_argstr = param( "GLEXEC" );
	if ( ! glexec_argstr ) {
		dprintf( D_ALWAYS,
		         "cannot use glexec to spawn starter: "
		         "GLEXEC not given in config\n" );
		return false;
	}

	// cons up a command line for my_system. we'll run the
	// script $(LIBEXEC)/glexec_starter_setup.sh, which
	// will create the starter's "private directory" (and
	// its log and execute subdirectories). the value of
	// glexec_private_dir will be passed as an argument to
	// the script

	// parse the glexec args for invoking glexec_starter_setup.sh.
	// do not free them yet, except on an error, as we use them
	// again below.
	std::string setup_err;
	ArgList  glexec_setup_args;
	glexec_setup_args.SetArgV1SyntaxToCurrentPlatform();
	if( ! glexec_setup_args.AppendArgsV1RawOrV2Quoted( glexec_argstr,
	                                                   setup_err ) ) {
		dprintf( D_ALWAYS,
		         "GLEXEC: failed to parse GLEXEC from config: %s\n",
		         setup_err.c_str() );
		free( glexec_argstr );
		return 0;
	}

	// set up the rest of the arguments for the glexec setup script
	char* tmp = param("LIBEXEC");
	if (tmp == NULL) {
		dprintf( D_ALWAYS,
		         "GLEXEC: LIBEXEC not defined; can't find setup script\n" );
		free( glexec_argstr );
		return 0;
	}
	std::string libexec = tmp;
	free(tmp);
	std::string setup_script = libexec;
	setup_script += "/glexec_starter_setup.sh";
	glexec_setup_args.AppendArg(setup_script.c_str());
	glexec_setup_args.AppendArg(glexec_private_dir.c_str());

	// debug info.  this display format totally screws up the quoting, but
	// my_system gets it right.
	std::string disp_args;
	glexec_setup_args.GetArgsStringForDisplay(disp_args, 0);
	dprintf (D_ALWAYS, "GLEXEC: about to glexec: ** %s **\n",
			disp_args.c_str());

	// the only thing actually needed by glexec at this point is the cert, so
	// that it knows who to map to.  the pipe outputs the username that glexec
	// ended up using, on a single text line by itself.
	SetEnv( "GLEXEC_CLIENT_CERT", proxy_file );

	// create the starter's private dir
	int ret = my_system(glexec_setup_args);

	// clean up
	UnsetEnv( "GLEXEC_CLIENT_CERT");

	if ( ret != 0 ) {
		dprintf(D_ALWAYS,
		        "GLEXEC: error creating private dir: my_system returned %d\n",
		        ret);
		free( glexec_argstr );
		return 0;
	}

	// now prepare the starter command line, starting with glexec and its
	// options (if any), then condor_glexec_wrapper.
	std::string err;
	if( ! glexec_args.AppendArgsV1RawOrV2Quoted( glexec_argstr,
	                                             err ) ) {
		dprintf( D_ALWAYS,
		         "failed to parse GLEXEC from config: %s\n",
		         err.c_str() );
		free( glexec_argstr );
		return 0;
	}
	free( glexec_argstr );
	std::string wrapper_path = libexec;
	wrapper_path += "/condor_glexec_wrapper";
	glexec_args.AppendArg(wrapper_path.c_str());

	// complete the command line by adding in the original
	// arguments. we also make sure that the full path to the
	// starter is given
	int starter_path_pos = glexec_args.Count();
	glexec_args.AppendArgsFromArgList( orig_args );
	glexec_args.RemoveArg( starter_path_pos );
	glexec_args.InsertArg( starter_path, starter_path_pos );

	// set up the environment stuff
	if( orig_env ) {
		// first merge in the original
		glexec_env.MergeFrom( *orig_env );
	}

	// GLEXEC_MODE - get account from lcmaps
	glexec_env.SetEnv( "GLEXEC_MODE", "lcmaps_get_account" );

	// GLEXEC_CLIENT_CERT - cert to use for the mapping
	glexec_env.SetEnv( "GLEXEC_CLIENT_CERT", proxy_file );

#if defined(HAVE_EXT_GLOBUS) && !defined(SKIP_AUTHENTICATION)
	// GLEXEC_SOURCE_PROXY -  proxy to provide to the child
	//                        (file is owned by us)
	glexec_env.SetEnv( "GLEXEC_SOURCE_PROXY", proxy_file );
	dprintf (D_ALWAYS,
	         "GLEXEC: setting GLEXEC_SOURCE_PROXY to %s\n",
	         proxy_file);

	// GLEXEC_TARGET_PROXY - child-owned file to copy its proxy to.
	// this needs to be in a directory owned by that user, and not world
	// writable.  glexec enforces this.  hence, all the whoami/mkdir mojo
	// above.
	std::string child_proxy_file = glexec_private_dir;
	child_proxy_file += "/glexec_starter_proxy";
	dprintf (D_ALWAYS, "GLEXEC: setting GLEXEC_TARGET_PROXY to %s\n",
		child_proxy_file.c_str());
	glexec_env.SetEnv( "GLEXEC_TARGET_PROXY", child_proxy_file.c_str() );

	// _CONDOR_GSI_DAEMON_PROXY - starter's proxy
	std::string var_name;
	formatstr(var_name, "_CONDOR_%s", STR_GSI_DAEMON_PROXY);
	glexec_env.SetEnv( var_name.c_str(), child_proxy_file.c_str() );
	formatstr(var_name, "_condor_%s", STR_GSI_DAEMON_PROXY);
	glexec_env.SetEnv( var_name.c_str(), child_proxy_file.c_str() );
#endif

	// the EXECUTE dir should be owned by the mapped user.  we created this
	// earlier, and now we override it in the condor_config via the
	// environment.
	std::string execute_dir = glexec_private_dir;
	execute_dir += "/execute";
	glexec_env.SetEnv ( "_CONDOR_EXECUTE", execute_dir.c_str());
	glexec_env.SetEnv ( "_condor_EXECUTE", execute_dir.c_str());

	// the LOG dir should be owned by the mapped user.  we created this
	// earlier, and now we override it in the condor_config via the
	// environment.
	std::string log_dir = glexec_private_dir;
	log_dir += "/log";
	glexec_env.SetEnv ( "_CONDOR_LOG", log_dir.c_str());
	glexec_env.SetEnv ( "_condor_LOG", log_dir.c_str());
	glexec_env.SetEnv ( "_CONDOR_LOCK", log_dir.c_str());
	glexec_env.SetEnv ( "_condor_LOCK", log_dir.c_str());

	// PROCD_ADDRESS: the Starter that we are about to create will
	// not have access to our ProcD. we'll explicitly set PROCD_ADDRESS
	// to be in its LOG directory. the Starter will see that its
	// PROCD_ADDRESS knob is different from what it inherits in
	// CONDOR_PROCD_ADDRESS, and know it needs to create its own ProcD
	//
	std::string procd_address = log_dir;
	procd_address += "/procd_pipe";
	glexec_env.SetEnv( "_CONDOR_PROCD_ADDRESS", procd_address.c_str() );
	glexec_env.SetEnv( "_condor_PROCD_ADDRESS", procd_address.c_str() );

	// CONDOR_GLEXEC_STARTER_CLEANUP_FLAG: this serves as a flag in the
	// Starter's environment that it will check for in order to determine
	// whether to do GLEXEC_STARTER-specific cleanup
	//
	glexec_env.SetEnv( "CONDOR_GLEXEC_STARTER_CLEANUP_FLAG",
	                   "CONDOR_GLEXEC_STARTER_CLEANUP_FLAG" );

	// now set up a socket pair for communication with
	// condor_glexec_wrapper
	//
	if (socketpair(PF_UNIX, SOCK_STREAM, 0, s_saved_sock_fds) == -1)
	{
		dprintf(D_ALWAYS,
		        "GLEXEC: socketpair error: %s\n",
		        strerror(errno));
		return false;
	}
	glexec_std_fds[0] = s_saved_sock_fds[1];
	if (orig_std_fds == NULL) {
		s_saved_starter_stdin = -1;
		glexec_std_fds[1] = glexec_std_fds[2] = -1;
	}
	else {
		s_saved_starter_stdin = orig_std_fds[0];
		glexec_std_fds[1] = orig_std_fds[1];
		glexec_std_fds[2] = orig_std_fds[2];
	}

	// save the environment we're handing back to the caller for use in
	// glexec_starter_handle_env()
	//
	s_saved_env.Clear();
	s_saved_env.MergeFrom(glexec_env);

	return true;
}

bool
glexec_starter_handle_env(pid_t pid)
{
	// we can now close the end of the socket that we handed down
	// to the wrapper; the other end we'll use to send stuff over
	//
	close(s_saved_sock_fds[1]);
	int sock_fd = s_saved_sock_fds[0];

	// if pid is 0, Create_Process failed; just close the socket
	// and return
	//
	if (pid == 0) {
		close(sock_fd);
		return false;
	}

	// before sending the environment, scrub some stuff that was
	// only for glexec
	//
	s_saved_env.SetEnv("GLEXEC_MODE", "");
	s_saved_env.SetEnv("GLEXEC_CLIENT_CERT", "");
	s_saved_env.SetEnv("GLEXEC_SOURCE_PROXY", "");
	s_saved_env.SetEnv("GLEXEC_TARGET_PROXY", "");

	// now send over the environment; what we send over is our own
	// environment with the stuff we added in prepareForGlexec
	// added in
	//
	Env env_to_send;
	env_to_send.MergeFrom(environ);
	env_to_send.MergeFrom(s_saved_env);
	MyString env_str;
	MyString merge_err;
	if (!env_to_send.getDelimitedStringV2Raw(&env_str, &merge_err)) {
		dprintf(D_ALWAYS,
		        "GLEXEC: Env::getDelimitedStringV2Raw error: %s\n",
		        merge_err.c_str());
		close(sock_fd);
		return false;
	}
	const char* env_buf = env_str.c_str();
	int env_len = env_str.length() + 1;
	errno = 0;
	if (write(sock_fd, &env_len, sizeof(env_len)) != sizeof(env_len)) {
		dprintf(D_ALWAYS,
		        "GLEXEC: error sending env size to wrapper: %s\n",
		        strerror(errno));
		close(sock_fd);
		return false;
	}
	errno = 0;
	if (write(sock_fd, env_buf, env_len) != env_len) {
		dprintf(D_ALWAYS,
		        "GLEXEC: error sending env to wrapper: %s\n",
		        strerror(errno));
		close(sock_fd);
		return false;
	}

	// now send over the FD that the Starter should use for stdin, if any
	// (we send a flag whether there is an FD to send first)
	//
	int flag = (s_saved_starter_stdin != -1) ? 1 : 0;
	if (write(sock_fd, &flag, sizeof(flag)) != sizeof(flag)) {
		dprintf(D_ALWAYS,
		        "GLEXEC: error sending flag to wrapper: %s\n",
		        strerror(errno));
		close(sock_fd);
		return false;
	}
	if (flag) {
		int fd;
		int rv = daemonCore->Get_Pipe_FD(s_saved_starter_stdin,
		                                 &fd);
		if (rv == FALSE) {
			fd = s_saved_starter_stdin;
		}
		if (fdpass_send(sock_fd, fd) == -1) {
			dprintf(D_ALWAYS, "GLEXEC: fdpass_send failed\n");
			close(sock_fd);
			return false;
		}
	}

	// now read any error messages produced by the wrapper
	//
	char err[256];
	ssize_t bytes = read(sock_fd, err, sizeof(err) - 1);
	if (bytes == -1) {
		dprintf(D_ALWAYS,
		        "GLEXEC: error reading message from wrapper: %s\n",
		        strerror(errno));
		close(sock_fd);
		return false;
	}
	if (bytes > 0) {
		err[bytes] = '\0';
		dprintf(D_ALWAYS, "GLEXEC: error from wrapper: %s\n", err);
		return false;
	}

	// if we're here, it all worked
	//
	close(sock_fd);
	return true;
}

