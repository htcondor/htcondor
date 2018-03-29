
#include "condor_common.h"

#include "singularity.h"

#include <vector>

#include "condor_config.h"
#include "my_popen.h"
#include "CondorError.h"
#include "basename.h"

using namespace htcondor;


bool Singularity::m_enabled = false;
bool Singularity::m_probed = false;
int  Singularity::m_default_timeout = 120;
MyString Singularity::m_singularity_version;


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


bool
Singularity::advertise(classad::ClassAd &ad)
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

	MyString displayString;
	infoArgs.GetArgsStringForLogging( & displayString );
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
		MyString line;
		line.readLine(pgm.output(), false); line.chomp();
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, line.c_str());
		err.pushf("Singularity::detect", 2, "'%s' exited with status %d", displayString.c_str(), exitCode);
		return false;
	}

	m_singularity_version.readLine(pgm.output(), false);
	m_singularity_version.chomp();
	dprintf( D_FULLDEBUG, "[singularity version] %s\n", m_singularity_version.c_str() );
	if (IsFulldebug(D_ALWAYS)) {
		MyString line;
		while (line.readLine(pgm.output(), false)) {
			line.readLine(pgm.output(), false);
			line.chomp();
			dprintf( D_FULLDEBUG, "[singularity info] %s\n", line.c_str() );
		}
	}

	m_enabled = ! m_singularity_version.empty();

	return true;
}

bool
Singularity::job_enabled(compat_classad::ClassAd &machineAd, compat_classad::ClassAd &jobAd)
{
	return param_boolean("SINGULARITY_JOB", false, false, &machineAd, &jobAd);
}


Singularity::result
Singularity::setup(ClassAd &machineAd,
		ClassAd &jobAd,
		MyString &exec,
		ArgList &args,
		const std::string &job_iwd,
		const std::string &execute_dir,
		Env &job_env)
{
	if (!param_boolean("SINGULARITY_JOB", false, false, &machineAd, &jobAd)) {return Singularity::DISABLE;}

	if (!enabled()) {
		dprintf(D_ALWAYS, "Singularity job has been requested but singularity does not appear to be configured on this host.\n");
		return Singularity::FAILURE;
	}
	std::string exec_str;
	if (!find_singularity(exec_str)) {
		return Singularity::FAILURE;
	}

	std::string image;
	if (!param_eval_string(image, "SINGULARITY_IMAGE_EXPR", "SingularityImage", &machineAd, &jobAd)) {
		dprintf(D_ALWAYS, "Singularity support was requested but unable to determine the image to use.\n");
		return Singularity::FAILURE;
	}

	std::string target_dir;
	bool has_target = param(target_dir, "SINGULARITY_TARGET_DIR") && !target_dir.empty();

	args.RemoveArg(0);
	std::string orig_exec_val = exec;
	if (has_target && (orig_exec_val.compare(0, execute_dir.length(), execute_dir) == 0)) {
		exec = target_dir + "/" + orig_exec_val.substr(execute_dir.length());
		dprintf(D_FULLDEBUG, "Updated executable path to %s for target directory mode.\n", exec.Value());
	}
	args.InsertArg(exec.Value(), 0);
	exec = exec_str;

	args.InsertArg(image.c_str(), 0);
	args.InsertArg("-C", 0);
	// Bind
	// Mount under scratch
	std::string scratch;
	if (param(scratch, "MOUNT_UNDER_SCRATCH")) {
		StringList scratch_list(scratch.c_str());
		scratch_list.rewind();
		char *next_dir;
		while ( (next_dir=scratch_list.next()) ) {
			if (!*next_dir) {
				scratch_list.deleteCurrent();
				continue;
			}
			args.InsertArg(next_dir, 0);
			args.InsertArg("-S", 0);
		}
	}
	if (job_iwd != execute_dir) {
		args.InsertArg(job_iwd.c_str(), 0);
		args.InsertArg("-B", 0);
	}
	// When overlayfs is unavailable, singularity cannot bind-mount a directory that
	// does not exist in the container.  Hence, we allow a specific fixed target directory
	// to be used instead.
	std::string bind_spec = execute_dir;
	if (has_target) {
		bind_spec += ":";
		bind_spec += target_dir;
		// Only change PWD to our new target dir if that's where we should startup.
		if (job_iwd == execute_dir) {
			args.InsertArg(target_dir.c_str(), 0);
			args.InsertArg("--pwd", 0);
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
		MyString proxy_file;
		if ( job_env.GetEnv( "X509_USER_PROXY", proxy_file ) &&
		     strncmp( execute_dir.c_str(), proxy_file.Value(),
                      execute_dir.length() ) == 0 ) {
			std::string new_proxy = target_dir + "/" + condor_basename( proxy_file.Value() );
			job_env.SetEnv( "X509_USER_PROXY", new_proxy.c_str() );
		}
	}
	args.InsertArg(bind_spec.c_str(), 0);
	args.InsertArg("-B", 0);

	if (param_eval_string(bind_spec, "SINGULARITY_BIND_EXPR", "SingularityBind", &machineAd, &jobAd)) {
		dprintf(D_FULLDEBUG, "Parsing bind mount specification for singularity: %s\n", bind_spec.c_str());
		StringList binds(bind_spec.c_str());
		binds.rewind();
		char *next_bind;
		while ( (next_bind=binds.next()) ) {
			args.InsertArg(next_bind, 0);
			args.InsertArg("-B", 0);
		}
	}

	args.InsertArg("exec", 0);
	args.InsertArg(exec.c_str(), 0);

	MyString args_string;
	args.GetArgsStringForDisplay(&args_string, 1);
	dprintf(D_FULLDEBUG, "Arguments updated for executing with singularity: %s %s\n", exec.Value(), args_string.Value());

	return Singularity::SUCCESS;
}


