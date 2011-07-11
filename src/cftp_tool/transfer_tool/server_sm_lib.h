#ifndef CFTP_SERVER_SM_LIB_H
#define CFTP_SERVER_SM_LIB_H

#include "frames.h"
#include "simple_parameters.h"
#include "utilities.h"
#include "receive.h"
	
/* Used by main to communicate with parse_opt. */
typedef struct _arguments
{
	int verbose, itimeout, ptimeout, debug, announce, aport, lport, single_client;
	long quota;
	char* ahost;
	char* lhost;
	char* tpath;
	char* uuid;
} ServerArguments;



//---------------------------------------------------

int confirm_arguments( ServerArguments* arg);
int handle_OOB_communications( Endpoint* e  );
int handle_client( Listener* l, TransferArguments* targs,
                   int* client_used, char** payload );
int handle_announce( Endpoint* e,
                     const char* ahost,
                     int aport,
                     const char* lhost,
                     int lport,
                     const char* uuid,
                     const char* payload,
                     int ttl );
void run_server( ServerArguments* arg);

/*
void start_server( ServerState* state );
void start_oob( ServerState* state );
void announce_server( ServerState* state );
void handle_client( ServerState* state );
void handle_oob_command( ServerState* state );
int run_state_machine( StateAction* states, ServerState* state );
int post_transfer_loop( ServerState* state);
int transition_table( ServerState* state, int condition_code );

int recv_cftp_frame( ServerState* state );
int recv_data_frame( ServerState* state );
int send_cftp_frame( ServerState* state );
int send_data_frame( ServerState* state );

void desc_cftp_frame( ServerState* state, int send_or_recv);
*/

#endif
