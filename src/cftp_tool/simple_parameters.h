#ifndef CFTP_SIMPLE_PARAMETERS_H
#define CFTP_SIMPLE_PARAMETERS_H

typedef struct _simple_parameters
{
	char      filename[512];
	long      filesize;
	int       chunk_size;
    int       num_chunks;
} simple_parameters;


#endif
