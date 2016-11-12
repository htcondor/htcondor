
#include "condor_common.h"

#include "singularity.h"

#include <vector>

#include "condor_config.h"
#include "my_popen.h"
#include "CondorError.h"

using namespace htcondor;


bool Singularity::m_enabled = false;
bool Singularity::m_probed = false;
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

bool
Singularity::version(std::string &version)
{
	CondorError err;
	if (!detect(err)) {return false;}
	version = m_singularity_version;
	return true;
}

bool
Singularity::detect(CondorError &/*err*/)
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

	FILE * singularityResults = my_popen( infoArgs, "r", 1 , 0, false);
	if (singularityResults == NULL) { 
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return false;
	}   

	// Even if we don't care about the success output, the failure output
	// can be handy for debugging...
	char buffer[1024];
	std::vector< std::string > output;
	while( fgets( buffer, 1024, singularityResults ) != NULL ) { 
		unsigned end = MIN(strlen(buffer) - 1, 1023UL);
		if( buffer[end] == '\n' ) { buffer[end] = '\0'; } 
		output.push_back(buffer);
	}   
	for( unsigned i = 0; i < output.size(); ++i ) { 
		dprintf( D_FULLDEBUG, "[singularity info] %s\n", output[i].c_str() );
	}   

	int exitCode = my_pclose( singularityResults );
	if (exitCode != 0) { 
		dprintf(D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, output[0].c_str() );
		return false;
	}

	m_enabled = true;
	m_singularity_version = output[0];

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


