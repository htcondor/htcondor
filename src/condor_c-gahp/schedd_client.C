/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "my_username.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "format_time.h"  // for format_time and friends
#include "daemon.h"
#include "dc_schedd.h"
#include "condor_xml_classads.h"
#include "condor_new_classads.h"
#include "setenv.h"
#include "globus_utils.h"
#include "FdBuffer.h"
#include "io_loop.h"

#include "schedd_client.h"
#include "SchedDCommands.h"

// Schedd contact timeout
#define QMGMT_TIMEOUT 15


// How often we contact the schedd (secs)
int contact_schedd_interval = 20;


// How often we check results in the pipe (secs)
int check_requests_interval = 5;

// Do we use XML-formatted classads when talking to our invoker
bool useXMLClassads = false;

// Pointer to a socket open for QMGMT operations
extern ReliSock* qmgmt_sock;

// Queue for pending commands to schedd
template class SimpleList<SchedDRequest*>;
SimpleList <SchedDRequest*> command_queue;

// Buffer for reading requests from the IO thread
FdBuffer request_buffer;


template class SimpleList <MyString*>;

int RESULT_OUTBOX = -1;
int REQUEST_INBOX = -1;

int checkRequestPipeTid = TIMER_UNSET;
int contactScheddTid = TIMER_UNSET;

char *ScheddAddr = NULL;
char *ScheddPool = NULL;

extern char *myUserName;

extern int main_shutdown_graceful();

int
request_pipe_handler() {

	MyString * next_line = NULL;

	if (request_buffer.IsError()) {
		dprintf (D_ALWAYS, "Request pipe, closed. Exiting...\n");
		main_shutdown_graceful();
	}

	if ( ((next_line = request_buffer.GetNextLine()) != NULL)) {
		dprintf (D_FULLDEBUG, "got work request: %s\n", next_line->Value());

		char ** argv;
		int argc;

			// Parse the command...
		if (!(parse_gahp_command (next_line->Value(), &argv, &argc) && handle_gahp_command (argv, argc))) {
			dprintf (D_ALWAYS, "ERROR processing %s\n", next_line->Value());
		}

			// Clean up...
		delete  next_line;
		while ((--argc) >= 0)
			free (argv[argc]);
		free( argv );

	}

	return TRUE;
}



int
doContactSchedd()
{
	if (command_queue.IsEmpty()) {
		daemonCore->Reset_Timer( contactScheddTid, contact_schedd_interval ); // Come back in a min
		return TRUE;
	}

	dprintf(D_FULLDEBUG,"in doContactSchedd\n");

	SchedDRequest * current_command = NULL;

	int error=FALSE;
	char error_msg[1000];
	CondorError errstack;

	// Try connecting to schedd
	DCSchedd dc_schedd ( ScheddAddr, ScheddPool );
	if (dc_schedd.error() || !dc_schedd.locate()) {
		sprintf (error_msg, "Error locating schedd %s", ScheddAddr);

		dprintf( D_ALWAYS, "%s\n", error_msg);

		// If you can't connect return "Failure" on every job request
		command_queue.Rewind();
		while (command_queue.Next(current_command)) {
			if (current_command->status != SchedDRequest::SDCS_NEW)
				continue;

			if (current_command->command == SchedDRequest::SDC_STATUS_CONSTRAINED) {
				const char * result[] = {
					GAHP_RESULT_FAILURE,
					error_msg,
					"0"};
				enqueue_result (current_command->request_id, result, 3);
			} else if (current_command->command == SchedDRequest::SDC_SUBMIT_JOB) {
				const char * result[] = {
									GAHP_RESULT_FAILURE,
									NULL,
									error_msg };
				enqueue_result (current_command->request_id, result, 3);
			} else {
				const char * result[] = {
									GAHP_RESULT_FAILURE,
									error_msg };
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
		
		
		StringList id_list;
		SimpleList <SchedDRequest*> this_batch;

		SchedDRequest::schedd_command_type this_command = commands[i];
		const char * this_action = command_titles[i];
		const char * this_reason = NULL;

		dprintf (D_FULLDEBUG, "Processing %s requests\n", this_action);
		
		error = FALSE;

		// Create a batch of commands with the same command type AND the same reason		
		command_queue.Rewind();
		while (command_queue.Next(current_command)) {
			if (current_command->status != SchedDRequest::SDCS_NEW)
				continue;

			if (current_command->command != this_command)
				continue;

			if ((this_reason != NULL) && (strcmp (current_command->reason, this_reason) != 0))
				continue;

			if (this_reason == NULL)
				this_reason = current_command->reason;
				
			char job_id_buff[30];
			sprintf (job_id_buff, "%d.%d",
				current_command->cluster_id,
				current_command->proc_id);
			id_list.append (job_id_buff);

			this_batch.Append (current_command);
		}

		// If we haven't found any....
		if (id_list.isEmpty()) {
			i++;
			continue;	// ... then try the next command
		}

		// Perform the appropriate command on the current batch
		ClassAd * result_ad= NULL;
		if (current_command->command == SchedDRequest::SDC_REMOVE_JOB)  {
			errstack.clear();
			result_ad=
				dc_schedd.removeJobs (
					&id_list,
					this_reason,
					&errstack);
		} else if (current_command->command == SchedDRequest::SDC_HOLD_JOB) {
			errstack.clear();
			result_ad=
				dc_schedd.holdJobs (
					&id_list,
					this_reason,
			 		&errstack);
		} else if (current_command->command == SchedDRequest::SDC_RELEASE_JOB)  {
			errstack.clear();
			result_ad=
				dc_schedd.releaseJobs (
					&id_list,
					this_reason,
					&errstack);
		}

		// Analyze the result ad
		if (!result_ad) {
			error = TRUE;
			sprintf (error_msg, "Error connecting to schedd %s %s: %s",
					 ScheddAddr, dc_schedd.addr(), errstack.getFullText() );
		}
		else {
			result_ad->dPrint (D_FULLDEBUG);
		}

		// Go through the batch again, and create responses for each request
		this_batch.Rewind();
		while (this_batch.Next(current_command)) {
			
			// Check the result
			char job_id_buff[30];
			if (result_ad && (error == FALSE)) {
				sprintf (job_id_buff, "job_%d_%d",
					current_command->cluster_id,
					current_command->proc_id);
				
				int remove_result;
				if (result_ad->LookupInteger (job_id_buff, remove_result)) {
					switch (remove_result) {
						case AR_ERROR:
							error = TRUE;
							strcpy (error_msg, "General Error");
							break;
						case AR_SUCCESS:
							error = FALSE;
							break;
						case AR_NOT_FOUND:
							error = TRUE;
							strcpy (error_msg, "Job not found");
							break;
						case AR_BAD_STATUS:
							error = TRUE;
							strcpy (error_msg, "Bad job status");
							break;
						case AR_ALREADY_DONE:
							error = TRUE;
							strcpy (error_msg, "Already done");
							break;
						case AR_PERMISSION_DENIED:
							error = TRUE;
							strcpy (error_msg, "Permission denied");
							break;
						default:
							error = TRUE;
							strcpy (error_msg, "Unknown Result");
					} // hctiws

					delete result_ad;
				} else {
					strcpy (error_msg, "Unable to get result");
				} // fi lookup result for job
			} // fi error == FALSE

			if (error) {
				dprintf (D_ALWAYS, "Error (operation: %s) %d.%d: %s\n",
						this_action,
						current_command->cluster_id,
						current_command->proc_id,
						error_msg);

				const char * result[2];
				result[0] = GAHP_RESULT_FAILURE;
				result[1] = error_msg;

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
	}

	dprintf (D_FULLDEBUG, "Processing JOB_STAGE_IN requests\n");
	

	// JOB_STAGE_IN
	int MAX_BATCH_SIZE=1; // This should be a config param

	SimpleList <SchedDRequest*> stage_in_batch;
	do {
		stage_in_batch.Clear();

		command_queue.Rewind();
		while (command_queue.Next(current_command)) {

			if (current_command->status != SchedDRequest::SDCS_NEW)
				continue;

			if (current_command->command != SchedDRequest::SDC_JOB_STAGE_IN)
				continue;

			dprintf (D_ALWAYS, "Adding %d.%d to STAGE_IN batch\n", 
					 current_command->cluster_id,
					 current_command->proc_id);

			stage_in_batch.Append (current_command);
			if (stage_in_batch.Number() >= MAX_BATCH_SIZE)
				break;
		}

		if (stage_in_batch.Number() > 0) {
			ClassAd ** array = new ClassAd*[stage_in_batch.Number()];
			i=0;
			stage_in_batch.Rewind();
			while (stage_in_batch.Next(current_command)) {
				array[i++] = current_command->classad;
			}

			error = FALSE;
			errstack.clear();
			if (!dc_schedd.spoolJobFiles( stage_in_batch.Number(),
										  array,
										  &errstack )) {
				error = TRUE;
				sprintf (error_msg, "Error connecting to schedd %s: %s", ScheddAddr, errstack.getFullText());
				dprintf (D_ALWAYS, "%s\n", error_msg);
			}
			delete [] array;
  
			stage_in_batch.Rewind();
			while (stage_in_batch.Next(current_command)) {
				current_command->status = SchedDRequest::SDCS_COMPLETED;

				if (error) {
					const char * result[] = {
						GAHP_RESULT_FAILURE,
						error_msg };
					enqueue_result (current_command->request_id, result, 2);

				} else {
					const char * result[] = {
						GAHP_RESULT_SUCCESS,
						NULL };
					enqueue_result (current_command->request_id, result, 2);
				}
			} // elihw (command_queue)
		} // fi has STAGE_IN requests
	} while (stage_in_batch.Number() > 0);

	dprintf (D_FULLDEBUG, "Processing JOB_STAGE_OUT requests\n");
	

	// JOB_STAGE_OUT
	SimpleList <SchedDRequest*> stage_out_batch;

	command_queue.Rewind();
	while (command_queue.Next(current_command)) {

		if (current_command->status != SchedDRequest::SDCS_NEW)
			continue;

		if (current_command->command != SchedDRequest::SDC_JOB_STAGE_OUT)
			continue;


		stage_out_batch.Append (current_command);
	}

	if (stage_out_batch.Number() > 0) {
		MyString constraint = "";
		stage_out_batch.Rewind();
		while (stage_out_batch.Next(current_command)) {
			constraint.sprintf_cat( "(ClusterId==%d&&ProcId==%d)||",
									current_command->cluster_id,
									current_command->proc_id );
		}
		constraint += "False";

		error = FALSE;
		errstack.clear();
		if (!dc_schedd.receiveJobSandbox( constraint.Value(),
										  &errstack )) {
			error = TRUE;
			sprintf (error_msg, "Error connecting to schedd %s", ScheddAddr);
			dprintf (D_ALWAYS, "%s\n", error_msg);
		}
  
		stage_out_batch.Rewind();
		while (stage_out_batch.Next(current_command)) {
			current_command->status = SchedDRequest::SDCS_COMPLETED;

			if (error) {
				const char * result[] = {
								GAHP_RESULT_FAILURE,
								error_msg };
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

	// JOB_REFRESH_PROXY
	command_queue.Rewind();
	while (command_queue.Next(current_command)) {

		if (current_command->status != SchedDRequest::SDCS_NEW)
			continue;

		if (current_command->command != SchedDRequest::SDC_JOB_REFRESH_PROXY)
			continue;

		bool result;
		errstack.clear();
		result = dc_schedd.updateGSIcredential (current_command->cluster_id,
												current_command->proc_id,
												current_command->proxy_file,
												&errstack);

		current_command->status = SchedDRequest::SDCS_COMPLETED;

		if (result == false) {
			sprintf (error_msg, "Error refreshing proxy to schedd %s: %s",
					 ScheddAddr, errstack.getFullText() );
			dprintf (D_ALWAYS, "%s\n", error_msg);

			const char * result[] = {
				GAHP_RESULT_FAILURE,
				error_msg };
			enqueue_result (current_command->request_id, result, 2);

		} else {
			const char * result[] = {
				GAHP_RESULT_SUCCESS,
				NULL };
			enqueue_result (current_command->request_id, result, 2);
		}

	}


	// Now do all the QMGMT transactions
	error = FALSE;

	// Try connecting to the queue
	Qmgr_connection * qmgr_connection;
	
	if ((qmgr_connection = ConnectQ(dc_schedd.addr(), QMGMT_TIMEOUT, false )) == NULL) {
		error = TRUE;
		sprintf (error_msg, "Error connecting to schedd %s", ScheddAddr);
		dprintf (D_ALWAYS, "%s\n", error_msg);
	} else {
		AbortTransaction(); // Just so we can call BeginTransaction() in the loop
	}


	dprintf (D_FULLDEBUG, "Processing UPDATE_CONSTRAINED/UDATE_JOB requests\n");
	
	// UPDATE_CONSTRAINED
	// UDATE_JOB
	command_queue.Rewind();
	while (command_queue.Next(current_command)) {
		
		if (current_command->status != SchedDRequest::SDCS_NEW)
			continue;

		if ((current_command->command != SchedDRequest::SDC_UPDATE_CONSTRAINED) &&
			(current_command->command != SchedDRequest::SDC_UPDATE_JOB))
			continue;

		if (qmgr_connection == NULL)
			goto update_report_result;
		
		error = FALSE;
		BeginTransaction();
		
		current_command->status = SchedDRequest::SDCS_PENDING;
			
		current_command->classad->ResetExpr();
		ExprTree *tree;
		while( (tree = current_command->classad->NextExpr()) ) {
			ExprTree *lhs = NULL, *rhs = NULL;
			char *lhstr = NULL, *rhstr = NULL;

			if( (lhs = tree->LArg()) ) { lhs->PrintToNewStr (&lhstr); }
			if( (rhs = tree->RArg()) ) { rhs->PrintToNewStr (&rhstr); }
			if( !lhs || !rhs || !lhstr || !rhstr) {
				sprintf (error_msg, "ERROR: ClassAd problem in Updating by constraint %s",
												 current_command->constraint );
				dprintf ( D_ALWAYS, "%s\n", error_msg);
				error = TRUE;
			} else {
				if (current_command->command == SchedDRequest::SDC_UPDATE_CONSTRAINED) {
					if( SetAttributeByConstraint(current_command->constraint,
												lhstr,
												rhstr) == -1 ) {
						sprintf (error_msg, "ERROR: Failed (errno=%d) to SetAttributeByConstraint %s=%s for constraint %s",
									errno, lhstr, rhstr, current_command->constraint );
						dprintf ( D_ALWAYS, "%s\n", error_msg);
						error = TRUE;
					}
				} else if (current_command->command == SchedDRequest::SDC_UPDATE_JOB) {
					if( SetAttribute(current_command->cluster_id,
											current_command->proc_id,
											lhstr,
											rhstr) == -1 ) {
						sprintf (error_msg, "ERROR: Failed to SetAttribute() %s=%s for job %d.%d",
										 lhstr, rhstr, current_command->cluster_id,  current_command->proc_id);
						dprintf ( D_ALWAYS, "%s\n", error_msg);
						error = TRUE;
					}
				}
			}

			free(lhstr);
			free(rhstr);

			if (error)
				break;
		} // elihw classad

update_report_result:
		if (error) {
			const char * result[] = {
				GAHP_RESULT_FAILURE,
				error_msg };


			//CloseConnection();
			enqueue_result (current_command->request_id, result, 2);
			current_command->status = SchedDRequest::SDCS_COMPLETED;
			if ( qmgr_connection != NULL ) {
				AbortTransaction();
			}
		} else {
			const char * result[] = {
				GAHP_RESULT_SUCCESS,
				NULL };
			enqueue_result (current_command->request_id, result, 2);
			current_command->status = SchedDRequest::SDCS_COMPLETED;
			CloseConnection();
		} // fi

	} // elihw

	dprintf (D_FULLDEBUG, "Processing SUBMIT_JOB requests\n");

	// SUBMIT_JOB
	command_queue.Rewind();
	while (command_queue.Next(current_command)) {

		if (current_command->status != SchedDRequest::SDCS_NEW)
			continue;

		if (current_command->command != SchedDRequest::SDC_SUBMIT_JOB)
			continue;

		int ClusterId = -1;
		int ProcId = -1;

		if (qmgr_connection == NULL)
			goto submit_report_result;

		BeginTransaction();
		error = FALSE;

		if ((ClusterId = NewCluster()) >= 0) {
			ProcId = NewProc (ClusterId);
		}

		if ( ClusterId < 0 ) {
			error = TRUE;
			strcpy (error_msg, "Unable to create a new job cluster");
			dprintf (D_ALWAYS, "%s\n", error_msg);
		} else if ( ProcId < 0 ) {
			error = TRUE;
			strcpy (error_msg, "Unable to create a new job proc");
			dprintf (D_ALWAYS, "%s\n", error_msg);
		}
		if ( ClusterId == -2 || ProcId == -2 ) {
			error = TRUE;
			strcpy (error_msg, 
				"Number of submitted jobs would exceed MAX_JOBS_SUBMITTED\n");
			dprintf (D_ALWAYS, "%s\n", error_msg);
		}

		if ( error == FALSE ) {
			current_command->classad->Assign(ATTR_CLUSTER_ID, ClusterId);
			current_command->classad->Assign(ATTR_PROC_ID, ProcId);

			// Set all the classad attribute on the remote classad
			current_command->classad->ResetExpr();
			ExprTree *tree;
			while( (tree = current_command->classad->NextExpr()) ) {
				ExprTree *lhs;
				ExprTree *rhs;
				char *lhstr, *rhstr;

				lhs = NULL, rhs = NULL;
				rhs = NULL, rhstr = NULL;

				if( (lhs = tree->LArg()) ) { lhs->PrintToNewStr (&lhstr); }
				if( (rhs = tree->RArg()) ) { rhs->PrintToNewStr (&rhstr); }
				if( !lhs || !rhs || !lhstr || !rhstr) {
					sprintf (error_msg, "ERROR: ClassAd problem in Updating by constraint %s",
												 current_command->constraint );
					dprintf ( D_ALWAYS, "%s\n", error_msg);
					error = TRUE;
				} else if( SetAttribute (ClusterId, ProcId,
											lhstr,
											rhstr) == -1 ) {
					sprintf (error_msg, "ERROR: Failed to SetAttribute %s=%s for job %d.%d",
									 lhstr, rhstr, ClusterId, ProcId );
					dprintf ( D_ALWAYS, "%s\n", error_msg);
					error = TRUE;
				}

				if (lhstr) free(lhstr);
				if (rhstr) free(rhstr);

				if (error) break;
			} // elihw classad
		} // fi error==FALSE

submit_report_result:
		char job_id_buff[30];
		sprintf (job_id_buff, "%d.%d", ClusterId, ProcId);

		if (error) {
			const char * result[] = {
								GAHP_RESULT_FAILURE,
								job_id_buff,
									error_msg };
			enqueue_result (current_command->request_id, result, 3);
			if ( qmgr_connection != NULL ) {
				AbortTransaction();
			}
			current_command->status = SchedDRequest::SDCS_COMPLETED;
		} else {
			const char * result[] = {
									GAHP_RESULT_SUCCESS,
									job_id_buff,
									NULL };
			enqueue_result (current_command->request_id, result, 3);
			CloseConnection();
			current_command->status = SchedDRequest::SDCS_COMPLETED;
		}
	} // elihw


	dprintf (D_FULLDEBUG, "Processing STATUS_CONSTRAINED requests\n");
		
	// STATUS_CONSTRAINED
	command_queue.Rewind();
	while (command_queue.Next(current_command)) {

		if (current_command->status != SchedDRequest::SDCS_NEW)
			continue;

		if (current_command->command != SchedDRequest::SDC_STATUS_CONSTRAINED)
			continue;

		if (qmgr_connection != NULL) {
			SimpleList <MyString *> matching_ads;

			error = FALSE;
			
			ClassAd * next_ad = GetNextJobByConstraint( current_command->constraint, 1 );
			while (next_ad != NULL) {
				MyString * da_buffer = new MyString();	// Use a ptr to avoid excessive copying
				if ( useXMLClassads ) {
					ClassAdXMLUnparser unparser;
					unparser.SetUseCompactSpacing(true);
					unparser.Unparse (next_ad, *da_buffer);
				} else {
					NewClassAdUnparser unparser;
					unparser.SetUseCompactSpacing(true);
					unparser.Unparse (next_ad, *da_buffer);
				}
				matching_ads.Append (da_buffer);
				if ( next_ad ) {
					FreeJobAd(next_ad);
				}
				next_ad = GetNextJobByConstraint( current_command->constraint, 0 );
			}

			// now output this list of classads into a result
			const char ** result  = new const char* [matching_ads.Length() + 3];

			MyString _ad_count;
			_ad_count += matching_ads.Length();

			int count=0;
			result[count++] = GAHP_RESULT_SUCCESS;
			result[count++] = NULL;
			result[count++] = _ad_count.Value();

			MyString *next_string;
			matching_ads.Rewind();
			while (matching_ads.Next(next_string)) {
				result[count++] = next_string->Value();
			}

			enqueue_result (current_command->request_id, result, count);
			current_command->status = SchedDRequest::SDCS_COMPLETED;

			// Cleanup
			matching_ads.Rewind();
			while (matching_ads.Next(next_string)) {
				delete next_string;
			}
			//CommitTransaction();
			delete [] result;
		}
		else {
			const char * result[] = {
				GAHP_RESULT_FAILURE,
				error_msg,
				"0" };
			//CloseConnection();
			enqueue_result (current_command->request_id, result, 3);
			current_command->status = SchedDRequest::SDCS_COMPLETED;
		}
	}	//elihw

	
	dprintf (D_FULLDEBUG, "Finishing doContactSchedd()\n");

	if ( qmgr_connection != NULL ) {
		DisconnectQ (qmgr_connection, FALSE);
	}
	qmgmt_sock = NULL;

	// Clean up the list
	command_queue.Rewind();
	while (command_queue.Next(current_command)) {
		if (current_command->status == SchedDRequest::SDCS_COMPLETED) {
			command_queue.DeleteCurrent();
			delete current_command;
		}
	}
	
	// Come back soon..
	// QUESTION: Should this always be a fixed time period?
	daemonCore->Reset_Timer( contactScheddTid, contact_schedd_interval );
	return TRUE;

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

		if (!(argc == 5 &&
			get_int (argv[1], &req_id) &&
			get_job_id (argv[3], &cluster_id, &proc_id))) {

			dprintf (D_ALWAYS, "Invalid args to %s\n", argv[0]);
			return FALSE;
		}

		enqueue_command (
			SchedDRequest::createRefreshProxyRequest(
				req_id,
				cluster_id,
				proc_id,
				argv[4]));
		return TRUE;
	} else if (strcasecmp (argv[0], GAHP_COMMAND_INITIALIZE_FROM_FILE)==0) {
		static bool init_done = false;

		if ( init_done == false ) {
			SetEnv( "X509_USER_PROXY", argv[1] );
			UnsetEnv( "X509_USER_CERT" );
			UnsetEnv( "X509_USER_KEY" );
			char *subject = x509_proxy_identity_name( argv[1] );
			char *daemon_subjects = param( "GSI_DAEMON_NAME" );
			MyString buff;
			if ( daemon_subjects ) {
				buff.sprintf( "%s,%s", daemon_subjects, subject );
			} else {
				buff.sprintf( "%s", subject );
			}
			dprintf( D_ALWAYS, "Setting %s=%s\n", "_condor_GSI_DAEMON_NAME", buff.Value() );
			SetEnv( "_condor_GSI_DAEMON_NAME", buff.Value() );
			if ( subject ) {
				free( subject );
			}
			if ( daemon_subjects ) {
				free( daemon_subjects );
			}
			daemonCore->Send_Signal( daemonCore->getpid(), SIGHUP );
			// TODO set any config values needed
			// TODO trigger a reconfig
			init_done = true;
		}
		return TRUE;
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


// String -> "cluster, proc"
int
get_job_id (const char * s, int * cluster_id, int * proc_id) {

	char * pdot = strchr(s, '.');
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
	if ( useXMLClassads ) {
		ClassAdXMLParser parser;
		*class_ad = parser.ParseClassAd (s);
	} else {
		NewClassAdParser parser;
		*class_ad = parser.ParseClassAd (s);
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
	command_queue.Append (request);
	return TRUE;
}

void
enqueue_result (int req_id, const char ** results, const int argc) {
	MyString buffer;

	// 1 Escape all the shit
	// 2 Create a string that is a concatenation of the shit in #1
	// 3 Flust the shit in #2 down the pipe (where shit ought to go)....

	buffer+= req_id;

	int i=0;
	for (; i<argc; i++) {

		buffer += " ";

		char * escaped = escape_string ( (results[i]) ? results[i] : "NULL" );

		buffer += escaped;

		free (escaped);
	}

	buffer += "\n";

	// Now flush:
	write (RESULT_OUTBOX, buffer.Value(), buffer.Length());
}

char *
escape_string (const char * string) {
	MyString my_string (string);
	MyString escape_this (" \t\n\r\\");

	MyString result = my_string.EscapeChars(escape_this, '\\');

	return strdup(result.Value());
}


