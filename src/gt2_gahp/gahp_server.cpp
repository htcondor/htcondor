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

#ifndef WIN32
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>

#include "config.h"

#include "globus_gram_protocol.h"
#include "globus_gram_client.h"
#include "globus_gass_server_ez.h"
#include "globus_gss_assist.h"

	/* Define this if the gahp server should fork before dropping core */
#undef FORK_FOR_CORE

// WIN32 doesn't have strcasecmp
#ifdef WIN32
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#define NULLSTRING "NULL"

	/* Macro used over and over w/ bad syntax */
#define HANDLE_SYNTAX_ERROR()		\
	{								\
	gahp_printf("E\n");		\
	gahp_sem_up(&print_control);	\
	all_args_free(user_arg);		\
	}

const char *commands_list = 
"COMMANDS "
"GASS_SERVER_INIT " 
"GRAM_CALLBACK_ALLOW "
"GRAM_ERROR_STRING "
"GRAM_JOB_CALLBACK_REGISTER "
"GRAM_JOB_CANCEL "
"GRAM_JOB_REQUEST "
"GRAM_JOB_SIGNAL "
"GRAM_JOB_STATUS "
"GRAM_PING "
"INITIALIZE_FROM_FILE "
"QUIT "
"RESULTS "
"VERSION "
"ASYNC_MODE_ON "
"ASYNC_MODE_OFF "
"RESPONSE_PREFIX "
"REFRESH_PROXY_FROM_FILE "
"CACHE_PROXY_FROM_FILE "
"USE_CACHED_PROXY "
"UNCACHE_PROXY "
"GRAM_JOB_REFRESH_PROXY "
"GRAM_GET_JOBMANAGER_VERSION";
/* The last command in the list should NOT have a space after it */

typedef struct gahp_semaphore {
	globus_mutex_t mutex;
} gahp_semaphore;
	
typedef struct ptr_ref_count {
	char *key;
	volatile int count;
	gss_cred_id_t cred;
} ptr_ref_count;

typedef struct user_arg_struct {
	char *req_id;
	ptr_ref_count * cred;
	globus_gram_client_attr_t gram_attr;
} user_arg_t;

/* These are internal Globus structures included here for hack */
#include "internal.h"

/* GLOBALS */
globus_fifo_t result_fifo;
gahp_semaphore print_control;
gahp_semaphore fifo_control;
globus_io_handle_t stdin_handle;
globus_hashtable_t handle_cache;

ptr_ref_count * current_cred = NULL;

/*
   !!!! NOTE !!!!
   The "\\" in this version string is essential for proper functioning
   of the gahp server.  The spaces in the final part ("UW Gahp") need
   to be escaped or the gahp server gets confused. :(
   !!! BEWARE !!!
*/ 
const char *VersionString ="$GahpVersion: 1.1.3 " __DATE__ " UW\\ Gahp $";

#define NUM_GASS_LISTENERS		20

volatile int ResultsPending;
volatile int AsyncResults;
volatile int GlobusActive;
globus_gass_transfer_listener_t gassServerListeners[NUM_GASS_LISTENERS];
char *ResponsePrefix = NULL;

/* These are all the command handlers. */
int handle_gram_job_request(void *);
int handle_gram_job_cancel(void *);
int handle_gram_job_status(void *);
int handle_gram_job_signal(void *);
int handle_gram_job_callback_register(void *);
int handle_gram_job_refresh_proxy(void *);
int handle_gass_server_init(void *);
int handle_gram_ping(void *);
int handle_gram_get_jobmanager_version(void*);

/* These are all of the callbacks for non-blocking async commands */
void
callback_gram_job_request(void *arg,
						  globus_gram_protocol_error_t operation_fc,
						  const char *job_contact,
						  globus_gram_protocol_job_state_t job_state,
						  globus_gram_protocol_error_t job_fc);
void
callback_gram_job_signal(void *arg,
						 globus_gram_protocol_error_t operation_fc,
						 const char *job_contact,
						 globus_gram_protocol_job_state_t job_state,
						 globus_gram_protocol_error_t job_fc);
void
callback_gram_job_status(void *arg,
						 globus_gram_protocol_error_t operation_fc,
						 const char *job_contact,
						 globus_gram_protocol_job_state_t job_state,
						 globus_gram_protocol_error_t job_fc);
void
callback_gram_ping(void *arg,
						 globus_gram_protocol_error_t operation_fc,
						 const char *job_contact,
						 globus_gram_protocol_job_state_t job_state,
						 globus_gram_protocol_error_t job_fc);
void
callback_gram_job_cancel(void *arg,
						 globus_gram_protocol_error_t operation_fc,
						 const char *job_contact,
						 globus_gram_protocol_job_state_t job_state,
						 globus_gram_protocol_error_t job_fc);
void
callback_gram_job_callback_register(void *arg,
									globus_gram_protocol_error_t operation_fc,
									const char *job_contact,
									globus_gram_protocol_job_state_t job_state,
									globus_gram_protocol_error_t job_fc);
void
callback_gram_job_refresh_proxy(void *arg,
								globus_gram_protocol_error_t operation_fc,
								const char *job_contact,
								globus_gram_protocol_job_state_t job_state,
								globus_gram_protocol_error_t job_fc);
void
callback_gram_get_jobmanager_version(void *arg,
									 const char *job_contact,
									 globus_gram_client_job_info_t *job_info);

/* These are all of the sync. command handlers */
int handle_gram_callback_allow(void *);
int handle_gram_error_string(void *);
int handle_async_mode_on(void *);
int handle_async_mode_off(void *);
int handle_response_prefix(void *);
int handle_refresh_proxy_from_file(void *);

/* This is the handler for job status update callbacks from the jobmanager */
void gram_callback_handler(void *callback_arg, char *job_contact, int state,
						   int error);

int main_activate_globus();
void main_deactivate_globus();

void gahp_sem_init( gahp_semaphore *, int initial_value);
void gahp_sem_up(  gahp_semaphore *);
void gahp_sem_down( gahp_semaphore * );
void unlink_ref_count( ptr_ref_count* , int );
user_arg_t * new_gram_arg( const char *req_id, ptr_ref_count *cred );
void delete_gram_arg( user_arg_t *gram_arg );

void enqueue_results( char *result_line );

int gahp_printf(const char *format, ...);

/* Escape spaces replaces ' ' with '\ '. It allocates memory, and 
   puts the escaped string in that memory, and returns it. A NULL is
   returned if something (really, just memory allocation) fails. 
   The caller is responsible for freeing the memory returned by this func 
*/ 
char *escape_spaces( const char *input_line );

/* all_args_free frees all the memory passed into a command handler */
void all_args_free( void *);

/* These routines verify that the ascii argument is either a number or an */
/* string - they do not allocate any memory - by default, output_line will */
/* be a pointer pointer to input_line, or NULL if input_line=="NULL" */

int process_string_arg( char *input_line, char **output_line);
int process_int_arg( char *input_line, int *result );

user_arg_t *
new_gram_arg( const char *req_id, ptr_ref_count *cred )
{
	int result;
	user_arg_t *gram_arg = (user_arg_t *) malloc( sizeof(user_arg_t) );
	if ( req_id ) {
		gram_arg->req_id = strdup( req_id );
	} else {
		gram_arg->req_id = NULL;
	}
	gram_arg->cred = cred;
	result = globus_gram_client_attr_init( &gram_arg->gram_attr );
	if ( result != GLOBUS_SUCCESS ) {
		gahp_printf( "F globus_gram_client_attr_init\\ failed!\n" );
		_exit( 1 );
	}
	if ( cred ) {
		result = globus_gram_client_attr_set_credential( gram_arg->gram_attr,
														 cred->cred );
		if ( result != GLOBUS_SUCCESS ) {
			gahp_printf( "F globus_gram_client_attr_set_credential\\ failed!\n" );
			_exit( 1 );
		}
		cred->count++;
	}
	return gram_arg;
}

void
delete_gram_arg( user_arg_t *gram_arg )
{
	if ( gram_arg->req_id ) {
		free( gram_arg->req_id );
	}
	if ( gram_arg->cred ) {
		unlink_ref_count( gram_arg->cred, 1 );
	}
	globus_gram_client_attr_destroy( &gram_arg->gram_attr );
	free( gram_arg );
}

int
gahp_printf(const char *format, ...)
{
	int ret_val;
	va_list ap;
	char buf[10000];
	int actual;

	globus_libc_lock();

	va_start(ap, format);
	actual = vsnprintf(buf, sizeof(buf)/sizeof(buf[0]), format, ap);
	va_end(ap);
	if(actual >= (int)(sizeof(buf)/sizeof(buf[0]))) {
		printf("gahp_printf\\ failed\\ sanity\\ check!!!\n");
		_exit(7);
	}

	if (ResponsePrefix) {
		ret_val = printf("%s%s",ResponsePrefix,buf);
	} else {
		ret_val = printf("%s",buf);
	}

	fflush(stdout);

	globus_libc_unlock();
	
	return ret_val;
}


void
gahp_sem_init( gahp_semaphore *sem, int /*initial_value*/) 
{
	globus_mutex_init(&sem->mutex, NULL);
}

void
gahp_sem_up( gahp_semaphore *sem)
{
	globus_mutex_unlock(&sem->mutex);
}

void
gahp_sem_down( gahp_semaphore *sem)
{
	globus_mutex_lock(&sem->mutex);
}


int 
process_string_arg( char *input_line, char **output)
{
	int i = 0;

	// if it's a NULL pointer, or it points to something that's zero
	// length, give up now.
    if(!input_line){
        return false;
    }
	i = strlen(input_line);
	if(!i) {
		return false;
	}

	// by default, just give back what they gave us.
	*output = input_line;
	// some commands are allowed to pass in "NULL" as their option. If this
	// is the case, set output to point to a NULL string, which we will pass
	// along to the globus routines...

    if(!strcasecmp(input_line, NULLSTRING)) {
    	*output = NULL;  
	}

	return true;
}

int 
process_int_arg( char *input, int *output)
{
	if( input && strlen(input)) {
		*output = atoi(input);
		return true;
	}
	return false;
}

void
enqueue_results( char *output) 
{
	gahp_sem_down(&print_control);
    gahp_sem_down(&fifo_control);
    globus_fifo_enqueue(&result_fifo, (void *)output);

	if(AsyncResults && !ResultsPending) {
		ResultsPending = 1;
		gahp_printf("R\n");
	}
	gahp_sem_up(&fifo_control);
	gahp_sem_up(&print_control);
	return;
}

char *
escape_spaces( const char *input_line) 
{
	int i;
	const char *temp;
	char *output_line;

	// first, count up the spaces
	temp = input_line;
	for(i = 0; *temp != '\0'; temp++) {
		if( *temp == ' ' || *temp == '\r' || *temp =='\n' || *temp == '\\' ) {
			i++;
		}
	}

	// get enough space to store it.  	
	output_line = (char *)malloc(strlen(input_line) + i + 200);

	// now, blast across it
	temp = input_line;
	for(i = 0; *temp != '\0'; temp++) {
		if( *temp == ' ' || *temp == '\r' || *temp =='\n' || *temp == '\\' ) {
			output_line[i] = '\\'; 
			i++;
		}
		output_line[i] = *temp;
		i++;
	}
	output_line[i] = '\0';
	// the caller is responsible for freeing this memory, not us
	return output_line;	
}

void
all_args_free( void * args)
{
	char **input_line = (char **) args;

	/* if we get a NULL pointer, give up now */
	if(!input_line) 
		return;

	/* input_line is a array of char*'s from read_command */
	/* it's a vector of command line arguemts */
	/* The last one will be null, so walk it until we hit the end */
	/* If this was a real library call this would be bad, since we */
	/* trust how we got this memory. */

	while(*input_line) {
		free(*input_line);
		input_line++;
	}
	free(args);
	return;	
}

int
handle_bad_request(void * user_arg)
{
	HANDLE_SYNTAX_ERROR();

	return 0;
}


int
handle_gram_job_request(void * user_arg)
{

	char **input_line = (char **) user_arg;
	int result;
	user_arg_t * gram_arg;

	// what the arguments get processed into
	char *req_id, *resource_contact, *callback_contact, *rsl;
	int limited_deleg;

	// req id
	if( !process_string_arg(input_line[1], &req_id ) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	
	// resource contact string
	if( !process_string_arg(input_line[2], &resource_contact) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	// callback contact
	if( !process_string_arg(input_line[3], &callback_contact ) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	 
	// should delegated proxy be limited?
	if( !process_int_arg(input_line[4], &limited_deleg ) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	// rsl
	if( !process_string_arg(input_line[5], &rsl) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	gram_arg = new_gram_arg( req_id, current_cred );

	if ( limited_deleg ) {
		globus_gram_client_attr_set_delegation_mode( gram_arg->gram_attr,
							GLOBUS_IO_SECURE_DELEGATION_MODE_LIMITED_PROXY );
	} else {
		globus_gram_client_attr_set_delegation_mode( gram_arg->gram_attr,
							GLOBUS_IO_SECURE_DELEGATION_MODE_FULL_PROXY );
	}

	gahp_printf("S\n");
	gahp_sem_up(&print_control);
	result = globus_gram_client_register_job_request(resource_contact, rsl, 
							GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL, 
							callback_contact, gram_arg->gram_attr,
							callback_gram_job_request, (void *)gram_arg);

	if (result != GLOBUS_SUCCESS) {
		callback_gram_job_request((void *)gram_arg, (globus_gram_protocol_error_t)result, NULL, (globus_gram_protocol_job_state_t) 0, (globus_gram_protocol_error_t) 0);
	}

	all_args_free(user_arg);
	return 0;
}

void
callback_gram_job_request(void *arg,
						  globus_gram_protocol_error_t operation_fc,
						  const char *job_contact,
						  globus_gram_protocol_job_state_t /*job_state*/,
						  globus_gram_protocol_error_t /*job_fc*/)
{
	char *output;
	user_arg_t * gram_arg;

	gram_arg = (user_arg_t *)arg;

	output = (char *)malloc(500);

	sprintf(output, "%s %d %s", gram_arg->req_id, operation_fc,
			job_contact ? job_contact : NULLSTRING);

	enqueue_results(output);	

	delete_gram_arg( gram_arg );
	return;
}

int
handle_gram_job_signal(void * user_arg)
{
	char **input_line = (char **) user_arg;
	int result;

	// what the arguments get processed into
	int signal;
	char *req_id, *job_contact,  *signal_string;
	user_arg_t * gram_arg;

	// reqid
	if( !process_string_arg(input_line[1], &req_id) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	// job contact
	if( !process_string_arg(input_line[2], &job_contact ) ){
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	// signal
	if( !process_int_arg(input_line[3], &signal)  ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if( !process_string_arg(input_line[4], &signal_string) )	{
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	
	gram_arg = new_gram_arg( req_id, current_cred );

	gahp_printf("S\n");
	gahp_sem_up(&print_control);
	result = globus_gram_client_register_job_signal(job_contact, ( globus_gram_protocol_job_signal_t) signal,
									signal_string,
									gram_arg->gram_attr,
									callback_gram_job_signal, (void *)gram_arg);

	if (result != GLOBUS_SUCCESS) {
		callback_gram_job_signal((void *)gram_arg, (globus_gram_protocol_error_t)result, NULL, (globus_gram_protocol_job_state_t)0, (globus_gram_protocol_error_t)0);
	}

	all_args_free(user_arg);
	return 0;
}

void
callback_gram_job_signal(void *arg,
						 globus_gram_protocol_error_t operation_fc,
						 const char * /*job_contact*/,
						 globus_gram_protocol_job_state_t job_state,
						 globus_gram_protocol_error_t job_fc)
{
	char *output;
	user_arg_t * gram_arg;

	gram_arg = (user_arg_t *)arg;

	output = (char *)malloc(500);

	sprintf(output, "%s %d %d %d", gram_arg->req_id, operation_fc,
			job_fc, job_state);

	enqueue_results(output);	

	delete_gram_arg( gram_arg );
	return;
}

int
handle_gram_job_status(void * user_arg)
{
	char **input_line = (char **) user_arg;
	int result;
	user_arg_t * gram_arg;

	// what the arguments get processed into
	char *req_id, *job_contact;

	if( !process_string_arg(input_line[1], &req_id) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if( !process_string_arg(input_line[2], &job_contact) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	gram_arg = new_gram_arg( req_id, current_cred );

	gahp_printf("S\n");
	gahp_sem_up(&print_control);
	result = globus_gram_client_register_job_status(job_contact,
									gram_arg->gram_attr,
									callback_gram_job_status, (void *)gram_arg);

	if (result != GLOBUS_SUCCESS) {
		callback_gram_job_status((void *)gram_arg, (globus_gram_protocol_error_t)result, NULL, (globus_gram_protocol_job_state_t)0, (globus_gram_protocol_error_t)0);
	}

	all_args_free(user_arg);
	return 0;
}

void
callback_gram_job_status(void *arg,
						 globus_gram_protocol_error_t operation_fc,
						 const char * /*job_contact*/,
						 globus_gram_protocol_job_state_t job_state,
						 globus_gram_protocol_error_t job_fc)
{
	char *output;
	user_arg_t * gram_arg;

	gram_arg = (user_arg_t *)arg;

	output = (char *)malloc(500);

	sprintf(output, "%s %d %d %d", gram_arg->req_id, operation_fc,
			job_fc, job_state);

	enqueue_results(output);	

	delete_gram_arg( gram_arg );
	return;
}

int
handle_gram_job_cancel(void * user_arg)
{
	char **input_line = (char **) user_arg;
	int result;
	user_arg_t * gram_arg;

	// what the arguments get processed into
	char *req_id, *job_contact;

	if( !process_string_arg(input_line[1], &req_id) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if( !process_string_arg(input_line[2], &job_contact) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	gram_arg = new_gram_arg( req_id, current_cred );

	gahp_printf("S\n");
	gahp_sem_up(&print_control);
	result = globus_gram_client_register_job_cancel(job_contact,
									gram_arg->gram_attr,
									callback_gram_job_cancel, (void *)gram_arg);

	if (result != GLOBUS_SUCCESS) {
		callback_gram_job_cancel((void *)gram_arg, (globus_gram_protocol_error_t)result, NULL, (globus_gram_protocol_job_state_t)0, (globus_gram_protocol_error_t)0);
	}

	all_args_free(user_arg);
	return 0;
}

void
callback_gram_job_cancel(void *arg,
						 globus_gram_protocol_error_t operation_fc,
						 const char * /*job_contact*/,
						 globus_gram_protocol_job_state_t /*job_state*/,
						 globus_gram_protocol_error_t /*job_fc*/)
{
	char *output;
	user_arg_t * gram_arg;

	gram_arg = (user_arg_t *)arg;

	output = (char *)malloc(500);

	sprintf(output, "%s %d", gram_arg->req_id, operation_fc);

	enqueue_results(output);	

	delete_gram_arg( gram_arg );
	return;
}

int
version_in_range( globus_module_descriptor_t *module, int low_major,
				  int low_minor, int high_major, int high_minor )
{
	int rc;
	globus_version_t vers;
	rc = globus_module_get_version( module, &vers );
	if ( rc != GLOBUS_SUCCESS ) {
		fprintf( stderr, "Failed to retrieve Globus version!\n" );
		return 0;
	}
	int low = low_major * 100 + low_minor;
	int high = high_major * 100 + high_minor;
	int cur = vers.major * 100 + vers.minor;
	/*
	fprintf( stderr, "Low=%d.%d, High=%d.%d, Curr=%d.%d\n", low_major,
			 low_minor, high_major, high_minor, vers.major, vers.minor );
	*/
	return ( low <= cur ) && ( cur <= high );
}

int
handle_gass_server_init(void * user_arg)
{
	char **input_line = (char **) user_arg;
	int result,i;
	char *output, *req_id;
	char *gassServerUrl = NULL;
	int port;
	int num_listeners = NUM_GASS_LISTENERS;

	if( !process_string_arg(input_line[1], &req_id) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if( !process_int_arg(input_line[2], &port) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	gahp_printf("S\n");
	gahp_sem_up(&print_control);


 	result = globus_module_activate( GLOBUS_GASS_SERVER_EZ_MODULE );
    if ( result != GLOBUS_SUCCESS ) {
		/*
        globus_gram_client_callback_disallow( gramCallbackContact );
        globus_module_deactivate( GLOBUS_GRAM_CLIENT_MODULE );
        return false; */
    }

	// We use a crude hack to optimize GASS performance by doing up to
	// 20 security authentications in parallel. Normally, globus_io only
	// does 1 at a time per port. This requires digging into the bowels
	// of globus_xio and globus_io internal data structures. Newer
	// versions may change how the structures are layed out, since they're
	// supposed to be private. So if we see a version we don't recgonize,
	// disable our optimization.
	if ( !version_in_range( GLOBUS_XIO_MODULE, 2, 8, 3, 6 ) ||
		 !version_in_range( GLOBUS_IO_MODULE, 6, 3, 9, 5 ) ||
		 !version_in_range( GLOBUS_GASS_TRANSFER_MODULE, 4, 3, 7, 2 ) ) {
		fprintf( stderr, "Unexpected module version, using low-performance GASS server!\n" );
		num_listeners = 1;
	}

		/* TODO This code requires 20 free ports to initialize, but only one
		 *   port afterwards. If we're port-restricted, this may be a
		 *   problem. We should close the additional ports as we're
		 *   initializing the gass listeners.
		 */
    result = GLOBUS_SUCCESS;
    for(i=0;i<num_listeners;i++) {
      int res;
      if (i==0) {
        res = globus_gass_server_ez_init( &gassServerListeners[i], NULL, NULL, NULL,
                                      GLOBUS_GASS_SERVER_EZ_READ_ENABLE |
                                      GLOBUS_GASS_SERVER_EZ_LINE_BUFFER |
                                      GLOBUS_GASS_SERVER_EZ_WRITE_ENABLE,
                                      NULL );
      } else {
        res = my_globus_gass_server_ez_init( &gassServerListeners[i], NULL, NULL, NULL,  
                                      GLOBUS_GASS_SERVER_EZ_READ_ENABLE |
                                      GLOBUS_GASS_SERVER_EZ_LINE_BUFFER |
                                      GLOBUS_GASS_SERVER_EZ_WRITE_ENABLE,
                                      NULL );
      }
      if (res != GLOBUS_SUCCESS) result=GLOBUS_FAILURE;
    }

    output = (char *)malloc(500);

    if (result == GLOBUS_SUCCESS && num_listeners > 1)
    {
      globus_gass_transfer_listener_struct_t *l0 = (globus_gass_transfer_listener_struct_t *) globus_handle_table_lookup(&globus_i_gass_transfer_listener_handles, gassServerListeners[0]);
      int l0_fd = ((my_globus_l_server_t*)l0->proto->handle->xio_server->entry[1].server_handle)->listener_fd;

      for(i=1;i<num_listeners;i++) {
        globus_gass_transfer_listener_struct_t *l = (globus_gass_transfer_listener_struct_t *)
          globus_handle_table_lookup(&globus_i_gass_transfer_listener_handles, gassServerListeners[i]);

        close( ((my_globus_l_server_t*)l->proto->handle->xio_server->entry[1].server_handle)->listener_fd );
        ((my_globus_l_server_t*)l->proto->handle->xio_server->entry[1].server_handle)->listener_fd = l0_fd;
        ((my_globus_l_server_t*)l->proto->handle->xio_server->entry[1].server_handle)->listener_system->fd = l0_fd;
      }
    }
    if (result == GLOBUS_SUCCESS) {
      gassServerUrl = globus_gass_transfer_listener_get_base_url(gassServerListeners[0]);
    }

	// TODO: need to account for gassServerUrl being NULL
	sprintf(output, "%s %d %s", req_id, result,
			gassServerUrl ? gassServerUrl : NULLSTRING);

	enqueue_results(output);	

	all_args_free(user_arg);
	return 0;
}

int
handle_gram_job_callback_register(void * user_arg)
{
	char **input_line = (char **) user_arg;
	int result;
	user_arg_t * gram_arg;

	// what the arguments get processed into
	char *req_id, *job_contact, *callback_contact;

	if( !process_string_arg(input_line[1], &req_id) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if( !process_string_arg(input_line[2], &job_contact) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if( !process_string_arg(input_line[3], &callback_contact) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	gram_arg = new_gram_arg( req_id, current_cred );

	gahp_printf("S\n");
	gahp_sem_up(&print_control);

	result = globus_gram_client_register_job_callback_registration(
									job_contact,
									GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL,
									callback_contact, 
									gram_arg->gram_attr,
									callback_gram_job_callback_register,
									(void *)gram_arg);

	if (result != GLOBUS_SUCCESS) {
		callback_gram_job_callback_register((void *)gram_arg, (globus_gram_protocol_error_t)result, NULL, (globus_gram_protocol_job_state_t)0, (globus_gram_protocol_error_t)0);
	}

	all_args_free(user_arg);
	return 0;
}

void
callback_gram_job_callback_register(void *arg,
									globus_gram_protocol_error_t operation_fc,
									const char * /*job_contact*/,
									globus_gram_protocol_job_state_t job_state,
									globus_gram_protocol_error_t job_fc)
{
	char *output;
	user_arg_t * gram_arg;

	gram_arg = (user_arg_t *)arg;

	output = (char *)malloc(500);

	sprintf(output, "%s %d %d %d", gram_arg->req_id, operation_fc,
			job_fc, job_state);

	enqueue_results(output);	

	delete_gram_arg( gram_arg );
	return;
}

int
handle_gram_ping(void * user_arg)
{
	char **input_line = (char **) user_arg;
	int result;
	char *req_id, *resource_contact;
	user_arg_t * gram_arg;

	if( !process_string_arg(input_line[1], &req_id) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if( !process_string_arg(input_line[2], &resource_contact ) ){
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	gram_arg = new_gram_arg( req_id, current_cred );

	gahp_printf("S\n");
	gahp_sem_up(&print_control);
	//result = globus_gram_client_ping(resource_contact);
	result = globus_gram_client_register_ping(resource_contact,
									gram_arg->gram_attr,
									callback_gram_ping, (void *)gram_arg);

	if (result != GLOBUS_SUCCESS) {
		callback_gram_ping((void *)gram_arg, (globus_gram_protocol_error_t)result, NULL, (globus_gram_protocol_job_state_t)0, (globus_gram_protocol_error_t)0);
	}

	all_args_free(user_arg);
	return 0;

}

void
callback_gram_ping(void *arg,
						 globus_gram_protocol_error_t operation_fc,
						 const char * /*job_contact*/,
						 globus_gram_protocol_job_state_t /*job_state*/,
						 globus_gram_protocol_error_t /*job_fc*/)
{
	char *output;
	user_arg_t * gram_arg;

	gram_arg = (user_arg_t *)arg;

	output = (char *)malloc(500);

	sprintf(output, "%s %d", gram_arg->req_id, operation_fc);

	enqueue_results(output);	

	delete_gram_arg( gram_arg );
	return;
}

int
handle_gram_job_refresh_proxy(void * user_arg)
{
	char **input_line = (char **) user_arg;
	int result;
	user_arg_t * gram_arg;
	gss_cred_id_t refreshing_cred = GSS_C_NO_CREDENTIAL;

	// what the arguments get processed into
	char *req_id, *job_contact;
	int limited_deleg = 1;

	// reqid
	if( !process_string_arg(input_line[1], &req_id) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	// job contact
	if( !process_string_arg( input_line[2], &job_contact ) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if( input_line[3] && !process_int_arg( input_line[3], &limited_deleg ) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	gram_arg = new_gram_arg( req_id, current_cred );
	if ( current_cred ) {
		refreshing_cred = current_cred->cred;
	}

	if ( limited_deleg ) {
		globus_gram_client_attr_set_delegation_mode( gram_arg->gram_attr,
							GLOBUS_IO_SECURE_DELEGATION_MODE_LIMITED_PROXY );
	} else {
		globus_gram_client_attr_set_delegation_mode( gram_arg->gram_attr,
							GLOBUS_IO_SECURE_DELEGATION_MODE_FULL_PROXY );
	}

	gahp_printf( "S\n" );
	gahp_sem_up( &print_control );
	result = globus_gram_client_register_job_refresh_credentials( job_contact,
									refreshing_cred,
									gram_arg->gram_attr,
									callback_gram_job_refresh_proxy,
									(void *)gram_arg);

	if (result != GLOBUS_SUCCESS) {
		callback_gram_job_refresh_proxy((void *)gram_arg, (globus_gram_protocol_error_t)result, NULL, (globus_gram_protocol_job_state_t)0, (globus_gram_protocol_error_t)0);
	}

	all_args_free( user_arg );
	return 0;
}

void
callback_gram_job_refresh_proxy(void *arg,
								globus_gram_protocol_error_t operation_fc,
								const char * /*job_contact*/,
								globus_gram_protocol_job_state_t /*job_state*/,
								globus_gram_protocol_error_t /*job_fc*/)
{
	char *output;
	user_arg_t * gram_arg;

	gram_arg = (user_arg_t *)arg;

	output = (char *)malloc(500);

	sprintf(output, "%s %d", gram_arg->req_id, operation_fc);

	enqueue_results(output);	

	delete_gram_arg( gram_arg );
	return;
}

int
handle_gram_get_jobmanager_version(void * user_arg)
{
	char **input_line = (char **) user_arg;
	int result;
	char *req_id, *resource_contact;
	user_arg_t *gram_arg;

	if( !process_string_arg(input_line[1], &req_id) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if( !process_string_arg(input_line[2], &resource_contact ) ){
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	gram_arg = new_gram_arg( req_id, current_cred );

	globus_gram_client_attr_set_delegation_mode( gram_arg->gram_attr,
												 GLOBUS_IO_SECURE_DELEGATION_MODE_LIMITED_PROXY );

	gahp_printf("S\n");
	gahp_sem_up(&print_control);

	result = globus_gram_client_register_get_jobmanager_version( resource_contact,
																 gram_arg->gram_attr,
																 callback_gram_get_jobmanager_version,
																 (void *)gram_arg );

	if (result != GLOBUS_SUCCESS) {
		globus_gram_client_job_info_t client_info;
		client_info.protocol_error_code = result;
		callback_gram_get_jobmanager_version( (void *)gram_arg, NULL,
											  &client_info );
	}

	all_args_free(user_arg);
	return 0;
}

void
callback_gram_get_jobmanager_version(void *arg,
									 const char * /*job_contact*/,
									 globus_gram_client_job_info_t *job_info)
{
	char *esc_str;
	char *output;
	user_arg_t * gram_arg;

	gram_arg = (user_arg_t *)arg;

	output = (char *)malloc(10240);

	sprintf(output, "%s %d", gram_arg->req_id,
			job_info->protocol_error_code);

	if ( job_info->protocol_error_code == GLOBUS_SUCCESS ) {
		globus_gram_protocol_extension_t *entry = (globus_gram_protocol_extension_t *)globus_hashtable_first( &job_info->extensions );
		while ( entry ) {
			strcat( output, " " );
			strcat( output, entry->attribute );
			strcat( output, "=" );
			esc_str = escape_spaces( entry->value );
			strcat( output, esc_str );
			free( esc_str );
			entry = (globus_gram_protocol_extension_t*) globus_hashtable_next( &job_info->extensions );
		}
	}

	enqueue_results(output);	

	delete_gram_arg( gram_arg );
	return;
}

int 
handle_gram_callback_allow(void * user_arg)
{
	char **input_line = (char **) user_arg;
	int result, port;
	char *req_id, *callback_contact = NULL, *error_string = NULL;
	char *saved_req_id = NULL;

	if( !process_string_arg(input_line[1], &req_id) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	// what port should we try and use by default...
	if( !process_int_arg(input_line[2], &port) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	// We need to stash a copy of of the req_id so Globus can give it to us
	// in our callback

	saved_req_id = (char *) malloc(strlen(req_id) + 1);
	strcpy(saved_req_id, req_id);
	result = globus_gram_client_callback_allow(gram_callback_handler,
											   saved_req_id,
											   &callback_contact );

	if( result != GLOBUS_SUCCESS) {
		error_string = escape_spaces(globus_gram_client_error_string(result));
		gahp_printf("F %d %s\n",  result, error_string);
		free(error_string);
	} else {
		gahp_printf("S %s\n", callback_contact);
		free(callback_contact);
	}
	gahp_sem_up(&print_control);

	all_args_free(user_arg);
	return 0;
}

void
gram_callback_handler(void *callback_arg, char *job_contact, int state,
					  int error)
{
	char *output;

	output = (char *)malloc(500);

	sprintf(output, "%s %s %d %d", (char *)callback_arg,
			job_contact, state, error);

	enqueue_results(output);
}

int
handle_commands(void * user_arg)
{
	gahp_printf("S %s\n", commands_list);
	gahp_sem_up(&print_control);
	all_args_free(user_arg);
	return 0;
}

int 
handle_results(void * user_arg)
{
	char *output;
	int count, i;

	gahp_sem_down(&fifo_control);
	count = globus_fifo_size(&result_fifo);

	gahp_printf("S %d\n", count);
	if(count > 0) {
		for(i = 0; i < count; i++) {
			output = (char *) globus_fifo_dequeue(&result_fifo);
			gahp_printf("%s\n", output);
			free(output);
		}
	}
	ResultsPending = 0;
	gahp_sem_up(&fifo_control);
	gahp_sem_up(&print_control);

	all_args_free(user_arg);

	return 0;
}

int 
handle_version(void * user_arg)
{
	gahp_printf("S %s\n", VersionString);

	gahp_sem_up(&print_control);

	all_args_free(user_arg);
	return 0;
}

int
handle_gram_error_string( void * user_arg)
{
	char **input_line = (char **) user_arg;
	char *output;
	const char *error_string = NULL;
	int error_code;

    if( !process_int_arg(input_line[1], &error_code) ) {
		HANDLE_SYNTAX_ERROR();
        return 0;
    }

	error_string = globus_gram_client_error_string(error_code);

	if( error_string )	{
		output = escape_spaces( error_string );
		gahp_printf("S %s\n", output); 
		free(output);
	} else{
		gahp_printf("F Unknown\\ Error\n"); 
	}

	gahp_sem_up(&print_control);

	all_args_free(user_arg);
	return 0;

}

int
handle_async_mode_on( void * user_arg)
{
	gahp_sem_down(&fifo_control);
	AsyncResults = 1;
	ResultsPending = 0;
	gahp_printf("S\n"); 

	gahp_sem_up(&fifo_control);
	gahp_sem_up(&print_control);

	all_args_free(user_arg);
	return 0;

}

int
handle_async_mode_off( void * user_arg)
{
	gahp_sem_down(&fifo_control);
	AsyncResults = 0;
	ResultsPending = 0;
	gahp_printf("S\n"); 
	gahp_sem_up(&fifo_control);
	gahp_sem_up(&print_control);

	all_args_free(user_arg);
	return 0;

}

int
handle_refresh_proxy_from_file(void * user_arg)
{
	char **input_line = (char **) user_arg;
	char *file_name, *environ_variable;
	gss_cred_id_t credential_handle = GSS_C_NO_CREDENTIAL;
	OM_uint32 major_status; 
	OM_uint32 minor_status; 

	file_name = NULL;

	if(!process_string_arg(input_line[1], &file_name) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	// if setenv copies it's second argument, this leaks memory
	if(file_name) {
		environ_variable = (char *) malloc(strlen(file_name) + 1);
		strcpy(environ_variable, file_name); 
		setenv("X509_USER_PROXY", environ_variable, 1);
		free(environ_variable);
	}

	// this is a macro, not a function - hence the lack of a semicolon
	major_status = globus_gss_assist_acquire_cred(&minor_status,
					GSS_C_INITIATE, /* or GSS_C_ACCEPT */
					&credential_handle); 

	if (major_status != GSS_S_COMPLETE) 
	{ 
			gahp_printf("F Error\\ Codes:\\ %d\\ %d\n", major_status,
								minor_status);	
			gahp_sem_up(&print_control);
			all_args_free(user_arg);
			return 0;
	} 

	globus_gram_client_set_credentials(credential_handle);

	gahp_printf("S\n");

	gahp_sem_up(&print_control);

	all_args_free(user_arg);
	return 0;
}


int 
handle_quit(void * /*user_arg*/)
{
	gahp_printf("S\n");
	main_deactivate_globus();
	_exit(0);

// never reached
	gahp_sem_up(&print_control);
	return 0;
}

int
handle_initialize_from_file(void * user_arg)
{
	char **input_line = (char **) user_arg;
	char *file_name;
	int result;

	file_name = NULL;

	if(!process_string_arg(input_line[1], &file_name) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if(file_name) {
		setenv("X509_USER_PROXY", file_name, 1);
	}

	if ( (result=main_activate_globus()) != GLOBUS_SUCCESS ) {
		gahp_printf("F %d Failed\\ to\\ activate\\ Globus modules\n",
							result);

		gahp_sem_up(&print_control);
		all_args_free(user_arg);
		return 0;
	}

	GlobusActive = true;
	gahp_printf("S\n");

	gahp_sem_up(&print_control);

	all_args_free(user_arg);
	return 0;
}

void
unlink_ref_count( ptr_ref_count* ptr, int decrement )
{
	int result;

	if (!ptr) return;

	ptr->count -= decrement;
	if ( ptr->count < 0 )
		ptr->count = 0;
	if ( ptr->key == NULL && ptr->count == 0 ) {
		/* we are all done with this entry.  kill it. */
		/* First, sanity-check that it's not the active credential. */
		if ( current_cred == ptr ) {
			gahp_printf("unlink_ref_count\\ failed\\ sanity\\ check!!!\n");
			_exit(4);
		}
		gss_release_cred( (OM_uint32*) &result,&ptr->cred);
		free(ptr);
	}
}


void
uncache_proxy(char *key)
{
	ptr_ref_count * ptr;

	if (!key) return;
	ptr = (ptr_ref_count *) globus_hashtable_remove(&handle_cache,key);
	if ( ptr ) {
		if (ptr->key==NULL || key==ptr->key || strcasecmp(ptr->key,key)) {
			gahp_printf("uncache_proxy\\ failed\\ sanity\\ check!!!\n");
			_exit(5);
		}
		free(ptr->key);
		ptr->key = NULL;
		unlink_ref_count(ptr,0);
	}
}

int
handle_uncache_proxy(void * user_arg)
{
	char **input_line = (char **) user_arg;
	char *handle = NULL;


	if(!process_string_arg(input_line[1], &handle) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	uncache_proxy(handle);

	gahp_printf("S\n");

	gahp_sem_up(&print_control);

	all_args_free(user_arg);
	return 0;
}

int
use_cached_proxy(char *handle)
{
	ptr_ref_count * ptr = NULL;
	ptr_ref_count * old_current_cred = NULL;

	if ( strcasecmp(handle,"DEFAULT")!=0 ) {
		ptr = (ptr_ref_count *) globus_hashtable_lookup(&handle_cache,handle);
		if (!ptr) {
			/* desired handle not found */
			return -1;
		}
	}

	if ( ptr == current_cred ) {
		/* already using the desired proxy */
		return 0;
	}

	old_current_cred = current_cred;
	current_cred = ptr;

	unlink_ref_count(old_current_cred,1);


	if ( current_cred ) {
		current_cred->count += 1;
	}

	return 0;
}

int
handle_use_cached_proxy(void * user_arg)
{
	char **input_line = (char **) user_arg;
	char *handle = NULL;


	if(!process_string_arg(input_line[1], &handle) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if (use_cached_proxy(handle) < 0) {
		gahp_printf("F handle\\ not\\ found\n");
	} else {
		gahp_printf("S\n");
	}

	gahp_sem_up(&print_control);

	all_args_free(user_arg);
	return 0;
}

int
handle_cache_proxy_from_file(void * user_arg)
{
	char **input_line = (char **) user_arg;
	char *file_name = NULL;
	char *id = NULL;
	int result;
	gss_buffer_desc import_buf;
	int min_stat;
	gss_cred_id_t cred_handle;
	ptr_ref_count * ref;
	char buf_value[4096];

	if(!process_string_arg(input_line[1], &id) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if(!process_string_arg(input_line[2], &file_name) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if ( id == NULL || file_name == NULL ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	if ( strcasecmp("DEFAULT",file_name)==0 ) {
		gahp_printf("F \"DEFAULT\"\\ not\\ allowed\n");
		goto cache_proxy_return;
	}

	result = strlen(file_name);
	if ( result < 1 || result > 4000 ) {
		gahp_printf("F bad\\ file\\ name\n");
		goto cache_proxy_return;
	}

	sprintf(buf_value,"X509_USER_PROXY=%s",file_name);
	import_buf.value = buf_value;
	import_buf.length = strlen(buf_value) + 1;  /* add 1 for the null */

	result = gss_import_cred((OM_uint32*) &min_stat, &cred_handle, GSS_C_NO_OID,
					1, &import_buf, 0, NULL);

	if ( result != GSS_S_COMPLETE ) {
		gahp_printf("F Failed\\ to\\ import\\ credential\\ maj=%d\\ min=%d\n",
							result,min_stat);
		goto cache_proxy_return;
	}

	/* Clear this entry from our hash table */
	uncache_proxy(id);

	/* Put into hash table */
	ref = (ptr_ref_count *) malloc( sizeof(ptr_ref_count) );
	ref->key = strdup(id);
	ref->count = 0;
	ref->cred = cred_handle;
	if ( globus_hashtable_insert(&handle_cache,ref->key,(void*)ref) < 0 )
	{
		gahp_printf("F globus_hashtable_insert\\ failed!\n");
		/* this should never ever happen!  exit */
		_exit(6);
	}


	gahp_printf("S\n");

cache_proxy_return:
	gahp_sem_up(&print_control);

	all_args_free(user_arg);
	return 0;
}

int
handle_response_prefix(void * user_arg)
{
	char **input_line = (char **) user_arg;
	char *prefix;

	if(!process_string_arg(input_line[1], & prefix ) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	gahp_printf("S\n");

		/* Now set global ResponsePrefix _before_ releasing the 
		 * print_control mutex. */
	if (ResponsePrefix) {
		free(ResponsePrefix);
		ResponsePrefix = NULL;
	}
	if ( prefix ) {
		ResponsePrefix = strdup(prefix);
	}


	gahp_sem_up(&print_control);

	all_args_free(user_arg);
	return 0;
}

int
main_activate_globus()
{
	int err;


	err = globus_module_activate( GLOBUS_GRAM_CLIENT_MODULE );
	if ( err != GLOBUS_SUCCESS ) {
		globus_module_deactivate( GLOBUS_GRAM_CLIENT_MODULE );
		return err;
	}

	return GLOBUS_SUCCESS;
}


void
main_deactivate_globus()
{
		// So sometimes calling globus_module_deactivate_all() hangs,
		// usually when trying to shutdown GASS.  :(.  So for now
		// we just comment it out, and let the operating system try
		// to reclaim all the resources.
	/* globus_module_deactivate_all(); */

	return;
}


char **
read_command(int stdin_fd)
{
	static char* buf = NULL;
	char** command_argv;
	int ibuf = 0;
	int iargv = 0;
	int result = 0;

	if ( buf == NULL ) {
		buf = (char *) malloc(1024 * 500);
	}

	/* Read a command from stdin.  We use the read() system call for this,
	 * since the POSIX threads standard requires that read just block
	 * the thread and not the entire process.  We do _not_ use 
	 * globus_libc_read() here because that holds onto a libc mutex 
	 * until read returns --- this would halt our other threads.
	 */
	
	command_argv = (char **) calloc(500, sizeof(char*));

	ibuf = 0;
	iargv = 0;

	for (;;) {
		result = read(stdin_fd, &(buf[ibuf]), 1 );

		/* Check return value from read() */
		if ( result < 0 ) {		/* Error reading */
			if ( errno == EINTR ||
				errno == EAGAIN ||
				errno == EWOULDBLOCK )
			{
				continue;
			}
			_exit(2);	/* some sort of fatal error */
		}
		if ( result == 0 ) {	/* End of File */
			/* Exit right here and now.  Don't try to deactivate globus
			 * modules first...  it appears this may cause a needless segfault 
			 */
#ifndef WIN32
			_exit(SIGPIPE);
#else
			_exit(3);
#endif
		}

		/* Check if charcater read was whitespace */
		if ( buf[ibuf]==' ' || buf[ibuf]=='\t' || buf[ibuf]=='\r' ) {
			/* Ignore leading whitespace */
			if ( ibuf == 0 ) {	
				continue;
			}
			/* Handle Transparency: if char is '\' followed by a space,
			 * it should be considered a space and not as a seperator
			 * between arguments. */
			if ( buf[ibuf]==' ' && buf[ibuf-1]=='\\' ) {
				buf[ibuf-1] = ' ';
				continue;
			}
			/* Trailing whitespace delimits a parameter to copy into argv */
			buf[ibuf] = '\0';
			command_argv[iargv] = (char *) malloc(ibuf + 5);
			strcpy(command_argv[iargv],buf);
			ibuf = 0;
			iargv++;
			continue;
		}

		/* If character was a newline, copy into argv and return */
		if ( buf[ibuf]=='\n' ) { 
			buf[ibuf] = '\0';
			command_argv[iargv] = (char *) malloc(ibuf + 5);
			strcpy(command_argv[iargv],buf);
			return command_argv;
		}

		/* Character read was just a regular one.. increment index
		 * and loop back up to read the next character */
		ibuf++;

	}	/* end of infinite for loop */
}


/* Two macros, either handle the event asynchornously (by spawning a 
 * a new thread) or syncrhonously by just doing it. The loop at the
 * bottom looks at "result" to decide how it worked - result will either
 * we didn't even try and start a thread. 
 */

#define HANDLE_ASYNC( x ) \
	if (strcasecmp(#x,input_line[0]) == 0) { \
		if(GlobusActive) { \
			result=globus_thread_create(&ignored_threadid, \
						NULL,handle_##x,(void *)input_line);\
			} \
		else {  result=-1; } \
		}

#define HANDLE_SYNC( x ) \
	if (strcasecmp(#x,input_line[0]) == 0) \
		result=handle_##x(input_line);

void
service_commands(void * /*arg*/,globus_io_handle_t* gio_handle,globus_result_t /*rest*/)
{
	char **input_line;
	int result;
	/* globus_thread_t ignored_threadid; */

		input_line = read_command(STDIN_FILENO);

		if ( input_line == NULL ) {
			goto reregister_and_return;
		}

		if ( input_line[0] == NULL ) {
			free( input_line );
			goto reregister_and_return;
		}

		// Each command that we run will return the semaphore to our control,
		// which will prevent it from printing things out in our stream
		gahp_sem_down(&print_control);

		HANDLE_SYNC( initialize_from_file ) else
		HANDLE_SYNC( cache_proxy_from_file ) else
		HANDLE_SYNC( uncache_proxy ) else
		HANDLE_SYNC( use_cached_proxy ) else
		HANDLE_SYNC( commands ) else
		HANDLE_SYNC( version ) else
		HANDLE_SYNC( quit ) else
		HANDLE_SYNC( results ) else
		HANDLE_SYNC( async_mode_on ) else
		HANDLE_SYNC( async_mode_off ) else
		HANDLE_SYNC( response_prefix ) else
		HANDLE_SYNC( gram_error_string ) else
		HANDLE_SYNC( refresh_proxy_from_file ) else
		HANDLE_SYNC( gram_callback_allow) else
		HANDLE_SYNC( gram_job_request ) else
		HANDLE_SYNC( gram_job_cancel ) else
		HANDLE_SYNC( gram_job_status ) else
		HANDLE_SYNC( gram_job_callback_register ) else
		HANDLE_SYNC( gram_ping ) else
		HANDLE_SYNC( gass_server_init ) else 
		HANDLE_SYNC( gram_job_signal ) else
		HANDLE_SYNC( gram_job_refresh_proxy ) else
		HANDLE_SYNC( gram_get_jobmanager_version ) else
		{
			handle_bad_request(input_line);
			result = 0;
		}

		if ( result != 0 ) {
			/* could not create a thread to handle the command... now what??
		 	* for now, flag it as a bad request.  Sigh.
		 	*/
			handle_bad_request(input_line);
		}
		
reregister_and_return:
	if (gio_handle) {
		result = (int)globus_io_register_select(gio_handle,
				(globus_io_callback_t)service_commands,(void*)NULL,	/* read callback func and arg */
				(globus_io_callback_t)NULL,(void*)NULL,	/* write callback func and arg */
				(globus_io_callback_t)NULL,(void*)NULL);/* except callback func and arg */
		if ( result != GLOBUS_SUCCESS ) {
			printf("ERROR %d Failed to globus_io_register_select stdin\n",
					result);
			_exit(1);
		}
	}

	return;
}


void
quit_on_signal(int sig)
{
	/* Posix says exit() is not signal safe, so call _exit() */
	_exit(sig);
}


void
quit_on_signal_with_core(int sig)
{
#ifndef WIN32
	sigset_t sigSet;
	struct sigaction act;

	/* A signal has been raised which should drop a core file.  The problem
	 * is that the core file from a multi-threaded process is often useless
	 * to try and look at with gdb on certain platforms.
	 * So, what we do here is first fork(), and then blow up with a core file.
	 * Why?  Because POSIX says that when a thread calls fork(), the child
	 * process will be created with just _one_ thread, and that one thread
	 * will contain the context of the thread in the parent which called fork().
	 * Thus we will end up with a core file with the one thread context which
	 * we are interested in, and also a core file which gdb can deal with.
	 * Ain't we smart?  --- Todd Tannenbaum <tannenba@cs.wisc.edu>
	 */
	int pid;
	pid = fork();
	if ( pid == -1 ) {
		_exit(66);
	}
	if ( pid ) {
		/* PARENT */
		_exit(sig);		/* call _exit because exit() is not signal safe */
	} else {
		/* CHILD */
		/* Ok, our parent is in the process of exiting.  Our job here in
		 * the child is to blow up with the critical signal, and as a result
		 * leave behind a decent core file. 
		 * 
		 * Currently, the critical signal is blocked because we are in the 
		 * handler for that signal, and this is what sigaction does by default.
		 * What we want to do is 
		 * 		a) reset the handler for this signal to the
		 * 			system default handler (which will drop core), and then 
		 * 		b) send ourselves the signal, and then 
		 * 		c) unblock the signal.
		 */
		sigemptyset(&act.sa_mask);	
		act.sa_flags = 0;
		act.sa_handler = SIG_DFL;
		sigaction(sig,&act,0);		/* reset handler */
		kill(getpid(),sig);			/* send ourselves the signal - now it is pending */
		sigemptyset(&sigSet);
		sigaddset(&sigSet,sig);
		sigprocmask(SIG_UNBLOCK,&sigSet,0);	/* unblock the signal.... BOOOM! */
		/* Should not make it here --- but exit just in case */
		_exit(sig);
	}
#endif
}


int main(int, char **)
{
	int result, err;
#ifndef WIN32
	sigset_t sigSet;
	struct sigaction act;
#endif
	ResultsPending = 0;
	GlobusActive = 0;
	AsyncResults = 0;

#ifndef WIN32
	/* Add the signals we want unblocked into sigSet */
	sigemptyset( &sigSet );
	sigaddset(&sigSet,SIGTERM);
	sigaddset(&sigSet,SIGQUIT);
	sigaddset(&sigSet,SIGPIPE);

	/* Set signal handlers */
	sigemptyset(&act.sa_mask);	/* do not block anything in handler */
	act.sa_flags = 0;
	
	/* Signals which should cause us to exit with status = signum */
	act.sa_handler = quit_on_signal;
	sigaction(SIGTERM,&act,0);
	sigaction(SIGQUIT,&act,0);
	sigaction(SIGPIPE,&act,0);

#ifdef FORK_FOR_CORE
	/* Signals which should cause us to exit with a usefull core file */
	act.sa_handler = quit_on_signal_with_core;
	sigaddset(&sigSet,SIGILL);
	sigaction(SIGILL,&act,0);
	sigaddset(&sigSet,SIGABRT);
	sigaction(SIGABRT,&act,0);
	sigaddset(&sigSet,SIGFPE);
	sigaction(SIGFPE,&act,0);
	sigaddset(&sigSet,SIGSEGV);
	sigaction(SIGSEGV,&act,0); 
#	ifdef SIGBUS
	sigaddset(&sigSet,SIGBUS);
	sigaction(SIGBUS,&act,0);
#	endif
#endif

	/* Unblock signals in our set */
	sigprocmask( SIG_UNBLOCK, &sigSet, NULL );

#endif

	/* In no event should we use a host cert for authentication.  Always
	 * use a user proxy cert.  At least until someone complains.
	 */
	unsetenv("X509_RUN_AS_SERVER");
	unsetenv("X509_USER_KEY");
	unsetenv("X509_USER_CERT");

	/* Parse command line args */

	/* Activate Globus modules we intend to use */

	// When loading a driver library for XIO, Globus
	// uses lt_dlopen(), which ignores our RPATH. This means that
	// it won't find the globus_xio_gsi_driver library that we
	// include in UW builds of Condor.
	// If we load the library with dlopen() first, then lt_dlopen()
	// will find it.
#if defined(LINUX)
	void *dl_ptr = dlopen( "libglobus_xio_gsi_driver.so", RTLD_LAZY);
	if ( dl_ptr == NULL ) {
		fprintf( stderr, "Failed to open globus_xio_gsi_driver.\n" );
		return 1;
	}
#endif

	if ( globus_thread_set_model(GLOBUS_THREAD_MODEL_NONE) != GLOBUS_SUCCESS ) {
		printf("Unable to set Globus thread model!\n");
		_exit(1);
	}

    err = globus_module_activate( GLOBUS_COMMON_MODULE );
    if ( err != GLOBUS_SUCCESS ) {
		printf("Unable to activate Globus Common Module!\n");
		_exit(1);
    }


	if ( (result=globus_fifo_init(&result_fifo)) != GLOBUS_SUCCESS ) {
		printf("ERROR %d Failed to activate Globus fifo\n",result);
		_exit(1);
	}

	if ( (result=globus_hashtable_init(&handle_cache,71,
					globus_hashtable_string_hash,
					globus_hashtable_string_keyeq)) )
	{
		printf("ERROR %d Failed to activate cred handle cache\n",result);
		_exit(1);
	}

	gahp_sem_init(&print_control, 1);
	gahp_sem_init(&fifo_control, 1);

	gahp_printf("%s\n", VersionString);

	/* At this point, all subsequent calls had better be thread safe */

	/* Here we are not going to run multi-threaded; instead, we will register
	 * a callback when there is activity on stdin
	 */
	err = globus_module_activate(GLOBUS_IO_MODULE);
    if ( err != GLOBUS_SUCCESS ) {
		printf("Unable to activate Globus IO Module!\n");
		_exit(1);
    }

	err = (int)globus_io_file_posix_convert(STDIN_FILENO,NULL,(globus_io_handle_t *)&stdin_handle);
	if ( err != GLOBUS_SUCCESS ) {
		printf("ERROR %d Failed to get globus_io handle to stdin\n",result);
		_exit(1);
	}
	err = (int)globus_io_register_select((globus_io_handle_t *)&stdin_handle,
					(globus_io_callback_t)service_commands,(void*)NULL,	/* read callback func and arg */
					(globus_io_callback_t)NULL,(void*)NULL,	/* write callback func and arg */
					(globus_io_callback_t)NULL,(void*)NULL); /* except callback func and arg */
	if ( err != GLOBUS_SUCCESS ) {
		printf("ERROR %d Failed to globus_io_register_select stdin\n",result);
		_exit(1);
	}

	globus_mutex_t manager_mutex;
	globus_cond_t  manager_cond;
	globus_mutex_init(&manager_mutex, NULL);
	globus_cond_init(&manager_cond, false);

	globus_mutex_lock(&manager_mutex);
	for (;;) {
		globus_cond_wait(&manager_cond, &manager_mutex);
	}
	globus_mutex_unlock(&manager_mutex);

	main_deactivate_globus();
	_exit(0);
}

