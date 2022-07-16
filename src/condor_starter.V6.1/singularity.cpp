
#include "condor_common.h"

#include "singularity.h"

#ifdef LINUX
#include <sys/wait.h>
#endif

#include <vector>

#include "condor_config.h"
#include "my_popen.h"
#include "CondorError.h"
#include "basename.h"
#include "stat_wrapper.h"
#include "stat_info.h"
#include "condor_attributes.h"

using namespace htcondor;


bool Singularity::m_enabled = false;
bool Singularity::m_probed = false;
bool Singularity::m_apptainer = false;
int  Singularity::m_default_timeout = 120;
std::string Singularity::m_singularity_version;

static bool find_singularity(std::string &exec)
{
#ifdef LINUX
	std::string singularity;
	if (!param(singularity, "SINGULARITY")) {
		dprintf(D_ALWAYS | D_FAILURE, "SINGULARITY is undefined.\n");
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
Singularity::advertise(ClassAd &ad)
{
	if (m_enabled && ad.InsertAttr("HasSingularity", true)) {
		return false;
	}
	if (!m_singularity_version.empty() && ad.InsertAttr("SingularityVersion", m_singularity_version)) {
		return false;
	}
	return true;
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
	dprintf(D_FULLDEBUG, "Attempting to run: '%s %s'.\n", exec.c_str(), displayString.c_str());

	MyPopenTimer pgm;
	if (pgm.start_program(infoArgs, true, NULL, false) < 0) {
		// treat 'file not found' as not really error
		int d_level = D_FULLDEBUG;
		if (pgm.error_code() != ENOENT) {
			d_level = D_ALWAYS | D_FAILURE;
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
	dprintf( D_FULLDEBUG, "[singularity version] %s\n", m_singularity_version.c_str() );
	if (IsFulldebug(D_ALWAYS)) {
		std::string line;
		while (pgm.output().readLine(line, false)) {
			chomp(line);
			dprintf( D_FULLDEBUG, "[singularity info] %s\n", line.c_str() );
		}
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

	jobAd.LookupBool(ATTR_WANT_SIF, wantSIF);
	jobAd.LookupBool(ATTR_WANT_SANDBOX_IMAGE, wantSandbox);

	if (wantSIF || wantSandbox) {
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
		Env &job_env)
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
	if (scratch.length() > 0) {
		StringList scratch_list(scratch.c_str());
		scratch_list.rewind();
		char *next_dir;
		while ( (next_dir=scratch_list.next()) ) {
			if (!*next_dir) {
				scratch_list.deleteCurrent();
				continue;
			}
			sing_args.AppendArg("-S");
			sing_args.AppendArg(next_dir);
		}
	}
	if (job_iwd != execute_dir) {
		sing_args.AppendArg("-B");
		sing_args.AppendArg(job_iwd.c_str());
	}

	sing_args.AppendArg("-W");
	sing_args.AppendArg(execute_dir.c_str());

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

	if (param_eval_string(bind_spec, "SINGULARITY_BIND_EXPR", "SingularityBind", &machineAd, &jobAd)) {
		dprintf(D_FULLDEBUG, "Parsing bind mount specification for singularity: %s\n", bind_spec.c_str());
		StringList binds(bind_spec.c_str());
		binds.rewind();
		char *next_bind;
		while ( (next_bind=binds.next()) ) {
			std::string bind_src_dir(next_bind);
			// BIND exprs can be src:dst:ro 
			size_t colon = bind_src_dir.find(':');
			if (colon != std::string::npos) {
				bind_src_dir = bind_src_dir.substr(0, colon);
			}
			StatWrapper sw(bind_src_dir.c_str());
			sw.Stat();
			if (! sw.IsBufValid()) {
				dprintf(D_ALWAYS, "Skipping invalid singularity bind source directory %s\n", next_bind);
				continue;
			} 

			// Older singularity versions that do not support underlay
			// may require the target directory to exist.  OSG wants
			// to ignore mount requests where this is the case.
			if (param_boolean("SINGULARITY_IGNORE_MISSING_BIND_TARGET", false)) {
				// We an only check this when the image format is a directory
				// That's OK for OSG, that's all they use
				StatInfo si(image.c_str());
				if (si.IsDirectory()) {
					// target dir is after the colon, if it exists
					std::string target_dir;
					char *colon = strchr(next_bind,':');
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
					StatInfo td(abs_target_dir.c_str());
					if (! td.IsDirectory()) {
						dprintf(D_ALWAYS, "Target directory %s does not exist in image, skipping mount\n", abs_target_dir.c_str());
						continue;
					}

				} else {
					dprintf(D_ALWAYS, "Image %s is NOT directory, skipping test for missing bind target for %s\n", image.c_str(), next_bind);
				}
			}
			sing_args.AppendArg("-B");
			sing_args.AppendArg(next_bind);
		}
	}

	if (!param_boolean("SINGULARITY_MOUNT_HOME", false, false, &machineAd, &jobAd)) {
		sing_args.AppendArg("--no-home");
	}

	sing_args.AppendArg("-C");

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

	// if the startd has assigned us a gpu, add --nv to the sing exec
	// arguments to mount the nvidia devices
	std::string assignedGpus;
	machineAd.LookupString("AssignedGPUs", assignedGpus);
	if  (assignedGpus.length() > 0) {
		sing_args.AppendArg("--nv");
	}

	sing_args.AppendArg(image.c_str());

	if (orig_exec_val.length() > 0) {
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
	std::string oldPath;
	job_env.GetEnv("PATH", oldPath);
	job_env.SetEnv("PATH", std::string("/usr/sbin:" + oldPath));

	// If reading an image from a docker hub, store it in the scratch dir
	// when we get AP sandboxes, that would be a better place to store these
	job_env.SetEnv("SINGULARITY_CACHEDIR", execute_dir);
	job_env.SetEnv("SINGULARITY_TEMPDIR", execute_dir);

	Singularity::convertEnv(&job_env);
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
		MyString myValue;
		job_env.GetEnv(name.c_str(), myValue);
		std::string  value = myValue;
		auto index_execute_dir = value.find(execute_dir);
		if (index_execute_dir != std::string::npos) {
			std::string new_name = environmentPrefix() + name;
			job_env.SetEnv(
				new_name.c_str(),
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

		job_env->GetEnv(name.c_str(), value);
		std::string new_name = environmentPrefix() + name;
		// Only copy over the value to the new_name if the new_name
		// does not already exist because perhaps it was already set
		// in retargetEnvs().  Note that 'value' is not touched if
		// GetEnv returns false.
		if (job_env->GetEnv(new_name.c_str(), value) == false) {
			job_env->SetEnv(new_name.c_str(), value);
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
Singularity::runTest(const std::string &JobName, const ArgList &args, int orig_args_len, const Env &env, std::string &errorMessage) {

	TemporaryPrivSentry sentry(PRIV_USER);

	// First replace "exec" with "test"
	ArgList testArgs;

	// The last orig_args_len args are the real exec + its args.  Skip those for the test
	for (int i = 0; i < args.Count() - orig_args_len; i++) {
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
	libexec_dir = param("LIBEXEC");
	return Singularity::canRun(libexec_dir + "/exit_37.sif");
}

bool
Singularity::canRunSandbox() {
	std::string sbin_dir;
	sbin_dir = param("SBIN");
	return Singularity::canRun(sbin_dir);
}

bool 
Singularity::canRun(const std::string &image) {
#ifdef LINUX
	ArgList sandboxArgs;
	std::string exec;
	if (!find_singularity(exec)) {
		return false;
	}
	std::string exit_37 = "/exit_37";

	sandboxArgs.AppendArg(exec);
	sandboxArgs.AppendArg("exec");
	sandboxArgs.AppendArg(image);
	sandboxArgs.AppendArg(exit_37);

	std::string displayString;
	sandboxArgs.GetArgsStringForLogging( displayString );
	dprintf(D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str());

	MyPopenTimer pgm;
	if (pgm.start_program(sandboxArgs, true, NULL, false) < 0) {
		if (pgm.error_code() != 0) {
			dprintf(D_ALWAYS, "Singularity exec of failed, this singularity can run some programs, but not these\n");
			return false;
		}
	}

	int exitCode = -1;
	pgm.wait_for_exit(m_default_timeout, &exitCode);
	if (WEXITSTATUS(exitCode) != 37) {  // hard coded return from exit_37
		pgm.close_program(1);
		std::string line;
		pgm.output().readLine(line, false);
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, line.c_str());
		return false;
	}
	dprintf(D_ALWAYS, "Successfully ran: '%s'.\n", displayString.c_str());
	return true;
#else
	(void)image;	// shut the compiler up
	return false;
#endif
}
