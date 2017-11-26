
#include "condor_common.h"

#include "charliecloud.h"

#include <vector>

#include "condor_config.h"
#include "my_popen.h"
#include "CondorError.h"

using namespace htcondor;


bool Charliecloud::m_enabled = false;
bool Charliecloud::m_probed = false;
int  Charliecloud::m_default_timeout = 120;
MyString Charliecloud::m_charliecloud_version;


static bool find_charliecloud(std::string &exec)
{
#ifdef LINUX
	std::string charliecloud;
	if (!param(charliecloud, "CHARLIECLOUD")) {
		dprintf(D_ALWAYS | D_FAILURE, "CHARLIECLOUD is undefined.\n");
		return false;
	}
	exec = charliecloud;
	return true;
#else
	(void) exec;
	return false;
#endif
}


bool
Charliecloud::advertise(classad::ClassAd &ad)
{
	if (m_enabled && ad.InsertAttr("HasCharliecloud", true)) {
		return false;
	}
	if (!m_charliecloud_version.empty() && ad.InsertAttr("CharliecloudVersion", m_charliecloud_version)) {
		return false;
	}
	return true;
}


bool
Charliecloud::enabled()
{
	CondorError err;
	return detect(err);
}

const char *
Charliecloud::version()
{
	CondorError err;
	if (!detect(err)) {return NULL;}
	return m_charliecloud_version.c_str();
}

bool
Charliecloud::detect(CondorError &err)
{
	if (m_probed) {return m_enabled;}

	m_probed = true;
	ArgList infoArgs;
	std::string exec;
	if (!find_charliecloud(exec)) {
		return false;
	}
	infoArgs.AppendArg(exec);
	infoArgs.AppendArg("--version");

	MyString displayString;
	infoArgs.GetArgsStringForLogging( & displayString );
	dprintf(D_FULLDEBUG, "Attempting to run: '%s %s'.\n", exec.c_str(), displayString.c_str());

	MyPopenTimer pgm;
	if (pgm.start_program(infoArgs, true, NULL, true) < 0) {
		// treat 'file not found' as not really error
		int d_level = D_FULLDEBUG;
		if (pgm.error_code() != ENOENT) {
			d_level = D_ALWAYS | D_FAILURE;
			err.pushf("Charliecloud::detect", 1, "Failed to run '%s' errno = %d %s.", displayString.c_str(), pgm.error_code(), pgm.error_str());
		}
		dprintf(d_level, "Failed to run '%s' errno=%d %s.\n", displayString.c_str(), pgm.error_code(), pgm.error_str() );
		return false;
	}

	int exitCode;
	if ( ! pgm.wait_for_exit(m_default_timeout, &exitCode) || exitCode != 0) {
		pgm.close_program(1);
		MyString line;
		line.readLine(pgm.output(), false); line.chomp();
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, line.c_str());
		err.pushf("Charliecloud::detect", 2, "'%s' exited with status %d", displayString.c_str(), exitCode);
		return false;
	}

	m_charliecloud_version.readLine(pgm.output(), false);
	m_charliecloud_version.chomp();
	dprintf( D_FULLDEBUG, "[charliecloud version] %s\n", m_charliecloud_version.c_str() );
	if (IsFulldebug(D_ALWAYS)) {
		MyString line;
		while (line.readLine(pgm.output(), false)) {
			line.readLine(pgm.output(), false);
			line.chomp();
			dprintf( D_FULLDEBUG, "[charliecloud info] %s\n", line.c_str() );
		}
	}

	m_enabled = ! m_charliecloud_version.empty();

	return true;
}

bool
Charliecloud::job_enabled(compat_classad::ClassAd &machineAd, compat_classad::ClassAd &jobAd)
{
	return param_boolean("CHARLIECLOUD_JOB", false, false, &machineAd, &jobAd);
}


Charliecloud::result
Charliecloud::setup(ClassAd &machineAd,
		ClassAd &jobAd,
		MyString &exec,
		ArgList &args,
		const std::string &job_iwd,
		const std::string &execute_dir,
		Env &job_env)
{
	if (!param_boolean("CHARLIECLOUD_JOB", false, false, &machineAd, &jobAd)) {return Charliecloud::DISABLE;}

	if (!enabled()) {
		dprintf(D_ALWAYS, "Charliecloud job has been requested but charliecloud does not appear to be configured on this host.\n");
		return Charliecloud::FAILURE;
	}
	std::string exec_str;
	if (!find_charliecloud(exec_str)) {
		return Charliecloud::FAILURE;
	}

	std::string container;
	if (!param_eval_string(container, "CHARLIECLOUD_CONTAINER_EXPR", "CharliecloudContainer", &machineAd, &jobAd)) {
		dprintf(D_ALWAYS, "Charliecloud support was requested but unable to determine the container to use.\n");
		return Charliecloud::FAILURE;
	}

	std::string target_dir;
	bool has_target = param(target_dir, "CHARLIECLOUD_TARGET_DIR") && !target_dir.empty();

	args.RemoveArg(0);
	std::string orig_exec_val = exec;
	if (has_target && (orig_exec_val.compare(0, execute_dir.length(), execute_dir) == 0)) {
		exec = target_dir + "/" + orig_exec_val.substr(execute_dir.length());
		dprintf(D_FULLDEBUG, "Updated executable path to %s for target directory mode.\n", exec.Value());
	}
	args.InsertArg(exec.Value(), 0);
	exec = exec_str;

	args.InsertArg(container.c_str(), 0);

	// Bind
	if (job_iwd != execute_dir) {
		std::string bind_spec = job_iwd + ":" + job_iwd;
		args.InsertArg(bind_spec.c_str(), 0);
		args.InsertArg("-b", 0);
	}

	if (!param_boolean("CHARLIECLOUD_MOUNT_HOME", true, true, &machineAd, &jobAd)) {
		args.InsertArg("--no-home", 0);
	}

	// Charliecloud in non-setuid root configuration (the default) cannot bind-mount a directory that
	// does not exist in the container.  Hence, we allow a specific fixed target directory
	// to be used instead.
	std::string bind_spec = execute_dir;
	if (has_target) {
		bind_spec += ":";
		bind_spec += target_dir;
		// Only chdir to our new target dir if that's where we should startup.
		if (job_iwd == execute_dir) {
			args.InsertArg(target_dir.c_str(), 0);
			args.InsertArg("--cd", 0);
		}
		// Update the environment variables
		job_env.SetEnv("_CONDOR_SCRATCH_DIR", target_dir.c_str());
		job_env.SetEnv("TEMP", target_dir.c_str());
		job_env.SetEnv("TMP", target_dir.c_str());
		std::string chirp = target_dir + "/.chirp.config";
		std::string machine_ad = target_dir + "/.machine.ad";
		std::string job_ad = target_dir + "/.job.ad";
		job_env.SetEnv("_CONDOR_CHIRP_CONFIG", chirp.c_str());
		job_env.SetEnv("_CONDOR_MACHINE_AD", machine_ad.c_str());
		job_env.SetEnv("_CONDOR_JOB_AD", job_ad.c_str());
	}
	args.InsertArg(bind_spec.c_str(), 0);
	args.InsertArg("-b", 0);

	if (param_eval_string(bind_spec, "CHARLIECLOUD_BIND_EXPR", "CharliecloudBind", &machineAd, &jobAd)) {
		dprintf(D_FULLDEBUG, "Parsing bind mount specification for charliecloud: %s\n", bind_spec.c_str());
		StringList binds(bind_spec.c_str());
		binds.rewind();
		char *next_bind;
		while ( (next_bind=binds.next()) ) {
			args.InsertArg(next_bind, 0);
			args.InsertArg("-b", 0);
		}
	}

	//args.InsertArg("exec", 0);
	args.InsertArg(exec.c_str(), 0);

	MyString args_string;
	args.GetArgsStringForDisplay(&args_string, 1);
	dprintf(D_FULLDEBUG, "Arguments updated for executing with charliecloud: %s %s\n", exec.Value(), args_string.Value());

	return Charliecloud::SUCCESS;
}


