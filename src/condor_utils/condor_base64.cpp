/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

// For base64 encoding
# include <openssl/sha.h>
# include <openssl/hmac.h>
# include <openssl/evp.h>
# include <openssl/bio.h>
# include <openssl/buffer.h>

// Caller needs to free the returned pointer
char* condor_base64_encode(const unsigned char *input, int length, bool include_newline)
{
	BIO *bmem, *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());
	if (!include_newline) {
		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	}
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, input, length);
	(void)BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	// Previous versions of this function stripped the trailing newline.
	int l = bptr->length + (include_newline ? 0 : 1);
	char * buff = (char *)malloc(l);
	ASSERT(buff);
	memcpy(buff, bptr->data, l - 1);
	buff[l-1] = 0;
	BIO_free_all(b64);

	return buff;
}

// Caller needs to free *output if non-NULL
void condor_base64_decode(const char *input,unsigned char **output, int *output_length, bool require_newline)
{
	BIO *b64, *bmem;

	ASSERT( input );
	ASSERT( output );
	ASSERT( output_length );

	int input_length = strlen(input);

		// assuming output length is <= input_length
	*output = (unsigned char *)malloc(input_length + 1);
	ASSERT( *output );
	memset(*output, 0, input_length);

	b64 = BIO_new(BIO_f_base64());
	if (!require_newline) {
		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	}
	bmem = BIO_new_mem_buf((void *)const_cast<char*>(input), input_length);
	bmem = BIO_push(b64, bmem);

	*output_length = BIO_read(bmem, *output, input_length);
	if( *output_length < 0 ) {
		free( *output );
		*output = NULL;
	}

	BIO_free_all(bmem);
}

