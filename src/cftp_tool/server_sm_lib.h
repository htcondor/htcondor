#ifndef CFTP_SERVER_SM_LIB_H
#define CFTP_SERVER_SM_LIB_H

#define SERVER_DEBUG 1

// State Debugging macros
#define ENTER_STATE if( state->arguments->debug) fprintf(stderr, "[ENTER STATE] %s\n", __func__ );
#define LEAVE_STATE(a) if( state->arguments->debug) { fprintf(stderr, "[LEAVE STATE] %s -- Condition Flag: %d\n\n\n", __func__, a ); } return a;

#define DEBUG(...) if( state->arguments->debug) { fprintf( stderr,  __VA_ARGS__ ); }
#define VERBOSE(...) if( state->arguments->verbose) { fprintf( stdout, __VA_ARGS__ ); }



#include "frames.h"
#include "simple_parameters.h"
#include "utilities.h"

enum ProtocolPhase { DISCOVERY, NEGOTIATION, TRANSFER, TEARDOWN }; 
enum Errors { NO_ERROR, };	
	
/* Used by main to communicate with parse_opt. */
typedef struct _arguments
{
	int verbose, itimeout, debug, announce;
	long quota;
	char* aport;
	char* lport;
	char* ahost;
	char* lhost;
	char* tpath;
	char* uuid;
} ServerArguments;

typedef struct _ServerState
{
	int          phase;
	int 		 state;
	int          last_error;
	char         error_string[256];

	FileRecord   local_file;
	ServerRecord server_info;
	ServerRecord oob_info;
	ClientRecord client_info;
	ServerArguments* arguments;

	int          session_token;
	int          parameter_length;
	int          parameter_format;
	char*        session_parameters;

	long         current_chunk;
	long         data_written;
	int          retry_count;

	int          recv_rdy;
    int		     send_rdy;

	cftp_frame   fsend_buf;
	cftp_frame   frecv_buf;

	char*        data_buffer;
	int          data_buffer_size;


} ServerState;

typedef int (*StateAction)(ServerState*);


//---------------------------------------------------

int confirm_arguments( ServerArguments* arg);

void run_server( ServerArguments* arg);
void start_server( ServerState* state );
void start_oob( ServerState* state );
void announce_server( ServerState* state );
void handle_client( ServerState* state );
int run_state_machine( StateAction* states, ServerState* state );
int transition_table( ServerState* state, int condition_code );

int recv_cftp_frame( ServerState* state );
int recv_data_frame( ServerState* state );
int send_cftp_frame( ServerState* state );
int send_data_frame( ServerState* state );

void desc_cftp_frame( ServerState* state, int send_or_recv);


#endif
