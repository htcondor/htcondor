/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "CondorError.h"

#if !defined(SKIP_AUTHENTICATION)

#include "condor_auth_passwd.h"
#include "CryptKey.h"
#include "store_cred.h"
#include "my_username.h"

Condor_Auth_Passwd :: Condor_Auth_Passwd(ReliSock * sock)
    : Condor_Auth_Base(sock, CAUTH_PASSWORD)
{
	challenge = NULL;
	challenge_len = 0;
	crypto = NULL;
}

Condor_Auth_Passwd :: ~Condor_Auth_Passwd()
{
	if (challenge) free(challenge);
	if (crypto) delete(crypto);
}

unsigned char * 
Condor_Auth_Passwd::fetchPassword(const char* username,const char* domainname)
{
	// for now, hard-coded for testing
	//return ((unsigned char*)
	//	   strdup("hey hey, my oh my ... rock n roll will never die"));
	
	return (unsigned char*)getStoredCredential(username,domainname);

}


bool
Condor_Auth_Passwd::create_challenge()
{	
		// Check if we already did this; if so, blow away old challenge
	if ( challenge ) {
		free(challenge);
		challenge = NULL;
	}
		// Create random challenge
	challenge = Condor_Crypt_Base::randomKey(24);	// must deaclloc w/ free()
	if ( challenge ) {
		challenge_len = 24;
		return true;
	} else {
		challenge_len = 0;
		return false;
	}
}

bool
Condor_Auth_Passwd::setupCrypto(unsigned char* key, const int keylen)
{
		// get rid of any old crypto object
	if ( crypto ) delete crypto;
	crypto = NULL;

	if ( !key || !keylen ) {
		// cannot setup anything without a key
		return false;
	}

	KeyInfo thekey(key,keylen,CONDOR_3DES);
	crypto = new Condor_Crypt_3des(thekey);
	return crypto ? true : false;
}

bool
Condor_Auth_Passwd::encrypt(unsigned char* input, 
					int input_len, unsigned char* & output, int& output_len)
{
	return encrypt_or_decrypt(true,input,input_len,output,output_len);
}

bool
Condor_Auth_Passwd::decrypt(unsigned char* input, 
					int input_len, unsigned char* & output, int& output_len)
{
	return encrypt_or_decrypt(false,input,input_len,output,output_len);
}

bool
Condor_Auth_Passwd::encrypt_or_decrypt(bool want_encrypt, unsigned char* input, 
					int input_len, unsigned char* & output, int& output_len)
{
	bool result;

	// clean up any old buffers that perhaps were left over
	if ( output ) free(output);
	output = NULL;
	output_len = 0;

	// check some intput params
	if (!input || input_len < 1) {
		return false;
	}

	// make certain we got a crypto object
	if (!crypto) {
		return false;
	}

	// do the work
	crypto->resetState();
	if (want_encrypt) {
		result = crypto->encrypt(input,input_len,output,output_len);
	} else {
		result = crypto->decrypt(input,input_len,output,output_len);
	}

	// mark output_len as zero upon failure
	if (!result) 
	{
		output_len = 0;
	}

	// an output_len of zero means failure; cleanup and return
	if ( output_len == 0 ) {
		if ( output ) free(output);
		output = NULL;
		return false;
	} 
		
	// if we made it here, we're golden!
	return true;
}

int 
Condor_Auth_Passwd::wrap(char *   input, 
                             int      input_len, 
                             char*&   output, 
                             int&     output_len)
{
	bool result;
	unsigned char* in = (unsigned char*)input;
	unsigned char* out = (unsigned char*)output;

	result = encrypt(in,input_len,out,output_len);

	output = (char*)out;

	return result ? TRUE : FALSE;
}

int 
Condor_Auth_Passwd::unwrap(char *   input, 
                             int      input_len, 
                             char*&   output, 
                             int&     output_len)
{
	bool result;
	unsigned char* in = (unsigned char*)input;
	unsigned char* out = (unsigned char*)output;

	result = decrypt(in,input_len,out,output_len);

	output = (char*)out;

	return result ? TRUE : FALSE;
}

void 
Condor_Auth_Passwd::compute_response(unsigned char* & response_buffer, 
									 int & response_len)
{
	int i;

	if ( response_buffer ) free(response_buffer);
	response_buffer = NULL;
	response_len = 0;

	if ( challenge_len < 1 || challenge == NULL ) {
		return;
	}

	response_len = challenge_len;
	response_buffer = (unsigned char*)malloc( response_len );
	ASSERT(response_buffer);

	for (i=0;i < response_len;i++) {
		response_buffer[i] = challenge[i] + i + 1;
	}
}

int 
Condor_Auth_Passwd::authenticate_server()
{
	int ret_value = 0;
	int error_code = 0;
	int temp_buffer_size = 0;
	unsigned char *temp_buffer = NULL;
	unsigned char *password = NULL;
	char *username = NULL;
	char *domainname = NULL;
	int response_len = 0;
	unsigned char *response_buffer = NULL;
	int client_response_len = 0;
	unsigned char *client_response_buffer = NULL;

	// ** server side of the authentication protocol **
	
	mySock_->decode();

		// first, get the error code int from the client to see if we should continue.
	if ( !mySock_->code(error_code) ||
		 !mySock_->end_of_message() )
	{
		goto bail_out;
	}

		// if error_code is zero then continue, otherwise client failed
	if ( error_code != 0 ) {
		dprintf(D_SECURITY,
				"Condor_Auth_Passwd: client reported error %d, failing.\n",
				error_code);
		goto bail_out;
	}

		// get the username and domainname of client
	if ( !mySock_->code(username) ||
		 !mySock_->code(domainname) ||
		 !mySock_->end_of_message() )
	{
		goto bail_out;
	}

		// make certain we have the remote user, domain, password, 
		// a 3DES crypto object, a random encrypted challenge,
		// and the expected response all ready before we continue.
	if ( !username ) {
		error_code = 1;
	} else if (!domainname) {
		error_code = 2;
	} else if ((password=(unsigned char*)fetchPassword(username,domainname)) == NULL) {
		error_code = 3;
	} else if (!setupCrypto(password,strlen((char*)password)) ) {
		error_code = 4;
	} else if (!create_challenge()) {
		error_code = 5;
	} else if (!encrypt(challenge,challenge_len,temp_buffer,temp_buffer_size)) {
		error_code = 6;
	} else {
		compute_response(response_buffer,response_len);
		if ( !response_buffer || !response_len ) {
			error_code = 7;
		}
	}

		// clear the password off the stack - note it is still buried 
		// in the crypto object.
	if ( password ) {
		memset((char*)password,0,strlen((char*)password));	// zero-out the password
		free(password);
		password =  NULL;
	}

		// now we need to send the enrypted challenge to the client.
		// first we send the size (number of bytes) of the challenge,
		// followed by the encrypted challenge itself. 		
	mySock_->encode();

		// if we had an error above, just send the size of the challenge
		// as zero - this tells the client we had an error.
	if ( error_code ) {
		dprintf(D_ALWAYS,"Condor_Auth_Passwd: server side had error %d, failing\n",
					error_code);
		temp_buffer_size = 0;
		mySock_->code(temp_buffer_size);
		mySock_->end_of_message();
		goto bail_out;
	}

		// our encrypted challenge is ready to go in temp_buffer from above,
		// so just send the size and buffer now to the client.
	if ( !mySock_->code(temp_buffer_size) ||
		 !mySock_->put_bytes(temp_buffer,temp_buffer_size) ||
		 !mySock_->end_of_message() ) {
		goto bail_out;
	}

		// now receive the response to our challenge from the client
	mySock_->decode();
	if ( !mySock_->code(client_response_len) ) {
		goto bail_out;
	}
	if ( client_response_len == 0 ) {
		mySock_->end_of_message();
		goto bail_out;
	}
	client_response_buffer = (unsigned char*) malloc(client_response_len + 1);
	ASSERT(client_response_buffer);
	if ( !mySock_->get_bytes(client_response_buffer,client_response_len) ||
		 !mySock_->end_of_message() ) {
		goto bail_out;
	}

		// decrypt the response from the client, and compare it to 
		// the expected result.
	if ( decrypt(client_response_buffer,client_response_len,temp_buffer,
				  temp_buffer_size) &&	
		 temp_buffer_size == response_len &&
		 memcmp(temp_buffer,response_buffer,response_len) == 0 )
	{
		ret_value = 1;	// success!!  we believe the client
			// tell the baseclass who the client is -- anything else to set here?
		setRemoteUser(username);
		setRemoteDomain(domainname);
	} else {
		ret_value = 0;	// client did not have the correct password
		// TODO -- should we alert someone here? could be a deliberate attack...
	}

		// now send the final result from server, 1=success, 0=failure.
		// TODO: can we make this last round-trip a challenge/response from
		// the server to get mutual authentication?  right now, the server knows
		// the client, but the client does not know the server...
	mySock_->encode();
	if ( !mySock_->code(ret_value) || !mySock_->end_of_message() ) {
		ret_value = 0;
	}

	//return 1 for success, 0 for failure. Server should send sucess/failure
	//back to client so client can know what to return.
bail_out:
	if (username) free(username);
	if (domainname) free(domainname);
	if (response_buffer) free(response_buffer);
	if (client_response_buffer) free(client_response_buffer);
	return ret_value;
}



int
Condor_Auth_Passwd::authenticate_client()
{
	int ret_value = 0;
	int error_code = 0;
	int temp_buffer_size = 0;
	unsigned char *temp_buffer = NULL;
	unsigned char *password = NULL;
	int response_len = 0;
	unsigned char *response_buffer = NULL;

	// ** client side of the authentication protocol **
	
	mySock_->encode();

		// decide the login name we will try to authenticate with.  if
		// we are root on Unix or SYSTEM on Win32, then user "condor", else
		// get the local user name here. 
	char * username = NULL;
	if ( is_root() ) {
		username = strdup("condor");
	} else {
		username = my_username();
	}
		// and the domain ... should this be the UID_DOMAIN or the win32 domain??
		// for now, use the UID_DOMAIN
	char * localDomain = (char*) getLocalDomain();

		// make certain we have the local user, domain, password, and 
		// a 3DES crypto object all ready before we continue.
	if ( !username ) {
		error_code = 1;
	} else if (!localDomain) {
		error_code = 2;
	} else if ((password = fetchPassword(username,localDomain)) == NULL) {
		error_code = 3;
	} else if (!setupCrypto(password,strlen((char*)password)) ) {
		error_code = 4;
	}

		// clear the password off the stack - note it is still buried 
		// in the crypto object.
	if ( password ) {
		memset(password,0,strlen((char*)password));
		free(password);
		password =  NULL;
	}

	dprintf(D_SECURITY,
		"Condor_Auth_Passwd: client sending user=%s,domain=%s,errcode=%d\n",
		username,localDomain,error_code);

	if ( !mySock_->code(error_code) || !mySock_->end_of_message() ) {
		goto bail_out;
	}

	if (error_code > 0) {
		goto bail_out;
	}

		// send the login name to the server
	if ( !mySock_->code(username) || !mySock_->code(localDomain) 
		 || !mySock_->end_of_message() ) 
	{
		goto bail_out;
	}

		// receive encrypted challenge
	mySock_->decode();
	if ( !mySock_->code(temp_buffer_size) ) {
		goto bail_out;
	}
	if ( temp_buffer_size == 0 ) {
		mySock_->end_of_message();
		goto bail_out;
	}
	temp_buffer = (unsigned char*) malloc(temp_buffer_size + 1);
	ASSERT(temp_buffer);
	if ( !mySock_->get_bytes(temp_buffer,temp_buffer_size) ||
		 !mySock_->end_of_message() ) {
		goto bail_out;
	}

		// decrypt challenge.  note challenge and challenge_len
		// are stored in the object, for use by compute_response()
		// and for use by wrap().
	decrypt(temp_buffer,temp_buffer_size,challenge,challenge_len); 

		// Now, if we have the decrypted challenge stashed away, 
		// formulate our response and encrypt it.
	compute_response(response_buffer, response_len);	
	encrypt(response_buffer,response_len,temp_buffer,temp_buffer_size);

		// Send the size of our encrypted response on the wire, followed
		// by the encrypted response itself.
	mySock_->encode();
	if ( !mySock_->code(temp_buffer_size) ) {
		goto bail_out;
	}
	if ( temp_buffer_size == 0 ) {
		mySock_->end_of_message();
		goto bail_out;
	}
	ASSERT(temp_buffer);
	if ( !mySock_->put_bytes(temp_buffer,temp_buffer_size) ||
		 !mySock_->end_of_message() ) {
		goto bail_out;
	}

		// now get the final result from server, 1=success, 0=failure.
		// TODO: can we make this last round-trip a challenge/response from
		// the server to get mutual authentication?  right now, the server knows
		// the client, but the client does not know the server...
	mySock_->decode();
	if ( !mySock_->code(ret_value) || !mySock_->end_of_message() ) {
		ret_value = 0;
	}
	
	//return 1 for success, 0 for failure. Server should send sucess/failure
	//back to client so client can know what to return.
bail_out:
	if (response_buffer) free(response_buffer);
	if (temp_buffer) free(temp_buffer);
	if (username) free(username);
	return ret_value;
}


int
Condor_Auth_Passwd::authenticate(const char * remoteHost, CondorError* errstack)
{
	int ret_value = 0;

	/*
	Authenticate via a challenge-response protocol that takes the following form: 

	[note: Carol=Client, Steve=Server]

	Step 1. Carol sends her identity to Steve, along with some random message. 
	Step 2. Steve sends Carol a random message, called a challenge. 
	Step 3. Carol performs some computation based on the challenge, the first random message, and her password. 
	She sends this response to Steve, who performs the same computation and verifies the correctness of Carol's response. 
	Since Steve's challenge is different for each authentication attempt, a captured response is useless for future sessions, defeating a simple replay attack. 
	*/

	if ( mySock_->isClient() ) {
		// ** client side authentication **
		
		ret_value = authenticate_client();
		
	}
	else {
		// ** server side authentication **

		ret_value = authenticate_server();
	}

		/* Reset the crypto object to use the challenge as the key instead of 
		   the user's password.  Why?  Because the challenge key is likely a
		   better session key as it is a completely random 24 bytes, and also
		   because doing so will purge the user's password out RAM.
		*/
	setupCrypto(challenge,challenge_len);

	//return 1 for success, 0 for failure. Server should send sucess/failure
	//back to client so client can know what to return.
	return ret_value;
}

int Condor_Auth_Passwd :: isValid() const
{
	if ( crypto ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

#endif	// of if !defined(SKIP_AUTHENTICATION)
