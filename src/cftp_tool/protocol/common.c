



/*


recv_cftp_frame


*/
int recv_cftp_frame( ServerState* state )
{
	if( !state->recv_rdy )
		return -1;
	state->recv_rdy = 0;

	memset( &state->frecv_buf, 0, sizeof( cftp_frame ) );

	recv( state->client_info.client_socket, 	
		  &state->frecv_buf,	
		  sizeof( cftp_frame ),
		  MSG_WAITALL );

	DEBUG("Read %ld bytes from client on frame_recv.\n", sizeof( cftp_frame ) );
	desc_cftp_frame(state, 0 );

	return sizeof( cftp_frame );
}



/*


recv_data_frame


*/
int recv_data_frame( ServerState* state )
{
	if( !state->recv_rdy )
		return -1;
	state->recv_rdy = 0;

	int recv_bytes;
		//int i;

	if( state->data_buffer_size <= 0 )
		return 0;

	if( state->data_buffer )
		free( state->data_buffer );

	state->data_buffer = (char*) malloc( state->data_buffer_size );
	memset( state->data_buffer, 0, state->data_buffer_size );

	recv_bytes = recv( state->client_info.client_socket, 	
					   state->data_buffer,	
					   state->data_buffer_size,
					   MSG_WAITALL );

	DEBUG("Read %d bytes from client on data_recv.\n", recv_bytes );
		/*
    for( i = 0; i < recv_bytes; i ++ )
		{
			if( i % 32 == 0 )
				fprintf( stderr, "\n" );
			fprintf( stderr, "%c", (char)*(state->data_buffer+i));
		}
	fprintf( stderr, "\n" );
		*/
	
	return recv_bytes;
}




/*


send_cftp_frame

*/
int send_cftp_frame( ServerState* state )
{
	int length = 0;
	int status = 0;

	if( !state->send_rdy )
		return -1;
	state->send_rdy = 0;

	length = sizeof( cftp_frame );
	status = sendall( state->client_info.client_socket,
					  (char*)(&state->fsend_buf),
					  &length );

	if( status == -1 )
		{
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Could not send CFTP frame: %s", strerror(errno));
			return 0;
		}
	else
		{
			DEBUG("Sent %d bytes to client on frame_send.\n", length );
			desc_cftp_frame(state, 1 );
			return length;
		}
}


/*


send_data_frame

*/
int send_data_frame( ServerState* state )
{
	int length = 0;
	int status = 0;

	if( !state->send_rdy )
		return -1;
	state->send_rdy = 0;

	length = state->data_buffer_size;
	if( length <= 0 )
		return 0;
	
	if( !state->data_buffer )
		{
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Could not send data: Data buffer null");
			return 0;
		}

	status = sendall( state->client_info.client_socket,
					  (char*)(&state->data_buffer),
					  &length );

	if( status == -1 )
		{
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Could not send data: %s", strerror(errno));
			return 0;
		}
	else
		{ 
			DEBUG("Sent %d bytes to client on data_send.\n", length );
			return length;
		}
}




/*

desc_cftp_frame




 */
void desc_cftp_frame( ServerState* state, int send_or_recv)
{
	cftp_frame* frame;

	if( send_or_recv == 1 )
		{
			frame = &state->fsend_buf;
			DEBUG("\tMessage type of Send Frame is " );
		}
	else
		{
			frame = &state->frecv_buf;
			DEBUG("\tMessage type of Recv Frame is " );
		}
	
	switch( frame->MessageType )
		{
		case DSF:
			DEBUG("DSF frame.\n" );
			break;
		case DRF:
			DEBUG("DRF frame.\n" );
			break;
		case SIF:
			DEBUG("SIF frame.\n" );
            DEBUG( "SIF error code: %d.\n",
                   ntohs(((cftp_sif_frame*)frame)->ErrorCode) );
            DEBUG( "SIF session token: %d.\n",
                   ((cftp_sif_frame*)frame)->SessionToken );
            DEBUG( "SIF parameter format: %d.\n",
                   ntohs(((cftp_sif_frame*)frame)->ParameterFormat) );
            DEBUG( "SIF parameter length: %d.\n",
                   ntohs(((cftp_sif_frame*)frame)->ParameterLength) );
			break;
		case SAF:
			DEBUG("SAF frame.\n" );
			break;
		case SRF:
			DEBUG("SRF frame.\n" );
			break;
		case SCF:
			DEBUG("SCF frame.\n" );
			break;
		case DTF:
			DEBUG("DTF frame.\n" );
			DEBUG("\tChunk Number: %ld\n" , ntohll(((cftp_dtf_frame*)frame)->BlockNum) );
			break;
		case DAF:
			DEBUG("DAF frame.\n" );
			DEBUG("\tChunk Number: %ld\n" , ntohll(((cftp_daf_frame*)frame)->BlockNum) );	
			break;
		case FFF:
			DEBUG("FFF frame.\n" );
			break;
		case FAF:
			DEBUG("FAF frame.\n" );
			break;
		default:
			DEBUG("Unknown frame.\n" );
			break;
		}


}
