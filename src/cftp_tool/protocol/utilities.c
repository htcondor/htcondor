#include "utilities.h"

#include "sha1-c/sha1.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
  open_file
  
  Opens a given filename and returns a FileRecord pointer.

  Returns NULL if the file is unable to be opened.

 */
FileRecord* open_file( char* filename)
{
	FileRecord* record;
	SHA1Context hashRecord;
	unsigned char chunk;
	unsigned int len;
    char* str_ptr;	
	char* old_str_ptr;
	

	record = (FileRecord*) malloc(sizeof(FileRecord));
	memset( record, 0, sizeof( FileRecord ));


		// Extract the base filename from the filepath given
	old_str_ptr = str_ptr = filename;
	while( str_ptr )
		{
			old_str_ptr = str_ptr;
			str_ptr = strchr( str_ptr+1, '/' );
			if( str_ptr )
				str_ptr = str_ptr + 1;
		}
	str_ptr = old_str_ptr;
    
	record->filename = (char*)malloc( strlen(str_ptr)+1 );
	strcpy( record->filename, str_ptr );

	record->path = (char*)malloc( str_ptr - filename );
	memcpy( record->path, filename, str_ptr - filename );


	record->fp = fopen( filename, "rb" );
	if( record->fp == NULL )
		{
			free(record);
			return NULL;
		}


		// Calculate the hash code for the file data
	SHA1Reset( &hashRecord );
	chunk = fgetc( record->fp );
	while( !feof(record->fp) )
		{
			SHA1Input( &hashRecord, &chunk, 1);			
			chunk = fgetc( record->fp );
		}


	len = SHA1Result( &hashRecord );
	if( len == 0 )
		fprintf(stderr, "Error while constructing hash. Hash is not right!\n" );

	rewind( record->fp);
	memcpy( record->hash, hashRecord.Message_Digest, 5*sizeof(int) );
	
	fseek( record->fp, 0, SEEK_END );
	record->file_size = ftell( record->fp );
	rewind( record->fp);

	record->chunk_size = 100; // Really bad hack
	
		// Count how many chunks there will be.
	record->num_chunks = record->file_size/record->chunk_size;
	if( record->file_size % record->chunk_size != 0 )
	    record->num_chunks = record->num_chunks + 1;


	return record;
}


//sendall - taken from the Beej guide to socket programming
//http://beej.us/guide/bgnet/output/html/multipage/advanced.html#sendall

int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n = 0;

	if( buf == NULL )
		return -1;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
} 



// readOOB
int readOOB(int s, void* buf, int len, int flags,
			struct sockaddr *from, socklen_t *fromlen,
			int timeout, char termChar)
{
	int rv;
	fd_set accept_set;
	struct timeval timeout_tv;
	unsigned long int total_time_waited;
	unsigned long int max_wait;
	unsigned long int next_wait;
	int results;
	int total_recvd;
	int terminate;

	max_wait = timeout * 1000000;
	fcntl(s, F_SETFL, O_NONBLOCK);
	total_time_waited = 0;
	results = 0;
	total_recvd = 0;
	terminate = 0;
	
	memset( buf, 0, len );

	while( total_recvd < len && total_time_waited < max_wait && !terminate )
		{
			printf( "TICK %d %ld %d\n", total_recvd, total_time_waited, terminate );
			FD_ZERO(&accept_set);
			FD_SET( s, &accept_set );
			next_wait = max_wait - total_time_waited;
			timeout_tv.tv_sec = next_wait / 1000000;
			timeout_tv.tv_usec = next_wait % 1000000;
			printf( "\tNext Wait: %ld %ld\n" , next_wait/1000000, next_wait%1000000 );
			results = select(s+1, &accept_set, NULL, NULL, &timeout_tv);
			
			if( results )
				{
					rv = recvfrom(s,
								  buf+total_recvd,
								  len-total_recvd,
								  0,
								  from,
								  fromlen);

					if( strchr( buf+total_recvd, termChar) )
						terminate = 1;

					total_recvd += rv;
				}
			total_time_waited += next_wait - (timeout_tv.tv_sec * 1000000 + timeout_tv.tv_usec);
		}

	return total_recvd;
}
