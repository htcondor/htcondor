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
#include <strings.h>
#include <libgen.h>
#include <ldap.h>
#include <dlfcn.h>

#include "config.h"

#include "globus_gss_assist.h"
#include "globus_io.h"
#include "globus_ftp_client.h"
#include "globus_rsl.h"

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

#define STAGE_IN_COMPLETE_FILE ".gahp_complete"

#define TRANSFER_BUFFER_SIZE	4096

	/* Macro used over and over w/ bad syntax */
#define HANDLE_SYNTAX_ERROR()		\
	{								\
	gahp_printf("E\n");		\
	gahp_sem_up(&print_control);	\
	all_args_free(input_line);		\
	}

#define MAX_ACTIVE_CTRL_CMDS	1
#define MAX_ACTIVE_DATA_CMDS	10

#define FTP_HANDLE_CACHE_TIME	30

static char *commands_list = 
"COMMANDS "
"NORDUGRID_SUBMIT " 
"NORDUGRID_STATUS "
"NORDUGRID_CANCEL "
"NORDUGRID_STAGE_IN "
"NORDUGRID_STAGE_OUT "
"NORDUGRID_STAGE_OUT2 "
"NORDUGRID_EXIT_INFO "
"NORDUGRID_PING "
"NORDUGRID_LDAP_QUERY "
"GRIDFTP_TRANSFER "
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
"UNCACHE_PROXY";
/* The last command in the list should NOT have a space after it */

typedef struct gahp_semaphore {
	globus_mutex_t mutex;
} gahp_semaphore;
	
typedef struct ptr_ref_count {
	char *key;
	volatile int count;
/*
	globus_gram_client_attr_t gram_attr;
*/
	gss_cred_id_t cred;
} ptr_ref_count;

typedef enum {
	FtpDataCmd,
	FtpCtrlCmd
} ftp_cmd_t;

typedef struct user_arg_struct {
	char **cmd;
	int stage_idx;
	int stage_last;
	globus_ftp_client_handle_t *handle;
	globus_ftp_client_operationattr_t *op_attr;
	ptr_ref_count *cred;
	globus_byte_t *buff;
	globus_size_t buff_len;
	globus_size_t buff_filled;
	int fd;
	globus_ftp_client_complete_callback_t first_callback;
	ftp_cmd_t cmd_type;
	char *server;
} user_arg_t;

/* GLOBALS */
globus_fifo_t result_fifo;
gahp_semaphore print_control;
gahp_semaphore fifo_control;
globus_io_handle_t stdin_handle;
globus_hashtable_t handle_cache;

ptr_ref_count * current_cred = NULL;

globus_ftp_client_handleattr_t ftp_handle_attr;

typedef struct ftp_handle_struct {
	ptr_ref_count *cred;
	globus_ftp_client_handle_t *handle;
	globus_ftp_client_operationattr_t *op_attr;
	time_t last_use;
} ftp_handle_t;

typedef struct ftp_cache_entry_struct {
	char *server;
	int num_active_ctrl_cmds;
	int num_active_data_cmds;
	globus_fifo_t queued_ctrl_cmds;		// list of user_arg_t*
	globus_fifo_t queued_data_cmds;		// list of user_arg_t*
	globus_list_t *cached_ftp_handles;	// list of ftp_handle_t*
} ftp_cache_entry_t;

globus_hashtable_t ftp_cache_table;

/*
   !!!! NOTE !!!!
   The "\\" in this version string is essential for proper functioning
   of the gahp server.  The spaces in the final part ("UW Gahp") need
   to be escaped or the gahp server gets confused. :(
   !!! BEWARE !!!
*/ 
static char *VersionString ="$GahpVersion: 1.3.0 " __DATE__ " Nordugrid\\ Gahp $";

volatile int ResultsPending;
volatile int AsyncResults;
volatile int GlobusActive;
char *ResponsePrefix = NULL;

/* These are all the command handlers. */
int handle_nordugrid_submit(char **);
int handle_nordugrid_status(char **);
int handle_nordugrid_cancel(char **);
int handle_nordugrid_stage_in(char **);
int handle_nordugrid_stage_out(char **);
int handle_nordugrid_stage_out2(char **);
int handle_nordugrid_exit_info(char **);
int handle_nordugrid_ping(char **);
int handle_gridftp_transfer(char **);

/* These are all of the callbacks for non-blocking async commands */
void nordugrid_submit_start_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error );
void nordugrid_submit_cwd1_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error );
void nordugrid_submit_write_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error,
								  globus_byte_t *buffer,
								  globus_size_t length,
								  globus_off_t offset,
								  globus_bool_t eof );
void nordugrid_submit_put_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error );
void nordugrid_submit_cwd2_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error );
void nordugrid_status_start_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error );
void nordugrid_status_read_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error,
								  globus_byte_t *buffer,
								  globus_size_t length,
								  globus_off_t offset,
								  globus_bool_t eof );
void nordugrid_status_get_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error );
void nordugrid_cancel_start_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error );
void nordugrid_cancel_rmdir_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error );
void nordugrid_stage_in_start_callback( void *arg,
									globus_ftp_client_handle_t *handle,
									globus_object_t *error );
void nordugrid_stage_in_exists_callback( void *arg,
									globus_ftp_client_handle_t *handle,
									globus_object_t *error );
void nordugrid_stage_in_start_file_callback( void *arg,
									globus_ftp_client_handle_t *handle,
									globus_object_t *error );
void nordugrid_stage_in_write_callback( void *arg,
									globus_ftp_client_handle_t *handle,
									globus_object_t *error,
									globus_byte_t *buffer,
									globus_size_t length,
									globus_off_t offset,
									globus_bool_t eof );
void nordugrid_stage_in_put_callback( void *arg,
									globus_ftp_client_handle_t *handle,
									globus_object_t *error );
void nordugrid_stage_out2_start_file_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error );
void nordugrid_stage_out2_read_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error,
									 globus_byte_t *buffer,
									 globus_size_t length,
									 globus_off_t offset,
									 globus_bool_t eof );
void nordugrid_stage_out2_get_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error );
void nordugrid_exit_info_start_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error );
void nordugrid_exit_info_read_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error,
									 globus_byte_t *buffer,
									 globus_size_t length,
									 globus_off_t offset,
									 globus_bool_t eof );
void nordugrid_exit_info_get_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error );
void nordugrid_ping_start_callback( void *arg,
								globus_ftp_client_handle_t *handle,
								globus_object_t *error );
void nordugrid_ping_exists_callback( void *arg,
								globus_ftp_client_handle_t *handle,
								globus_object_t *error );
void gridftp_transfer_start_callback( void *arg,
									  globus_ftp_client_handle_t *handle,
									  globus_object_t *error );
void gridftp_transfer_write_callback( void *arg,
									  globus_ftp_client_handle_t *handle,
									  globus_object_t *error,
									  globus_byte_t *buffer,
									  globus_size_t length,
									  globus_off_t offset,
									  globus_bool_t eof );
void gridftp_transfer_read_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error,
									 globus_byte_t *buffer,
									 globus_size_t length,
									 globus_off_t offset,
									 globus_bool_t eof );
void gridftp_transfer_done_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error );

/* These are all of the sync. command handlers */
int handle_async_mode_on(char **);
int handle_async_mode_off(char **);
int handle_response_prefix(char **);
int handle_refresh_proxy_from_file(char **);

int main_activate_globus();
void main_deactivate_globus();

void gahp_sem_init( gahp_semaphore *, int initial_value);
void gahp_sem_up(  gahp_semaphore *);
void gahp_sem_down( gahp_semaphore * );
void unlink_ref_count( ptr_ref_count* , int );

void enqueue_results( char *result_line );

/* Escape spaces replaces ' ' with '\ '. It allocates memory, and 
   puts the escaped string in that memory, and returns it. A NULL is
   returned if something (really, just memory allocation) fails. 
   The caller is responsible for freeing the memory returned by this func 
*/ 
char *escape_spaces( const char *input_line );
/* Behaves the same as escape_spaces(), but embedded newlines are
   converted to spaces and trailing spaces are removed.
 */
char *escape_err_msg( const char *input_line );

/* all_args_free frees all the memory passed into a command handler */
void all_args_free( char ** );

/* These routines verify that the ascii argument is either a number or an */
/* string - they do not allocate any memory - by default, output_line will */
/* be a pointer pointer to input_line, or NULL if input_line=="NULL" */

int process_string_arg( char *input_line, char **output_line);
int process_int_arg( char *input_line, int *result );

typedef struct my_string {
	char *data;
	int length;
	int capacity;
} my_string;

my_string *
my_string_malloc()
{
	my_string *str = (my_string *)malloc( sizeof(my_string) );
	str->capacity = 1024;
	str->data = (char *)malloc( str->capacity );
	str->length = 0;
	str->data[0] = '\0';
	return str;
}

void 
my_string_free( my_string *str ) {
	if ( str ) {
		free( str->data );
		free( str );
	}
}

char *
my_string_convert( my_string *str ) {
	char *rc = NULL;
	if ( str ) {
		rc = str->data;
		free( str );
	}
	return rc;
}

void
my_strcat( my_string *dst, const char *src )
{
	int src_len = strlen( src );
	int new_capacity = dst->capacity;
	while( dst->length + src_len >= new_capacity ) {
		new_capacity *= 2;
	}
	if ( new_capacity > dst->capacity ) {
		dst->data = (char *) realloc( dst->data, new_capacity );
		dst->capacity = new_capacity;
	}
	strcpy( &dst->data[dst->length], src );
	dst->length += src_len;
}

int
gahp_printf(const char *format, ...)
{
	int ret_val = 0;
	va_list ap;

	globus_libc_lock();

	if (ResponsePrefix) {
		ret_val = printf("%s",ResponsePrefix);
	}

	if ( ret_val >= 0 ) {
		va_start(ap, format);
		ret_val = vprintf(format, ap);
		va_end(ap);
	}

	if ( ret_val < 0 && errno == EPIPE ) {
		/* Exit if our stdout pipe has been closed on us. */
		_exit(SIGPIPE);
	}

	fflush(stdout);

	globus_libc_unlock();
	
	return ret_val;
}


void
gahp_sem_init( gahp_semaphore *sem, int initial_value) 
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
	// if it's a NULL pointer, give up now.
    if(!input_line){
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
	output_line = (char*) malloc(strlen(input_line) + i + 200);

	// now, blast across it
	temp = input_line;
	for(i = 0; *temp != '\0'; temp++) {
		if( *temp == ' ' || *temp == '\r' || *temp == '\n' || *temp == '\\' ) {
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

char *
escape_err_msg( const char *input_line) 
{
	int i;
	const char *temp;
	char *output_line;

	// first, count up the spaces
	temp = input_line;
	for(i = 0; *temp != '\0'; temp++) {
		if( *temp == ' ' || *temp == '\\' ) {
			i++;
		}
	}

	// get enough space to store it.  	
	output_line = (char *) malloc(strlen(input_line) + i + 200);

	// now, blast across it
	temp = input_line;
	for(i = 0; *temp != '\0'; temp++) {
		if ( *temp == '\r' || *temp == '\n' ) {
			output_line[i] = '\\';
			i++;
			output_line[i] = ' ';
			i++;
		} else {
			if( *temp == ' ' || *temp == '\\' ) {
				output_line[i] = '\\'; 
				i++;
			}
			output_line[i] = *temp;
			i++;
		}
	}
	// trim trailing spaces and the backslashes that escape them
	i--;
	while ( output_line[i] == ' ' ) {
		i--;
		output_line[i] = '\0';
		i--;
	}
	// the caller is responsible for freeing this memory, not us
	return output_line;	
}

void
all_args_free( char **input_line )
{
	char **ptr;
	/* if we get a NULL pointer, give up now */
	if(!input_line) 
		return;

	/* input_line is a array of char*'s from read_command */
	/* it's a vector of command line arguemts */
	/* The last one will be null, so walk it until we hit the end */
	/* If this was a real library call this would be bad, since we */
	/* trust how we got this memory. */

	ptr = input_line;
	while(*ptr) {
		free(*ptr);
		ptr++;
	}
	free(input_line);
	return;	
}

user_arg_t *malloc_user_arg( char **cmd, ptr_ref_count *cred,
							 const char *server )
{
	user_arg_t *user_arg;

	user_arg = (user_arg_t*) malloc( sizeof(user_arg_t) );
	user_arg->cmd = cmd;
	user_arg->buff = NULL;
	user_arg->buff_len = 0;
	user_arg->buff_filled = 0;
	user_arg->stage_idx = 0;
	user_arg->stage_last = 0;
	user_arg->fd = -1;
	user_arg->cred = cred;
	if ( cred ) {
		cred->count++;
	}
	user_arg->op_attr = NULL;
	if ( server ) {
		user_arg->server = strdup( server );
	} else {
		user_arg->server = NULL;
	}
	user_arg->first_callback = NULL;
	user_arg->cmd_type = FtpCtrlCmd;

	return user_arg;
}

void free_user_arg( user_arg_t *user_arg )
{
	if ( user_arg->cmd ) {
		all_args_free( user_arg->cmd );
	}
	if ( user_arg->buff ) {
		free( user_arg->buff );
	}
	if ( user_arg->fd >= 0 ) {
		close( user_arg->fd );
	}
	if ( user_arg->server ) {
		free( user_arg->server );
	}
	if ( user_arg->cred ) {
		unlink_ref_count( user_arg->cred, 1 );
	}
	free( user_arg );
}

void clean_ftp_handles( void *arg )
{
	time_t now = time(NULL);
	ftp_cache_entry_t *entry;
	globus_list_t *curr_elem;
	globus_list_t *next_elem;
	ftp_handle_t *curr_handle;
	int result;

	for ( entry = (ftp_cache_entry_t *) globus_hashtable_first( &ftp_cache_table ); entry;
		  entry = (ftp_cache_entry_t *) globus_hashtable_next( &ftp_cache_table ) ) {

		curr_elem = entry->cached_ftp_handles;
		while ( curr_elem ) {
			next_elem = globus_list_rest( curr_elem );
			curr_handle = (ftp_handle_t *) globus_list_first( curr_elem );
			if ( curr_handle->last_use < now - FTP_HANDLE_CACHE_TIME ) {
				// Remove cached handle from list and destroy it
				globus_list_remove( &entry->cached_ftp_handles,
									curr_elem );

				result = globus_ftp_client_operationattr_destroy( curr_handle->op_attr );
				if (result) assert( result == GLOBUS_SUCCESS );
				free( curr_handle->op_attr );

				result = globus_ftp_client_handle_destroy( curr_handle->handle );
				if (result) assert( result == GLOBUS_SUCCESS );
				free( curr_handle->handle );

				if ( curr_handle->cred ) {
					unlink_ref_count( curr_handle->cred, 1 );
				}

				free( curr_handle );
			}

			curr_elem = next_elem;
		}
	}
}

void dispatch_ftp_command( ftp_cache_entry_t *entry )
{
	user_arg_t *next_cmd = NULL;
	globus_list_t *list_elem = NULL;
	int found_it = 0;
	int result;

	if ( entry->num_active_ctrl_cmds < MAX_ACTIVE_CTRL_CMDS &&
		 !globus_fifo_empty( &entry->queued_ctrl_cmds ) ) {

		next_cmd = (user_arg_t *) globus_fifo_dequeue( &entry->queued_ctrl_cmds );
		entry->num_active_ctrl_cmds++;

	} else if ( entry->num_active_data_cmds < MAX_ACTIVE_DATA_CMDS &&
				!globus_fifo_empty( &entry->queued_data_cmds ) ) {

		next_cmd = (user_arg_t *) globus_fifo_dequeue( &entry->queued_data_cmds );
		entry->num_active_data_cmds++;
	}

	if ( next_cmd == NULL ) {
		return;
	}

	for ( list_elem = entry->cached_ftp_handles; list_elem;
		  list_elem = globus_list_rest( list_elem ) ) {

		ftp_handle_t *next_handle = (ftp_handle_t *) globus_list_first( list_elem );
		if ( next_cmd->cred == next_handle->cred ) {
			globus_list_remove( &entry->cached_ftp_handles, list_elem );
			next_cmd->handle = next_handle->handle;
			next_cmd->op_attr = next_handle->op_attr;	
			if ( next_handle->cred ) {
				unlink_ref_count( next_handle->cred, 1 );
			}
			free( next_handle );
			found_it = 1;
			break;
		}
	}
	if ( !found_it ) {
		next_cmd->handle = (globus_ftp_client_handle_t*) malloc( sizeof(globus_ftp_client_handle_t) );

		result = globus_ftp_client_handle_init( next_cmd->handle,
												&ftp_handle_attr );
		if (result) assert( result == GLOBUS_SUCCESS );

		next_cmd->op_attr = (globus_ftp_client_operationattr_t*)malloc( sizeof(globus_ftp_client_operationattr_t) );

		result = globus_ftp_client_operationattr_init( next_cmd->op_attr );
		if (result) assert( result == GLOBUS_SUCCESS );

		if ( next_cmd->cred ) {
			result = globus_ftp_client_operationattr_set_authorization(
														next_cmd->op_attr,
														next_cmd->cred->cred,
														NULL,
														NULL,
														NULL,
														NULL );
			if (result) assert( result == GLOBUS_SUCCESS );
		}
	}

	(*(next_cmd->first_callback))( next_cmd, next_cmd->handle, GLOBUS_SUCCESS );}

void begin_ftp_command( user_arg_t *user_arg )
{
	ftp_cache_entry_t *entry;

	entry = (ftp_cache_entry_t *) globus_hashtable_lookup( &ftp_cache_table, (void *)user_arg->server );
	if ( entry == NULL ) {
		entry = (ftp_cache_entry_t *) malloc( sizeof( ftp_cache_entry_t ) );
		entry->server = strdup( user_arg->server );
		entry->num_active_ctrl_cmds = 0;
		entry->num_active_data_cmds = 0;
		globus_fifo_init( &entry->queued_ctrl_cmds );
		globus_fifo_init( &entry->queued_data_cmds );
		entry->cached_ftp_handles = NULL;
		globus_hashtable_insert( &ftp_cache_table, entry->server, entry );
	}

	switch ( user_arg->cmd_type ) {
	case FtpDataCmd:
		globus_fifo_enqueue( &entry->queued_data_cmds, user_arg );
		break;
	case FtpCtrlCmd:
		globus_fifo_enqueue( &entry->queued_ctrl_cmds, user_arg );
		break;
	default:
		fprintf( stderr, "Unknown ftp command type: %d\n", user_arg->cmd_type );
		exit( 1 );
	}

	dispatch_ftp_command( entry );
}

void finish_ftp_command( user_arg_t *user_arg )
{
	ftp_cache_entry_t *entry;
	ftp_handle_t *saved_handle;

	entry = (ftp_cache_entry_t *) globus_hashtable_lookup( &ftp_cache_table, (void *)user_arg->server );
	assert( entry );

	switch ( user_arg->cmd_type ) {
	case FtpCtrlCmd:
		entry->num_active_ctrl_cmds--;
		break;
	case FtpDataCmd:
		entry->num_active_data_cmds--;
		break;
	default:
		fprintf( stderr, "Unknown ftp command type: %d\n", user_arg->cmd_type );
		exit( 1 );
	}

	saved_handle = (ftp_handle_t *)malloc( sizeof(ftp_handle_t) );
	saved_handle->last_use = time(NULL);
	saved_handle->handle = user_arg->handle;
	saved_handle->op_attr = user_arg->op_attr;
	saved_handle->cred = user_arg->cred;
	if ( saved_handle->cred ) {
		saved_handle->cred->count++;
	}
	globus_list_insert( &entry->cached_ftp_handles, saved_handle );

	dispatch_ftp_command( entry );
}

int
handle_bad_request(char **input_line)
{
	HANDLE_SYNTAX_ERROR();

	return 0;
}


int
handle_nordugrid_submit( char **input_line )
{
	user_arg_t *user_arg;

	if ( input_line[1] == NULL || input_line[2] == NULL ||
		 input_line[3] == NULL ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	 
	gahp_printf("S\n");
	gahp_sem_up(&print_control);

		/* modify the rsl */
	{
		char *str;
		globus_rsl_t *rsl;
		globus_list_t *rsl_list;
		globus_rsl_t *inputfiles_rsl = NULL;

		rsl = globus_rsl_parse( input_line[3] );
		if ( rsl == NULL ) {
			char *esc = escape_err_msg( "Failed to parsed RSL" );
			char *output = (char *) malloc( 10 + strlen( input_line[1] ) +
														strlen( esc ) );
			sprintf( output, "%s 1 NULL %s", input_line[1], esc );
			free( esc );
			enqueue_results( output );
			all_args_free( input_line );
			return 0;
		}

			/* add '(.gahp_complete "")' to inputfiles */
		inputfiles_rsl = NULL;

		rsl_list = globus_rsl_boolean_get_operand_list(rsl);
		while ( !globus_list_empty( rsl_list ) ) {
			if ( globus_rsl_is_relation_attribute_equal( (globus_rsl_t*) globus_list_first( rsl_list ), "inputfiles" ) ) {
				inputfiles_rsl = (globus_rsl_t*) globus_list_first( rsl_list );
				break;
			}
			rsl_list = globus_list_rest( rsl_list );
		}
		if ( inputfiles_rsl ) {
			globus_list_t *parent_list = NULL;
			globus_list_t *new_list = NULL;
			globus_rsl_value_t *new_value = NULL;
			globus_rsl_value_t *parent_value = NULL;
			globus_list_insert( &new_list, globus_rsl_value_make_literal( strdup( "" ) ) );
			globus_list_insert( &new_list, globus_rsl_value_make_literal( strdup( STAGE_IN_COMPLETE_FILE ) ) );
			new_value = globus_rsl_value_make_sequence( new_list );
			parent_value = globus_rsl_relation_get_value_sequence( inputfiles_rsl );
			parent_list = globus_rsl_value_sequence_get_value_list( parent_value );
			globus_list_insert( &parent_list, new_value );
			parent_value->value.sequence.value_list = parent_list;
		}

		str = globus_rsl_unparse( rsl );
		assert( str != NULL );

		globus_rsl_free_recursive( rsl );

		free( input_line[3] );
		input_line[3] = str;
	}

	user_arg = malloc_user_arg( input_line, current_cred, input_line[2] );
	user_arg->cmd_type = FtpCtrlCmd;
	user_arg->first_callback = nordugrid_submit_start_callback;

	begin_ftp_command( user_arg );

	return 0;
}

void
nordugrid_submit_start_callback( void *arg,
							 globus_ftp_client_handle_t *handle,
							 globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	char url[1024];

	if ( error != GLOBUS_SUCCESS ) {
		nordugrid_submit_cwd2_callback( arg, handle, error );
		return;
	}

	sprintf( url, "gsiftp://%s/jobs/new", user_arg->cmd[2] );

	result = globus_ftp_client_cwd( user_arg->handle, url,
									user_arg->op_attr,
									&user_arg->buff,
									&user_arg->buff_len,
									nordugrid_submit_cwd1_callback, arg );
	if ( result != GLOBUS_SUCCESS ) {
		nordugrid_submit_cwd2_callback( arg, handle,
									 globus_error_peek( result ) );
	}
}

void nordugrid_submit_cwd1_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	char url[1024];
	char job_id[128];

	if ( error != GLOBUS_SUCCESS ) {
		nordugrid_submit_cwd2_callback( arg, handle, error );
		return;
	}

	/* If the server doesn't return a 250 response to the CWD command,
	 * then the gridftp library won't allocate a buffer with the response
	 * line (and it doesn't flag an error). We've seen some ARC servers
	 * return '226 Abort finished'.
	 */
	if ( user_arg->buff == NULL ||
		 sscanf( (char *)user_arg->buff, "250 \"jobs/%[A-Za-z0-9]", job_id ) != 1 ) {
			result = globus_error_put( globus_error_construct_string(
											NULL,
											NULL,
											"Failed to parse job id" ) );
			nordugrid_submit_cwd2_callback( arg, handle,
										 globus_error_peek( result ) );
			return;
	}
	strcpy( (char *)user_arg->buff, job_id );

	sprintf( url, "gsiftp://%s/jobs/new/job", user_arg->cmd[2] );

	result = globus_ftp_client_put( user_arg->handle, url,
									user_arg->op_attr, NULL,
									nordugrid_submit_put_callback, arg );
	if ( result != GLOBUS_SUCCESS ) {
		nordugrid_submit_cwd2_callback( arg, handle,
									 globus_error_peek( result ) );
		return;
	} else {
		result = globus_ftp_client_register_write( user_arg->handle,
												  (globus_byte_t*) user_arg->cmd[3],
												   strlen( user_arg->cmd[3] ),
												   0, GLOBUS_TRUE,
												   nordugrid_submit_write_callback,
												   arg );
		if ( result != GLOBUS_SUCCESS ) {
				/* What to do? */
			char *mesg = globus_error_print_friendly( globus_error_peek( result ) );
			fprintf( stderr, "nordugrid_submit_cwd1_callback: %s\n", mesg );
			free( mesg );
		}
	}
}

void nordugrid_submit_write_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error,
								  globus_byte_t *buffer,
								  globus_size_t length,
								  globus_off_t offset,
								  globus_bool_t eof )
{
	/* user_arg_t *user_arg = (user_arg_t *)arg; */

	if ( error != GLOBUS_SUCCESS ) {
			/* What to do? */
		char *mesg = globus_error_print_friendly( error );
		fprintf( stderr, "nordugrid_submit_write_callback: %s\n", mesg );
		free( mesg );
		return;
	}
}

void
nordugrid_submit_put_callback( void *arg,
							 globus_ftp_client_handle_t *handle,
							 globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	char url[1024];

	if ( error != GLOBUS_SUCCESS ) {
		nordugrid_submit_cwd2_callback( arg, handle, error );
		return;
	}

	sprintf( url, "gsiftp://%s/jobs", user_arg->cmd[2] );

	result = globus_ftp_client_cwd( user_arg->handle, url,
									user_arg->op_attr, NULL, NULL,
									nordugrid_submit_cwd2_callback, arg );
	if ( result != GLOBUS_SUCCESS ) {
		nordugrid_submit_cwd2_callback( arg, handle,
									 globus_error_peek( result ) );
	}
}

void
nordugrid_submit_cwd2_callback( void *arg,
							 globus_ftp_client_handle_t *handle,
							 globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	char *output;

	if ( error == GLOBUS_SUCCESS ) {
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) +
									 strlen( (char *) user_arg->buff ) );
		sprintf( output, "%s 0 %s NULL", user_arg->cmd[1],
							 (char *)user_arg->buff );
	} else {
		char *err_str = globus_error_print_friendly( error );
		char *esc = escape_err_msg( err_str );
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) +
									 strlen( esc ) );
		sprintf( output, "%s 1 NULL %s", user_arg->cmd[1], esc );
		free( err_str );
		free( esc );
	}

	enqueue_results(output);	

	finish_ftp_command( user_arg );
	free_user_arg( user_arg );
	return;
}

int
handle_nordugrid_status( char **input_line )
{
	user_arg_t *user_arg;

	if ( input_line[1] == NULL || input_line[2] == NULL ||
		 input_line[3] == NULL ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	 
	gahp_printf("S\n");
	gahp_sem_up(&print_control);

	user_arg = malloc_user_arg( input_line, current_cred, input_line[2] );
	user_arg->cmd_type = FtpCtrlCmd;
	user_arg->first_callback = nordugrid_status_start_callback;

	begin_ftp_command( user_arg );

	return 0;
}

void nordugrid_status_start_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	char url[1024];

	if ( error != GLOBUS_SUCCESS ) {
		nordugrid_status_get_callback( arg, handle, error );
		return;
	}

	sprintf( url, "gsiftp://%s/jobs/info/%s/status",
						 user_arg->cmd[2], user_arg->cmd[3] );

	result = globus_ftp_client_get( user_arg->handle, url,
									user_arg->op_attr, NULL,
									nordugrid_status_get_callback, arg );
	if ( result != GLOBUS_SUCCESS ) {
		nordugrid_status_get_callback( arg, handle,
									 globus_error_peek( result ) );
	} else {
		globus_byte_t *read_buff = malloc( TRANSFER_BUFFER_SIZE );
		result = globus_ftp_client_register_read( user_arg->handle,
												  read_buff,
												  TRANSFER_BUFFER_SIZE,
												  nordugrid_status_read_callback,
												  arg );
		if ( result != GLOBUS_SUCCESS ) {
				/* What to do? */
			char *mesg = globus_error_print_friendly( globus_error_peek( result ) );
			fprintf( stderr, "nordugrid_status_start_callback: %s\n", mesg );
			free( mesg );
			free( read_buff );
		}
	}
}

void nordugrid_status_read_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error,
								  globus_byte_t *buffer,
								  globus_size_t length,
								  globus_off_t offset,
								  globus_bool_t eof )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;

	if ( error != GLOBUS_SUCCESS ) {
			/* What to do? */
		char *mesg = globus_error_print_friendly( error );
		fprintf( stderr, "nordugrid_status_read_callback: %s\n", mesg );
		free( mesg );
		return;
	}

	if ( offset + length > user_arg->buff_len ) {
		while ( offset + length > user_arg->buff_len ) {
			user_arg->buff_len += 4096;
		}
		user_arg->buff = realloc( user_arg->buff,
											  user_arg->buff_len );
	}
	bcopy( buffer, (void *)(user_arg->buff + offset), length );
	if ( user_arg->buff_filled < offset + length ) {
		user_arg->buff_filled = offset + length;
	}

	if ( !eof ) {
		result = globus_ftp_client_register_read( user_arg->handle,
												  buffer,
												  TRANSFER_BUFFER_SIZE,
												  nordugrid_status_read_callback,
												  arg );
		if ( result != GLOBUS_SUCCESS ) {
				/* What to do? */
			char *mesg = globus_error_print_friendly( globus_error_peek( result ) );
			fprintf( stderr, "nordugrid_status_read_callback: %s\n", mesg );
			free( mesg );
		}
	} else {
		free( buffer );
	}
}

void nordugrid_status_get_callback( void *arg,
								  globus_ftp_client_handle_t *handle,
								  globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	char *output;

	if ( error == GLOBUS_SUCCESS ) {
		(user_arg->buff)[user_arg->buff_filled-1] = '\0';
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) +
									 strlen( (char *) user_arg->buff ) );
		sprintf( output, "%s 0 %s NULL", user_arg->cmd[1],
							 (char *)user_arg->buff );
	} else {
		char *err_str = globus_error_print_friendly( error );
		char *esc = escape_err_msg( err_str );
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) +
									 strlen( esc ) );
		sprintf( output, "%s 1 NULL %s", user_arg->cmd[1], esc );
		free( err_str );
		free( esc );
	}

	enqueue_results(output);	

	finish_ftp_command( user_arg );
	free_user_arg( user_arg );
	return;
}

int
handle_nordugrid_cancel( char **input_line)
{
	user_arg_t *user_arg;

	if ( input_line[1] == NULL || input_line[2] == NULL ||
		 input_line[3] == NULL ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	 
	gahp_printf("S\n");
	gahp_sem_up(&print_control);

	user_arg = malloc_user_arg( input_line, current_cred, input_line[2] );
	user_arg->cmd_type = FtpCtrlCmd;
	user_arg->first_callback = nordugrid_cancel_start_callback;

	begin_ftp_command( user_arg );

	return 0;
}

void
nordugrid_cancel_start_callback( void *arg,
							 globus_ftp_client_handle_t *handle,
							 globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	char url[1024];

	if ( error != GLOBUS_SUCCESS ) {
		nordugrid_cancel_rmdir_callback( arg, handle, error );
		return;
	}

	sprintf( url, "gsiftp://%s/jobs/%s", user_arg->cmd[2],
						 user_arg->cmd[3] );

	result = globus_ftp_client_rmdir( user_arg->handle, url,
									  user_arg->op_attr,
									  nordugrid_cancel_rmdir_callback, arg );
	if ( result != GLOBUS_SUCCESS ) {
		nordugrid_cancel_rmdir_callback( arg, handle,
									 globus_error_peek( result ) );
	}
}

void
nordugrid_cancel_rmdir_callback( void *arg,
							 globus_ftp_client_handle_t *handle,
							 globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	char *output;

	if ( error == GLOBUS_SUCCESS ) {
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) );
		sprintf( output, "%s 0 NULL", user_arg->cmd[1] );
	} else {
		char *err_str = globus_error_print_friendly( error );
		char *esc = escape_err_msg( err_str );
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) +
									 strlen( esc ) );
		sprintf( output, "%s 1 %s", user_arg->cmd[1], esc );
		free( err_str );
		free( esc );
	}

	enqueue_results(output);	

	finish_ftp_command( user_arg );
	free_user_arg( user_arg );
	return;
}

int
handle_nordugrid_stage_in( char **input_line )
{
	user_arg_t *user_arg;
	int i, num_files;

	if ( input_line[1] == NULL || input_line[2] == NULL ||
		 input_line[3] == NULL || input_line[4] == NULL ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	num_files = atoi( input_line[4] );
	for ( i = 0; i < num_files; i++ ) {
		if ( input_line[5 + i] == NULL ) {
			HANDLE_SYNTAX_ERROR();
			return 0;
		}
	}
	 
	gahp_printf("S\n");
	gahp_sem_up(&print_control);

	user_arg = malloc_user_arg( input_line, current_cred, input_line[2] );
	user_arg->cmd_type = FtpDataCmd;
	user_arg->first_callback = nordugrid_stage_in_start_callback;
	user_arg->stage_idx = 5;
	user_arg->stage_last = 4 + num_files;

	begin_ftp_command( user_arg );

	return 0;
}

void
nordugrid_stage_in_start_callback( void *arg,
							   globus_ftp_client_handle_t *handle,
							   globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	char url[1024];

	if ( error != GLOBUS_SUCCESS ) {
		nordugrid_stage_in_start_file_callback( arg, handle, error );
		return;
	}

	sprintf( url, "gsiftp://%s/jobs/%s/%s", user_arg->cmd[2],
						 user_arg->cmd[3], STAGE_IN_COMPLETE_FILE );

	result = globus_ftp_client_exists( user_arg->handle, url,
									   user_arg->op_attr,
									   nordugrid_stage_in_exists_callback, arg );
	if ( result != GLOBUS_SUCCESS ) {
		nordugrid_stage_in_start_file_callback( arg, handle,
									   globus_error_peek( result ) );
	}
}

void
nordugrid_stage_in_exists_callback( void *arg,
							   globus_ftp_client_handle_t *handle,
							   globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;

	if ( error == GLOBUS_SUCCESS ) {
			/* One past the last file means to stage the stage-in complete
			   file. Two past means everything's done.
			*/
		user_arg->stage_idx = user_arg->stage_last + 2;
	}

	nordugrid_stage_in_start_file_callback( arg, handle, GLOBUS_SUCCESS );
}

void nordugrid_stage_in_start_file_callback( void *arg,
									globus_ftp_client_handle_t *handle,
									globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	char url[1024];
	char *output;

	if ( error == NULL && user_arg->stage_idx <= user_arg->stage_last + 1 ) {
			/* start staging another file */
		char *filename;
		if ( user_arg->stage_idx <= user_arg->stage_last ) {
			filename = strdup( user_arg->cmd[user_arg->stage_idx] );
			user_arg->fd = open( filename, O_RDONLY );
		} else {
			filename = strdup( STAGE_IN_COMPLETE_FILE );
			user_arg->fd = open( "/dev/null", O_RDONLY );
		}

		if ( user_arg->fd < 0 ) {
			result = globus_error_put( globus_error_construct_string(
											NULL,
											NULL,
											"Failed to open local file '%s'",
											filename ) );
			error = globus_error_peek( result );
			free( filename );
			goto stage_in_end;
		}

		sprintf( url, "gsiftp://%s/jobs/%s/%s",
							 user_arg->cmd[2], user_arg->cmd[3],
							 basename( filename ) );
		free( filename );

		result = globus_ftp_client_put( user_arg->handle, url,
										user_arg->op_attr, NULL,
										nordugrid_stage_in_put_callback, arg );
		if ( result != GLOBUS_SUCCESS ) {
			error = globus_error_peek( result );
			goto stage_in_end;
		} else {
			globus_byte_t *write_buff = malloc( TRANSFER_BUFFER_SIZE );
			nordugrid_stage_in_write_callback( arg, handle, error, write_buff,
										   0, 0, GLOBUS_FALSE );
		}

		return;
	}

 stage_in_end:
	if ( error == GLOBUS_SUCCESS ) {

		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) );
		sprintf( output, "%s 0 NULL", user_arg->cmd[1] );

	} else {

		char *err_str = globus_error_print_friendly( error );
		char *esc = escape_err_msg( err_str );
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) +
									 strlen( esc ) );
		sprintf( output, "%s 1 %s", user_arg->cmd[1], esc );
		free( err_str );
		free( esc );

	}

	enqueue_results(output);	

	finish_ftp_command( user_arg );
	free_user_arg( user_arg );
	return;
}

/* This function can be called by nordugrid_stage_in_start_file_callback() before
 * any data is actually written. It will pass 0 for both length and offset,
 * and GLOBUS_FALSE for eof.
 */
void nordugrid_stage_in_write_callback( void *arg,
									globus_ftp_client_handle_t *handle,
									globus_object_t *error,
									globus_byte_t *buffer,
									globus_size_t length,
									globus_off_t offset,
									globus_bool_t eof )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	int rc;
	globus_off_t pos = 0;

	if ( error != GLOBUS_SUCCESS ) {
			/* What to do? */
		char *mesg = globus_error_print_friendly( error );
		fprintf( stderr, "nordugrid_stage_in_write_callback: %s\n", mesg );
		free( mesg );
		return;
	}

	pos = lseek( user_arg->fd, 0, SEEK_CUR );
	if (pos) assert( pos == (offset + (globus_off_t)length) );

	if ( eof ) {
		free( buffer );
		return;
	}

	rc = read( user_arg->fd, buffer, TRANSFER_BUFFER_SIZE );
	if ( rc < 0 ) {
		result = globus_error_put( globus_error_construct_string(
											NULL,
											NULL,
											"Failed to read local file" ) );
		nordugrid_stage_in_put_callback( arg, handle,
									   globus_error_peek( result ) );
		return;
	}

	result = globus_ftp_client_register_write( user_arg->handle,
											   buffer,
											   rc,
											   offset + length,
											   rc == 0,
											   nordugrid_stage_in_write_callback,
											   arg );
	if ( result != GLOBUS_SUCCESS ) {
			/* What to do? */
		char *mesg = globus_error_print_friendly( globus_error_peek( result ) );
		fprintf( stderr, "nordugrid_stage_in_write_callback: %s\n", mesg );
		free( mesg );
	}
}

void nordugrid_stage_in_put_callback( void *arg,
									globus_ftp_client_handle_t *handle,
									globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;

	if ( close( user_arg->fd ) < 0 ) {
		if ( error == GLOBUS_SUCCESS ) {
			globus_result_t result;
			result = globus_error_put( globus_error_construct_string(
											NULL,
											NULL,
											"Failed to close local file" ) );
			error = globus_error_peek( result );
		}
	}
	user_arg->fd = -1;

	if ( error == GLOBUS_SUCCESS ) {
		user_arg->stage_idx++;
	}

	nordugrid_stage_in_start_file_callback( arg, handle, error );
}

int
handle_nordugrid_stage_out( char **input_line )
{
	int i, num_files;
	char **new_line;

	if ( input_line[1] == NULL || input_line[2] == NULL ||
		 input_line[3] == NULL || input_line[4] == NULL ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	num_files = atoi( input_line[4] );
	for ( i = 0; i < num_files; i++ ) {
		if ( input_line[5 + i] == NULL ) {
			HANDLE_SYNTAX_ERROR();
			return 0;
		}
	}

		/* Turn this request into a STAGE_OUT2 request. The STAGE_OUT2
		 * command is a superset of the STAGE_OUT command, allowing
		 * output files to be renamed as they're staged back.
		 */
	new_line = (char **)calloc( 6 + 2 * num_files, sizeof(char*) );

	for ( i = 0; i < 5; i++ ) {
		new_line[i] = strdup( input_line[i] );
	}

	for ( i = 0; i < num_files; i++ ) {
		new_line[5 + 2 * i] =
			strdup( basename( input_line[5 + i] ) );
		new_line[5 + 2 * i + 1] =
			strdup( input_line[5 + i] );
	}

	all_args_free( input_line );

	return handle_nordugrid_stage_out2( new_line );
}

int
handle_nordugrid_stage_out2( char **input_line )
{
	user_arg_t *user_arg;
	int i, num_files;

	if ( input_line[1] == NULL || input_line[2] == NULL ||
		 input_line[3] == NULL || input_line[4] == NULL ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	num_files = atoi( input_line[4] );
	for ( i = 0; i < num_files; i++ ) {
		if ( input_line[5 + i] == NULL ) {
			HANDLE_SYNTAX_ERROR();
			return 0;
		}
	}
	 
	gahp_printf("S\n");
	gahp_sem_up(&print_control);

	user_arg = malloc_user_arg( input_line, current_cred, input_line[2] );
	user_arg->cmd_type = FtpDataCmd;
	user_arg->first_callback = nordugrid_stage_out2_start_file_callback;
	user_arg->stage_idx = 5;
	user_arg->stage_last = 4 + 2 * num_files;

	begin_ftp_command( user_arg );

	return 0;
}

void nordugrid_stage_out2_start_file_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	char url[1024];
	char *output;
	char *src_file;
	char *dst_file;
		

	if ( error == NULL && user_arg->stage_idx <= user_arg->stage_last ) {
			/* start staging another file */

		src_file = user_arg->cmd[user_arg->stage_idx];
		dst_file = user_arg->cmd[user_arg->stage_idx + 1];

		user_arg->fd = open( dst_file, O_CREAT|O_WRONLY|O_TRUNC, 0644 );
		if ( user_arg->fd < 0 ) {
			result = globus_error_put( globus_error_construct_string(
											NULL,
											NULL,
											"Failed to open local file '%s'",
											dst_file ) );
			error = globus_error_peek( result );
			goto stage_out2_end;
		}

		sprintf( url, "gsiftp://%s/jobs/%s/%s",
							 user_arg->cmd[2], user_arg->cmd[3], src_file );

		result = globus_ftp_client_get( user_arg->handle, url,
										user_arg->op_attr, NULL,
										nordugrid_stage_out2_get_callback, arg );
		if ( result != GLOBUS_SUCCESS ) {
			error = globus_error_peek( result );
			goto stage_out2_end;
		} else {
			globus_byte_t *read_buff = malloc( TRANSFER_BUFFER_SIZE );
			result = globus_ftp_client_register_read( user_arg->handle,
													 read_buff,
													  TRANSFER_BUFFER_SIZE,
													  nordugrid_stage_out2_read_callback,
													  arg );
			if ( result != GLOBUS_SUCCESS ) {
					/* What to do? */
				char *mesg = globus_error_print_friendly( globus_error_peek( result ) );
				fprintf( stderr, "nordugrid_stage_out2_start_file_callback: %s\n", mesg );
				free( mesg );
				free( read_buff );
			}
		}

		return;
	}

 stage_out2_end:
	if ( error == GLOBUS_SUCCESS ) {

		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) );
		sprintf( output, "%s 0 NULL", user_arg->cmd[1] );

	} else {

		char *err_str = globus_error_print_friendly( error );
		char *esc = escape_err_msg( err_str );
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) +
									 strlen( esc ) );
		sprintf( output, "%s 1 %s", user_arg->cmd[1], esc );
		free( err_str );
		free( esc );

	}

	enqueue_results(output);	

	finish_ftp_command( user_arg );
	free_user_arg( user_arg );
	return;
}

void nordugrid_stage_out2_read_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error,
									 globus_byte_t *buffer,
									 globus_size_t length,
									 globus_off_t offset,
									 globus_bool_t eof )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	globus_size_t written = 0;
	globus_off_t pos = 0;

	if ( error != GLOBUS_SUCCESS ) {
			/* What to do? */
		char *mesg = globus_error_print_friendly( error );
		fprintf( stderr, "nordugrid_stage_out2_read_callback: %s\n", mesg );
		free( mesg );
		return;
	}

	pos = lseek( user_arg->fd, 0, SEEK_CUR );
	if (pos) assert( pos == offset );
	while ( written < length ) {
		int rc = write( user_arg->fd, buffer + written, length - written );
		if ( rc < 0 ) {
				/* TODO Bad! */
			result = globus_error_put( globus_error_construct_string(
											NULL,
											NULL,
											"Failed to write local file" ) );
			nordugrid_stage_out2_get_callback( arg, handle,
											globus_error_peek( result ) );
			return;
		}
		written += rc;
	}

	if ( !eof ) {
		result = globus_ftp_client_register_read( user_arg->handle,
												  buffer,
												  TRANSFER_BUFFER_SIZE,
												  nordugrid_stage_out2_read_callback,
												  arg );
		if ( result != GLOBUS_SUCCESS ) {
				/* What to do? */
			char *mesg = globus_error_print_friendly( globus_error_peek( result ) );
			fprintf( stderr, "nordugrid_stage_out2_read_callback: %s\n", mesg );
			free( mesg );
		}
	} else {
		free( buffer );
	}
}

void nordugrid_stage_out2_get_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;

	if ( close( user_arg->fd ) < 0 ) {
		if ( error == GLOBUS_SUCCESS ) {
			globus_result_t result;
			result = globus_error_put( globus_error_construct_string(
											NULL,
											NULL,
											"Failed to close local file" ) );
			error = globus_error_peek( result );
		}
	}
	user_arg->fd = -1;

	if ( error == GLOBUS_SUCCESS ) {
		user_arg->stage_idx += 2;
	}

	nordugrid_stage_out2_start_file_callback( arg, handle, error );
}

int
handle_nordugrid_exit_info( char **input_line )
{
	user_arg_t *user_arg;

	if ( input_line[1] == NULL || input_line[2] == NULL ||
		 input_line[3] == NULL ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	 
	gahp_printf("S\n");
	gahp_sem_up(&print_control);

	user_arg = malloc_user_arg( input_line, current_cred, input_line[2] );
	user_arg->cmd_type = FtpCtrlCmd;
	user_arg->first_callback = nordugrid_exit_info_start_callback;

	begin_ftp_command( user_arg );

	return 0;
}

void nordugrid_exit_info_start_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	char url[1024];

	if ( error != GLOBUS_SUCCESS ) {
		nordugrid_exit_info_get_callback( arg, handle, error );
		return;
	}

	sprintf( url, "gsiftp://%s/jobs/info/%s/diag",
						 user_arg->cmd[2], user_arg->cmd[3] );

	result = globus_ftp_client_get( user_arg->handle, url,
									user_arg->op_attr, NULL,
									nordugrid_exit_info_get_callback, arg );
	if ( result != GLOBUS_SUCCESS ) {
		nordugrid_exit_info_get_callback( arg, handle,
										globus_error_peek( result ) );
	} else {
		globus_byte_t *read_buff = malloc( TRANSFER_BUFFER_SIZE );
		result = globus_ftp_client_register_read( user_arg->handle,
												  read_buff,
												  TRANSFER_BUFFER_SIZE,
												  nordugrid_exit_info_read_callback,
												  arg );
		if ( result != GLOBUS_SUCCESS ) {
				/* What to do? */
			char *mesg = globus_error_print_friendly( globus_error_peek( result ) );
			fprintf( stderr, "nordugrid_exit_info_start_callback: %s\n", mesg );
			free( mesg );
			free( read_buff );
		}
	}
}

void nordugrid_exit_info_read_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error,
									 globus_byte_t *buffer,
									 globus_size_t length,
									 globus_off_t offset,
									 globus_bool_t eof )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;

	if ( error != GLOBUS_SUCCESS ) {
			/* What to do? */
		char *mesg = globus_error_print_friendly( error );
		fprintf( stderr, "nordugrid_exit_info_read_callback: %s\n", mesg );
		free( mesg );
		return;
	}

	if ( offset + length >= user_arg->buff_len ) {
		int old_len = user_arg->buff_len;
		while ( offset + length > user_arg->buff_len ) {
			user_arg->buff_len += 4096;
		}
		user_arg->buff = realloc( user_arg->buff,
											  user_arg->buff_len );
		memset( (void *)(user_arg->buff + old_len), '*', (long) user_arg->buff_len - old_len );
	}
	bcopy( buffer, (void*)(user_arg->buff + offset), length );
	if ( user_arg->buff_filled < offset + length ) {
		user_arg->buff_filled = offset + length;
	}

	if ( !eof ) {
		result = globus_ftp_client_register_read( user_arg->handle,
												  buffer,
												  TRANSFER_BUFFER_SIZE,
												  nordugrid_exit_info_read_callback,
												  arg );
		if ( result != GLOBUS_SUCCESS ) {
				/* What to do? */
			char *mesg = globus_error_print_friendly( globus_error_peek( result ) );
			fprintf( stderr, "nordugrid_exit_info_read_callback: %s\n", mesg );
			free( mesg );
		}
	} else {
		free( buffer );
	}
}

void nordugrid_exit_info_get_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	char *output;

	if ( error == GLOBUS_SUCCESS ) {

		int normal_exit;
		int exit_code;
		char user_cpu[64];
		char sys_cpu[64];
		char wallclock[64];
		char *file = (char *) user_arg->buff;

			/* Add a null to the end of the buffer so that we don't
			 * run off the end when doing string searches or printing.
			 */
		(user_arg->buff)[user_arg->buff_filled] = '\0';

		while ( strncmp( file, "WallTime", 8 ) ) {
			file = strchr( file, '\n' );
			if ( file == NULL ) {
				break;
			} else {
				file += 1;
			}
		}
		if ( file ) {
			file = strchr( file, '\n' ) + 1;
		}
			/* Skip lines until we see 'Command' or 'WallTime'.
			 * If we don't find what we're looking for, generate
			 * an error.
			 */
		while ( file && strncmp( file, "Command", 7 ) &&
				strncmp( file, "WallTime", 8 ) ) {
			file = strchr( file, '\n' );
			if ( file == NULL ) {
				break;
			} else {
				file += 1;
			}
		}
		if ( file == NULL ) {
			char *err_str = escape_err_msg( "Failed to parse job usage info" );
			output = (char *) malloc( 16 + strlen( user_arg->cmd[1] ) +
										 strlen( err_str ) );
			sprintf( output, "%s 1 0 0 0 0 0 %s", user_arg->cmd[1],
								 err_str );
			free( err_str );

			fprintf( stderr, "job info diag file:\n%s",
					 (char *)user_arg->buff );
			goto print_results;
		}

		if ( sscanf( file, "Command exited with non-zero status %d",
					 &exit_code ) == 1 ) {
			normal_exit = 1;
			file = strchr( file, '\n' ) + 1;

		} else if ( sscanf( file, "Command terminated by signal %d",
							&exit_code ) == 1 ) {
			normal_exit = 0;
			file = strchr( file, '\n' ) + 1;

		} else {
			normal_exit = 1;
			exit_code = 0;
		}

			/* Skip lines until we see 'WallTime'.
			 * If we don't find what we're looking for, place our
			 * pointer at the null at the end of the buffer.
			 */
		while ( strncmp( file, "WallTime", 8 ) ) {
			file = strchr( file, '\n' );
			if ( file == NULL ) {
				file = ( (char *)user_arg->buff + user_arg->buff_filled );
				break;
			} else {
				file += 1;
			}
		}

		if ( sscanf( file, "WallTime=%[0-9.]s\nKernelTime=%[0-9.]s\nUserTime=%[0-9.]s",
					 wallclock, sys_cpu, user_cpu ) != 3 ) {

			char *err_str = escape_err_msg( "Failed to parse job usage info" );
			output = (char *)malloc( 16 + strlen( user_arg->cmd[1] ) +
										 strlen( err_str ) );
			sprintf( output, "%s 1 0 0 0 0 0 %s", user_arg->cmd[1],
								 err_str );
			free( err_str );

			fprintf( stderr, "job info diag file:\n%s",
					 (char *)user_arg->buff );
			goto print_results;
		}

		// the remaining lines contain more usage info
		// we can read more if we want, but not for now

		output = (char *) malloc( 40 + strlen( user_arg->cmd[1] ) +
									 strlen( wallclock ) + strlen( sys_cpu ) +
									 strlen( user_cpu ) );
		sprintf( output, "%s 0 %d %d %s %s %s NULL",
							 user_arg->cmd[1], normal_exit, exit_code,
							 wallclock, sys_cpu, user_cpu );

	} else {

		char *err_str = globus_error_print_friendly( error );
		char *esc = escape_err_msg( err_str );
		output = (char *) malloc( 16 + strlen( user_arg->cmd[1] ) +
									 strlen( esc ) );
		sprintf( output, "%s 1 0 0 0 0 0 %s", user_arg->cmd[1],
							 esc );
		free( err_str );
		free( esc );

	}

 print_results:
	enqueue_results(output);	

	finish_ftp_command( user_arg );
	free_user_arg( user_arg );
	return;
}

int
handle_nordugrid_ping( char **input_line )
{
	user_arg_t *user_arg;

	if ( input_line[1] == NULL || input_line[2] == NULL ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	 
	gahp_printf("S\n");
	gahp_sem_up(&print_control);

	user_arg = malloc_user_arg( input_line, current_cred, input_line[2] );
	user_arg->cmd_type = FtpCtrlCmd;
	user_arg->first_callback = nordugrid_ping_start_callback;

	begin_ftp_command( user_arg );

	return 0;
}

void
nordugrid_ping_start_callback( void *arg,
						   globus_ftp_client_handle_t *handle,
						   globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	char url[1024];

	if ( error != GLOBUS_SUCCESS ) {
		nordugrid_ping_exists_callback( arg, handle, error );
		return;
	}

	sprintf( url, "gsiftp://%s/jobs", user_arg->cmd[2] );

	result = globus_ftp_client_exists( user_arg->handle, url,
									   user_arg->op_attr,
									   nordugrid_ping_exists_callback, arg );
	if ( result != GLOBUS_SUCCESS ) {
		nordugrid_ping_exists_callback( arg, handle,
								   globus_error_peek( result ) );
	}
}

void
nordugrid_ping_exists_callback( void *arg,
						   globus_ftp_client_handle_t *handle,
						   globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	char *output;

	if ( error == GLOBUS_SUCCESS ) {
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) );
		sprintf( output, "%s 0 NULL", user_arg->cmd[1] );
	} else {
		char *err_str = globus_error_print_friendly( error );
		char *esc = escape_err_msg( err_str );
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) +
									 strlen( esc ) );
		sprintf( output, "%s 1 %s", user_arg->cmd[1], esc );
		free( err_str );
		free( esc );
	}

	enqueue_results(output);	

	finish_ftp_command( user_arg );
	free_user_arg( user_arg );
	return;
}

int
handle_nordugrid_ldap_query( char **input_line )
{
	user_arg_t *user_arg;
	char *output;
	char *err_str = NULL;
	my_string *reply = NULL;
	char *attrs_str = NULL;

	if ( input_line[1] == NULL || input_line[2] == NULL ||
		 input_line[3] == NULL || input_line[4] == NULL ||
		 input_line[5] == NULL) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	 
	gahp_printf("S\n");
	gahp_sem_up(&print_control);

	user_arg = malloc_user_arg( input_line, NULL, NULL );

	int rc;
	LDAP *hdl = NULL;
	char *server = user_arg->cmd[2];
	int port = 2135;
	char *search_base = user_arg->cmd[3];
	char *search_filter = user_arg->cmd[4];
	char **attrs = NULL;
	LDAPMessage *search_result = NULL;
	LDAPMessage *next_entry = NULL;
	int first_entry = 1;

	if(process_string_arg( user_arg->cmd[5], &attrs_str ) &&
		attrs_str && attrs_str[0] ) {
		int num_attrs = 1;
		char* next = attrs_str;
		char *prev;
		int i;
		while ( (next = strchr( next, ',' )) ) {
			num_attrs++;
			next++;
		}
		attrs = (char **)malloc( (num_attrs + 1) * sizeof(char *) );
		prev = attrs_str;
		i = 0;
		while ( (next = strchr( prev, ',' )) ) {
			attrs[i] = prev;
			*next = '\0';
			prev = next + 1;
			i++;
		}
		attrs[i] = prev;
		attrs[i + 1] = NULL;
	}

	hdl = ldap_init( server, port );
	if ( hdl == NULL ) {
		err_str = strdup( "ldap_open failed" );
		goto ldap_query_done;
	}

		// This is the synchronous version
	rc = ldap_simple_bind_s( hdl, NULL, NULL );
	if ( rc != 0 ) {
			// TODO free resources?
			//ldap_perror( hdl, "ldap_simple_bind_s failed" );
		err_str = strdup( "ldap_simple_bind_s failed" );
		goto ldap_query_done;
	}

		// This is the synchronous version
	rc = ldap_search_s( hdl, search_base, LDAP_SCOPE_SUBTREE, search_filter,
						attrs, 0, &search_result );
	if ( rc != 0 ) {
			// TODO free resources?
			//ldap_perror( hdl, "ldap_search_s failed" );
		err_str = strdup( "ldap_search_s failed" );
		goto ldap_query_done;
	}

	reply = my_string_malloc();
	my_strcat( reply, user_arg->cmd[1] );
	my_strcat( reply, " 0 NULL" );

	first_entry = 1;
	next_entry = ldap_first_entry( hdl, search_result );
	while ( next_entry ) {
		BerElement *ber;
		char *next_attr;
//		char *dn;

		if ( !first_entry ) {
			my_strcat( reply, " " );
		}
		first_entry = 0;

//		dn = ldap_get_dn( hdl, next_entry );
//		printf( "dn: %s\n", dn );
//		ldap_memfree( dn );

		next_attr = ldap_first_attribute( hdl, next_entry, &ber );
		while ( next_attr ) {
			char **values;
			int i;
			char *esc_str;

			values = ldap_get_values( hdl, next_entry, next_attr );
			for ( i = 0; values[i]; i++ ) {
				my_strcat( reply, " " );
				my_strcat( reply, next_attr );
				my_strcat( reply, ":\\ " );
				esc_str = escape_spaces( values[i] );
				my_strcat( reply, esc_str );
				free( esc_str );
			}

			ldap_value_free( values );

			ldap_memfree( next_attr );
			next_attr = ldap_next_attribute( hdl, next_entry, ber );
		}

		ber_free( ber, 0 );
		next_entry = ldap_next_entry( hdl, next_entry );
	}

 ldap_query_done:
	if ( hdl ) {
		ldap_unbind( hdl );
	}
	if ( search_result ) {
		ldap_msgfree( search_result );
	}
	free( attrs );
	if ( !err_str ) {
		output = my_string_convert( reply );
	} else {
		char *esc = escape_err_msg( err_str );
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) +
									 strlen( esc ) );
		sprintf( output, "%s 1 %s", user_arg->cmd[1], esc );
		free( err_str );
		free( esc );
		my_string_free( reply );
	}

	enqueue_results(output);	

	free_user_arg( user_arg );
	return 0;
}

int
handle_gridftp_transfer( char **input_line )
{
	user_arg_t *user_arg;
	globus_url_t url;
	char *server = NULL;

	if ( input_line[1] == NULL || input_line[2] == NULL ||
		 input_line[3] == NULL ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}
	 
	gahp_printf("S\n");
	gahp_sem_up(&print_control);

	if ( globus_url_parse( input_line[2], &url ) == GLOBUS_SUCCESS ) {
		if ( url.host != NULL ) {
			int len = strlen( url.host ) + 10;
			server = (char*) malloc( len * sizeof(char) );
			if ( url.port != 0 ) {
				sprintf( server, "%s:%d", url.host, url.port );
			} else {
				sprintf( server, "%s", url.host );
			}
		}
		globus_url_destroy( &url );
	}
	if ( !server &&
		 globus_url_parse( input_line[3], &url ) == GLOBUS_SUCCESS ) {
		if ( url.host != NULL ) {
			int len = strlen( url.host ) + 10;
			server = (char *) malloc( len * sizeof(char) );
			if ( url.port != 0 ) {
				sprintf( server, "%s:%d", url.host, url.port );
			} else {
				sprintf( server, "%s", url.host );
			}
		}
		globus_url_destroy( &url );
	}
	if ( !server ) {
		server = strdup( "" );
	}

	user_arg = malloc_user_arg( input_line, current_cred, server );
	user_arg->cmd_type = FtpDataCmd;
	user_arg->first_callback = gridftp_transfer_start_callback;

	free( server );

	begin_ftp_command( user_arg );

	return 0;
}

void gridftp_transfer_start_callback( void *arg,
										globus_ftp_client_handle_t *handle,
										globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;

	if ( error != GLOBUS_SUCCESS ) {
		gridftp_transfer_done_callback( arg, handle, error );
		return;
	}

	if ( strncmp( user_arg->cmd[2], "file://", 7 ) == 0 ) {
		// stage-in transfer
		user_arg->fd = open( user_arg->cmd[2] + 7, O_RDONLY );
		if ( user_arg->fd < 0 ) {
			result = globus_error_put( globus_error_construct_string(
											NULL,
											NULL,
											"Failed to open local file '%s'",
											user_arg->cmd[2]+7 ) );
			gridftp_transfer_done_callback( arg, handle,
											globus_error_peek( result ) );
			return;
		}

		result = globus_ftp_client_put( user_arg->handle, user_arg->cmd[3],
										user_arg->op_attr, NULL,
										gridftp_transfer_done_callback, arg );
		if ( result != GLOBUS_SUCCESS ) {
			gridftp_transfer_done_callback( arg, handle,
											globus_error_peek( result ) );
			return;
		} else {
			globus_byte_t *write_buff = malloc( TRANSFER_BUFFER_SIZE );
			gridftp_transfer_write_callback( arg, handle, error, write_buff,
											 0, 0, GLOBUS_FALSE );
		}

	} else if ( strncmp( user_arg->cmd[3], "file://", 7 ) == 0 ) {
		// stage-out transfer
		user_arg->fd = open( user_arg->cmd[3] + 7, O_CREAT|O_WRONLY|O_TRUNC, 0644 );
		if ( user_arg->fd < 0 ) {
			result = globus_error_put( globus_error_construct_string(
											NULL,
											NULL,
											"Failed to open local file '%s'",
											user_arg->cmd[3] + 7 ) );
			gridftp_transfer_done_callback( arg, handle, globus_error_peek( result ) );
			return;
		}

		result = globus_ftp_client_get( user_arg->handle, user_arg->cmd[2],
										user_arg->op_attr, NULL,
										gridftp_transfer_done_callback, arg );
		if ( result != GLOBUS_SUCCESS ) {
			gridftp_transfer_done_callback( arg, handle, globus_error_peek( result ) );
			return;
		} else {
			globus_byte_t *read_buff = malloc( TRANSFER_BUFFER_SIZE );
			result = globus_ftp_client_register_read( user_arg->handle,
													  read_buff,
													  TRANSFER_BUFFER_SIZE,
													  gridftp_transfer_read_callback,
													  arg );
			if ( result != GLOBUS_SUCCESS ) {
					/* What to do? */
				char *mesg = globus_error_print_friendly( globus_error_peek( result ) );
				fprintf( stderr, "gridftp_transfer_start_callback: %s\n", mesg );
				free( mesg );
				free( read_buff );
			}
		}

	} else {
		// third-party transfer
		result = globus_ftp_client_third_party_transfer( user_arg->handle,
														 user_arg->cmd[2],
														 user_arg->op_attr,
														 user_arg->cmd[3],
														 user_arg->op_attr,
														 NULL,
														 gridftp_transfer_done_callback,
														 arg );
		if ( result != GLOBUS_SUCCESS ) {
			gridftp_transfer_done_callback( arg, handle,
											globus_error_peek( result ) );
		}
	}
}

/* This function can be called by gridftp_transfer_start_callback() before
 * any data is actually written. It will pass 0 for both length and offset,
 * and GLOBUS_FALSE for eof.
 */
void gridftp_transfer_write_callback( void *arg,
									  globus_ftp_client_handle_t *handle,
									  globus_object_t *error,
									  globus_byte_t *buffer,
									  globus_size_t length,
									  globus_off_t offset,
									  globus_bool_t eof )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	int rc;
	globus_off_t pos = 0;

	if ( error != GLOBUS_SUCCESS ) {
			/* What to do? */
		char *mesg = globus_error_print_friendly( error );
		fprintf( stderr, "gridftp_transfer_write_callback: %s\n", mesg );
		free( mesg );
		return;
	}

	pos = lseek( user_arg->fd, 0, SEEK_CUR );
	if (pos) assert( pos == (offset + (globus_off_t)length) );

	if ( eof ) {
		free( buffer );
		return;
	}

	rc = read( user_arg->fd, buffer, TRANSFER_BUFFER_SIZE );
	if ( rc < 0 ) {
		result = globus_error_put( globus_error_construct_string(
											NULL,
											NULL,
											"Failed to read local file" ) );
		gridftp_transfer_done_callback( arg, handle,
										globus_error_peek( result ) );
		return;
	}

	result = globus_ftp_client_register_write( user_arg->handle,
											   buffer,
											   rc,
											   offset + length,
											   rc == 0,
											   gridftp_transfer_write_callback,
											   arg );
	if ( result != GLOBUS_SUCCESS ) {
			/* What to do? */
		char *mesg = globus_error_print_friendly( globus_error_peek( result ) );
		fprintf( stderr, "gridftp_transfer_write_callback: %s\n", mesg );
		free( mesg );
	}
}

void gridftp_transfer_read_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error,
									 globus_byte_t *buffer,
									 globus_size_t length,
									 globus_off_t offset,
									 globus_bool_t eof )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	globus_result_t result;
	globus_size_t written = 0;
	globus_off_t pos = 0;

	if ( error != GLOBUS_SUCCESS ) {
			/* What to do? */
		char *mesg = globus_error_print_friendly( error );
		fprintf( stderr, "gridftp_transfer_read_callback: %s\n", mesg );
		free( mesg );
		return;
	}

	pos = lseek( user_arg->fd, 0, SEEK_CUR );
	if (pos) assert( pos == offset );
	while ( written < length ) {
		int rc = write( user_arg->fd, buffer + written, length - written );
		if ( rc < 0 ) {
				/* TODO Bad! */
			result = globus_error_put( globus_error_construct_string(
											NULL,
											NULL,
											"Failed to write local file" ) );
			gridftp_transfer_done_callback( arg, handle,
											globus_error_peek( result ) );
			return;
		}
		written += rc;
	}

	if ( !eof ) {
		result = globus_ftp_client_register_read( user_arg->handle,
												  buffer,
												  TRANSFER_BUFFER_SIZE,
												  gridftp_transfer_read_callback,
												  arg );
		if ( result != GLOBUS_SUCCESS ) {
				/* What to do? */
			char *mesg = globus_error_print_friendly( globus_error_peek( result ) );
			fprintf( stderr, "gridftp_transfer_read_callback: %s\n", mesg );
			free( mesg );
		}
	} else {
		free( buffer );
	}
}

void gridftp_transfer_done_callback( void *arg,
									 globus_ftp_client_handle_t *handle,
									 globus_object_t *error )
{
	user_arg_t *user_arg = (user_arg_t *)arg;
	char *output;

	if ( error == GLOBUS_SUCCESS ) {
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) );
		sprintf( output, "%s 0 NULL", user_arg->cmd[1] );
	} else {
		char *err_str = globus_error_print_friendly( error );
		char *esc = escape_err_msg( err_str );
		output = (char *) malloc( 10 + strlen( user_arg->cmd[1] ) +
									 strlen( esc ) );
		sprintf( output, "%s 1 %s", user_arg->cmd[1], esc );
		free( err_str );
		free( esc );
	}

	enqueue_results(output);	

	finish_ftp_command( user_arg );
	free_user_arg( user_arg );
	return;
}

int
handle_commands( char **input_line )
{
	gahp_printf("S %s\n", commands_list);
	gahp_sem_up(&print_control);
	all_args_free(input_line);
	return 0;
}

int 
handle_results( char **input_line )
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

	all_args_free(input_line);

	return 0;
}

int 
handle_version( char **input_line )
{
	gahp_printf("S %s\n", VersionString);

	gahp_sem_up(&print_control);

	all_args_free(input_line);
	return 0;
}

int
handle_async_mode_on( char **input_line )
{
	gahp_sem_down(&fifo_control);
	AsyncResults = 1;
	ResultsPending = 0;
	gahp_printf("S\n"); 

	gahp_sem_up(&fifo_control);
	gahp_sem_up(&print_control);

	all_args_free(input_line);
	return 0;

}

int
handle_async_mode_off( char **input_line )
{
	gahp_sem_down(&fifo_control);
	AsyncResults = 0;
	ResultsPending = 0;
	gahp_printf("S\n"); 
	gahp_sem_up(&fifo_control);
	gahp_sem_up(&print_control);

	all_args_free(input_line);
	return 0;

}

int
handle_refresh_proxy_from_file( char **input_line )
{
	char *file_name, *environ_variable;
	gss_cred_id_t credential_handle = GSS_C_NO_CREDENTIAL;
	OM_uint32 major_status; 
	OM_uint32 minor_status; 

	file_name = NULL;

	if(!process_string_arg(input_line[1], &file_name) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	environ_variable = getenv("X509_USER_PROXY");
	if ( file_name &&
		 ( !environ_variable || strcmp( environ_variable, file_name ) ) ) {
		setenv("X509_USER_PROXY", file_name, 1);
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
			all_args_free(input_line);
			return 0;
	} 

	gss_release_cred( &minor_status, &credential_handle );
/*
	globus_gram_client_set_credentials(credential_handle);
*/

	gahp_printf("S\n");

	gahp_sem_up(&print_control);

	all_args_free(input_line);
	return 0;
}


int 
handle_quit( char **input_line )
{
	gahp_printf("S\n");
	main_deactivate_globus();
	_exit(0);

// never reached
	gahp_sem_up(&print_control);
	return 0;
}

int
handle_initialize_from_file( char **input_line )
{
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
		all_args_free(input_line);
		return 0;
	}

	GlobusActive = true;
	gahp_printf("S\n");

	gahp_sem_up(&print_control);

	all_args_free(input_line);
	return 0;
}

void
unlink_ref_count( ptr_ref_count* ptr, int decrement )
{
	/* gss_cred_id_t old_cred; */
	int result;

	if (!ptr) return;

	ptr->count -= decrement;
	if ( ptr->count < 0 )
		ptr->count = 0;
	if ( ptr->key == NULL && ptr->count == 0 ) {
		/* we are all done with this entry.  kill it. */
		/* first, retrieve the cred from the gram_attr as a sanity check */
/*
		globus_gram_client_attr_get_credential(ptr->gram_attr,&old_cred);
		if ( old_cred != ptr->cred || current_cred == ptr ) {
			gahp_printf("unlink_ref_count\\ failed\\ sanity\\ check!!!\n");
			_exit(4);
		}
		globus_gram_client_attr_destroy(&(ptr->gram_attr));
		gss_release_cred(&result,&old_cred);
*/
gss_release_cred((OM_uint32*)&result,&ptr->cred);
		free(ptr);
	}
}


void
uncache_proxy(char *key)
{
	ptr_ref_count * ptr;

	if (!key) return;
	ptr = (ptr_ref_count *)globus_hashtable_remove(&handle_cache,key);
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
handle_uncache_proxy( char **input_line )
{
	char *handle = NULL;


	if(!process_string_arg(input_line[1], &handle) ) {
		HANDLE_SYNTAX_ERROR();
		return 0;
	}

	uncache_proxy(handle);

	gahp_printf("S\n");

	gahp_sem_up(&print_control);

	all_args_free(input_line);
	return 0;
}

int
use_cached_proxy(char *handle)
{
	ptr_ref_count * ptr = NULL;
	ptr_ref_count * old_current_cred = NULL;

	if ( strcasecmp(handle,"DEFAULT")!=0 ) {
		ptr = (ptr_ref_count*) globus_hashtable_lookup(&handle_cache,handle);
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
handle_use_cached_proxy( char **input_line )
{
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

	all_args_free(input_line);
	return 0;
}

int
handle_cache_proxy_from_file( char **input_line )
{
	char *file_name = NULL;
	char *id = NULL;
	int result;
	gss_buffer_desc import_buf;
	int min_stat;
	gss_cred_id_t cred_handle;
/*
	globus_gram_client_attr_t gram_attr;
*/
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

	result = gss_import_cred((OM_uint32*)&min_stat, &cred_handle, GSS_C_NO_OID,
					1, &import_buf, 0, NULL);

	if ( result != GSS_S_COMPLETE ) {
		gahp_printf("F Failed\\ to\\ import\\ credential\\ maj=%d\\ min=%d\n",
							result,min_stat);
		goto cache_proxy_return;
	}

/*
	result = globus_gram_client_attr_init(&gram_attr);
	if ( result != GLOBUS_SUCCESS ) {
		gahp_printf("F globus_gram_client_attr_init\\ failed\\ err=%d\n",result);
		goto cache_proxy_return;
	}

	result = globus_gram_client_attr_set_credential(gram_attr,cred_handle);
	if ( result != GLOBUS_SUCCESS ) {
		gahp_printf("F globus_gram_client_attr_set_credential\\ failed\\ err=%d\n",result);
		goto cache_proxy_return;
	}
*/

	/* Clear this entry from our hash table */
	uncache_proxy(id);

	/* Put into hash table */
	ref = (ptr_ref_count *) malloc( sizeof(ptr_ref_count) );
	ref->key = strdup(id);
	ref->count = 0;
/*
	ref->gram_attr = gram_attr;
*/
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

	all_args_free(input_line);
	return 0;
}

int
handle_response_prefix( char **input_line )
{
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

	all_args_free(input_line);
	return 0;
}

int
main_activate_globus()
{
	int err;

	err = globus_module_activate( GLOBUS_FTP_CLIENT_MODULE );
	if ( err != GLOBUS_SUCCESS ) {
		globus_module_deactivate( GLOBUS_FTP_CLIENT_MODULE );
		return err;
	}

	err = globus_ftp_client_handleattr_init( &ftp_handle_attr );
	assert( err == GLOBUS_SUCCESS );

	err = globus_ftp_client_handleattr_set_cache_all( &ftp_handle_attr,
													  GLOBUS_TRUE );
	assert( err == GLOBUS_SUCCESS );

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
	int argv_size = 20;
	int result = 0;
	int escape_seen = 0;

	if ( buf == NULL ) {
		buf = (char *) malloc(1024 * 500);
	}

	command_argv = (char **) malloc(argv_size * sizeof(char*));

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

		/* If we just saw an escaping backslash, let this character
		 * through unmolested and without special meaning.
		 */
		if ( escape_seen ) {
			ibuf++;
			escape_seen = 0;
			continue;
		}

		/* Check if the character read is a backslash. If it is, then it's
		 * escaping the next character.
		 */
		if ( buf[ibuf] == '\\' ) {
			escape_seen = true;
			continue;
		}

		/* Unescaped carriage return characters are ignored */
		if ( buf[ibuf] == '\r' ) {
			continue;
		}

		/* An unescaped space delimits a parameter to copy into argv */
		if ( buf[ibuf] == ' ' ) {
			buf[ibuf] = '\0';
			command_argv[iargv] = (char *) malloc(ibuf + 5);
			strcpy(command_argv[iargv],buf);
			ibuf = 0;
			iargv++;
			if ( iargv >= argv_size ) {
				argv_size += 20;
				command_argv =
					(char **)realloc( command_argv,
												  argv_size * sizeof(char *) );
			}
			continue;
		}

		/* If character was a newline, copy into argv and return */
		if ( buf[ibuf]=='\n' ) { 
			buf[ibuf] = '\0';
			command_argv[iargv] = (char *) malloc(ibuf + 5);
			strcpy(command_argv[iargv],buf);
			iargv++;
			if ( iargv >= argv_size ) {
				argv_size += 20;
				command_argv = 
					(char **)realloc( command_argv,
												  argv_size * sizeof(char *) );
			}
			command_argv[iargv] = NULL;
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
service_commands(void *arg,globus_io_handle_t* gio_handle,globus_result_t rest)
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
		HANDLE_SYNC( refresh_proxy_from_file ) else
		HANDLE_SYNC( nordugrid_submit ) else
		HANDLE_SYNC( nordugrid_status ) else
		HANDLE_SYNC( nordugrid_cancel ) else
		HANDLE_SYNC( nordugrid_stage_in ) else
		HANDLE_SYNC( nordugrid_stage_out ) else
		HANDLE_SYNC( nordugrid_stage_out2 ) else
		HANDLE_SYNC( nordugrid_exit_info ) else
		HANDLE_SYNC( nordugrid_ping ) else
		HANDLE_SYNC( nordugrid_ldap_query ) else
		HANDLE_SYNC( gridftp_transfer ) else
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


int main(int argc, char ** argv)
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

	/* Signals we want to ignore */
	act.sa_handler = SIG_IGN;
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

	// TODO Try enabling threaded mode.
	if ( globus_thread_set_model(GLOBUS_THREAD_MODEL_NONE) != GLOBUS_SUCCESS ) {
		printf("Unable to set Globus thread model!\n");
		_exit(1);
	}

	/* Activate Globus modules we intend to use */
    err = globus_module_activate( GLOBUS_COMMON_MODULE );
    if ( err != GLOBUS_SUCCESS ) {
		printf("Unable to active Globus!\n");
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

	if ( (result=globus_hashtable_init(&ftp_cache_table,71,
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
		printf("ERROR %d Failed to activate Globus IO Module\n",result);
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

	globus_reltime_t delay;
	GlobusTimeReltimeSet( delay, FTP_HANDLE_CACHE_TIME, 0 );
	err = globus_callback_register_periodic( NULL, &delay, &delay,
											 clean_ftp_handles, NULL );
	if ( err != GLOBUS_SUCCESS ) {
		printf("ERROR %d globus_callback_register_periodic() failed\n",result);
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

