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
#include "condor_uid.h"

#include "jic_shadow.h"
#include "staging_directory.h"


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
	s->jic->runPrepareJobHook();
}


void
Starter::requestGuidanceCommandJobSetup(
	Starter * s, const ClassAd & context,
	std::function<void(void)> continue_conversation
) {
	ClassAd guidance;
	ClassAd request;
	request.InsertAttr(ATTR_REQUEST_TYPE, RTYPE_JOB_SETUP);
	request.InsertAttr(ATTR_HAS_COMMON_FILES_TRANSFER, CFT_VERSION);

	// ClassAds assume they own their attribute's value, so (a) you have
	// to copy them to avoid changing them and (b) you can't copy them to
	// locals, because then their destructor will be called twice.  *sigh*
	ClassAd * my_context = new ClassAd(context);
	request.Insert(ATTR_CONTEXT_AD, my_context);

	GuidanceResult rv = GuidanceResult::Invalid;
	if( s->jic->genericRequestGuidance( request, rv, guidance ) ) {
		if( rv == GuidanceResult::Command ) {
			auto lambda = [=] (const ClassAd & c) -> void { requestGuidanceCommandJobSetup(s, c, continue_conversation); };
			if( handleJobSetupCommand( s, guidance, lambda ) ) { return; }
		} else {
			dprintf( D_ALWAYS, "Problem requesting guidance from AP (%d); carrying on.\n", static_cast<int>(rv) );
		}
	}

	// Carry on.
	s->jic->resetInputFileCatalog();
	continue_conversation();
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
			s->jic->runPrepareJobHook();
			return true;
		} else if( command == COMMAND_JOB_SETUP ) {
			ClassAd context;
			context.InsertAttr( ATTR_JOB_ENVIRONMENT_READY, true );

			// We can't just call requestGuidanceSetupJobEnvironment() here,
			// because it assumes that when handleJobSetupCommand() returns
			// false (from COMMAND_CARRY_ON), it should call
			// setupJobEnvironment(), which the starter has already done (and
			// indeed, may be further up the call chain).  That can be hacked
			// around, but because of COMMAND_RETRY_REQUEST, this case
			// must return (back into the event loop) without doing anything
			// else -- implying that the hack would have to also involve
			// changing carrying-on to do something else.
			//
			// Or we could register a zero-second timer call to a variant of
			// requestGuidanceSetupJobEnvironment() that does the right thing
			// (call s->jic->resetInputFileCatalog() and then
			// continue_conversation() before returning.)

			daemonCore->Register_Timer(
				0, 0,
				[=] (int /* timerID */) -> void {
					ClassAd context;
					context.InsertAttr( ATTR_JOB_ENVIRONMENT_READY, true );
					requestGuidanceCommandJobSetup(
						s, context, continue_conversation
					);
				},
				"COMMAND_JOB_SETUP"
			);

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

			ASSERT(s->deferral_tid >= 0);

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


#define REPLY_WITH_ERROR(cmd,code,subcode) \
	{ \
	ClassAd context; \
	context.InsertAttr( ATTR_COMMAND, cmd ); \
	context.InsertAttr( ATTR_RESULT, false ); \
	context.InsertAttr( ATTR_REQUEST_RESULT_CODE, (int)code ); \
	context.InsertAttr( ATTR_REQUEST_RESULT_SUBCODE, (int)subcode ); \
	continue_conversation(context); \
	return true; \
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
		dprintf( D_ZKM, "Received the following guidance: '%s'\n", command.c_str() );

		if( command == COMMAND_CARRY_ON ) {
			dprintf( D_ZKM, "Carrying on according to guidance...\n" );

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
			dprintf( D_ZKM, "Will stage common input files '%s'\n", commonInputFiles.c_str() );

			std::string cifName;
			if(! guidance.LookupString( ATTR_NAME, cifName )) {
				dprintf( D_ALWAYS, "Guidance was malformed (no %s attribute), carrying on.\n", ATTR_NAME );
				return false;
			}
			dprintf( D_ZKM, "Will stage common input files as '%s'\n", cifName.c_str() );

			std::string transferSocket;
			if(! guidance.LookupString( ATTR_TRANSFER_SOCKET, transferSocket )) {
				dprintf( D_ALWAYS, "Guidance was malformed (no %s attribute), carrying on.\n", ATTR_TRANSFER_SOCKET );
				return false;
			}
			dprintf( D_ZKM, "Will connect to transfer socket '%s'\n", transferSocket.c_str() );

			std:: string transferKey;
			if(! guidance.LookupString( ATTR_TRANSFER_KEY, transferKey )) {
				dprintf( D_ALWAYS, "Guidance was malformed (no %s attribute), carrying on.\n", ATTR_TRANSFER_KEY );
				return false;
			}
			// dprintf( D_ZKM, "Will send transfer key '%s'\n", transferKey.c_str() );

			//
			// Create the staging directory.
			//

			std::filesystem::path executeDir( s->GetSlotDir() );
			std::filesystem::path parentDir = executeDir / "staging";

			StagingDirectoryFactory sdf;
			auto staging = sdf.make(parentDir, cifName);
			if(! staging) {
				dprintf( D_ALWAYS, "Failed to make() staging directory, reporting failure.\n" );
				REPLY_WITH_ERROR( COMMAND_STAGE_COMMON_FILES, RequestResult::InternalError, -1 );
			}

			int errorCode = staging->create();
			if( errorCode ) {
				dprintf( D_ALWAYS, "Failed to create() staging directory, reporting failure.\n" );
				REPLY_WITH_ERROR( COMMAND_STAGE_COMMON_FILES, RequestResult::InternalError, errorCode );
			}


			//
			// Transfer the common files.
			//
			ClassAd ftAd( guidance );
			ftAd.Assign( ATTR_JOB_IWD, staging->path().string() );
			// ... blocking, at least for now.
			dprintf( D_ALWAYS, "Starting common files transfer.\n" );
			bool result = s->jic->transferCommonInput( & ftAd );
			dprintf( D_ALWAYS, "Finished common files transfer: %s.\n", result ? "success" : "failure" );

			if( result ) {
				// To help reduce silly mistakes, make the staging directory
				// and its contents suitable for mapping before we actually
				// do the mapping.  This will need to be synchronized with
				// our mapping implementation.
				if(! staging->modify()) {
					dprintf( D_ALWAYS, "Failed to convert %s to a staging directory.\n", staging->path().string().c_str() );
				}
			}

			// We could send a generic event notification here, as we did
			// for diagnostic results, but let's try sending the reply in
			// the next guidance request.
			ClassAd context;
			context.InsertAttr( ATTR_COMMAND, COMMAND_STAGE_COMMON_FILES );
			context.InsertAttr( ATTR_RESULT, result );
			context.InsertAttr( "StagingDir", staging->path().string() );
			continue_conversation(context);
			return true;
		} else if( command == COMMAND_COLOR_SLOT ) {
			//
			// Color the slot.
			//
			classad::ExprTree * e = guidance.Lookup( ATTR_COLOR_AD );
			ClassAd * colorAd = dynamic_cast<ClassAd *>(e);
			if( colorAd == NULL ) {
				dprintf( D_ALWAYS, "Guidance was malformed (no %s attribute), carrying on.\n", ATTR_COLOR_AD );
				return false;
            } else {
				dPrintAd( D_ALWAYS, * colorAd );
			}

			ClassAd replyAd;
			bool success = s->jic->colorSlot( * colorAd, replyAd );
			if(! success) {
				dprintf( D_ALWAYS, "Unable to color slot because of a communications failure.\n" );
			}
			success = false;
			replyAd.LookupBool( ATTR_RESULT, success );
			if(! success) {
				dprintf( D_ALWAYS, "The startd failed to color the slot.\n" );
			}


			//
			// Construct the reply.
			//

			const ClassAd * secretsAd = s->jic->getMachineSecretsAd();
			// If we're not talking to a shadow, then who's guiding us?
			ASSERT(secretsAd != NULL);
			std::string splitClaimID;
			success = secretsAd->LookupString( ATTR_SPLIT_CLAIM_ID, splitClaimID );
			if(! success) {
			    dprintf( D_ALWAYS, "The secrets ad did not contain %s, or it wasn't a string.\n", ATTR_SPLIT_CLAIM_ID );
			}


			// (HTCONDOR-3521)  How much space should be reserved?
			long long sizeOnDiskInMB = 1000;

			ClassAd context;
			context.InsertAttr( ATTR_COMMAND, COMMAND_COLOR_SLOT );
			context.InsertAttr( ATTR_RESULT, success );
			context.InsertAttr( ATTR_SPLIT_CLAIM_ID, splitClaimID );
			context.InsertAttr( ATTR_COMMON_INPUT_FILES_SIZE_MB, sizeOnDiskInMB );
			context.Insert( ATTR_SLOT_AD, s->jic->getMachineAd() );
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

			if( stagingDir.empty() ) {
				// Assume the staging directory is in our machine ad.
				ClassAd * machineAd = s->jic->machClassAd();
				if(! machineAd) {
					dprintf( D_ALWAYS, "s->jic->machClassAd() returned NULL.\n" );

					ClassAd context;
					context.InsertAttr( ATTR_COMMAND, COMMAND_MAP_COMMON_FILES );
					context.InsertAttr( ATTR_RESULT, false );
					continue_conversation(context);
					return true;
				} else {
					// For now, to avoid the color ads colliding and creating
					// new types of exotic subatomic particles, the startd
					// names coloring ads from a slot "colors_of_<slot-name>".
					// We don't know the name of the slot which transferred
					// the common files, so we have to check all of them.
					std::vector< std::pair< std::string, ExprTree *> > components;
					machineAd->GetComponents(components);
					for( auto & [name, value] : components ) {
						if(! starts_with( name, "colors_of_" ) ) { continue; }

						// It's sad, but the least-awful way to extract the
						// `StagingDir` corresponding to the cifName we're mapping
						// is to evaluate a textual ClassAd expression.
						std::string expression;
						formatstr(
							expression,
							// This assumes that there is exactly one matching Name.
							"join(evalInEachContext( ifthenelse( Name == \"%s\", StagingDir, undefined ), %s.CommonCatalogsAd.CommonCatalogsList ))",
							cifName.c_str(),
							name.c_str()
						);

						classad::Value v;
						if( machineAd->EvaluateExpr( expression, v ) ) {
							v.IsStringValue( stagingDir );
						}
					}

					if(! stagingDir.empty()) {
						dprintf( D_ALWAYS, "cxfer: found '%s' -> '%s'\n", cifName.c_str(), stagingDir.c_str() );
					} else {
						dprintf( D_ALWAYS, "Failed to find staging directory for '%s'.\n", cifName.c_str() );

						ClassAd context;
						context.InsertAttr( ATTR_COMMAND, COMMAND_MAP_COMMON_FILES );
						context.InsertAttr( ATTR_RESULT, false );
						continue_conversation(context);
						return true;
					}
				}
			}

			StagingDirectoryFactory sdf;
			auto staging = sdf.make( stagingDir );
			if(! staging) {
				dprintf( D_ALWAYS, "Failed to make() staging directory, reporting failure.\n" );

				ClassAd context;
				context.InsertAttr( ATTR_COMMAND, COMMAND_MAP_COMMON_FILES );
				context.InsertAttr( ATTR_RESULT, false );
				continue_conversation(context);
				return true;
			}

			dprintf( D_ZKM, "Will map common files %s at %s\n", cifName.c_str(), stagingDir.c_str() );
			const bool OUTER = false;
			std::filesystem::path sandbox( s->GetWorkingDir(OUTER) );
			dprintf( D_ALWAYS, "Mapping common files into job's initial working directory...\n" );
			bool result = staging->map( sandbox );

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

			ASSERT(s->deferral_tid >= 0);

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
	ClassAd request;
	request.InsertAttr(ATTR_REQUEST_TYPE, RTYPE_JOB_SETUP);
	request.InsertAttr(ATTR_HAS_COMMON_FILES_TRANSFER, CFT_VERSION);

	// ClassAds assume they own their attribute's value, so (a) you have
	// to copy them to avoid changing them and (b) you can't copy them to
	// locals, because then their destructor will be called twice.  *sigh*
	ClassAd * my_context = new ClassAd(context);
	request.Insert(ATTR_CONTEXT_AD, my_context);

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
