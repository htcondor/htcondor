#ifndef CFTP_SERVICE_RECIEVE_H_
#define CFTP_SERVICE_RECIEVE_H_

#include "cftp_service.h"

#include "frames.h"
#include "simple_parameters.h"
#include "utilities.h"

enum ProtocolPhase { DISCOVERY, NEGOTIATION, TRANSFER, TEARDOWN }; 
enum Errors { NO_ERROR, };	
	
/* Used by main to communicate with parse_opt. */
typedef struct _arguments
{
	int verbose, itimeout, ptimeout, debug, announce;
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

class CFTP_RecieveService : public Service
{
    CFTP_RecieveService( int handle );
    virtual ~CFTP_RecieveService();

    virtual void step();
}

#endif
