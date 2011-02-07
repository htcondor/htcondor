#ifndef CFTP_SERVER_SM_LIB_H
#define CFTP_SERVER_SM_LIB_H

#define SERVER_DEBUG 1

#include "frames.h"
#include "simple_parameters.h"
#include "utilities.h"

#include "server_sm/discovery.h"
#include "server_sm/negotiation.h"
#include "server_sm/transfer.h"
#include "server_sm/teardown.h"

enum ProtocolPhase { DISCOVERY, NEGOTIATION, TRANSFER, TEARDOWN }; 
enum Errors { NO_ERROR, };	
	


typedef struct _ServerState
{
	int          phase;
	int 		 state;
	int          last_error;

	FileRecord   local_file;
	ServerRecord server_info;
	ClientRecord client_info;

	simple_parameters* session_parameters;

	cftp_frame   fsend_buf;
	cftp_frame   frecv_buf;

	char*        data_buffer;
	int          data_buffer_size;


} ServerState;

const int STATE_NUM = 100; // Will need to change this later

typedef int (*StateAction)(ServerState*);


//---------------------------------------------------

void run_server( char* server_name,
				 char* server_port);
void start_server( ServerRecord* master_server );
void handle_client( ServerRecord* master_server, ServerState* session_state );
int run_state_machine( StateAction* states, ServerState* session_state );

int transition_table( ServerState* session_state, int condition_code );




#endif
