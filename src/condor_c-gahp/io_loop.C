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

#include "io_loop.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "schedd_client.h"
#include "FdBuffer.h"


// Queue for pending commands to schedd
template class SimpleList<char *>;
SimpleList <char *> request_out_buffer;


int
io_loop(void * arg, Stream * sock) {

	inter_thread_io_t * inter_thread_io = (inter_thread_io_t *)arg;
	close (inter_thread_io->request_pipe[0]);
	close (inter_thread_io->result_pipe[1]);

	bool worker_ready = false;

	dprintf (D_FULLDEBUG,"In io_loop\n");

	int async_mode = 0;
	int new_results_signaled = 0;
	StringList result_list;

	int exit_signaled = 0;

	// Set up structures for bufferin' input
	// This is because we might get an incomplete command (or response from worker thread)
	// So these puppies will hold on to it, until we get the rest
	FdBuffer stdin_buffer(STDIN_FILENO);
    FdBuffer result_buffer(inter_thread_io->result_pipe[0]);
    FdBuffer request_ack_buffer (inter_thread_io->request_ack_pipe[0]);


	const char * version = "$GahpVersion 2.0.0 Jan 21 2004 Condor\\ GAHP $";
	printf ("%s\n", version);
	fflush(stdout);

	do {
		char ** argv;
		int argc;

		FdBuffer * wait_on [] = {
			&stdin_buffer,
			&result_buffer,
			&request_ack_buffer };

		int is_ready[3];

		int select_result =
			FdBuffer::Select (wait_on, 3, 5, is_ready);

		if (select_result == 0) {
			continue; // what else are you gonna do?
		} else if (select_result < 0) {
			// Error: one of the fd's is broken
			// Either parent exited or we're in early stages of a thermonuclear meltdown
			// So exit
			return 1;
		}



		if (is_ready[0]) {

			// Don't make this a while() loop b/c
			// stdin read is blocking
			MyString * line = NULL;
			if ((line = stdin_buffer.GetNextLine()) != NULL) {

				const char * command = line->Value();

				dprintf (D_ALWAYS, "got stdin: %s\n", command);

				if (parse_gahp_command (command, &argv, &argc) &&
					verify_gahp_command (argv, argc)) {

					// Catch "special commands first
					if (strcasecmp (argv[0], GAHP_COMMAND_RESULTS) == 0) {
						// Print number of results
						MyString rn_buff;
						rn_buff+=result_list.number();
						const char * commands [] = {
							GAHP_RESULT_SUCCESS,
							rn_buff.Value() };
							gahp_output_return (commands, 2);

						// Print each result line
						char * next;
						result_list.rewind();
						while ((next = result_list.next()) != NULL) {
							printf ("%s\n", next);
							fflush(stdout);
							result_list.deleteCurrent();
						}

						new_results_signaled = FALSE;
					} else if (strcasecmp (argv[0], GAHP_COMMAND_VERSION) == 0) {
						printf ("S %s\n", version);
						fflush (stdout);
					} else if (strcasecmp (argv[0], GAHP_COMMAND_QUIT) == 0) {
						exit_signaled = TRUE;
						gahp_output_return_success();
					} else if (strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0) {
						async_mode = TRUE;
						new_results_signaled = FALSE;
						gahp_output_return_success();
					} else if (strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
						async_mode = FALSE;
						gahp_output_return_success();
					} else if (strcasecmp (argv[0], GAHP_COMMAND_QUIT) == 0) {
						gahp_output_return_success();
						return 0; // exit
					} else if (strcasecmp (argv[0], GAHP_COMMAND_COMMANDS) == 0) {
						const char * commands [] = {
							GAHP_RESULT_SUCCESS,
							GAHP_COMMAND_JOB_SUBMIT,
							GAHP_COMMAND_JOB_REMOVE,
							GAHP_COMMAND_JOB_STATUS_CONSTRAINED,
							GAHP_COMMAND_JOB_UPDATE_CONSTRAINED,
							GAHP_COMMAND_JOB_UPDATE,
							GAHP_COMMAND_JOB_HOLD,
							GAHP_COMMAND_JOB_RELEASE,
							GAHP_COMMAND_JOB_STAGE_IN,
							GAHP_COMMAND_JOB_STAGE_OUT,
							GAHP_COMMAND_JOB_REFRESH_PROXY,
							GAHP_COMMAND_ASYNC_MODE_ON,
							GAHP_COMMAND_ASYNC_MODE_OFF,
							GAHP_COMMAND_RESULTS,
							GAHP_COMMAND_QUIT,
							GAHP_COMMAND_VERSION,
							GAHP_COMMAND_COMMANDS};
						gahp_output_return (commands, 15);
					} else {
						// Pass it on to the worker thread
						// Actually buffer it, until the worker says it's ready
						queue_request (command);
						gahp_output_return_success();
					}
				} else {
					gahp_output_return_error();
				}

				delete [] argv;
				delete line;
			} else {
				// We read in 0 bytes, but the select on this fd triggered
				// Therefore the pipe must be closed -> we're done
				if (stdin_buffer.IsError())
					return 1;
			}
		} // fi FD_ISSET (stdin)


		if (is_ready[2]) {
			// Worker is ready for more requests
			char dummy;
			worker_ready=true;
			read (request_ack_buffer.getFd(), &dummy, 1);	// Swallow the "ready" signal
		}
		if(worker_ready) {
			if (flush_next_request(inter_thread_io->request_pipe[1])) {
				worker_ready=false;
			}
		}

		if (is_ready[1]) {
			MyString * line = NULL;
			if ((line = result_buffer.GetNextLine()) != NULL) {

				dprintf (D_FULLDEBUG, "Master received:\"%s\"\n", line->Value());

				// Add this to the list
				result_list.append (line->Value());

				if (async_mode) {
					if (!new_results_signaled) {
						printf ("R\n");
						fflush (stdout);
					}
					new_results_signaled = TRUE;	// So that we only do it once
				}

				delete line;
			}

			if (result_buffer.IsError()) {
				return 1;
			}
		}

	} while (!exit_signaled);

	return 0;
}


// Check the parameters to make sure the command
// is syntactically correct
int
verify_gahp_command(char ** argv, int argc) {

	if (strcasecmp (argv[0], GAHP_COMMAND_JOB_REMOVE) ==0 ||
		strcasecmp (argv[0], GAHP_COMMAND_JOB_HOLD) ==0 ||
		strcasecmp (argv[0], GAHP_COMMAND_JOB_RELEASE) ==0) {
		// Expecting:GAHP_COMMAND_JOB_REMOVE <req_id> <schedd_name> <job_id> <reason>
		return verify_number_args (argc, 5) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_job_id (argv[3]);

	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_STATUS_CONSTRAINED) ==0) {
		// Expected: CONDOR_JOB_STATUS_CONSTRAINED <req id> <schedd> <constraint>
		return verify_number_args (argc, 4) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_constraint (argv[3]);
	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_UPDATE_CONSTRAINED) == 0) {
		// Expected: CONDOR_JOB_UPDATE_CONSTRAINED <req id> <schedd name> <constraint> <update ad>
		return verify_number_args (argc, 5) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_constraint (argv[3]) &&
				verify_class_ad (argv[4]);

		return TRUE;
	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_UPDATE) == 0) {
		// Expected: CONDOR_JOB_UPDATE <req id> <schedd name> <job id> <update ad>
		return verify_number_args (argc, 5) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_job_id (argv[3]) &&
				verify_class_ad (argv[4]);

		return TRUE;
	} else if ((strcasecmp (argv[0], GAHP_COMMAND_JOB_SUBMIT) == 0) ||
			   (strcasecmp (argv[0], GAHP_COMMAND_JOB_STAGE_IN) == 0) ) {
		// Expected: CONDOR_JOB_SUBMIT <req id> <schedd name> <job ad>
		return verify_number_args (argc, 4) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_class_ad (argv[3]);

		return TRUE;
	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_STAGE_OUT) == 0) {
		// Expected: CONDOR_JOB_STAGE_OUT <req id> <schedd name> <job id>
		return verify_number_args (argc, 4) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_job_id (argv[3]);

		return TRUE;
	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_REFRESH_PROXY) == 0) {
		// Expecting:GAHP_COMMAND_JOB_REFRESH_PROXY <req_id> <schedd_name> <job_id> <proxy file>
		return verify_number_args (argc, 5) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_job_id (argv[3]);

	} else if (strcasecmp (argv[0], GAHP_COMMAND_RESULTS) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_VERSION) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_COMMANDS) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_QUIT) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
	    // These are no-arg commands
	    return verify_number_args (argc, 1);
	}


	dprintf (D_ALWAYS, "Unknown command\n");

	return FALSE;
}

int
verify_number_args (const int is, const int should_be) {
	if (is != should_be) {
		dprintf (D_ALWAYS, "Wrong # of args %d, should be %d\n", is, should_be);
		return FALSE;
	}
	return TRUE;
}

int
verify_request_id (const char * s) {
    unsigned int i;
	for (i=0; i<strlen(s); i++) {
		if (!isdigit(s[i])) {
			dprintf (D_ALWAYS, "Bad request id %s\n", s);
			return FALSE;
		}
	}

	return TRUE;
}

int
verify_schedd_name (const char * s) {
	// TODO: Check against our schedd name, we can only accept one schedd
    return (s != NULL) && (strlen(s) > 0);
}

int
verify_constraint (const char * s) {
	// TODO: How can we verify a constraint?
    return (s != NULL) && (strlen(s) > 0);
}
int
verify_class_ad (const char * s) {
	// TODO: How can we verify XML?
	return (s != NULL) && (strlen(s) > 0);
}

int
verify_job_id (const char * s) {
    int dot_count = 0;
    int ok = TRUE;
    unsigned int i;
    for (i=0; i<strlen (s); i++) {
		if (s[i] == '.') {
			dot_count++;
			if ((dot_count > 1) || (i==0) || (s[i+1] == '\0')) {
				ok = FALSE;
				break;
			}
		} else if (!isdigit(s[i])) {
			ok = FALSE;
			break;
		}
	}

	if ((!ok) || (dot_count != 1)) {
		dprintf (D_ALWAYS, "Bad job id %s\n", s);
		return FALSE;
	}

	return TRUE;
}




int
parse_gahp_command (const char* raw, char *** _argv, int * _argc) {

	*_argv = NULL;
	*_argc = 0;

	if (!raw) {
		dprintf(D_ALWAYS,"ERROR parse_gahp_command: empty command\n");
		return FALSE;
	}

	char ** argv = (char**)calloc (10,sizeof(char*)); 	// Max possible number of arguments
	int argc = 0;

	int beginning = 0;

	int len=strlen(raw);

	char * buff = (char*)malloc(len+1);
	int buff_len = 0;

	for (int i = 0; i<len; i++) {

		if ( raw[i] == '\\' ) {
			i++; 			//skip this char
			if (i<(len-1))
				buff[buff_len++] = raw[i];
			continue;
		}

		/* Check if charcater read was whitespace */
		if ( raw[i]==' ' || raw[i]=='\t' || raw[i]=='\r' || raw[i] == '\n') {

			/* Handle Transparency: we would only see these chars
			if they WEREN'T escaped, so treat them as arg separators
			*/
			buff[buff_len++] = '\0';
			argv[argc++] = strdup(buff);
			buff_len = 0;	// re-set temporary buffer

			beginning = i+1; // next char will be one after whitespace
		}
		else {
			// It's just a regular character, save it
			buff[buff_len++] = raw[i];
		}
	}

	/* Copy the last portion */
	buff[buff_len++] = '\0';
	argv[argc++] = strdup (buff);

	*_argv = argv;
	*_argc = argc;

	free (buff);
	return TRUE;


}

void
queue_request (const char * request) {
	request_out_buffer.Append (strdup(request));
}

int
flush_next_request(int fd) {
	request_out_buffer.Rewind();
	char * command;
	if (request_out_buffer.Next(command)) {
		dprintf (D_FULLDEBUG, "Sending %s to worker\n", command);
		write (  fd, command, strlen (command));
		write (  fd, "\n", 1);

		request_out_buffer.DeleteCurrent();
		free (command);
		return TRUE;
	}

	return FALSE;
}

void
gahp_output_return (const char ** results, const int count) {
	int i=0;

	for (i=0; i<count; i++) {
		printf ("%s", results[i]);
		if (i < (count - 1 )) {
			printf (" ");
		}
	}


	printf ("\n");
	fflush(stdout);
}

void
gahp_output_return_success() {
	const char* result[] = {GAHP_RESULT_SUCCESS};
	gahp_output_return (result, 1);
}

void
gahp_output_return_error() {
	const char* result[] = {GAHP_RESULT_ERROR};
	gahp_output_return (result, 1);
}
