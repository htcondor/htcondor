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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_base64.h"

// For base64 coding, we use functions in the gSOAP support library
#include "stdsoap2.h"

// Caller needs to free the returned pointer
char* condor_base64_encode(const unsigned char *input, int length)
{
	char *buff = NULL;

	if ( length < 1 ) {
		buff = (char *)malloc(1);
		buff[0] = '\0';
		return buff;
	}

	int buff_len = (length+2)/3*4+1;
	buff = (char *)malloc(buff_len);
	ASSERT(buff);
	memset(buff,0,buff_len);

	struct soap soap;
	soap_init(&soap);

	soap_s2base64(&soap,input,buff,length);

	soap_destroy(&soap);
	soap_end(&soap);
	soap_done(&soap);

	return buff;
}

// Caller needs to free *output if non-NULL
void condor_base64_decode(const char *input,unsigned char **output, int *output_length)
{
	ASSERT( input );
	ASSERT( output );
	ASSERT( output_length );
	int input_length = strlen(input);

		// safe to assume output length is <= input_length
	*output = (unsigned char *)malloc(input_length);
	ASSERT( *output );
	memset(*output, 0, input_length);

	struct soap soap;
	soap_init(&soap);

	soap_base642s(&soap,input,(char*)(*output),input_length,output_length);

	soap_destroy(&soap);
	soap_end(&soap);
	soap_done(&soap);

	if( *output_length < 0 ) {
		free( *output );
		*output = NULL;
	}
}
