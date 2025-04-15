/***************************************************************
 *
 * Copyright (C) 2024, Condor Team, Computer Sciences Department,
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

#include "starter.h"

#include <filesystem>

#include "condor_daemon_core.h"
#include "dc_coroutines.h"
#include "condor_mkstemp.h"
#include "condor_base64.h"

#include "jic_shadow.h"

#define SEND_REPLY_AND_CONTINUE_CONVERSATION \
	int ignored = -1; \
	jic->notifyGenericEvent( diagnosticResultAd, ignored ); \
	continue_conversation(); \
	co_return;

condor::cr::void_coroutine
run_diagnostic_reply_and_request_additional_guidance(
  std::string diagnostic, JobInfoCommunicator * jic,
  std::function<void(void)> continue_conversation
) {
	ClassAd diagnosticResultAd;
	diagnosticResultAd.InsertAttr( ATTR_EVENT_TYPE, ETYPE_DIAGNOSTIC_RESULT );
	diagnosticResultAd.InsertAttr( ATTR_DIAGNOSTIC, diagnostic );

	//
	// Run the diagnostic.
	//

	// Like MASTER_SHUTDOWN_<NAME>,  we want EPs to be
	// configurable with a certain set of admin-approved
	// diagnostic scripts.  Obviously, the shadow can't
	// know what any new <NAME> means, but HTCSS will
	// provide a bunch of them.
	std::string diagnostic_knob = "STARTER_DIAGNOSTIC_" + diagnostic;
	std::string diagnostic_path_str;
	if(! param( diagnostic_path_str, diagnostic_knob.c_str() )) {
		dprintf( D_ALWAYS,
			"... diagnostic '%s' not registered in param table (%s).\n",
			diagnostic.c_str(), diagnostic_knob.c_str()
		);

		diagnosticResultAd.InsertAttr( "Result", "Error - Unregistered" );
	} else {
		std::filesystem::path diagnostic_path( diagnostic_path_str );
		if(! std::filesystem::exists(diagnostic_path)) {
			diagnosticResultAd.InsertAttr( "Result", "Error - specified path does not exist" );
		} else {
			//
			// We need to capture the standard output and error streams.   We
			// don't want to use pipes, because then we'd have to register
			// a pipe handler and have it communicate either back here, or
			// extend the AwaitableDeadlineReaper to return the captured
			// contents.  Instead, let's just redirect the streams to a
			// temporary file.
			//
#if defined(WINDOWS)
			int dev_null_fd = open("NUL", O_RDONLY);
#else
			int dev_null_fd = open("/dev/null", O_RDONLY);
#endif /* defined(WINDOWS) */
			if( dev_null_fd == -1) {
				diagnosticResultAd.InsertAttr( "Result", "Error - Unable to open /dev/null" );
				SEND_REPLY_AND_CONTINUE_CONVERSATION;
			}
			char tmpl[] = "XXXXXX";
			int log_file_fd = condor_mkstemp(tmpl);
			if( log_file_fd == -1 ) {
				diagnosticResultAd.InsertAttr( "Result", "Error - Unable to open temporary file" );
				SEND_REPLY_AND_CONTINUE_CONVERSATION;
			}

			int the_redirects[3] = {dev_null_fd, log_file_fd, log_file_fd};

			condor::dc::AwaitableDeadlineReaper logansRun;

			std::vector< std::string > diagnostic_args;
			diagnostic_args.push_back( diagnostic_path.string() );

			Env diagnostic_env;
			// This should include CONDOR_CONFIG.
			diagnostic_env.Import();

			std::string condor_bin;
			param( condor_bin, "BIN" );
			if(! condor_bin.empty()) {
				diagnostic_env.SetEnv("_CONDOR_BIN", condor_bin);
			}

			// Windows doesn't have getppid().
			std::string _condor_starter_pid;
			formatstr( _condor_starter_pid, "%d", getpid() );
			diagnostic_env.SetEnv("_CONDOR_STARTER_PID", _condor_starter_pid);

			OptionalCreateProcessArgs diagnostic_process_opts;
			int spawned_pid = daemonCore->CreateProcessNew(
				diagnostic_path.string(),
				diagnostic_args,
				diagnostic_process_opts
					.reaperID(logansRun.reaper_id())
					.priv(PRIV_USER_FINAL)
					.std(the_redirects)
					.env(& diagnostic_env)
			);

			logansRun.born( spawned_pid, 20 ); // ... seconds of careful research

			while( logansRun.living() ) {
				auto [pid, timed_out, status] = co_await logansRun;
				if( timed_out ) {
					diagnosticResultAd.InsertAttr( "Result", "Error - Timed Out" );
				} else {
					std::string log_bytes;

					size_t bytes_read = 0;
					size_t total_bytes_read = 0;

					const size_t BUFFER_SIZE = 32768;
					char buffer[BUFFER_SIZE];
					if( lseek(log_file_fd, 0, SEEK_SET) == (off_t)-1 ) {
						diagnosticResultAd.InsertAttr( "Result", "Error - failed to lseek() log" );
						diagnosticResultAd.InsertAttr( "ExitStatus", status );
						SEND_REPLY_AND_CONTINUE_CONVERSATION;
					}

					while( (bytes_read = full_read(log_file_fd, (void *)buffer, BUFFER_SIZE)) != 0 ) {
						log_bytes.insert(total_bytes_read, buffer, bytes_read);
						total_bytes_read += bytes_read;
						if( bytes_read != BUFFER_SIZE ) {
							break;
						}
					}


					char * base64Encoded = condor_base64_encode(
						(const unsigned char *)log_bytes.c_str(),
						total_bytes_read, false
					);
					diagnosticResultAd.InsertAttr( "Contents", base64Encoded );
					free( base64Encoded );


					diagnosticResultAd.InsertAttr( "Result", "Completed" );
					diagnosticResultAd.InsertAttr( "ExitStatus", status );
				}
			}
		}
	}


	//
	// Reply.
	//
	SEND_REPLY_AND_CONTINUE_CONVERSATION;
}


condor::cr::void_coroutine
retrySetupJobEnvironment(JobInfoCommunicator * jic) {
	// This is a hack, but setting up `co_yield 0;` to do the right thing
	// would be a lot of work and lack flexibility, plus be a little bit
	// of an abuse of how system is supposed to work.
	condor::dc::AwaitableDeadlineSocket ads;
	ads.deadline( nullptr, 0 );

	// This seems to work, which is moderately terrifying,
	// because it's never previously been called twice.
	jic->setupJobEnvironment();

	co_return;
}


void
Starter::requestGuidanceJobEnvironmentReady( Starter * s ) {
	ClassAd request;
	ClassAd guidance;
	request.InsertAttr(ATTR_REQUEST_TYPE, RTYPE_JOB_ENVIRONMENT);

	GuidanceResult rv = GuidanceResult::Invalid;
	if( s->jic->genericRequestGuidance( request, rv, guidance ) ) {
		if( rv == GuidanceResult::Command ) {
			auto lambda = [=] (void) -> void { requestGuidanceJobEnvironmentReady(s); };
			if( handleJobEnvironmentCommand( s, guidance, lambda ) ) { return; }
		} else {
			dprintf( D_ALWAYS, "Problem requesting guidance from AP (%d); carrying on.\n", static_cast<int>(rv) );
		}
	}

	// Carry on.
	s->jobWaitUntilExecuteTime();
}


bool
Starter::handleJobEnvironmentCommand(
  Starter * s,
  const ClassAd & guidance,
  std::function<void(void)> continue_conversation
) {
	std::string command;
	if(! guidance.LookupString( ATTR_COMMAND, command )) {
		dprintf( D_ALWAYS, "Received guidance but didn't understand it; carrying on.\n" );
		dPrintAd( D_ALWAYS, guidance );

		return false;
	} else {
		dprintf( D_ALWAYS, "Received the following guidance: '%s'\n", command.c_str() );
		if( command == COMMAND_START_JOB ) {
			dprintf( D_ALWAYS, "Starting job as guided...\n" );
			// In Starter::cleanupJobs(), if m_setupStatus is JOB_SHOULD_HOLD,
			// we put the job on hold.  "0" should be a #defined constant,
			// but more importantly, we should think about adding another
			// constant, maybe SHADOW_SAID_GO, that record that setup failed
			// but we proceeded anyway.
			s->m_setupStatus = 0;
			// This schedules a zero-second timer.
			s->jobWaitUntilExecuteTime();
			return true;
		} else if( command == COMMAND_RETRY_TRANSFER ) {
			dprintf( D_ALWAYS, "Retrying transfer as guided...\n" );
			// This schedules a zero-second timer.
			retrySetupJobEnvironment(s->jic);
			return true;
		} else if( command == COMMAND_RUN_DIAGNOSTIC ) {
			std::string diagnostic;
			if(! guidance.LookupString(ATTR_DIAGNOSTIC, diagnostic)) {
				dprintf( D_ALWAYS, "Received guidance 'RunDiagnostic', but could not find a diagnostic to run; carrying on, instead.\n" );

				return false;
			} else {
				dprintf( D_ALWAYS, "Running diagnostic '%s' as guided...\n", diagnostic.c_str() );

				run_diagnostic_reply_and_request_additional_guidance(
					diagnostic, s->jic, continue_conversation
				);

				return true;
			}
		} else if( command == COMMAND_ABORT ) {
			dprintf( D_ALWAYS, "Aborting job as guided...\n" );
			s->deferral_tid = daemonCore->Register_Timer(
				0, 0,
				[=](int timerID) -> void { s->SkipJobs(timerID); },
				"SkipJobs"
			);

			if( s->deferral_tid < 0 ) {
				EXCEPT( "Can't register SkipJob DaemonCore timer" );
			}

			dprintf( D_ALWAYS, "Skipping execution of Job %d.%d because of job environment setup failure.\n",
				s->jic->jobCluster(),
				s->jic->jobProc()
			);

			return true;
		} else if( command == COMMAND_CARRY_ON ) {
			dprintf( D_ALWAYS, "Carrying on according to guidance...\n" );

			return false;
		} else if( command == COMMAND_RETRY_REQUEST ) {
			int retry_delay = 20 /* seconds of careful research */;
			guidance.LookupInteger( ATTR_RETRY_DELAY, retry_delay );

			daemonCore->Register_Timer( retry_delay, 0,
				[continue_conversation](int /* timerID */) -> void { continue_conversation(); },
				"guidance: retry request"
			);

			return true;
		} else {
			dprintf( D_ALWAYS, "Guidance '%s' unknown, carrying on.\n", command.c_str() );

			return false;
		}
	}
}


void
Starter::requestGuidanceJobEnvironmentUnready( Starter * s ) {
	ClassAd request;
	ClassAd guidance;
	request.InsertAttr(ATTR_REQUEST_TYPE, RTYPE_JOB_ENVIRONMENT);

	GuidanceResult rv = GuidanceResult::Invalid;
	if( s->jic->genericRequestGuidance( request, rv, guidance ) ) {
		if( rv == GuidanceResult::Command ) {
			auto lambda = [=] (void) -> void { requestGuidanceJobEnvironmentUnready(s); };
			if( handleJobEnvironmentCommand( s, guidance, lambda ) ) { return; }
		} else {
			dprintf( D_ALWAYS, "Problem requesting guidance from AP (%d); carrying on.\n", static_cast<int>(rv) );
		}
	}

	// Carry on.
	s->skipJobImmediately();
}


//
// Assume ownership is correct, but make each file 0444 and each
// directory (including the root) 0500.
//
// This allows root to hardlink each file into place (root can traverse
// the directory tree even if the starters are running as different OS
// acconts).  Since the file permisisons are 0444, they can be read, but
// the source hardlink can not be deleted (because of ownership or that
// the source directory isn't writable).  Simultaneously, the starter
// will be able to clean up the destination hardlinks as normal.
//
bool
convertToStagingDirectory(
	const std::filesystem::path & location
) {
	using std::filesystem::perms;
	std::error_code ec;


	TemporaryPrivSentry tps(PRIV_USER);

	dprintf( D_ZKM, "convertToStagingDirectory(): begin.\n" );

	if(! std::filesystem::is_directory( location )) {
		dprintf( D_ALWAYS, "convertToStagingDirectory(): '%s' not a directory, aborting.\n", location.string().c_str() );
		return false;
	}

	std::filesystem::permissions(
		location,
		perms::owner_read | perms::owner_exec,
		ec
	);
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "Failed to set permissions on staging directory '%s': %s (%d)\n", location.string().c_str(), ec.message().c_str(), ec.value() );
		return false;
	}

	std::filesystem::recursive_directory_iterator rdi(
		location, {}, ec
	);
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "Failed to construct recursive_directory_iterator(%s): %s (%d)\n", location.string().c_str(), ec.message().c_str(), ec.value() );
		return false;
	}

	for( const auto & entry : rdi ) {
		if( entry.is_directory() ) {
			std::filesystem::permissions(
				entry.path(),
				perms::owner_read | perms::owner_exec,
				ec
			);
			if( ec.value() != 0 ) {
				dprintf( D_ALWAYS, "Failed to set permissions(%s): %s (%d)\n", entry.path().string().c_str(), ec.message().c_str(), ec.value() );
				return false;
			}
			continue;
		}
		std::filesystem::permissions(
			entry.path(),
			perms::owner_read | perms::group_read | perms::others_read,
			ec
		);
		if( ec.value() != 0 ) {
			dprintf( D_ALWAYS, "Failed to set permissions(%s): %s (%d)\n", entry.path().string().c_str(), ec.message().c_str(), ec.value() );
			return false;
		}
	}


	dprintf( D_ZKM, "convertToStagingDirectory(): end.\n" );
	return true;
}


bool
check_permissions(
	const std::filesystem::path & l,
	const std::filesystem::perms p
) {
	std::error_code ec;
	auto status = std::filesystem::status( l, ec );
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "check_permissions(): status(%s) failed: %s (%d)\n", l.string().c_str(), ec.message().c_str(), ec.value() );
		return false;
	}
	auto permissions = status.permissions();
	return permissions == p;
}


bool
mapContentsOfDirectoryInto(
	const std::filesystem::path & location,
	const std::filesystem::path & sandbox
) {
	using std::filesystem::perms;
	std::error_code ec;

	dprintf( D_ZKM, "mapContentsOfDirectoryInto(): begin.\n" );


	TemporaryPrivSentry tps(PRIV_USER);

	if(! std::filesystem::is_directory( sandbox )) {
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' not a directory, aborting.\n", sandbox.string().c_str() );
		return false;
	}

	if(! std::filesystem::is_directory( location )) {
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' not a directory, aborting.\n", location.string().c_str() );
		return false;
	}

	if(! check_permissions( location, perms::owner_read | perms::owner_exec )) {
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
		return false;
	}


	std::filesystem::recursive_directory_iterator rdi(
		location, {}, ec
	);
	if( ec.value() != 0 ) {
		dprintf( D_ALWAYS, "Failed to construct recursive_directory_iterator(%s): %s (%d)\n", location.string().c_str(), ec.message().c_str(), ec.value() );
		return false;
	}

	for( const auto & entry : rdi ) {
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s'\n", entry.path().string().c_str() );
		auto relative_path = entry.path().lexically_relative(location);
		dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s'\n", relative_path.string().c_str() );
		if( entry.is_directory() ) {
			if(! check_permissions( entry, perms::owner_read | perms::owner_exec )) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
				return false;
			}

			std::filesystem::create_directory( sandbox/relative_path, ec );
			if( ec.value() != 0 ) {
				dprintf( D_ALWAYS, "Failed to create_directory(%s): %s (%d)\n", (sandbox/relative_path).string().c_str(), ec.message().c_str(), ec.value() );
				return false;
			}
			continue;
		} else {
			dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): hardlink(%s, %s)\n", (sandbox/relative_path).string().c_str(), entry.path().string().c_str() );

			if(! check_permissions( entry, perms::owner_read | perms::group_read | perms::others_read )) {
				dprintf( D_ALWAYS, "mapContentsOfDirectoryInto(): '%s' has the wrong permissions, aborting.\n", location.string().c_str() );
				return false;
			}

			// If this fails because sandbox/relative_path exists, consider it
			// a success for purposes of the overall success of the mapping:
			// the proc-specific input should "beat" the common input.
			std::filesystem::create_hard_link(
				entry.path(), sandbox/relative_path, ec
			);
			if( ec.value() != 0 ) {
				dprintf( D_ALWAYS, "Failed to create_hard_link(%s, %s): %s (%d)\n", entry.path().string().c_str(), (sandbox/relative_path).string().c_str(), ec.message().c_str(), ec.value() );
				return false;
			}
		}
	}


	dprintf( D_ZKM, "mapContentsOfDirectoryInto(): end.\n" );
	return true;
}


bool
Starter::handleJobSetupCommand(
  Starter * s,
  const ClassAd & guidance,
  std::function<void(const ClassAd & context)> continue_conversation
) {
	using std::filesystem::perms;

	std::string command;
	if(! guidance.LookupString( ATTR_COMMAND, command )) {
		dprintf( D_ALWAYS, "Received guidance but didn't understand it; carrying on.\n" );
		dPrintAd( D_ALWAYS, guidance );

		return false;
	} else {
		dprintf( D_ALWAYS, "Received the following guidance: '%s'\n", command.c_str() );

		if( command == COMMAND_CARRY_ON ) {
			dprintf( D_ALWAYS, "Carrying on according to guidance...\n" );

			return false;
		} else if( command == COMMAND_RETRY_REQUEST ) {
			int retry_delay = 20 /* seconds of careful research */;
			guidance.LookupInteger( ATTR_RETRY_DELAY, retry_delay );

			ClassAd context;
			ClassAd * context_p = NULL;
			context_p = dynamic_cast<ClassAd *>(guidance.Lookup( ATTR_CONTEXT_AD ));
			if( context_p != NULL ) { context.CopyFrom(* context_p); }

			daemonCore->Register_Timer( retry_delay, 0,
				[continue_conversation, context](int /* timerID */) -> void { continue_conversation(context); },
				"guidance: retry request"
			);

			return true;
		} else if( command == COMMAND_STAGE_COMMON_FILES ) {
			// I would like to implement this command in terms of a general
			// capability to conduct file-transfer operations as commanded,
			// rather than as implied by the job ad, but the FileTransfer
			// class is a long way from allowing that to happen.
			//
			// The following may end up being a lie because the shadow has
			// to prepare _its_ side of the FTO ... and the starter side
			// undoubtedly has to match.
			//
			// Instead, the shadow will send a ATTR_TRANSFER_INPUT_FILES value
			// whose entries are the common input files.  The starter is
			// responsible for adding whatever other attributes might be
			// necessary, as well as for creating the staging dirctory
			// (and, when or if it becomes necessary, telling the startd about
			// the staging directory and its size/lifetime/etc).

			std::string commonInputFiles;
			if(! guidance.LookupString( ATTR_COMMON_INPUT_FILES, commonInputFiles )) {
				dprintf( D_ALWAYS, "Guidance was malformed (no %s attribute), carrying on.\n", ATTR_COMMON_INPUT_FILES );
				return false;
			}
			dprintf( D_ALWAYS, "Will stage common input files '%s'\n", commonInputFiles.c_str() );

			std::string cifName;
			if(! guidance.LookupString( ATTR_NAME, cifName )) {
				dprintf( D_ALWAYS, "Guidance was malformed (no %s attribute), carrying on.\n", ATTR_NAME );
				return false;
			}
			dprintf( D_ALWAYS, "Will stage common input files as '%s'\n", cifName.c_str() );

			std::string transferSocket;
			if(! guidance.LookupString( ATTR_TRANSFER_SOCKET, transferSocket )) {
				dprintf( D_ALWAYS, "Guidance was malformed (no %s attribute), carrying on.\n", ATTR_TRANSFER_SOCKET );
				return false;
			}
			dprintf( D_ALWAYS, "Will connect to transfer socket '%s'\n", transferSocket.c_str() );

			std:: string transferKey;
			if(! guidance.LookupString( ATTR_TRANSFER_KEY, transferKey )) {
				dprintf( D_ALWAYS, "Guidance was malformed (no %s attribute), carrying on.\n", ATTR_TRANSFER_KEY );
				return false;
			}
			// dprintf( D_ALWAYS, "Will send transfer key '%s'\n", transferKey.c_str() );

			//
			// Construct the new FileTransfer object.
			//

			std::filesystem::path executeDir( s->GetSlotDir() );
			std::filesystem::path stagingDir = executeDir / "staging";

			{
				TemporaryPrivSentry tps(PRIV_CONDOR);

				std::error_code errorCode;
				std::filesystem::create_directory( stagingDir, errorCode );
				if( errorCode ) {
					dprintf( D_ALWAYS, "Unable to create staging directory, aborting: %s (%d)\n", errorCode.message().c_str(), errorCode.value() );
					return false;
				}

				std::filesystem::permissions(
					stagingDir,
					perms::owner_read | perms::owner_write | perms::owner_exec,
					errorCode
				);
				if( errorCode ) {
					dprintf( D_ALWAYS, "Unable to set permissions on staging directory, aborting: %s (%d).\n", errorCode.message().c_str(), errorCode.value() );
					return false;
				}
			}
			{
				TemporaryPrivSentry tps(PRIV_ROOT);

				int rv = chown( stagingDir.string().c_str(), get_user_uid(), get_user_gid() );
				if( rv != 0 ) {
					dprintf( D_ALWAYS, "Unable change owner of staging directory, aborting: %s (%d)\n", strerror(errno), errno );
					return false;
				}
			}

			ClassAd ftAd( guidance );
			ftAd.Assign( ATTR_JOB_IWD, stagingDir.string() );
			// ... blocking, at least for now.
			bool result = s->jic->transferCommonInput( & ftAd );

			if( result ) {
				// To help reduce silly mistakes, make the staging directory
				// and its contents suitable for mapping before we actually
				// do the mapping.  This will need to be synchronized with
				// our mapping implementation.
				if( convertToStagingDirectory( stagingDir ) ) {
					// We'll need to do this, or something like it, at some
					// point in the future -- probably just telling the
					// startd about it.  When we do, be sure check if it worked.
					// s->jic->setCommonFilesLocation( cifName, stagingDir );
				} else {
					dprintf( D_ALWAYS, "Failed to convert %s to a staging directory.\n", stagingDir.string().c_str() );
				}
			}

			// We could send a generic event notification here, as we did
			// for diagnostic results, but let's try sending the reply in
			// the next guidance request.
			ClassAd context;
			context.InsertAttr( ATTR_COMMAND, COMMAND_STAGE_COMMON_FILES );
			context.InsertAttr( ATTR_RESULT, result );
			context.InsertAttr( "StagingDir", stagingDir.string() );
			continue_conversation(context);
			return true;
		} else if( command == COMMAND_MAP_COMMON_FILES ) {
			std::string cifName;
			if(! guidance.LookupString( ATTR_NAME, cifName )) {
				dprintf( D_ALWAYS, "Guidance was malformed (no %s attribute), carrying on.\n", ATTR_NAME );
				return false;
			}

			// This is a hack: this starter should ask the startd for the cifName.
			std::string stagingDir;
			if(! guidance.LookupString( "StagingDir", stagingDir )) {
				dprintf( D_ALWAYS, "Guidance was malformed (no %s attribute), carrying on.\n", "StagingDir" );
				return false;
			}

			std::filesystem::path location(stagingDir);
			// s->jic->getCommonFilesLocation( cifName, location );

			dprintf( D_ALWAYS, "Will map common files %s at %s\n", cifName.c_str(), location.string().c_str() );
			const bool OUTER = false;
			std::filesystem::path sandbox( s->GetWorkingDir(OUTER) );
			bool result = mapContentsOfDirectoryInto( location, sandbox );

			ClassAd context;
			context.InsertAttr( ATTR_COMMAND, COMMAND_MAP_COMMON_FILES );
			context.InsertAttr( ATTR_RESULT, result );
			continue_conversation(context);
			return true;
		} else if( command == COMMAND_ABORT ) {
			dprintf( D_ALWAYS, "Aborting job as guided...\n" );
			s->deferral_tid = daemonCore->Register_Timer(
				0, 0,
				[=](int timerID) -> void { s->SkipJobs(timerID); },
				"SkipJobs"
			);

			if( s->deferral_tid < 0 ) {
				EXCEPT( "Can't register SkipJob DaemonCore timer" );
			}

			dprintf( D_ALWAYS, "Skipping execution of Job %d.%d because of job setup failure.\n",
				s->jic->jobCluster(),
				s->jic->jobProc()
			);

			// This should return all the way out of Starter::Init() and
			// back into the main event loop, which won't have anything to
			// do except handle the timer we just set, above.
			return true;
		} else {
			dprintf( D_ALWAYS, "Guidance '%s' unknown, carrying on.\n", command.c_str() );

			return false;
		}
	}
}


void
Starter::requestGuidanceSetupJobEnvironment( Starter * s, const ClassAd & context ) {
	ClassAd guidance;
	ClassAd request(context);
	request.InsertAttr(ATTR_REQUEST_TYPE, RTYPE_JOB_SETUP);

	GuidanceResult rv = GuidanceResult::Invalid;
	if( s->jic->genericRequestGuidance( request, rv, guidance ) ) {
		if( rv == GuidanceResult::Command ) {
			auto lambda = [=] (const ClassAd & c) -> void { requestGuidanceSetupJobEnvironment(s, c); };
			if( handleJobSetupCommand( s, guidance, lambda ) ) { return; }
		} else {
			dprintf( D_ALWAYS, "Problem requesting guidance from AP (%d); carrying on.\n", static_cast<int>(rv) );
		}
	}

	// Carry on.
	s->jic->setupJobEnvironment();
}
