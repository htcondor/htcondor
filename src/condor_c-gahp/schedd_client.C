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


// Pointer to a socket open for QMGMT operations
extern ReliSock* qmgmt_sock;

// Queue for pending commands to schedd
template class SimpleList<SchedDRequest*>;
SimpleList <SchedDRequest*> command_queue;

// Buffer for reading requests from the IO thread
FdBuffer request_buffer;

int RESULT_OUTBOX = -1;
int REQUEST_INBOX = -1;
int REQUEST_ACK_OUTBOX = -1;

int checkRequestPipeTid = TIMER_UNSET;
int contactScheddTid = TIMER_UNSET;

char *ScheddAddr = NULL;

extern char *myUserName;

extern int main_shutdown_graceful();

int
schedd_thread (void * arg, Stream * sock) {

   	inter_thread_io_t * inter_thread_io = (inter_thread_io_t*)arg;
	RESULT_OUTBOX = inter_thread_io->result_pipe[1];
	REQUEST_INBOX = inter_thread_io->request_pipe[0];
	REQUEST_ACK_OUTBOX = inter_thread_io->request_ack_pipe[1];

	request_buffer.setFd( REQUEST_INBOX );

    // Just set up timers....
    contactScheddTid = daemonCore->Register_Timer( contact_schedd_interval,
										(TimerHandler)&doContactSchedd,
										"doContactSchedD", NULL );

	checkRequestPipeTid = daemonCore->Register_Timer( check_requests_interval,
										(TimerHandler)&checkRequestPipe,
										"checkRequestPipe", NULL );


	write (REQUEST_ACK_OUTBOX, "R", 1); // Signal that we're ready for the first request

	return TRUE;
}

/**
 * Check whether the IO thread has sent us any new requests
 * If so, read them, parse them and add them to the queue
 */
int
checkRequestPipe () {

	dprintf (D_FULLDEBUG, "In checkRequestPipe\n");

	time_t time1 = time (NULL);

	MyString * next_line = NULL;

	FdBuffer * wait_on [] = { &request_buffer };
	int is_ready = FALSE;
	int select_result = 0;

	do {
		if (request_buffer.IsError()) {
			dprintf (D_ALWAYS, "Request pipe, closed. Exiting...\n");
			main_shutdown_graceful();
		}

		select_result =
			FdBuffer::Select (wait_on, 1, 3, &is_ready);

		if ( (select_result > 0) && (is_ready) && ((next_line = request_buffer.GetNextLine()) != NULL)) {
			dprintf (D_FULLDEBUG, "got work request: %s\n", next_line->Value());

			char ** argv;
			int argc;

			// Parse the command...
			if (!(parse_gahp_command (next_line->Value(), &argv, &argc) && handle_gahp_command (argv, argc))) {
				dprintf (D_ALWAYS, "error processing %s\n", next_line->Value());
				write (REQUEST_ACK_OUTBOX, "R", 1); // Signal that we're ready again
				continue;
			}

			// Clean up...
			delete  next_line;
			while ((--argc) >= 0)
				free (argv[argc]);
			delete [] argv;

			write (REQUEST_ACK_OUTBOX, "R", 1); // Signal that we're ready again
		}
	} while (select_result > 0); // Keep selecting while there's stuff to read

	time_t time2 = time (NULL);

	// Come back soon....
	int next_contact_interval = check_requests_interval - (time2 - time1);
	if (next_contact_interval < 0)
		next_contact_interval = 1;
	daemonCore->Reset_Timer ( checkRequestPipeTid, next_contact_interval);

	return TRUE;
}

template class SimpleList <MyString*>;

int
doContactSchedd()
{
	if (command_queue.IsEmpty()) {
		daemonCore->Reset_Timer( contactScheddTid, contact_schedd_interval ); // Come back in a min
		return TRUE;
	}

	dprintf(D_FULLDEBUG,"in doContactSchedd\n");

	SchedDRequest * current_command = NULL;

	CondorError errstack;
	char error_msg[1000];

	// Try connecting to schedd
	DCSchedd dc_schedd ( ScheddAddr );
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
		}

		current_command->status = SchedDRequest::SDCS_COMPLETED;

	}



	command_queue.Rewind();
	while (command_queue.Next(current_command)) {
		if (current_command->status != SchedDRequest::SDCS_NEW) {
			delete current_command;
			command_queue.DeleteCurrent();
			continue;
		}

		if (current_command->command == SchedDRequest::SDC_REMOVE_JOB ||
			current_command->command == SchedDRequest::SDC_HOLD_JOB ||
			current_command->command == SchedDRequest::SDC_RELEASE_JOB) {
			int error = 0;

			// Make a job string
			char job_id_buff[30];
			sprintf (job_id_buff, "%d.%d",
				current_command->cluster_id,
				current_command->proc_id);

			StringList id_list;
			id_list.append (job_id_buff);
			current_command->status = SchedDRequest::SDCS_PENDING;

			const char * action;
			ClassAd * result_ad= NULL;

			if (current_command->command == SchedDRequest::SDC_REMOVE_JOB)  {
				result_ad=
					dc_schedd.removeJobs (&id_list,
							current_command->reason,
							&errstack);
				action = "removing";
			} else if (current_command->command == SchedDRequest::SDC_HOLD_JOB) {
				result_ad=
					dc_schedd.holdJobs (&id_list,
							current_command->reason,
				 			&errstack);
				action = "putting on hold";
			} else if (current_command->command == SchedDRequest::SDC_RELEASE_JOB)  {
				result_ad=
					dc_schedd.releaseJobs (&id_list,
							current_command->reason,
							&errstack);
				action = "releasing";
			}

			if (!result_ad) {
				error = TRUE;
				sprintf (error_msg, "Error connecting to schedd %s", ScheddAddr);
			} else {
				result_ad->dPrint (D_FULLDEBUG);

				// Check the result
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
				} // fi get result
			} // if remove result ad

			if (error) {
				dprintf (D_ALWAYS, "Error %s %d,%d\n",
						action,
						current_command->cluster_id,
						current_command->proc_id);

				const char * result[2];
				result[0] = GAHP_RESULT_FAILURE;
				result[1] = error_msg;

				enqueue_result (current_command->request_id, result, 2);
			} else {
				dprintf (D_ALWAYS, "Succeess %s %d,%d\n",
						action,
						current_command->cluster_id,
						current_command->proc_id);
				current_command->status = SchedDRequest::SDCS_COMPLETED;

				const char * result[2];
				result[0] = GAHP_RESULT_SUCCESS;
				result[1] = NULL;

				enqueue_result (current_command->request_id, result, 2);
			} // fi error

				// Mark the status
			current_command->status = SchedDRequest::SDCS_COMPLETED;
		} else if (current_command->command == SchedDRequest::SDC_UPDATE_CONSTRAINED ) {
			int error = 0;


			if (!BeginQmgmtTransaction(dc_schedd, TRUE)) {
				error = TRUE;
				sprintf (error_msg, "Error connecting to schedd %s", ScheddAddr);
				dprintf (D_ALWAYS, "%s\n", error_msg);
			} else {
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
					} else if( SetAttributeByConstraint(current_command->constraint,
												lhstr,
												rhstr) == -1 ) {
						sprintf (error_msg, "ERROR: Failed (errno=%d) to SetAttributeByConstraint %s=%s for constraint %s",
										 errno, lhstr, rhstr, current_command->constraint );
						dprintf ( D_ALWAYS, "%s\n", error_msg);

						error = TRUE;
					}

					free(lhstr);
					free(rhstr);

					if (error)
						break;
				} // elihw classad
			}

			if (error) {
				const char * result[] = {
					GAHP_RESULT_FAILURE,
					error_msg };
				FinishTransaction(FALSE);
				enqueue_result (current_command->request_id, result, 2);
			} else {
				const char * result[] = {
					GAHP_RESULT_SUCCESS,
					NULL };
				FinishTransaction(TRUE);
				enqueue_result (current_command->request_id, result, 2);
			} // fi
			current_command->status = SchedDRequest::SDCS_COMPLETED;
		} else if (current_command->command == SchedDRequest::SDC_UPDATE_JOB ) {
			int error = 0;

			if (!BeginQmgmtTransaction(dc_schedd, TRUE)) {
				error = TRUE;
				sprintf (error_msg, "Error connecting to schedd %s", ScheddAddr);
				dprintf (D_ALWAYS, "%s\n", error_msg);
			} else {
				current_command->classad->ResetExpr();
				ExprTree *tree;
				while( (tree = current_command->classad->NextExpr()) ) {
					ExprTree *lhs = NULL, *rhs = NULL;
					char *lhstr = NULL, *rhstr = NULL;

					if( (lhs = tree->LArg()) ) { lhs->PrintToNewStr (&lhstr); }
					if( (rhs = tree->RArg()) ) { rhs->PrintToNewStr (&rhstr); }
					if( !lhs || !rhs || !lhstr || !rhstr) {
						sprintf (error_msg, "ERROR: ClassAd problem in Updating for job %d.%d",
														 current_command->cluster_id,
														 current_command->proc_id);
						dprintf ( D_ALWAYS, "%s\n", error_msg);
						error = TRUE;
					} else if( SetAttribute(current_command->cluster_id,
											current_command->proc_id,
											lhstr,
											rhstr) == -1 ) {
						sprintf (error_msg, "ERROR: Failed to SetAttribute() %s=%s for job %d.%d",
										 lhstr, rhstr, current_command->cluster_id,  current_command->proc_id);
						dprintf ( D_ALWAYS, "%s\n", error_msg);

						error = TRUE;
					}

					free(lhstr);
					free(rhstr);

					if (error)
						break;
				} // elihw classad
			}

			if (error) {
				const char * result[] = {
					GAHP_RESULT_FAILURE,
					error_msg };
				FinishTransaction(FALSE);
				enqueue_result (current_command->request_id, result, 2);
			} else {
				const char * result[] = {
					GAHP_RESULT_SUCCESS,
					NULL };
				FinishTransaction(TRUE);
				enqueue_result (current_command->request_id, result, 2);
			} // fi

			current_command->status = SchedDRequest::SDCS_COMPLETED;

		} else if (current_command->command == SchedDRequest::SDC_STATUS_CONSTRAINED ) {
			SimpleList <MyString *> matching_ads;

			int error = FALSE;

			ClassAdXMLUnparser XMLer;
			XMLer.SetUseCompactSpacing(true);

			if (!BeginQmgmtTransaction(dc_schedd, FALSE)) {
				error = TRUE;
				sprintf (error_msg, "Error connecting to schedd %s", ScheddAddr);
				dprintf (D_ALWAYS, "%s\n", error_msg);
			} else {
				ClassAd * next_ad = GetNextJobByConstraint( current_command->constraint, 1 );
				while (next_ad != NULL) {
					MyString * da_buffer = new MyString();	// Use a ptr to avoid excessive copying
					XMLer.Unparse (next_ad, *da_buffer);
					matching_ads.Append (da_buffer);
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
				FinishTransaction (TRUE);
				delete [] result;
			}

			if (error) {
				FinishTransaction(FALSE);

				const char * result[] = {
					GAHP_RESULT_FAILURE,
					error_msg,
					"0"};
				enqueue_result (current_command->request_id, result, 3);
			}
		} else if (current_command->command == SchedDRequest::SDC_SUBMIT_JOB ) {
			int error = 0;
			int ClusterId = -1;
			int ProcId = -1;

			// TODO (optimization):
			// Find all other jobs from this cluster
			// And submit them together
			// This is tricky because the first job of this cluster might
			// have been submitted on the previous cycle of this function

			if (!BeginQmgmtTransaction(dc_schedd, TRUE)) {
				error = TRUE;
				sprintf (error_msg, "Error connecting to schedd %s", ScheddAddr);
				dprintf (D_ALWAYS, "%s\n", error_msg);
			} else if ((ClusterId = NewCluster()) == -1) {
				error = TRUE;
				strcpy (error_msg, "Unable to create a new job cluster");
				dprintf (D_ALWAYS, "%s\n", error_msg);
			} else {
				ProcId = NewProc (ClusterId);

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

				// Commit
				if (error)
					FinishTransaction(FALSE);
				else
					FinishTransaction(TRUE);


				// Now transfer the sandbox
				// This has been moved to a separate command, JOB_STAGE_IN,
				// by Jaime's request, despite Carey's disgruntlement....

				/*CondorError errstack;
				ClassAd* array [1];
				array[0] = current_command->classad;
				if (!dc_schedd.spoolJobFiles( 1,
												  array,
												  &errstack )) {
					error = TRUE;
					sprintf (error_msg, "Error transferring sandbox for job %d.%d", ClusterId, ProcId);
					dprintf (D_ALWAYS, "%s\n", error_msg);
				}*/

				current_command->status = SchedDRequest::SDCS_COMPLETED;



			} // fi NewCluster()

			char job_id_buff[30];
			sprintf (job_id_buff, "%d.%d", ClusterId, ProcId);

			if (error) {
				const char * result[] = {
									GAHP_RESULT_FAILURE,
									job_id_buff,
									error_msg };
				enqueue_result (current_command->request_id, result, 3);

			} else {
				const char * result[] = {
										GAHP_RESULT_SUCCESS,
										job_id_buff,
										NULL };
				enqueue_result (current_command->request_id, result, 3);
			}
		} else if (current_command->command == SchedDRequest::SDC_JOB_STAGE_IN ) {
			int error = FALSE;
			CondorError errstack;
			ClassAd* array[] = { current_command->classad };

			if (!dc_schedd.spoolJobFiles( 1,
										  array,
										  &errstack )) {
				error = TRUE;
				sprintf (error_msg, "Error connecting to schedd %s", ScheddAddr);
				dprintf (D_ALWAYS, "%s\n", error_msg);
			}

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
		} else {
			// Unknown command (should never happen because of prior verifications)
		} // fi commmand = ...
	} // elihw (command_queue)

	// Come back soon..
	// QUESTION: Should this always be a fixed time period?
	daemonCore->Reset_Timer( contactScheddTid, contact_schedd_interval );
	return TRUE;

}

int
BeginQmgmtTransaction(DCSchedd & dc_schedd, int write) {
	// Prepare for Queue management commands
	CondorError errstack;
	qmgmt_sock = (ReliSock*) dc_schedd.startCommand( QMGMT_CMD,
													 Stream::reli_sock,
													 QMGMT_TIMEOUT,
													 &errstack);
	if (!qmgmt_sock) {
		//TODO: oh shit, what do we do?
		dprintf (D_ALWAYS, "Unable to start QMGMT_CMD\n");
		return FALSE;
	}

	if (!write)
		return TRUE;

	// Re-authenticate...
	// I'm not sure why we have to do this.. i just borrowed this code from ConnectQ()
	char *username = my_username();
	char *domain = my_domainname();
	if (!username) {
		dprintf (D_FULLDEBUG, "Unable to get username to re-authenticate!\n");
	}else {
		qmgmt_sock->setOwner( username );

		if (!qmgmt_sock->isAuthenticated()) {
			int rval;

			 if ( !write ) {
				rval = InitializeReadOnlyConnection( username );
			} else {
				rval = InitializeConnection( username, domain );
			}

			char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "CLIENT");
			MyString methods;
			if (p) {
				methods = p;
				free(p);
			} else {
				methods = SecMan::getDefaultAuthenticationMethods();
			}

			if (!qmgmt_sock->authenticate(methods.Value(), &errstack)) {
				dprintf (D_ALWAYS, "Can't authenticate\n");
				delete qmgmt_sock;
				qmgmt_sock = NULL;
				return FALSE;
			}
		}
	}
	if (username) free(username);
	if (domain) free (domain);

	return TRUE;
}

int
FinishTransaction(int commit) {
	if (commit)
		CloseConnection();

	if (qmgmt_sock)
		delete qmgmt_sock;

	qmgmt_sock = NULL;

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
	static ClassAdXMLParser parser;
    *class_ad = parser.ParseClassAd (s);
	return TRUE;
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

	// Now flush:
	write (RESULT_OUTBOX, buffer.Value(), buffer.Length());
	write (RESULT_OUTBOX, "\n", 1);
}

char *
escape_string (const char * string) {
	MyString my_string (string);
	MyString escape_this (" \t\n\r");

	MyString result = my_string.EscapeChars(escape_this, '\\');

	return strdup(result.Value());
}


