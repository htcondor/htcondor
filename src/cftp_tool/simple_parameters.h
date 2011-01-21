#ifndef CFTP_SIMPLE_PARAMETERS_H
#define CFTP_SIMPLE_PARAMETERS_H

typedef struct _simple_parameters
{
	char           filename[512];
	unsigned long  filesize;
	unsigned int   hash[5];
	unsigned long  chunk_size;
    unsigned long  num_chunks;
} simple_parameters;


#endif
