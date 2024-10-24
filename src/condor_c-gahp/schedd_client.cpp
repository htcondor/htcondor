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
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "my_username.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "format_time.h"  // for format_time and friends
#include "daemon.h"
#include "dc_schedd.h"
#include "setenv.h"
#include "globus_utils.h"
#include "PipeBuffer.h"
#include "io_loop.h"
#include "file_transfer.h"

#include <algorithm>
#include "schedd_client.h"
#include "SchedDCommands.h"

// Schedd contact timeout
#define QMGMT_TIMEOUT 15


// How often we contact the schedd (secs)
int contact_schedd_interval = 20;


// What is the subject name of our jobs' GSI proxies? This is taken from
// INITIALIZE_FROM_FILE and is added to GSI_DAEMON_NAME
char *proxySubjectName = NULL;

// Queue for pending commands to schedd
std::vector<SchedDRequest*> command_queue;

// Buffer for reading requests from the IO thread
PipeBuffer request_buffer;



// Queue for command results to be written to our output pipe.
std::vector<std::string *> results_queue;

int RESULT_OUTBOX = -1;
int REQUEST_INBOX = -1;

int checkRequestPipeTid = TIMER_UNSET;
int contactScheddTid = TIMER_UNSET;

char *ScheddAddr = NULL;
char *ScheddPool = NULL;

void flush_results();
void enqueue_result (int req_id, const char ** results, const int argc);

extern char *myUserName;

extern void main_shutdown_graceful();

int
request_pipe_handler(int) {

	std::string* next_line;
	while ((next_line = request_buffer.GetNextLine()) != NULL) {

		dprintf (D_FULLDEBUG, "got work request: %s\n", next_line->c_str());

		Gahp_Args args;

			// Parse the command...
		if (!(parse_gahp_command (next_line->c_str(), &args) &&
			  handle_gahp_command (args.argv, args.argc))) {
			dprintf (D_ALWAYS, "ERROR processing %s\n", next_line->c_str());
		}

			// Clean up...
		delete  next_line;
	}

	// check for an error in GetNextLine
	if (request_buffer.IsError() || request_buffer.IsEOF()) {
		dprintf (D_ALWAYS, "Request pipe closed. Exiting...\n");
		DC_Exit (1);
	}

	return TRUE;
}


void
doContactSchedd(int /* tid */)
{
	if (command_queue.empty()) {
		daemonCore->Reset_Timer( contactScheddTid, contact_schedd_interval ); // Come back in a min
		return;
	}

	dprintf(D_FULLDEBUG,"in doContactSchedd\n");

	int error=FALSE;
	std::string error_msg;
	CondorError errstack;
	bool do_reschedule = false;
	int failure_line_num = 0;
	int failure_errno = 0;
	std::set< std::string, classad::CaseIgnLTStr > filter_attrs;
	std::set< std::string, classad::CaseIgnLTStr > proc_attrs;
	bool rerun_immediately = false;
	int max_file_requests = param_integer("C_GAHP_MAX_FILE_REQUESTS", 10);
	int curr_file_requests = 0;

	// Try connecting to schedd
	DCSchedd dc_schedd ( ScheddAddr, ScheddPool );
	if (dc_schedd.error() || !dc_schedd.locate()) {
		formatstr( error_msg, "Error locating schedd %s", ScheddAddr );

		dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );

		// If you can't connect return "Failure" on every job request
		for (auto current_command: command_queue) {
			if (current_command->status != SchedDRequest::SDCS_NEW)
				continue;

			if (current_command->command == SchedDRequest::SDC_STATUS_CONSTRAINED) {
				const char * result[] = {
					GAHP_RESULT_FAILURE,
					error_msg.c_str(),
					"0"};
				enqueue_result (current_command->request_id, result, 3);
			} else if (current_command->command == SchedDRequest::SDC_SUBMIT_JOB) {
				const char * result[] = {
									GAHP_RESULT_FAILURE,
									NULL,
									error_msg.c_str() };
				enqueue_result (current_command->request_id, result, 3);
			} else if (current_command->command == SchedDRequest::SDC_UPDATE_LEASE) {
				const char * result[] = {
									GAHP_RESULT_FAILURE,
									error_msg.c_str(),
									NULL };
				enqueue_result (current_command->request_id, result, 3);
			} else {
				const char * result[] = {
									GAHP_RESULT_FAILURE,
									error_msg.c_str() };
				enqueue_result (current_command->request_id, result, 2);
			}

			current_command->status = SchedDRequest::SDCS_COMPLETED;
		}
	}

	SchedDRequest::schedd_command_type commands [] = {
		SchedDRequest::SDC_REMOVE_JOB,
		SchedDRequest::SDC_HOLD_JOB,
		SchedDRequest::SDC_RELEASE_JOB };

	const char * command_titles [] = {
		"REMOVE_JOB", "HOLD_JOB", "RELEASE_JOB" };

	// REMOVE
	// HOLD
	// RELEASE
	int i=0;
	while (i<3) {
		
		std::vector<std::string> id_list;
		std::vector<SchedDRequest*> this_batch;

		SchedDRequest::schedd_command_type this_command = commands[i];
		const char * this_action = command_titles[i];
		const char * this_reason = NULL;

		dprintf (D_FULLDEBUG, "Processing %s requests\n", this_action);
		
		error = FALSE;

		// Create a batch of commands with the same command type AND the same reason		
		for (auto current_command: command_queue) {
			if (current_command->status != SchedDRequest::SDCS_NEW)
				continue;

			if (current_command->command != this_command)
				continue;

			if ((this_reason != NULL) && (strcmp (current_command->reason, this_reason) != 0))
				continue;

			if (this_reason == NULL)
				this_reason = current_command->reason;
				
			char job_id_buff[30];
			snprintf (job_id_buff, sizeof(job_id_buff), "%d.%d",
				current_command->cluster_id,
				current_command->proc_id);
			id_list.emplace_back(job_id_buff);

			this_batch.push_back(current_command);
		}

		// If we haven't found any....
		if (id_list.empty()) {
			i++;
			continue;	// ... then try the next command
		}

		// Perform the appropriate command on the current batch
		ClassAd * result_ad= NULL;
		if (this_command == SchedDRequest::SDC_REMOVE_JOB)  {
			errstack.clear();
			result_ad=
				dc_schedd.removeJobs (
					id_list,
					this_reason,
					&errstack);
		} else if (this_command == SchedDRequest::SDC_HOLD_JOB) {
			errstack.clear();
			result_ad=
				dc_schedd.holdJobs (
					id_list,
					this_reason,
					NULL,
			 		&errstack);
		} else if (this_command == SchedDRequest::SDC_RELEASE_JOB)  {
			errstack.clear();
			result_ad=
				dc_schedd.releaseJobs (
					id_list,
					this_reason,
					&errstack);
		} else {
			EXCEPT( "Unexpected command type %d in doContactSchedd",
					this_command );
		}

		// Analyze the result ad
		if (!result_ad) {
			error = TRUE;
			formatstr( error_msg, "Error connecting to schedd %s %s: %s",
					 ScheddAddr, dc_schedd.addr(), errstack.getFullText().c_str() );
		}
		else {
			dPrintAd (D_FULLDEBUG, *result_ad);
			if ( this_command == SchedDRequest::SDC_RELEASE_JOB ) {
				do_reschedule = true;
			}
		}

		// Go through the batch again, and create responses for each request
		for(auto current_command : this_batch) {
			// Check the result
			char job_id_buff[30];
			if (result_ad && (error == FALSE)) {
				snprintf (job_id_buff, sizeof(job_id_buff), "job_%d_%d",
					current_command->cluster_id,
					current_command->proc_id);
				
				int remove_result;
				if (result_ad->LookupInteger (job_id_buff, remove_result)) {
					switch (remove_result) {
						case AR_ERROR:
							error = TRUE;
							error_msg = "General Error";
							break;
						case AR_SUCCESS:
							error = FALSE;
							break;
						case AR_NOT_FOUND:
							error = TRUE;
							error_msg = "Job not found";
							break;
						case AR_BAD_STATUS:
							error = TRUE;
							error_msg = "Bad job status";
							break;
						case AR_ALREADY_DONE:
							error = TRUE;
							error_msg = "Already done";
							break;
						case AR_PERMISSION_DENIED:
							error = TRUE;
							error_msg = "Permission denied";
							break;
						default:
							error = TRUE;
							error_msg = "Unknown Result";
					} // hctiws

				} else {
					error_msg = "Unable to get result";
				} // fi lookup result for job
			} // fi error == FALSE

			if (error) {
				dprintf (D_ALWAYS, "Error (operation: %s) %d.%d: %s\n",
						this_action,
						current_command->cluster_id,
						current_command->proc_id,
						error_msg.c_str());

				const char * result[2];
				result[0] = GAHP_RESULT_FAILURE;
				result[1] = error_msg.c_str();

				enqueue_result (current_command->request_id, result, 2);
			} else {
				dprintf (D_ALWAYS, "Succeess (operation: %s) %d.%d\n",
						this_action,
						current_command->cluster_id,
						current_command->proc_id);

				const char * result[2];
				result[0] = GAHP_RESULT_SUCCESS;
				result[1] = NULL;

				enqueue_result (current_command->request_id, result, 2);
			} // fi error

			// Mark the status
			current_command->status = SchedDRequest::SDCS_COMPLETED;
		} // elihw this_batch

		if ( result_ad ) {
			delete result_ad;
		}
	}

	dprintf (D_FULLDEBUG, "Processing JOB_STAGE_IN requests\n");
	

	// JOB_STAGE_IN
	int MAX_BATCH_SIZE=1; // This should be a config param
	curr_file_requests = 0;

	std::vector<SchedDRequest*> stage_in_batch;
	do {

		stage_in_batch.clear();

		for (auto current_command: command_queue) {
			if (current_command->status != SchedDRequest::SDCS_NEW)
				continue;

			if (current_command->command != SchedDRequest::SDC_JOB_STAGE_IN)
				continue;

			dprintf (D_ALWAYS, "Adding %d.%d to STAGE_IN batch\n", 
					 current_command->cluster_id,
					 current_command->proc_id);

			stage_in_batch.push_back(current_command);
			curr_file_requests++;
			if (stage_in_batch.size() >= (size_t) MAX_BATCH_SIZE || curr_file_requests > max_file_requests)
				break;
		}

		if (stage_in_batch.size() > 0) {
			ClassAd ** array = new ClassAd*[stage_in_batch.size()];
			i=0;
			for (auto current_command: stage_in_batch) {
				array[i++] = current_command->classad;
			}

			error = FALSE;
			errstack.clear();
			if (!dc_schedd.spoolJobFiles( (int) stage_in_batch.size(),
										  array,
										  &errstack )) {
				error = TRUE;
				formatstr( error_msg, "Error sending files to schedd %s: %s", ScheddAddr, errstack.getFullText().c_str() );
				dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
			}
			delete [] array;
  
			for (auto current_command: stage_in_batch) {
				current_command->status = SchedDRequest::SDCS_COMPLETED;

				if (error) {
					const char * result[] = {
						GAHP_RESULT_FAILURE,
						error_msg.c_str() };
					enqueue_result (current_command->request_id, result, 2);

				} else {
					const char * result[] = {
						GAHP_RESULT_SUCCESS,
						NULL };
					enqueue_result (current_command->request_id, result, 2);
				}
			} // elihw (command_queue)
		} // fi has STAGE_IN requests
	} while (stage_in_batch.size() > 0 && curr_file_requests < max_file_requests);

	if (curr_file_requests >= max_file_requests) {
		rerun_immediately = true;
	}

	dprintf (D_FULLDEBUG, "Processing JOB_STAGE_OUT requests\n");
	

	// JOB_STAGE_OUT
	std::vector <SchedDRequest*> stage_out_batch;
	curr_file_requests = 0;

	for (auto current_command: command_queue) {
		if (curr_file_requests >= max_file_requests) break;

		if (current_command->status != SchedDRequest::SDCS_NEW)
			continue;

		if (current_command->command != SchedDRequest::SDC_JOB_STAGE_OUT)
			continue;


		stage_out_batch.push_back(current_command);
		curr_file_requests++;
	}

	if (curr_file_requests >= max_file_requests) {
		rerun_immediately = true;
	}

	if (stage_out_batch.size() > 0) {
		std::string constraint = "";
		int jobsexpected = stage_out_batch.size();
		for (auto current_command : stage_out_batch) {
			formatstr_cat( constraint, "(ClusterId==%d&&ProcId==%d)||",
									current_command->cluster_id,
									current_command->proc_id );
		}
		constraint += "False";

		error = FALSE;
		errstack.clear();
		int jobssent;
		if (!dc_schedd.receiveJobSandbox( constraint.c_str(),
										  &errstack, &jobssent )) {
			error = TRUE;
			formatstr( error_msg, "Error receiving files from schedd %s: %s",
							   ScheddAddr, errstack.getFullText().c_str() );
			dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
		}

		if(error == FALSE && jobssent != jobsexpected) {
			error = TRUE;
			formatstr( error_msg, "Schedd %s didn't send expected files",
					 ScheddAddr );
			dprintf (D_ALWAYS, "Transfered files for %d jobs but got files for %d jobs. (Schedd %s with contraint %s\n", jobsexpected, jobssent, ScheddAddr, constraint.c_str());
		}
  
		for (auto current_command : stage_out_batch) {
			current_command->status = SchedDRequest::SDCS_COMPLETED;

			if (error) {
				const char * result[] = {
								GAHP_RESULT_FAILURE,
								error_msg.c_str() };
				enqueue_result (current_command->request_id, result, 2);

			} else {
				const char * result[] = {
										GAHP_RESULT_SUCCESS,
										NULL };
				enqueue_result (current_command->request_id, result, 2);
			}
		} // elihw (command_queue)
	} // fi has STAGE_OUT requests


	dprintf (D_FULLDEBUG, "Processing JOB_REFRESH_PROXY requests\n");

	CondorVersionInfo ver_info(dc_schedd.version());
	bool delegate_credential;
	if ( ver_info.built_since_version(6,7,19) &&
		 param_boolean( "DELEGATE_JOB_GSI_CREDENTIALS", true ) ) {
		delegate_credential = true;
	} else {
		delegate_credential = false;
	}

	// JOB_REFRESH_PROXY
	curr_file_requests = 0;
	for (auto current_command: command_queue) {
		if (curr_file_requests >= max_file_requests) break;

		if (current_command->status != SchedDRequest::SDCS_NEW)
			continue;

		if (current_command->command != SchedDRequest::SDC_JOB_REFRESH_PROXY)
			continue;

		curr_file_requests++;

		time_t expiration_time = (current_command->proxy_expiration >= 0) ? current_command->proxy_expiration : GetDesiredDelegatedJobCredentialExpiration(current_command->classad);
		time_t result_expiration_time = 0;

		bool result;
		errstack.clear();
		if ( delegate_credential ) {
			result = dc_schedd.delegateGSIcredential( 
												current_command->cluster_id,
												current_command->proc_id,
												current_command->proxy_file,
												expiration_time,
												&result_expiration_time,
												&errstack );

				// Currently, we do not propagate the actual resulting
				// expiration time back to the gridmanager.  We
				// probably should.
		} else {
			result = dc_schedd.updateGSIcredential( 
												current_command->cluster_id,
												current_command->proc_id,
												current_command->proxy_file,
												&errstack );
		}

		current_command->status = SchedDRequest::SDCS_COMPLETED;

		if (result == false) {
			formatstr( error_msg, "Error refreshing proxy to schedd %s: %s",
					 ScheddAddr, errstack.getFullText().c_str() );
			dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );

			const char * result_to_queue[] = {
				GAHP_RESULT_FAILURE,
				error_msg.c_str() };
			enqueue_result (current_command->request_id, result_to_queue, 2);

		} else {
			const char * result_to_queue[] = {
				GAHP_RESULT_SUCCESS,
				NULL };
			enqueue_result (current_command->request_id, result_to_queue, 2);
		}

	}

	if ( curr_file_requests >= max_file_requests ) {
		rerun_immediately = true;
	}

	if ( rerun_immediately ) {
		dprintf(D_FULLDEBUG, "Maxed out file requests, will rerun immediately\n");
	}

	// Now do all the QMGMT transactions
	error = FALSE;

	// Limit the time we spend connected to the schedd
	int interaction_time = param_integer("CGAHP_SCHEDD_INTERACTION_TIME", 5);
	time_t starttime = time(NULL);

	// Try connecting to the queue
	Qmgr_connection * qmgr_connection;

	errstack.clear();
	if ((qmgr_connection = ConnectQ(dc_schedd, QMGMT_TIMEOUT, false, &errstack)) == NULL) {
		error = TRUE;
		formatstr( error_msg, "Error connecting to schedd %s: %s", ScheddAddr,
				   errstack.getFullText().c_str() );
		dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
	} else {
		errno = 0;
		AbortTransaction(); // Just so we can call BeginTransaction() in the loop
		if ( errno == ETIMEDOUT ) {
			failure_line_num = __LINE__;
			failure_errno = errno;
			dprintf( D_ALWAYS, "Network error talking to schedd at line %d in "
					 "doContactSchedd(), probably an authorization failure\n",
					 failure_line_num );
			formatstr( error_msg, "Network error talking to schedd, "
					   "probably an authorization failure" );
			goto contact_schedd_disconnect;
		}
	}


	dprintf (D_FULLDEBUG, "Processing UPDATE_CONSTRAINED/UDATE_JOB requests\n");
	
	// UPDATE_CONSTRAINED
	// UDATE_JOB
	for (auto current_command: command_queue) {
		
		if (current_command->status != SchedDRequest::SDCS_NEW)
			continue;

		if ((current_command->command != SchedDRequest::SDC_UPDATE_CONSTRAINED) &&
			(current_command->command != SchedDRequest::SDC_UPDATE_JOB))
			continue;

		if (qmgr_connection == NULL)
			goto update_report_result;

		if (time(NULL) - starttime > interaction_time) {
			rerun_immediately = true;
			break;
		}

		error = FALSE;
		errno = 0;
		BeginTransaction();
		if ( errno == ETIMEDOUT ) {
			failure_line_num = __LINE__;
			failure_errno = errno;
			goto contact_schedd_disconnect;
		}

		ExprTree *tree;
		const char *lhstr, *rhstr;
		for( auto itr = current_command->classad->begin(); itr != current_command->classad->end(); itr++ ) {

			lhstr = itr->first.c_str();
			tree = itr->second;
			rhstr = ExprTreeToString( tree );
			if( !lhstr || !rhstr) {
				formatstr( error_msg, "ERROR: ClassAd problem in Updating by constraint %s",
												 current_command->constraint );
				dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
				error = TRUE;
			} else {
				if (current_command->command == SchedDRequest::SDC_UPDATE_CONSTRAINED) {
					if( SetAttributeByConstraint(current_command->constraint,
												lhstr,
												rhstr) == -1 ) {
						if ( errno == ETIMEDOUT ) {
							failure_line_num = __LINE__;
							failure_errno = errno;
							goto contact_schedd_disconnect;
						}
						formatstr( error_msg, "ERROR: Failed (errno=%d) to SetAttributeByConstraint %s=%s for constraint %s",
									errno, lhstr, rhstr, current_command->constraint );
						dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
						error = TRUE;
					}
				} else if (current_command->command == SchedDRequest::SDC_UPDATE_JOB) {
					if( SetAttribute(current_command->cluster_id,
											current_command->proc_id,
											lhstr,
											rhstr,
											SetAttribute_NoAck) == -1 ) {
						if ( errno == ETIMEDOUT ) {
							failure_line_num = __LINE__;
							failure_errno = errno;
							goto contact_schedd_disconnect;
						}
						formatstr( error_msg, "ERROR: Failed to SetAttribute() %s=%s for job %d.%d",
										 lhstr, rhstr, current_command->cluster_id,  current_command->proc_id);
						dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
						error = TRUE;
					}
				}
			}

			if (error)
				break;
		} // elihw classad

update_report_result:
		if (error) {
			const char * result[] = {
				GAHP_RESULT_FAILURE,
				error_msg.c_str() };


			//RemoteCommitTransaction();
			enqueue_result (current_command->request_id, result, 2);
			current_command->status = SchedDRequest::SDCS_COMPLETED;
			if ( qmgr_connection != NULL ) {
				errno = 0;
				AbortTransaction();
				if ( errno == ETIMEDOUT ) {
					failure_line_num = __LINE__;
					failure_errno = errno;
					goto contact_schedd_disconnect;
				}
			}
		} else {
			if ( RemoteCommitTransaction() < 0 ) {
				// We assume the preceeding SetAttribute() with NoAck
				// is what really failed. Mark this command as failed
				// and jump to the end (since the schedd has closed
				// the connection). Any subsequent commands will be
				// tried the next time we come through.
				error_msg =  "ERROR: Failed to set attribute";
				const char * result[] = {
					GAHP_RESULT_FAILURE,
					error_msg.c_str() };
				enqueue_result (current_command->request_id, result, 2);
				current_command->status = SchedDRequest::SDCS_COMPLETED;
				rerun_immediately = true;
				goto contact_schedd_disconnect;
			}
			const char * result[] = {
				GAHP_RESULT_SUCCESS,
				NULL };
			enqueue_result (current_command->request_id, result, 2);
			current_command->status = SchedDRequest::SDCS_COMPLETED;
		} // fi

	} // elihw

	
	dprintf (D_FULLDEBUG, "Processing UPDATE_LEASE requests\n");

	// UPDATE_LEASE
	for (auto current_command: command_queue) {
		if (current_command->status != SchedDRequest::SDCS_NEW)
			continue;

		if (current_command->command != SchedDRequest::SDC_UPDATE_LEASE)
			continue;

		if (time(NULL) - starttime > interaction_time) {
			rerun_immediately = true;
			break;
		}

		std::string success_job_ids="";
		if (qmgr_connection != NULL) {
			error = FALSE;
			errno = 0;
			BeginTransaction();
			if ( errno == ETIMEDOUT ) {
				failure_line_num = __LINE__;
				failure_errno = errno;
				goto contact_schedd_disconnect;
			}
		
			for (i=0; i<current_command->num_jobs; i++) {
			
				time_t time_now = time(NULL);
				int duration = 
					current_command->expirations[i].expiration - time_now;

				dprintf (D_FULLDEBUG, 
						 "Job %d.%d SetTimerAttribute=%d\n",
						 current_command->expirations[i].cluster,
						 current_command->expirations[i].proc,
						 duration);
		
				if (SetTimerAttribute (current_command->expirations[i].cluster,
									   current_command->expirations[i].proc,
									   ATTR_TIMER_REMOVE_CHECK,
									   duration) < 0) {

					if ( errno == ETIMEDOUT ) {
						failure_line_num = __LINE__;
						failure_errno = errno;
						goto contact_schedd_disconnect;
					}
					dprintf (D_ALWAYS, 
							 "Unable to SetTimerAttribute(%d, %d), errno=%d\n",
							 current_command->expirations[i].cluster,
							 current_command->expirations[i].proc,
							 errno);
						 
				} else {
						// Append job id to the result line
					if (success_job_ids.length() > 0)
						success_job_ids += ",";

					formatstr_cat( success_job_ids,
						"%d.%d",
						current_command->expirations[i].cluster,
						current_command->expirations[i].proc);
				}
			} //rof jobs for request
		} // fi error


		if (error) {
			const char * result[] = {
				GAHP_RESULT_FAILURE,
				error_msg.c_str(),
				NULL
			};


			//RemoteCommitTransaction();
			enqueue_result (current_command->request_id, result, 3);
			current_command->status = SchedDRequest::SDCS_COMPLETED;
			if ( qmgr_connection != NULL ) {
				errno = 0;
				AbortTransaction();
				if ( errno == ETIMEDOUT ) {
					failure_line_num = __LINE__;
					failure_errno = errno;
					goto contact_schedd_disconnect;
				}
			}
		} else {
			if ( RemoteCommitTransaction() < 0 ) {
				failure_line_num = __LINE__;
				failure_errno = errno;
				goto contact_schedd_disconnect;
			}
			const char * result[] = {
				GAHP_RESULT_SUCCESS,
				NULL,
				success_job_ids.length()?success_job_ids.c_str():NULL
			};
			enqueue_result (current_command->request_id, result, 3);
			current_command->status = SchedDRequest::SDCS_COMPLETED;
		} // fi

	} // elihw UPDATE_LEASE requests

	dprintf (D_FULLDEBUG, "Processing SUBMIT_JOB requests\n");

	// Starting in 8.5.8, schedd clients can't set X509-related attributes
	// other than the name of the proxy file.
	if ( ver_info.built_since_version(8,5,8) ) {
		 filter_attrs.insert( ATTR_X509_USER_PROXY_SUBJECT );
		 filter_attrs.insert( ATTR_X509_USER_PROXY_EXPIRATION );
		 filter_attrs.insert( ATTR_X509_USER_PROXY_EMAIL );
		 filter_attrs.insert( ATTR_X509_USER_PROXY_VONAME );
		 filter_attrs.insert( ATTR_X509_USER_PROXY_FIRST_FQAN );
		 filter_attrs.insert( ATTR_X509_USER_PROXY_FQAN );
	}
	filter_attrs.insert( ATTR_TOKEN_SUBJECT );
	filter_attrs.insert( ATTR_TOKEN_ISSUER );
	filter_attrs.insert( ATTR_TOKEN_GROUPS );
	filter_attrs.insert( ATTR_TOKEN_SCOPES );
	filter_attrs.insert( ATTR_TOKEN_ID );

	proc_attrs.insert(ATTR_PROC_ID);
	proc_attrs.insert(ATTR_JOB_STATUS);

	// SUBMIT_JOB
	for (auto current_command: command_queue) {
		if (current_command->status != SchedDRequest::SDCS_NEW)
			continue;

		if (current_command->command != SchedDRequest::SDC_SUBMIT_JOB)
			continue;

		if (time(NULL) - starttime > interaction_time) {
			rerun_immediately = true;
			break;
		}

		int ClusterId = -1;
		int ProcId = -1;
		bool put_in_proc = false;

		if (qmgr_connection == NULL) {
			error = TRUE;
			goto submit_report_result;
		}

		errno = 0;
		BeginTransaction();
		if ( errno == ETIMEDOUT ) {
			failure_line_num = __LINE__;
			failure_errno = errno;
			goto contact_schedd_disconnect;
		}
		error = FALSE;

		if ((ClusterId = NewCluster()) >= 0) {
			ProcId = NewProc (ClusterId);
		}
		if ( errno == ETIMEDOUT ) {
			failure_line_num = __LINE__;
			failure_errno = errno;
			goto contact_schedd_disconnect;
		}

		if ( ClusterId < 0 ) {
			error = TRUE;
			error_msg = "Unable to create a new job cluster";
			dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
		} else if ( ProcId < 0 ) {
			error = TRUE;
			error_msg = "Unable to create a new job proc";
			dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
		}
		if ( ClusterId == -2 || ProcId == -2 ) {
			error = TRUE;
			error_msg =
				"Number of submitted jobs would exceed MAX_JOBS_SUBMITTED\n";
			dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
		}
		if( ProcId == -3 ) {
			error = TRUE;
			error_msg =
				"Number of submitted jobs would exceed MAX_JOBS_PER_OWNER\n";
			dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
		}
		if( ProcId == -4 ) {
			error = TRUE;
			error_msg =
				"Number of submitted jobs would exceed MAX_JOBS_PER_SUBMISSION\n";
			dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
		}


		if( error == FALSE ) {
				// See the comment in the function body of ExpandInputFileList
				// for an explanation of what is going on here.
			std::string transfer_input_error_msg;
			if( !FileTransfer::ExpandInputFileList( current_command->classad, transfer_input_error_msg ) ) {
				dprintf( D_ALWAYS, "%s\n", transfer_input_error_msg.c_str() );
				error = TRUE;
			}
		}

		if ( error == FALSE ) {
			current_command->classad->Assign(ATTR_CLUSTER_ID, ClusterId);
			current_command->classad->Assign(ATTR_PROC_ID, ProcId);

			// Special case for the job lease
			time_t expire_time;
			if ( current_command->classad->LookupInteger( ATTR_TIMER_REMOVE_CHECK, expire_time ) ) {
				if ( SetTimerAttribute( ClusterId, ProcId,
										ATTR_TIMER_REMOVE_CHECK,
										expire_time - time(NULL) ) == -1 ) {
					if ( errno == ETIMEDOUT ) {
						failure_line_num = __LINE__;
						failure_errno = errno;
						goto contact_schedd_disconnect;
					}
					formatstr( error_msg, "ERROR: Failed to SetTimerAttribute %s=%ld for job %d.%d",
							 ATTR_TIMER_REMOVE_CHECK, expire_time - time(NULL), ClusterId, ProcId );
					dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
					error = TRUE;
					goto submit_report_result;
				}
				current_command->classad->Delete( ATTR_TIMER_REMOVE_CHECK );
			}

			// Set all the classad attribute on the remote classad
			ExprTree *tree;
			const char *lhstr, *rhstr;
			for( auto itr = current_command->classad->begin(); itr != current_command->classad->end(); itr++ ) {
				lhstr = itr->first.c_str();
				tree = itr->second;

				if ( filter_attrs.find( lhstr ) != filter_attrs.end() ) {
					continue;
				}
				put_in_proc = proc_attrs.find(lhstr) != proc_attrs.end();
				rhstr = ExprTreeToString( tree );
				if( !lhstr || !rhstr) {
					formatstr( error_msg, "ERROR: ClassAd problem in Updating by constraint %s",
												 current_command->constraint );
					dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
					error = TRUE;
				} else if( SetAttribute (ClusterId, put_in_proc ? ProcId : -1,
											lhstr,
											rhstr,
											SetAttribute_NoAck) == -1 ) {
					if ( errno == ETIMEDOUT ) {
						failure_line_num = __LINE__;
						failure_errno = errno;
						goto contact_schedd_disconnect;
					}
					formatstr( error_msg, "ERROR: Failed to SetAttribute %s=%s for job %d.%d",
									 lhstr, rhstr, ClusterId, ProcId );
					dprintf( D_ALWAYS, "%s\n", error_msg.c_str() );
					error = TRUE;
				}

				if (error) break;
			} // elihw classad
		} // fi error==FALSE

submit_report_result:
		char job_id_buff[30];
		snprintf (job_id_buff, sizeof(job_id_buff), "%d.%d", ClusterId, ProcId);

		if (error) {
			const char * result[] = {
								GAHP_RESULT_FAILURE,
								job_id_buff,
								error_msg.c_str() };
			enqueue_result (current_command->request_id, result, 3);
			if ( qmgr_connection != NULL ) {
				errno = 0;
				AbortTransaction();
				if ( errno == ETIMEDOUT ) {
					failure_line_num = __LINE__;
					failure_errno = errno;
					goto contact_schedd_disconnect;
				}
			}
			current_command->status = SchedDRequest::SDCS_COMPLETED;
		} else {
			CondorError errstack;
			if ( RemoteCommitTransaction(0, &errstack) < 0 ) {
				// We assume the preceeding SetAttribute() with NoAck
				// is what really failed. Mark this command as failed
				// and jump to the end (since the schedd has closed
				// the connection). Any subsequent commands will be
				// tried the next time we come through.
				error_msg =  "ERROR: Failed to submit job";
				if (errstack.subsys())
				{
					error_msg += ".  " + errstack.getFullText();
				}
				const char * result[] = {
					GAHP_RESULT_FAILURE,
					job_id_buff,
					error_msg.c_str() };
				enqueue_result (current_command->request_id, result, 3);
				current_command->status = SchedDRequest::SDCS_COMPLETED;
				rerun_immediately = true;
				goto contact_schedd_disconnect;
			}
			const char * result[] = {
									GAHP_RESULT_SUCCESS,
									job_id_buff,
									NULL };
			enqueue_result (current_command->request_id, result, 3);
			current_command->status = SchedDRequest::SDCS_COMPLETED;
		}
	} // elihw


	dprintf (D_FULLDEBUG, "Processing STATUS_CONSTRAINED requests\n");
		
	// STATUS_CONSTRAINED
	if (qmgr_connection != NULL)
	{
		DisconnectQ(qmgr_connection, FALSE);
		if ((qmgr_connection = ConnectQ(dc_schedd, QMGMT_TIMEOUT, true, &errstack )) == NULL)
		{
			formatstr(error_msg, "Error connecting to schedd %s for read-only commands: %s", ScheddAddr, errstack.getFullText().c_str());
			dprintf(D_ALWAYS, "%s\n", error_msg.c_str());
			failure_line_num = __LINE__;
			failure_errno = errno;
			goto contact_schedd_disconnect;
		}
	}
	for (auto current_command: command_queue) {
		if (current_command->status != SchedDRequest::SDCS_NEW)
			continue;

		if (current_command->command != SchedDRequest::SDC_STATUS_CONSTRAINED)
			continue;

		if (time(NULL) - starttime > interaction_time) {
			rerun_immediately = true;
			break;
		}

		if (qmgr_connection != nullptr) {
			std::vector<std::string *> matching_ads;

			ClassAd *next_ad;
			ClassAdList adlist;
				// Only use GetAllJobsByConstraint if remote schedd is
				// 6.9.5 or newer.  Previous versions either did not
				// support this call, or they closed the Qmgmt connection
				// as a side-effect of this call.
			if( ver_info.built_since_version(6,9,5) ) {
				dprintf( D_FULLDEBUG, "Calling GetAllJobsByConstraint(%s)\n",
						 current_command->constraint );
					// NOTE: this could be made more efficient if we knew
					// the list of attributes to query.  For lack of that,
					// we just get all attributes.
				GetAllJobsByConstraint( current_command->constraint, "", adlist);
			}
			else {
					// This is the old latency-prone method.
				dprintf( D_FULLDEBUG, "Calling GetNextJobByConstraint(%s)\n",
						 current_command->constraint );
				next_ad = GetNextJobByConstraint( current_command->constraint, 1 );
				while( next_ad != NULL ) {
					adlist.Insert( next_ad );
					next_ad = GetNextJobByConstraint( current_command->constraint, 0 );
				}
			}

				// NOTE: ClassAdList will deallocate the ClassAds in it

			adlist.Rewind();
			while( (next_ad=adlist.Next()) ) {
				std::string * da_buffer = new std::string();	// Use a ptr to avoid excessive copying
				classad::ClassAdUnParser unparser;
				unparser.Unparse( *da_buffer, next_ad );
				matching_ads.push_back(da_buffer);
			}
			if ( errno == ETIMEDOUT ) {
				failure_line_num = __LINE__;
				failure_errno = errno;
				goto contact_schedd_disconnect;
			}

			// now output this list of classads into a result
			const char ** result  = new const char* [matching_ads.size() + 3];
			ASSERT(result);

			std::string _ad_count;
			formatstr( _ad_count, "%zu", matching_ads.size() );

			int count=0;
			result[count++] = GAHP_RESULT_SUCCESS;
			result[count++] = NULL;
			result[count++] = _ad_count.c_str();

			for (auto next_string : matching_ads) {
				result[count++] = next_string->c_str();
			}

			enqueue_result (current_command->request_id, result, count);
			current_command->status = SchedDRequest::SDCS_COMPLETED;

			// Cleanup
			for (auto next_string : matching_ads) { 
				delete next_string;
			}
			//CommitTransaction();
			delete [] result;
		}
		else {
			const char * result[] = {
				GAHP_RESULT_FAILURE,
				error_msg.c_str(),
				"0" };
			//RemoteCommitTransaction();
			enqueue_result (current_command->request_id, result, 3);
			current_command->status = SchedDRequest::SDCS_COMPLETED;
		}
	}	//elihw

	
 contact_schedd_disconnect:
	if ( qmgr_connection != NULL ) {
		DisconnectQ (qmgr_connection, FALSE);
	}

	if ( failure_line_num ) {
			// We had an error talking to the schedd. Take all of our
			// incomplete commands and mark them as failed.
			// TODO Consider retrying these commands, rather than
			//   immediately marking them as failed.
		if ( error_msg.empty() ) {
			if ( failure_errno == ETIMEDOUT ) {
				dprintf( D_ALWAYS, "Network error talking to schedd at line %d in "
						 "doContactSchedd()\n", failure_line_num );
				formatstr( error_msg, "Network error talking to schedd" );
			} else {
				dprintf( D_ALWAYS, "Error talking to schedd at line %d in "
						 "doContactSchedd(), errno=%d (%s)\n", failure_line_num,
						 failure_errno, strerror(failure_errno) );
				formatstr( error_msg, "Error talking to schedd" );
			}
		}
		for (auto current_command: command_queue) {
			if ( current_command->status != SchedDRequest::SDCS_NEW ) {
				continue;
			}
			switch( current_command->command ) {
			case SchedDRequest::SDC_UPDATE_JOB:
			case SchedDRequest::SDC_UPDATE_CONSTRAINED:
			{
				const char *result[2] = { GAHP_RESULT_FAILURE, error_msg.c_str() };
				enqueue_result (current_command->request_id, result, 2);
				current_command->status = SchedDRequest::SDCS_COMPLETED;
			}
				break;
			case SchedDRequest::SDC_UPDATE_LEASE:
			{
				const char *result[3] = { GAHP_RESULT_FAILURE, error_msg.c_str(), NULL };
				enqueue_result (current_command->request_id, result, 3);
				current_command->status = SchedDRequest::SDCS_COMPLETED;
			}
				break;
			case SchedDRequest::SDC_SUBMIT_JOB:
			{
				const char *result[3] = { GAHP_RESULT_FAILURE, "-1.-1", error_msg.c_str() };
				enqueue_result (current_command->request_id, result, 3);
				current_command->status = SchedDRequest::SDCS_COMPLETED;
			}
				break;
			case SchedDRequest::SDC_STATUS_CONSTRAINED:
			{
				const char *result[3] = { GAHP_RESULT_FAILURE, error_msg.c_str(), "0" };
				enqueue_result (current_command->request_id, result, 3);
				current_command->status = SchedDRequest::SDCS_COMPLETED;
			}
				break;
			default:
					// Do nothing
				;
			}
		}
	}

	if ( do_reschedule ) {
		dc_schedd.reschedule();
	}

		// Write all of our results to our parent.
	flush_results();

	dprintf (D_FULLDEBUG, "Finishing doContactSchedd()\n");

	auto delete_completed = [](SchedDRequest *req) {
		if (req->status == SchedDRequest::SDCS_COMPLETED) {
			delete req;
			return true;
		} else {
			return false;
		}
	};

	// Clean up the list
	auto last = std::remove_if(command_queue.begin(), command_queue.end(), delete_completed);
	command_queue.erase(last, command_queue.end());

	dprintf (D_FULLDEBUG, "Schedd interaction took %ld seconds.\n", time(nullptr)-starttime);
	if (rerun_immediately) {
		dprintf (D_FULLDEBUG, "Schedd interaction time hit limit; will retry immediately.\n");
	}

	// Come back soon..
	// QUESTION: Should this always be a fixed time period?
	daemonCore->Reset_Timer( contactScheddTid, rerun_immediately ? 1 : contact_schedd_interval );
}


int
handle_gahp_command(char ** argv, int argc) {
	// Assume it's been verified

	if (strcasecmp (argv[0], GAHP_COMMAND_JOB_REMOVE)==0) {
		int req_id = 0;
		int cluster_id, proc_id;

		if (!(argc == 5 &&
			get_int (argv[1], &req_id) &&
			get_job_id (argv[3], &cluster_id, &proc_id))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		enqueue_command (
			SchedDRequest::createRemoveRequest(
				req_id,
				cluster_id,
				proc_id,
				argv[4]));
		return TRUE;
	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_HOLD)==0) {
		int req_id = 0;
		int cluster_id, proc_id;

		if (!(argc == 5 &&
			get_int (argv[1], &req_id) &&
			get_job_id (argv[3], &cluster_id, &proc_id))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		enqueue_command (
			SchedDRequest::createHoldRequest(
				req_id,
				cluster_id,
				proc_id,
				argv[4]));
		return TRUE;

	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_RELEASE)==0) {

		int req_id = 0;
		int cluster_id, proc_id;

		if (!(argc == 5 &&
			get_int (argv[1], &req_id) &&
			get_job_id (argv[3], &cluster_id, &proc_id))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		enqueue_command (
			SchedDRequest::createReleaseRequest(
				req_id,
				cluster_id,
				proc_id,
				argv[4]));
		return TRUE;

	}  else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_STATUS_CONSTRAINED) ==0) {

		int req_id;

		if (!(argc == 4 &&
			get_int (argv[1], &req_id))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		char * constraint = argv[3];


		enqueue_command (
			SchedDRequest::createStatusConstrainedRequest(
				req_id,
				constraint));

		return TRUE;
	}  else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_UPDATE_CONSTRAINED) ==0) {
		int req_id;
		ClassAd * classad;

		if (!(argc == 5 &&
			get_int (argv[1], &req_id) &&
			get_class_ad (argv[4], &classad))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		char * constraint = argv[3];

		enqueue_command (
			SchedDRequest::createUpdateConstrainedRequest(
				req_id,
				constraint,
				classad));

		delete classad;
		return TRUE;

	}  else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_UPDATE) ==0) {

		int req_id;
		ClassAd * classad;
		int cluster_id, proc_id;

		if (!(argc == 5 &&
			get_int (argv[1], &req_id) &&
			get_job_id (argv[3], &cluster_id, &proc_id) &&
			get_class_ad (argv[4], &classad))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		//char * constraint = argv[3];

		enqueue_command (
			SchedDRequest::createUpdateRequest(
				req_id,
				cluster_id,
				proc_id,
				classad));

		delete classad;
		return TRUE;
	}  else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_SUBMIT) ==0) {
		int req_id;
		ClassAd * classad;

		if (!(argc == 4 &&
			get_int (argv[1], &req_id) &&
			get_class_ad (argv[3], &classad))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		enqueue_command (
			SchedDRequest::createSubmitRequest(
				req_id,
				classad));

		delete classad;
		return TRUE;
	}  else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_UPDATE_LEASE) ==0) {
		int req_id;
		int num_jobs;

		if (!(argc >= 4 &&
			get_int (argv[1], &req_id) &&
			get_int (argv[3], &num_jobs))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		job_expiration * expirations = new job_expiration[num_jobs];
		int i;
		for (i=0; i<num_jobs; i++) {
			if (!get_job_id(argv[4+i*2], 
							&(expirations[i].cluster),
							&(expirations[i].proc))) {
				delete[] expirations;
				return FALSE;
			}

			if (!get_ulong (argv[4+i*2+1], &(expirations[i].expiration))) {
				delete [] expirations;
				return FALSE;
			}
		}	

		enqueue_command (
			SchedDRequest::createUpdateLeaseRequest(
													req_id,
													num_jobs,
													expirations));

		delete [] expirations;
		return TRUE;
	}  else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_STAGE_IN) ==0) {
		int req_id;
		ClassAd * classad;

		if (!(argc == 4 &&
			get_int (argv[1], &req_id) &&
			get_class_ad (argv[3], &classad))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		enqueue_command (
			SchedDRequest::createJobStageInRequest(
				req_id,
				classad));

		delete classad;
		return TRUE;
	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_STAGE_OUT) ==0) {
		int req_id;
		int cluster_id, proc_id;

		if (!(argc == 4 &&
			  get_int (argv[1], &req_id) &&
			  get_job_id (argv[3], &cluster_id, &proc_id))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		enqueue_command (
			SchedDRequest::createJobStageOutRequest(
				req_id,
				cluster_id,
				proc_id));

		return TRUE;
	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_REFRESH_PROXY)==0) {
		int req_id = 0;
		int cluster_id, proc_id;
		int proxy_expire = -1;

		if (!((argc == 5 ||argc == 6) &&
			get_int (argv[1], &req_id) &&
			get_job_id (argv[3], &cluster_id, &proc_id) &&
			(argc == 5 || get_int(argv[5], &proxy_expire)))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		enqueue_command (
			SchedDRequest::createRefreshProxyRequest(
				req_id,
				cluster_id,
				proc_id,
				argv[4],
				proxy_expire));
		return TRUE;
	} else if (strcasecmp (argv[0], GAHP_COMMAND_INITIALIZE_FROM_FILE)==0) {
		static bool init_done = false;

		if ( init_done == false ) {
			SetEnv( "X509_USER_PROXY", argv[1] );
			UnsetEnv( "X509_USER_CERT" );
			UnsetEnv( "X509_USER_KEY" );
			proxySubjectName = x509_proxy_identity_name( argv[1] );
			if ( !proxySubjectName ) {
				dprintf( D_ALWAYS, "Failed to query certificate identity "
						 "from %s\n",  argv[1] );
				return TRUE;
			}
			Reconfig();
			init_done = true;
		}
		return TRUE;
	} else if (!strcasecmp(argv[0], GAHP_COMMAND_UPDATE_TOKEN_FROM_FILE)) {
		if (argc != 2) {
			dprintf(D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return false;
		}
		param_insert("SCITOKENS_FILE", argv[1]);
		return true;
	}

	dprintf (D_ALWAYS, "Invalid command %s\n", argv[0]);
	return FALSE;
}




// String -> int
int
get_int (const char * blah, int * s) {
	*s = atoi(blah);
	return TRUE;
}

int
get_ulong (const char * blah, unsigned long * s) {
	*s=(unsigned long)atol(blah);
	return TRUE;
}

// String -> "cluster, proc"
int
get_job_id (const char * s, int * cluster_id, int * proc_id) {

	char const *pdot = strchr(s, '.');
	if (pdot) {
		char * buff = strdup (s);
		buff[pdot-s]='\0';
		*cluster_id=atoi(buff);

		*proc_id = atoi ((char*)(&buff[pdot-s+1]));

		free (buff);
		return TRUE;
	}

	return FALSE;
}

// String (XML) -> classad
int
get_class_ad (const char * s, ClassAd ** class_ad) {
	classad::ClassAdParser parser;
	*class_ad = new ClassAd;
	if ( !parser.ParseClassAd( s, **class_ad ) ) {
		delete *class_ad;
		*class_ad = NULL;
	}
	if ( *class_ad ) {
		return TRUE;
	} else {
		return FALSE;
	}
}


int
enqueue_command (SchedDRequest * request) {
	request->status = SchedDRequest::SDCS_NEW;
	command_queue.push_back(request);
	return TRUE;
}

void
flush_results()
{
	for (auto next_str : results_queue) {
		daemonCore->Write_Pipe( RESULT_OUTBOX, next_str->c_str(),
								next_str->length() );
		delete next_str;
	}
	results_queue.clear();
}

void
enqueue_result (int req_id, const char ** results, const int argc)
{
	std::string *buffer = new std::string();

	formatstr( *buffer, "%d", req_id );

	for ( int i = 0; i < argc; i++ ) {
		*buffer += ' ';
		if ( results[i] == NULL ) {
			*buffer += "NULL";
		} else {
			for ( int j = 0; results[i][j] != '\0'; j++ ) {
				switch ( results[i][j] ) {
				case ' ':
				case '\\':
				case '\r':
				case '\n':
					*buffer += '\\';
					// Fall through...
					//@fallthrough@
				default:
					*buffer += results[i][j];
				}
			}
		}
	}

	*buffer += '\n';
	results_queue.push_back( buffer );
}
