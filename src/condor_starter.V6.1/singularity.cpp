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

#include "singularity.h"

#ifdef LINUX
#include <sys/wait.h>
#endif

#include <vector>
#include <regex>

#include "condor_config.h"
#include "my_popen.h"
#include "CondorError.h"
#include "basename.h"
#include "condor_attributes.h"
#include "directory.h"

using namespace htcondor;


bool Singularity::m_enabled = false;
bool Singularity::m_use_pid_namespaces = true;
bool Singularity::m_probed = false;
bool Singularity::m_apptainer = false;
int  Singularity::m_default_timeout = 120;
std::string Singularity::m_singularity_version;
std::string Singularity::m_lastSingularityErrorLine;

static bool find_singularity(std::string &exec)
{
#ifdef LINUX
	std::string singularity;
	if (!param(singularity, "SINGULARITY")) {
		dprintf(D_ERROR, "SINGULARITY is undefined.\n");
		return false;
	}
	exec = singularity;
	return true;
#else
	(void) exec;
	return false;
#endif
}

// If singularity is really "apptainer", we need to
// pass environment variables from host to container by
// prefixing them with APPTAINERENV_.  Otherwise, they need
// to be prefixed with SINGULARITY_ENV
std::string 
Singularity::environmentPrefix() {
	return Singularity::m_apptainer ? "APPTAINERENV_" : "SINGULARITYENV_";
}

bool
Singularity::enabled()
{
	CondorError err;
	return detect(err);
}

const char *
Singularity::version()
{
	CondorError err;
	if (!detect(err)) {return NULL;}
	return m_singularity_version.c_str();
}

bool
Singularity::detect(CondorError &err)
{
	if (m_probed) {return m_enabled;}

	m_probed = true;
	ArgList infoArgs;
	std::string exec;
	if (!find_singularity(exec)) {
		return false;
	}
	infoArgs.AppendArg(exec);
	infoArgs.AppendArg("--version");

	std::string displayString;
	infoArgs.GetArgsStringForLogging( displayString );
	dprintf(D_ALWAYS, "Attempting to run: '%s %s'.\n", exec.c_str(), displayString.c_str());

	MyPopenTimer pgm;
	if (pgm.start_program(infoArgs, true, NULL, false) < 0) {
		// treat 'file not found' as not really error
		int d_level = D_FULLDEBUG;
		if (pgm.error_code() != ENOENT) {
			d_level = D_ERROR;
			err.pushf("Singularity::detect", 1, "Failed to run '%s' errno = %d %s.", displayString.c_str(), pgm.error_code(), pgm.error_str());
		}
		dprintf(d_level, "Failed to run '%s' errno=%d %s.\n", displayString.c_str(), pgm.error_code(), pgm.error_str() );
		return false;
	}

	int exitCode;
	if ( ! pgm.wait_for_exit(m_default_timeout, &exitCode) || exitCode != 0) {
		pgm.close_program(1);
		std::string line;
		pgm.output().readLine(line, false);
		chomp(line);
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, line.c_str());
		err.pushf("Singularity::detect", 2, "'%s' exited with status %d", displayString.c_str(), exitCode);
		return false;
	}

	pgm.output().readLine(m_singularity_version, false);
	chomp(m_singularity_version);
	dprintf( D_ALWAYS, "[singularity version] %s\n", m_singularity_version.c_str() );
	std::string line;
	while (pgm.output().readLine(line, false)) {
		chomp(line);
		dprintf( D_ALWAYS, "[singularity info] %s\n", line.c_str() );
	}

	if (m_singularity_version.find("apptainer") != std::string::npos) {
		m_apptainer = true;
	}
	m_enabled = ! m_singularity_version.empty();

	return true;
}

bool
Singularity::job_enabled(ClassAd &machineAd, ClassAd &jobAd)
{
	bool wantSIF = false;
	bool wantSandbox  = false;
	bool wantDockerImage = false;

	jobAd.LookupBool(ATTR_WANT_SIF, wantSIF);
	jobAd.LookupBool(ATTR_WANT_SANDBOX_IMAGE, wantSandbox);
	jobAd.LookupBool(ATTR_WANT_DOCKER_IMAGE, wantDockerImage);

	if (wantSIF || wantSandbox || wantDockerImage) {
		return true;
	}

	return param_boolean("SINGULARITY_JOB", false, false, &machineAd, &jobAd);
}


Singularity::result
Singularity::setup(ClassAd &machineAd,
		ClassAd &jobAd,
		std::string &exec,
		ArgList &job_args,
		const std::string &job_iwd,
		const std::string &execute_dir,
		Env &job_env,
		UseLauncher launcher)
{
	ArgList sing_args;

	if (!job_enabled(machineAd, jobAd)) {return Singularity::DISABLE;}

	if (!enabled()) {
		dprintf(D_ALWAYS, "Singularity job has been requested but singularity does not appear to be configured on this host.\n");
		return Singularity::FAILURE;
	}
	std::string sing_exec_str;
	if (!find_singularity(sing_exec_str)) {
		return Singularity::FAILURE;
	}

	std::string image;
	if (!param_eval_string(image, "SINGULARITY_IMAGE_EXPR", "SingularityImage", &machineAd, &jobAd)) {
		jobAd.LookupString(ATTR_CONTAINER_IMAGE, image);
		if (image.length() == 0) {
			dprintf(D_ALWAYS, "Singularity support was requested but unable to determine the image to use.\n");
			return Singularity::FAILURE;
		}
	}

	std::string target_dir;
	bool has_target = hasTargetDir(jobAd, target_dir); 

	// If we have a process to exec, remove it from the args
	if (exec.length() > 0) {
		job_args.RemoveArg(0);
	}

	std::string orig_exec_val = exec;
	if (has_target && (orig_exec_val.compare(0, execute_dir.length(), execute_dir) == 0)) {
		exec = target_dir + "/" + orig_exec_val.substr(execute_dir.length());
		dprintf(D_FULLDEBUG, "Updated executable path to %s for target directory mode.\n", exec.c_str());
	}
	sing_args.AppendArg(sing_exec_str.c_str());

	std::string sing_verbosity;
	param(sing_verbosity, "SINGULARITY_VERBOSITY", "-s");
	if (sing_verbosity.length() > 0) {
		sing_args.AppendArg(sing_verbosity);
	}

	// If no "Executable" is specified, we get a zero-length exec string
	// use "singularity run" to run in this case, and assume
	// that there is an appropriate runscript inside the image
	if (orig_exec_val.length() > 0) {
		sing_args.AppendArg("exec");
	} else {
		sing_args.AppendArg("run");
	}

	// Bind
	// Mount under scratch
	std::string scratch;
	if (!param_eval_string(scratch, "MOUNT_UNDER_SCRATCH", "", &jobAd)) {
		param(scratch, "MOUNT_UNDER_SCRATCH");
	}

	// Now add in scratch mounts requested by the job.
	std::string job_mount_under_scratch;
	jobAd.LookupString(ATTR_JOB_MOUNT_UNDER_SCRATCH, job_mount_under_scratch);
	if (job_mount_under_scratch.length() > 0) {
		if (scratch.length() > 0) {
			scratch += ' ';
		}
		scratch += job_mount_under_scratch;
	}

	if (scratch.length() > 0) {
		for (const auto& next_dir: StringTokenIterator(scratch)) {
			sing_args.AppendArg("-S");
			sing_args.AppendArg(next_dir);
		}
	}
	if (job_iwd != execute_dir) {
		sing_args.AppendArg("-B");
		sing_args.AppendArg(job_iwd.c_str());
	}

	// Bind the launcher shell script into the job
	//
	if (launcher == USE_LAUNCHER) {
		std::string libexec_dir;
		param(libexec_dir, "LIBEXEC");
		std::string launcher_bind = libexec_dir;
		launcher_bind += '/';
		launcher_bind += "condor_container_launcher.sh";
		launcher_bind += ':';
		launcher_bind += "/condor_container_launcher.sh";
		sing_args.AppendArg("-B");
		sing_args.AppendArg(launcher_bind);
	}

	sing_args.AppendArg("-W");
	sing_args.AppendArg(execute_dir.c_str());

	// Singularity and Apptainer prohibit setting HOME.  Just delete it
	job_env.DeleteEnv("HOME");
	job_env.DeleteEnv("APPTAINER_BIND");
	job_env.DeleteEnv("APPTAINER_BINDDIR");
	job_env.DeleteEnv("SINGULARITY_BIND");
	job_env.DeleteEnv("SINGULARITY_BINDDIR");

	// Bind-mount the execute directory.
	// When overlayfs is unavailable, singularity cannot bind-mount a directory that
	// does not exist in the container.  Hence, we allow a specific fixed target directory
	// to be used instead.
	std::string bind_spec = execute_dir;
	if (has_target) {
		bind_spec += ":";
		bind_spec += target_dir;
		// Only change PWD to our new target dir if that's where we should startup.
		if (job_iwd == execute_dir) {
			sing_args.AppendArg("--pwd");
			sing_args.AppendArg(target_dir.c_str());
		}
		// Update the environment variables
		retargetEnvs(job_env, target_dir, execute_dir);

	} else {
		sing_args.AppendArg("--pwd");
		sing_args.AppendArg(job_iwd.c_str());
	}
	sing_args.AppendArg("-B");
	sing_args.AppendArg(bind_spec.c_str());

	// if the startd has assigned us a gpu, add --nv to the sing exec
	// arguments to mount the nvidia devices
	// ... and if the host has OpenCL drivers, bind-mount the drivers
	// so that OpenCL programs can also run in the container.
	std::vector<std::string> additional_bind_mounts;
	std::string assignedGpus;
	machineAd.LookupString("AssignedGPUs", assignedGpus);
	if (assignedGpus.length() > 0) {
		sing_args.AppendArg("--nv");
		static const char* open_cl_path = "/etc/OpenCL/vendors";
		if (IsDirectory(open_cl_path)) {
			additional_bind_mounts.emplace_back(open_cl_path);
		}
	}

	// Now handle requested bind-mounts.  We will mount everything specified with
	// SINGULARITY_BIND_EXPR, plus any mounts in additional_bind_mounts list.
	if (param_eval_string(bind_spec, "SINGULARITY_BIND_EXPR", "SingularityBind", &machineAd, &jobAd)) {
		dprintf(D_FULLDEBUG, "Parsing bind mount specification for singularity: %s\n", bind_spec.c_str());
		std::vector<std::string> binds = split(bind_spec);
		// Singularity outputs warnings about duplicate mounts to stderr, so
		// let's try to avoid that.
		for (const auto& tmp: additional_bind_mounts) {
			if (!contains(binds, tmp)) {
				binds.emplace_back(tmp);
			}
		}
		for (const auto& next_bind: binds) {
			std::string bind_src_dir(next_bind);
			// BIND exprs can be src:dst:ro 
			size_t colon = bind_src_dir.find(':');
			if (colon != std::string::npos) {
				bind_src_dir = bind_src_dir.substr(0, colon);
			}
			struct stat sw;
			if (stat(bind_src_dir.c_str(), &sw) != 0) {
				dprintf(D_ALWAYS, "Skipping invalid singularity bind source directory %s\n", next_bind.c_str());
				continue;
			} 

			// Older singularity versions that do not support underlay
			// may require the target directory to exist.  OSG wants
			// to ignore mount requests where this is the case.
			if (param_boolean("SINGULARITY_IGNORE_MISSING_BIND_TARGET", false)) {
				// We an only check this when the image format is a directory
				// That's OK for OSG, that's all they use
				struct stat si = {};
				stat(image.c_str(), &si);
				if (si.st_mode & S_IFDIR) {
					// target dir is after the colon, if it exists
					std::string target_dir;
					const char *colon = strchr(next_bind.c_str(),':');
					if (colon == nullptr) {
						// "/dir"
						target_dir = next_bind;
					} else {
						// "/dir:dir2"
						target_dir = colon + 1;
					}
					size_t colon_pos = target_dir.find(':');
					if (colon_pos != std::string::npos) {
						target_dir = target_dir.substr(0, colon_pos);
					}

					std::string abs_target_dir = image + "/" + target_dir;
					struct stat td = {};
					stat(abs_target_dir.c_str(), &td);
					if ( !(td.st_mode & S_IFDIR) ) {
						dprintf(D_ALWAYS, "Target directory %s does not exist in image, skipping mount\n", abs_target_dir.c_str());
						continue;
					}

				} else {
					dprintf(D_ALWAYS, "Image %s is NOT directory, skipping test for missing bind target for %s\n", image.c_str(), next_bind.c_str());
				}
			}
			sing_args.AppendArg("-B");
			sing_args.AppendArg(next_bind);
		}
	}

	// If file xfer is on...
	// pass --home <scratch_dir>
	// 1) bind_mounts /home/<user_name> onto <scratch_dir>
	// 2) sets $HOME to /home/<user_name>
	// 3) puts <scratch_dir> as the home dir entry in the /etc/passwd entry

	if (job_iwd == execute_dir) {
		sing_args.AppendArg("--home");
		sing_args.AppendArg(execute_dir);
	}

	// Setup Singularity containerization options.
	std::string knob;
	param(knob, "SINGULARITY_USE_PID_NAMESPACES", "auto");
	if (!string_is_boolean_param(knob.c_str(), m_use_pid_namespaces)) {
		// SINGULARITY_USE_PID_NAMESPACES is not explicitly set to True or False, so we set it automatically.
		// At this point, we should have a slot attribute that says if pid namespaces supported - this
		// slot attribute was set when the starter was run with the '-classad' option.
		m_use_pid_namespaces = false;  // if our slotad does not have the result of our testing, go w/ false
		machineAd.LookupBool(ATTR_HAS_SINGULARITY_PIDNAMESPACES, m_use_pid_namespaces);
	}
	add_containment_args(sing_args);

	std::string args_error;
	std::string sing_extra_args;

	if (!param_eval_string(sing_extra_args, "SINGULARITY_EXTRA_ARGUMENTS", "", &machineAd, &jobAd)) {
		// SINGULARITY_EXTRA_ARGUMENTS isn't a valid expression.  Try just an unquoted string
		char *xtra = param("SINGULARITY_EXTRA_ARGUMENTS");
		if (xtra) {
			sing_extra_args = xtra;
			free(xtra);
		}
	}

	if(!sing_extra_args.empty() && !sing_args.AppendArgsV1RawOrV2Quoted(sing_extra_args.c_str(),args_error)) {
		dprintf(D_ALWAYS,"singularity: failed to parse extra arguments: %s\n",
		args_error.c_str());
		return Singularity::FAILURE;
	}

	if (job_iwd != execute_dir) {
		// File xfer off, if image was a relative path, prepend iwd
		if (image[0] != '/') {
			image = job_iwd + '/' + image;
		}
	}

	sing_args.AppendArg(image.c_str());

	if (orig_exec_val.length() > 0) {
		if (launcher == USE_LAUNCHER) {
			sing_args.AppendArg("/condor_container_launcher.sh");
		}
		sing_args.AppendArg(exec.c_str());
	}

	sing_args.AppendArgsFromArgList(job_args);

	std::string args_string;
	job_args = sing_args;
	job_args.GetArgsStringForDisplay(args_string, 1);
	exec = sing_exec_str;
	dprintf(D_FULLDEBUG, "Arguments updated for executing with singularity: %s %s\n", exec.c_str(), args_string.c_str());

	// For some reason, singularity really wants /usr/sbin near the beginning of the PATH
	// when running /usr/sbin/mksquashfs when running docker: images
	//
	// Update:  If PATH wasn't set, singularity will set it from the
	// image.  In this case, we are injecting a new PATH which prevents
	// the image path from being set, which breaks all kinds of things.
	// comment this out for now until we find a better solution.
	// This means that to run docker images, the user will have to have
	// /usr/sbin in their path
	//std::string oldPath;
	//job_env.GetEnv("PATH", oldPath);
	//job_env.SetEnv("PATH", std::string("/usr/sbin:" + oldPath));

	// If reading an image from a docker hub, store it in the scratch dir
	// when we get AP sandboxes, that would be a better place to store these
	if (Singularity::m_apptainer) {
		job_env.SetEnv("APPTAINER_CACHEDIR", execute_dir);
		job_env.SetEnv("APPTAINER_TMPDIR", execute_dir);
	} else {
		job_env.SetEnv("SINGULARITY_CACHEDIR", execute_dir);
		job_env.SetEnv("SINGULARITY_TMPDIR", execute_dir);
	}

	Singularity::convertEnv(&job_env);

	// Set the shell prompt so that it doesn't confuse Todd when he ssh-to-job's
	// into the container
	std::string hostname;
	machineAd.LookupString(ATTR_NAME, hostname);
	std::string shell_prompt = hostname + "$ ";

	if (Singularity::m_apptainer) {
		job_env.SetEnv("APPTAINERENV_PS1", shell_prompt);
	} else {
		job_env.SetEnv("SINGULARITYENV_PS1", shell_prompt);
	}
	return Singularity::SUCCESS;
}

static bool
envToList(void *list, const std::string & name, const std::string & /*value*/) {
	std::list<std::string> *slist = (std::list<std::string> *)list;
	slist->push_back(name);
	return true;
}

bool
Singularity::retargetEnvs(Env &job_env, const std::string &target_dir, const std::string &execute_dir) {

	// if SINGULARITY_TARGET_DIR is set, we need to reset
	// all the job's environment variables that refer to the scratch dir

	std::string oldScratchDir;
	job_env.GetEnv("_CONDOR_SCRATCH_DIR", oldScratchDir);

	// Walk thru all the environment variables, and for each one where the value
	// contains the scratch dir path, make a new SINGULARITYENV_xxx variable that
	// replaces the scratch dir path in the value with the target_dir path.
	// This way processes outside of the container, such as the USER_JOB_WRAPPER, will
	// get the original environment variable values, but variables passed into the
	// container will have the path changed to target_dir.

	std::list<std::string> envNames;
	job_env.Walk(envToList, (void *)&envNames);
	for (const std::string & name : envNames) {
		std::string value;
		job_env.GetEnv(name, value);
		auto index_execute_dir = value.find(execute_dir);
		if (index_execute_dir != std::string::npos) {
			std::string new_name = environmentPrefix() + name;
			job_env.SetEnv(
				new_name,
				value.replace(index_execute_dir, execute_dir.length(), target_dir)
			);
		}
	}

	job_env.SetEnv("_CONDOR_SCRATCH_DIR_OUTSIDE_CONTAINER", oldScratchDir);

	return true;
}

bool
Singularity::convertEnv(Env *job_env) {
	std::list<std::string> envNames;
	job_env->Walk(envToList, (void *)&envNames);
	std::list<std::string>::iterator it;
	for (it = envNames.begin(); it != envNames.end(); it++) {
		std::string name = *it;
		std::string  value;

		// Skip env vars that already start with SINGULARITYENV_, as they
		// have already been converted (probably via retargetEnvs()).
		if (name.rfind(environmentPrefix(),0)==0) continue;

		job_env->GetEnv(name, value);
		std::string new_name = environmentPrefix() + name;
		// Only copy over the value to the new_name if the new_name
		// does not already exist because perhaps it was already set
		// in retargetEnvs().  Note that 'value' is not touched if
		// GetEnv returns false.
		if (job_env->GetEnv(new_name, value) == false) {
			job_env->SetEnv(new_name, value);
		}
	}
	return true;
}

bool 
Singularity::hasTargetDir(const ClassAd &jobAd, /* not const */ std::string &target_dir) {
	target_dir = "";
	bool has_target = param(target_dir, "SINGULARITY_TARGET_DIR") && !target_dir.empty();

	// If the admin hasn't specification as target_dir, let the job select one
	// We assume that the job has also selected the image as well
	if (!has_target) {
		has_target = jobAd.LookupString(ATTR_CONTAINER_TARGET_DIR, target_dir);
	}
	return has_target;
}

bool 
Singularity::runTest(const std::string &JobName, const ArgList &args, int orig_args_len, Env &env, std::string &errorMessage) {

	TemporaryPrivSentry sentry(PRIV_USER);

	// Cleanse environment
	env.DeleteEnv("HOME");
	env.DeleteEnv("APPTAINER_BIND");
	env.DeleteEnv("APPTAINER_BINDDIR");
	env.DeleteEnv("SINGULARITY_BIND");
	env.DeleteEnv("SINGULARITY_BINDDIR");
	//
	// First replace "exec" with "test"
	ArgList testArgs;

	// The last orig_args_len args are the real exec + its args.  Skip those for the test
	for (size_t i = 0; i < args.Count() - orig_args_len; i++) {
		const char *arg = args.GetArg(i);
		if ((strcmp(arg, "run") == 0) || (strcmp(arg, "exec")) == 0) {
			// Stick a -q before test to keep the download quiet
			testArgs.AppendArg("-q");
			arg = "test";
		}
		testArgs.AppendArg(arg);
	}

	std::string stredArgs;
	testArgs.GetArgsStringForDisplay(stredArgs);

	dprintf(D_FULLDEBUG, "Runnning singularity test for job %s cmd is %s\n", JobName.c_str(), stredArgs.c_str());
	FILE *sing_test_output = my_popen(testArgs, "r", MY_POPEN_OPT_WANT_STDERR, &env, true);
	if (!sing_test_output) {
		dprintf(D_ALWAYS, "Error running singularity test job: %s\n", stredArgs.c_str());
		return false;
	}

	char buf[256];
	buf[0] = '\0';

	int nread = fread(buf, 1, 255, sing_test_output);
	if (nread > 0) buf[nread] = '\0';
	char *pos = nullptr;
	if ((pos = strchr(buf, '\n'))) {
		*pos = '\0';
	}

	errorMessage = buf;

	// Singularity puts various ansi escape sequences into its output to do things
	// like change text color to red if an error.  Remove those sequences.
	errorMessage = RemoveANSIcodes(errorMessage);

	// my_pclose will return an error if there is more than a pipe full
	// of output at close time.  Drain the input pipe to prevent this.
	while (!feof(sing_test_output)) {
		int r = fread(buf,1,255, sing_test_output);
		if (r < 0) {
			dprintf(D_ALWAYS, "Error %d draining singularity test pipe\n", errno);
		}
	}

	int rc = my_pclose(sing_test_output);
	dprintf(D_ALWAYS, "singularity test returns %d\n", rc);

	if (rc != 0)  {
		dprintf(D_ALWAYS, "Non zero return code %d from singularity test of %s\n", rc, stredArgs.c_str());
		dprintf(D_ALWAYS, "     singularity output was %s\n", errorMessage.c_str());
		return false;
	}

	
	dprintf(D_FULLDEBUG, "Successfully ran singularity test\n");
	return true;
}

// Test to see if this singularity can run an exploded directory
// We'll use the sbin directory, which should have a static linked
// binary, exit_37 in it as the root of the "sandbox dir".
// If we can run this from the sandbox, then singularity should 
// be able to run with sandboxes

bool
Singularity::canRunSIF() {
	std::string libexec_dir;
	param(libexec_dir, "LIBEXEC");
	std::string ignored;
	return Singularity::canRun(libexec_dir + "/exit_37.sif", "/exit_37", ignored);
}

bool
Singularity::canRunSandbox(bool &can_use_pidnamespaces) {
	std::string sandbox_dir;
	param(sandbox_dir, "SINGULARITY_TEST_SANDBOX");
	int sandbox_timeout = param_integer("SINGULARITY_TEST_SANDBOX_TIMEOUT", m_default_timeout);
	std::string ignored;
	bool result = Singularity::canRun(sandbox_dir, "/exit_37", ignored, sandbox_timeout);  // canRun() will also set m_use_pid_namespaces
	can_use_pidnamespaces = m_use_pid_namespaces;
	return result;
}

Singularity::IsSetuid 
Singularity::usesUserNamespaces() {
	std::string sandbox_dir;
	std::string sing_user_ns_str;
	param(sandbox_dir, "SINGULARITY_TEST_SANDBOX");
	int sandbox_timeout = param_integer("SINGULARITY_TEST_SANDBOX_TIMEOUT", m_default_timeout);

	bool result = Singularity::canRun(sandbox_dir, "/get_user_ns", sing_user_ns_str, sandbox_timeout); 
	if (result) {
		uint64_t  sing_user_ns = atoll(sing_user_ns_str.c_str());;
		struct stat buf;
		int r = stat("/proc/self/ns/user", &buf);
		if (r == 0) {
			if (buf.st_ino == sing_user_ns) {
				// If the user namespaces are the same on the outside as the
				// inside, then we are setuid
				return SingSetuid;
			} else {
				return SingUserNamespaces;
			}
		}
	}
	return SingSetuidUnknown; // I guess we'll never know
}

void
Singularity::add_containment_args(ArgList & sing_args)
{
	// By default, we will ideally pass "-C" which tells Singularity to contain
	// everything, which includes file systems, PID, IPC, and environment and whatever
	// they dream up next.
	// However, if we are told to not use PID namespaces, then we cannot pass "-C".
	if (m_use_pid_namespaces) {
		// containerize everything with -C, ie use pid namespaces
		sing_args.AppendArg("-C");
	}
	else {
		// We cannot use pid namespaces, so we cannot use -C to contain everything.
		// Unfortunately, Singulariry does not have a way to just disable pid namespaces.
		// So instead we pass as many other contain flags as we can.
		sing_args.AppendArg("--contain");	//  minimal dev and other dirs
		sing_args.AppendArg("--ipc");		// contain ipc namespace
		sing_args.AppendArg("--cleanenv");
	}
}


bool
Singularity::canRun(const std::string &image, const std::string &command, std::string &firstLine, int timeout) {
#ifdef LINUX
	bool success = true;
	bool retry_on_fail_without_namespaces = false;

	static bool first_run_attempt = true;

	if (first_run_attempt) {
		first_run_attempt = false;
		std::string knob;
		param(knob, "SINGULARITY_USE_PID_NAMESPACES", "auto");
		if (string_is_boolean_param(knob.c_str(), m_use_pid_namespaces)) {
			// SINGULARITY_USE_PID_NAMESPACES is explicitly set to True or False
			retry_on_fail_without_namespaces = false;
		}
		else {
			// SINGULARITY_USE_PID_NAMESPACES is auto, so attempt to use
			// pid namespaces the first time, and try again without on failure
			m_use_pid_namespaces = true;
			retry_on_fail_without_namespaces = true;
		}
	}


	ArgList sandboxArgs;
	std::string exec;
	if (!find_singularity(exec)) {
		return false;
	}

	sandboxArgs.AppendArg(exec);
	sandboxArgs.AppendArg("exec");
	add_containment_args(sandboxArgs);
	sandboxArgs.AppendArg(image);
	sandboxArgs.AppendArg(command);

	std::string displayString;
	sandboxArgs.GetArgsStringForLogging( displayString );
	dprintf(D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str());

	// Note! We need to use PRIV_CONDOR_FINAL here, because Singularity
	// needs to know if it was started with setuid root or not, and it does this
	// not by looking at the filesystem, but by comparing the euid to the ruid.
	// If the euid and ruid are different, it *assumes* setuid root, even if it is not.
	// So we do the tests here as PRIV_CONDOR_FINAL so that the euid and ruid are kept
	// the same so Singularity can correctly deduce if it is setuid or not.
	// If we used PRIV_CONDOR instead of PRIV_CONDOR_FINAL here, Singularity would fail
	// if HTCondor is running as root and Singularity is configured to use user-namespaces.
	TemporaryPrivSentry sentry(PRIV_CONDOR_FINAL);

	MyPopenTimer pgm;
	Env env;
	if (pgm.start_program(sandboxArgs, false /*capture stderr*/, &env, false) < 0) {
		if (pgm.error_code() != 0) {
			dprintf(D_ALWAYS, "Test launch singularity exec failed, this singularity can run some programs, but not these\n");
			success =  false;
		}
	}
	else {
		int exitCode = -1;
		pgm.wait_for_exit(timeout, &exitCode);
		if (WEXITSTATUS(exitCode) != 37) {  // hard coded return from exit_37
			pgm.close_program(1);
			dprintf(D_ALWAYS, "'%s' did not exit successfully (code %d); stderr is :\n",
				displayString.c_str(), exitCode);
			std::string line;
			while (pgm.output().readLine(line, false)) {
				line = RemoveANSIcodes(line);
				chomp(line);
				trim(line);
				if (!line.empty()) {
					dprintf(D_ALWAYS, "[singularity stderr]: %s\n", line.c_str());
					m_lastSingularityErrorLine = line;
				}
			}
			success =  false;
		}
		pgm.output().readLine(firstLine);
	}

	if (success) {
		dprintf(D_ALWAYS, "Successfully ran: '%s'.\n", displayString.c_str());
		return true;
	}

	// If we made it here, we failed to launch Singularity... perhaps retry?
	if (retry_on_fail_without_namespaces && m_use_pid_namespaces) {
		m_use_pid_namespaces = false;
		dprintf(D_ALWAYS, "Singularity exec failed, trying again without pid namespaces\n");
		std::string ignored;
		return canRun(image, "/exit_37", ignored, timeout);	// Ooooh... recursion!  fancy!
	}
	else {
		return false;
	}
#else
	(void)image;	// shut the compiler up
	return false;
#endif
}
