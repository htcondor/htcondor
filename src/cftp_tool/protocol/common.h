#ifndef CFTP_PROTOCOL_COMMON_H_
#define CFTP_PROTOCOL_COMMON_H_

#include "config.h"
#include "frames.h"
#include "utilities.h"
#include "simple_parameters.h"


#define PROTOCOL_DEBUG 1

// State Debugging macros
#define ENTER_STATE if( state->arguments->debug) fprintf(stderr, "[ENTER STATE] %s\n", __func__ );
#define LEAVE_STATE(a) if( state->arguments->debug) { fprintf(stderr, "[LEAVE STATE] %s -- Condition Flag: %d\n\n\n", __func__, a ); } return a;

#define DEBUG(...) if( state->arguments->debug) { fprintf( stderr,  __VA_ARGS__ ); fflush( stderr ); }
#define VERBOSE(...) if( state->arguments->verbose) { fprintf( stdout, __VA_ARGS__ ); }


typedef struct _ServerRecord
{
	char* server_name;
	char* server_port;
	int         server_socket;
} ServerRecord;

typedef struct _ClientRecord
{
	char* client_name;
	char* client_port;
	int         client_socket;
} ClientRecord;

enum ProtocolPhase { DISCOVERY, NEGOTIATION, TRANSFER, TEARDOWN }; 
enum Errors { NO_ERROR, };	

typedef struct {
		// Common Options
	Connection local_socket;
	int   debug;
	int   verbose;


		// Receive Options
	char* store_path;
	long int quota;



		// Send Options
    char* file_path;
	


		// TODO: Fill in needed arguments
} TransferArguments;

typedef struct _TransferState
{
	int          phase;
	int 		 state;
	int          last_error;
	char         error_string[256];

	FileRecord   local_file;
	TransferArguments*  arguments;

	int          session_token;
	int          parameter_length;
	int          parameter_format;
	char*        session_parameters;

	long         current_chunk;
	long         data_written;
	int          retry_count;

    int          net_timeout;

	int          recv_rdy;
    int		     send_rdy;

	cftp_frame   fsend_buf;
	cftp_frame   frecv_buf;

	char*        data_buffer;
	int          data_buffer_size;
} TransferState;


typedef int (*StateAction)(TransferState*);

int recv_cftp_frame( TransferState* state );
int recv_data_frame( TransferState* state );
int send_cftp_frame( TransferState* state );
int send_data_frame( TransferState* state );

void desc_cftp_frame( TransferState* state, int send_or_recv);

void free_TransferState( TransferState* state);

#endif
