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

#if !defined(SKIP_AUTHENTICATION) && defined(HAVE_EXT_OPENSSL)
#define ouch(x) dprintf(D_ALWAYS,x)
#include "authentication.h"
#include "condor_auth_ssl.h"
#include "condor_string.h"
#include "condor_environ.h"
#include "CondorError.h"
#include "get_full_hostname.h"
#include "openssl/rand.h"
#include "condor_netdb.h"

Condor_Auth_SSL :: Condor_Auth_SSL(ReliSock * sock, int remote)
    : Condor_Auth_Base    ( sock, CAUTH_SSL )
{
	m_crypto = NULL;
}

Condor_Auth_SSL :: ~Condor_Auth_SSL()
{
    ERR_remove_state( 0 );
	if(m_crypto) delete(m_crypto);
}

int Condor_Auth_SSL::authenticate(const char * remoteHost, CondorError* errstack)
{
    long err;
    char *buffer;
    char err_buf[500];
    int ssl_status = AUTH_SSL_A_OK;
    int server_status = AUTH_SSL_A_OK;
    int client_status = AUTH_SSL_A_OK;
    int done = 0;
    int success = (0 == 0);
    int fail = (0 == 1);
    int round_ctr = 0;
    BIO *conn_in = NULL, *conn_out = NULL;
    SSL *ssl = NULL;
    SSL_CTX *ctx = NULL;
	unsigned char session_key[AUTH_SSL_SESSION_KEY_LEN];

    /* This next bit is just to get the fqdn of the host we're communicating
       with.  One would think that remoteHost would have this, but it doesn't
       seem to. -Ian
    */
    /* After some discussion with Zach, we don't actually do any checking
       that involves the host name, so whatever...
    const char *peerHostAddr = getRemoteHost();
    struct hostent *he = condor_gethostbyname(peerHostAddr);
    dprintf(D_SECURITY,"Peer addr: '%s'\n", peerHostAddr);
    const char *peerHostName = get_full_hostname_from_hostent(
        condor_gethostbyaddr(he->h_addr, sizeof he->h_addr, AF_INET), NULL);
    dprintf(D_SECURITY,"Got hostname for peer: '%s'\n", peerHostName);
    */

	// allocate a large buffer for comminications
	buffer = (char*) malloc( AUTH_SSL_BUF_SIZE );
    
    if( mySock_->isClient() ) {
        if( init_OpenSSL( ) != AUTH_SSL_A_OK ) {
            ouch( "Error initializing OpenSSL for authentication\n" );
            client_status = AUTH_SSL_ERROR;
        }
        if( !(ctx = setup_ssl_ctx( false )) ) {
            ouch( "Error initializing client security context\n" );
            client_status = AUTH_SSL_ERROR;
        }
        if( !(conn_in = BIO_new( BIO_s_mem( ) )) 
            || !(conn_out = BIO_new( BIO_s_mem( ) )) ) {
            ouch( "Error creating buffer for SSL authentication\n" );
            client_status = AUTH_SSL_ERROR;
        }
        if( !(ssl = SSL_new( ctx )) ) {
            ouch( "Error creating SSL context\n" );
            client_status = AUTH_SSL_ERROR;
        }
        server_status = client_share_status( client_status );
        if( server_status != AUTH_SSL_A_OK || client_status != AUTH_SSL_A_OK ) {
            ouch( "SSL Authentication fails, terminating\n" );
			free(buffer);
            return fail;
        }

        SSL_set_bio( ssl, conn_in, conn_out );

        done = 0;
        round_ctr = 0;

			// Sonny suggested that this loop could be avoided,
			// perhaps increasing efficiency, if we were willing to
			// talk SSL directly on the Cedar sock. -Ian
        while( !done ) {
            if( client_status != AUTH_SSL_HOLDING ) {
                ouch("Trying to connect.\n");
                ssl_status = SSL_connect( ssl );
                dprintf(D_SECURITY, "Tried to connect: %d\n", ssl_status);
            }
            if( ssl_status < 1 ) {
                client_status = AUTH_SSL_QUITTING;
                done = 1;
                //ouch( "Error performing SSL authentication\n" );
                err = SSL_get_error( ssl, ssl_status );
                switch( err ) {
                case SSL_ERROR_ZERO_RETURN:
                    ouch("SSL: connection has been closed.\n");
                    break;
                case SSL_ERROR_WANT_READ:
                    ouch("SSL: trying to continue reading.\n");
                    client_status = AUTH_SSL_RECEIVING;
                    done = 0;
                    break;
                case SSL_ERROR_WANT_WRITE:
                    ouch("SSL: trying to continue writing.\n");
                    client_status = AUTH_SSL_SENDING;
                    done = 0;
                    break;
                case SSL_ERROR_WANT_CONNECT:
                case SSL_ERROR_WANT_ACCEPT:
                    ouch("SSL: error want connect/accept.\n");
                    break;
                case SSL_ERROR_WANT_X509_LOOKUP:
                    ouch("SSL: X509_LOOKUP: callback incomplete.\n" );
                    break;
                case SSL_ERROR_SYSCALL:
                    ouch("SSL: Syscall.\n" );
                    break;
                case SSL_ERROR_SSL:
                    ouch("SSL: library failure.  see error queue?\n");
                    break;
                default:
                    ouch("SSL: unknown error?\n" );
                    break;
                }
            } else {
                client_status = AUTH_SSL_HOLDING;
            }
            round_ctr++;
            dprintf(D_SECURITY,"Round %d.\n", round_ctr);            
            if(round_ctr % 2 == 1) {
                if(AUTH_SSL_ERROR == client_send_message(
                       client_status, buffer, conn_in, conn_out )) {
                   server_status = AUTH_SSL_QUITTING;
                }
            } else {
                server_status = client_receive_message(
                    client_status, buffer, conn_in, conn_out );
            }
            dprintf(D_SECURITY,"Status (c: %d, s: %d)\n", client_status, server_status);
            /* server_status = client_exchange_messages( client_status,
             *  buffer, conn_in,
             *  conn_out );
             */
            if( server_status == AUTH_SSL_ERROR ) {
                server_status = AUTH_SSL_QUITTING;
            }
            if( server_status == AUTH_SSL_HOLDING
                && client_status == AUTH_SSL_HOLDING ) {
                done = 1;
            }
            if( client_status == AUTH_SSL_QUITTING
                || server_status == AUTH_SSL_QUITTING ) {
                ouch( "SSL Authentication failed\n" );
				free(buffer);
                return fail;
            }
        }
        dprintf(D_SECURITY,"Client trying post connection check.\n");
        if((err = post_connection_check(
                ssl, AUTH_SSL_ROLE_CLIENT )) != X509_V_OK ) {
            ouch( "Error on check of peer certificate\n" );
            snprintf(err_buf, 500, "%s\n",
                     X509_verify_cert_error_string( err ));
            ouch( err_buf );
            client_status = AUTH_SSL_QUITTING;
        } else {
            client_status = AUTH_SSL_A_OK;
        }

        dprintf(D_SECURITY,"Client performs one last exchange of messages.\n");

        if( client_status == AUTH_SSL_QUITTING
            || server_status == AUTH_SSL_QUITTING ) {
            ouch( "SSL Authentication failed\n" );
			free(buffer);
            return fail;
        }

        client_status = server_status = AUTH_SSL_RECEIVING;
        done = 0;
        round_ctr = 0;
			//unsigned char session_key[AUTH_SSL_SESSION_KEY_LEN];
        while(!done) {
            dprintf(D_SECURITY,"Reading round %d.\n",++round_ctr);
            if(round_ctr > 256) {
                ouch("Too many rounds exchanging key: quitting.\n");
                done = 1;
                client_status = AUTH_SSL_QUITTING;
                break;
            }
            if( client_status != AUTH_SSL_HOLDING) {
                ssl_status = SSL_read(ssl, 
									  session_key, AUTH_SSL_SESSION_KEY_LEN);
            }
            if(ssl_status < 1) {
                err = SSL_get_error( ssl, ssl_status);
                switch( err ) {
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                    ouch("SSL: continue read/write.\n");
                    done = 0;
                    client_status = AUTH_SSL_RECEIVING;
                    break;
                default:
                    client_status = AUTH_SSL_QUITTING;
                    done = 1;
                    ouch("SSL: error on write.  Can't proceed.\n");
                    break;
                }
            } else {
                dprintf(D_SECURITY,"SSL read has succeeded.\n");
                client_status = AUTH_SSL_HOLDING;
            }
            if(round_ctr % 2 == 1) {
                server_status = client_receive_message(
                    client_status, buffer, conn_in, conn_out );
            } else {
                if(AUTH_SSL_ERROR == client_send_message(
                       client_status, buffer, conn_in, conn_out )) {
                    server_status = AUTH_SSL_QUITTING;
                }
            }
            dprintf(D_ALWAYS, "Status: c: %d, s: %d\n", client_status, server_status);
            if(server_status == AUTH_SSL_HOLDING
               && client_status == AUTH_SSL_HOLDING) {
                done = 1;
            }
            if(server_status == AUTH_SSL_QUITTING) {
                done = 1;
            }
        }
        if( server_status == AUTH_SSL_QUITTING
            || client_status == AUTH_SSL_QUITTING ) {
            ouch( "SSL Authentication failed at session key exchange.\n" );
			free(buffer);
            return fail;
        }
        //dprintf(D_SECURITY, "Got session key: '%s'.\n", session_key);
        setup_crypto( session_key, AUTH_SSL_SESSION_KEY_LEN );
    } else { // Server
        
        if( init_OpenSSL(  ) != AUTH_SSL_A_OK ) {
            ouch( "Error initializing OpenSSL for authentication\n" );
            server_status = AUTH_SSL_ERROR;
        }
        if( !(ctx = setup_ssl_ctx( true )) ) {
            ouch( "Error initializing server security context\n" );
            server_status = AUTH_SSL_ERROR;
        }
        if( !(conn_in = BIO_new( BIO_s_mem( ) ))
            || !(conn_out = BIO_new( BIO_s_mem( ) )) ) {
            ouch( "Error creating buffer for SSL authentication\n" );
            server_status = AUTH_SSL_ERROR;
        }
        if (!(ssl = SSL_new(ctx))) {
            ouch("Error creating SSL context\n");
            server_status = AUTH_SSL_ERROR;
        }
        client_status = server_share_status( server_status );
        if( client_status != AUTH_SSL_A_OK
            || server_status != AUTH_SSL_A_OK ) {
            ouch( "SSL Authentication fails, terminating\n" );
			free(buffer);
            return fail;
        }
  
        // SSL_set_accept_state(ssl); // Do I really have to do this?
        SSL_set_bio(ssl, conn_in, conn_out);

        done = 0;
        round_ctr = 0;
        while( !done ) {
            if( server_status != AUTH_SSL_HOLDING ) {
                ouch("Trying to accept.\n");
                ssl_status = SSL_accept( ssl );
                dprintf(D_SECURITY, "Accept returned %d.\n", ssl_status);
            }
            if( ssl_status < 1 ) {
                server_status = AUTH_SSL_QUITTING;
                done = 1;
                err = SSL_get_error( ssl, ssl_status );
                switch( err ) {
                case SSL_ERROR_ZERO_RETURN:
                    ouch("SSL: connection has been closed.\n");
                    break;
                case SSL_ERROR_WANT_READ:
                    ouch("SSL: trying to continue reading.\n");
                    server_status = AUTH_SSL_RECEIVING;
                    done = 0;
                    break;
                case SSL_ERROR_WANT_WRITE:
                    ouch("SSL: trying to continue writing.\n");
                    server_status = AUTH_SSL_SENDING;
                    done = 0;
                    break;
                case SSL_ERROR_WANT_CONNECT:
                case SSL_ERROR_WANT_ACCEPT:
                    ouch("SSL: error want connect/accept.\n");
                    break;
                case SSL_ERROR_WANT_X509_LOOKUP:
                    ouch("SSL: X509_LOOKUP: callback incomplete.\n" );
                    break;
                case SSL_ERROR_SYSCALL:
                    ouch("SSL: Syscall.\n" );
                    break;
                case SSL_ERROR_SSL:
                    ouch("SSL: library failure.  see error queue?\n");
                    break;
                default:
                    ouch("SSL: unknown error?\n" );
                    break;
                }
            } else {
                server_status = AUTH_SSL_HOLDING;
            }
            round_ctr++;
            dprintf(D_SECURITY,"Round %d.\n", round_ctr);
            if(round_ctr %2 == 1) {
                client_status = server_receive_message(
                    server_status, buffer, conn_in, conn_out );
            } else {
                if(AUTH_SSL_ERROR == server_send_message(
                       server_status, buffer, conn_in, conn_out )) {
                    client_status = AUTH_SSL_QUITTING;
                }
            }
            dprintf(D_SECURITY,"Status (c: %d, s: %d)\n", client_status, server_status);            
            /*
             * client_status = server_exchange_messages( server_status,
             * buffer,
             * conn_in, conn_out );
             */
            if (client_status == AUTH_SSL_ERROR) {
                client_status = AUTH_SSL_QUITTING;
            }
            if( client_status == AUTH_SSL_HOLDING
                && server_status == AUTH_SSL_HOLDING ) {
                done = 1;
            }
            if( client_status == AUTH_SSL_QUITTING
                || server_status == AUTH_SSL_QUITTING ) {
                ouch( "SSL Authentication failed\n" );
				free(buffer);
                return fail;
            }
        }
        ouch("Server trying post connection check.\n");
        if ((err = post_connection_check(ssl, AUTH_SSL_ROLE_SERVER)) != X509_V_OK) {
            ouch( "Error on check of peer certificate\n" );

            char errbuf[500];
            snprintf(errbuf, 500, "%s\n", X509_verify_cert_error_string(err));
            ouch( errbuf );
            ouch( "Error checking SSL object after connection\n" );
            server_status = AUTH_SSL_QUITTING;
        } else {
            server_status = AUTH_SSL_A_OK;
        }

        if( server_status == AUTH_SSL_QUITTING
            || client_status == AUTH_SSL_QUITTING ) {
            ouch( "SSL Authentication failed\n" );
			free(buffer);
            return fail;
        }
        if(!RAND_bytes(session_key, AUTH_SSL_SESSION_KEY_LEN)) {
            ouch("Couldn't generate session key.\n");
            server_status = AUTH_SSL_QUITTING;
        }
        //dprintf(D_SECURITY,"Generated session key: '%s'\n", session_key);

        client_status = server_status = AUTH_SSL_RECEIVING;
        done = 0;
        round_ctr = 0;
        while(!done) {
            dprintf(D_SECURITY,"Writing round %d.\n", ++round_ctr);
            if(round_ctr > 256) {
                ouch("Too many rounds exchanging key: quitting.\n");
                done = 1;
                server_status = AUTH_SSL_QUITTING;
                break;
            }
            if( server_status != AUTH_SSL_HOLDING ) {
                ssl_status = SSL_write(ssl, 
									   session_key, AUTH_SSL_SESSION_KEY_LEN);
            }
            if(ssl_status < 1) {
                err = SSL_get_error( ssl, ssl_status);
                switch( err ) {
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                    ouch("SSL: continue read/write.\n");
                    done = 0;
                    server_status = AUTH_SSL_RECEIVING;
                    break;
                default:
                    server_status = AUTH_SSL_QUITTING;
                    done = 1;
                    ouch("SSL: error on write.  Can't proceed.\n");
                    break;
                }
            } else {
                dprintf(D_SECURITY, "SSL write has succeeded.\n");
                if(client_status == AUTH_SSL_HOLDING) {
                    done = 1;
                }
                server_status = AUTH_SSL_HOLDING;
            }
            if(round_ctr % 2 == 1) {
                if(AUTH_SSL_ERROR == server_send_message(
                       server_status, buffer, conn_in, conn_out )) {
                    client_status = AUTH_SSL_QUITTING;
                }
            } else {
                client_status = server_receive_message(
                    server_status, buffer, conn_in, conn_out );
            }
            dprintf(D_ALWAYS, "Status: c: %d, s: %d\n", client_status, server_status);
            if(server_status == AUTH_SSL_HOLDING
               && client_status == AUTH_SSL_HOLDING) {
                done = 1;
            }
            if(client_status == AUTH_SSL_QUITTING) {
                done = 1;
            }
        }
        if( server_status == AUTH_SSL_QUITTING
            || client_status == AUTH_SSL_QUITTING ) {
            ouch( "SSL Authentication failed at key exchange.\n" );
			free(buffer);
            return fail;
        }
        setup_crypto( session_key, AUTH_SSL_SESSION_KEY_LEN );
    }

    char subjectname[1024];
    X509 *peer = SSL_get_peer_certificate(ssl);
    X509_NAME_oneline(X509_get_subject_name(peer), subjectname, 1024);
    setAuthenticatedName( subjectname );
    setRemoteUser( "ssl" );
	setRemoteDomain( UNMAPPED_DOMAIN );

    dprintf(D_SECURITY,"SSL authentication succeeded to %s\n", subjectname);
		//free(key);
	SSL_CTX_free(ctx);
	SSL_free(ssl);
		//BIO_free(conn_in); // Thanks, valgrind.
		//BIO_free(conn_out);
	free(buffer);
    return success;
}


int Condor_Auth_SSL::isValid() const
{
	if ( m_crypto ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

bool
Condor_Auth_SSL::setup_crypto(unsigned char* key, const int keylen)
{
		// get rid of any old crypto object
	if ( m_crypto ) delete m_crypto;
	m_crypto = NULL;

	if ( !key || !keylen ) {
		// cannot setup anything without a key
		return false;
	}

		// This could be 3des -- maybe we should use "best crypto" indirection.
	KeyInfo thekey(key,keylen,CONDOR_3DES);
	m_crypto = new Condor_Crypt_3des(thekey);
	return m_crypto ? true : false;
}

bool
Condor_Auth_SSL::encrypt(unsigned char* input, 
					int input_len, unsigned char* & output, int& output_len)
{
	return encrypt_or_decrypt(true,input,input_len,output,output_len);
}

bool
Condor_Auth_SSL::decrypt(unsigned char* input, int input_len, 
							unsigned char* & output, int& output_len)
{
	return encrypt_or_decrypt(false,input,input_len,output,output_len);
}

bool
Condor_Auth_SSL::encrypt_or_decrypt(bool want_encrypt, 
									   unsigned char* input, 
									   int input_len, 
									   unsigned char* &output, 
									   int &output_len)
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
	if (!m_crypto) {
		return false;
	}

		// do the work
	m_crypto->resetState();
	if (want_encrypt) {
		result = m_crypto->encrypt(input,input_len,output,output_len);
	} else {
		result = m_crypto->decrypt(input,input_len,output,output_len);
	}
	
		// mark output_len as zero upon failure
	if (!result) {
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
Condor_Auth_SSL::wrap(char *   input, 
						 int      input_len, 
						 char*&   output, 
						 int&     output_len)
{
	bool result;
	unsigned char* in = (unsigned char*)input;
	unsigned char* out = (unsigned char*)output;
	dprintf(D_SECURITY, "In wrap.\n");
	result = encrypt(in,input_len,out,output_len);
	
	output = (char *)out;
	
	return result ? TRUE : FALSE;
}

int 
Condor_Auth_SSL::unwrap(char *   input, 
						   int      input_len, 
						   char*&   output, 
						   int&     output_len)
{
	bool result;
	unsigned char* in = (unsigned char*)input;
	unsigned char* out = (unsigned char*)output;
	
	dprintf(D_SECURITY, "In unwrap.\n");
	result = decrypt(in,input_len,out,output_len);
	
	output = (char *)out;
	
	return result ? TRUE : FALSE;
}


/*
 * This works on Linux.
 */
/*int seed_pnrg(void)
{
    if (!RAND_load_file("/dev/urandom", 1024))
        return 0;
    return 1;
}*/

int Condor_Auth_SSL :: init_OpenSSL(void)
{
    if (!SSL_library_init()) {
        return AUTH_SSL_ERROR;
    }
    SSL_load_error_strings();
    // seed_pnrg(); TODO: 
    return AUTH_SSL_A_OK;
}

int verify_callback(int ok, X509_STORE_CTX *store)
{
    char data[256];
 
    if (!ok) {
        X509 *cert = X509_STORE_CTX_get_current_cert(store);
        int  depth = X509_STORE_CTX_get_error_depth(store);
        int  err = X509_STORE_CTX_get_error(store);

        dprintf( D_SECURITY, "-Error with certificate at depth: %i\n", depth );
        X509_NAME_oneline( X509_get_issuer_name( cert ), data, 256 );
        dprintf( D_SECURITY, "  issuer   = %s\n", data );
        X509_NAME_oneline( X509_get_subject_name( cert ), data, 256 );
        dprintf( D_SECURITY, "  subject  = %s\n", data );
        dprintf( D_SECURITY, "  err %i:%s\n", err, X509_verify_cert_error_string( err ) );
    }
 
    return ok;
}

int Condor_Auth_SSL :: send_status( int status )
{
    mySock_ ->encode( );
    if( !(mySock_ ->code( status ))
        || !(mySock_ ->end_of_message( )) ) {
        ouch( "Error communicating status\n" );
        return AUTH_SSL_ERROR;
    }
    return AUTH_SSL_A_OK;
}

int Condor_Auth_SSL :: receive_status( int &status )
{
    mySock_ ->decode( );
    if( !(mySock_ ->code( status ))
        || !(mySock_ ->end_of_message( )) ) {
        ouch( "Error communicating status\n" );
        return AUTH_SSL_ERROR;
    }
    return AUTH_SSL_A_OK;
}
    

int Condor_Auth_SSL :: client_share_status( int client_status )
{
    int server_status;
    if( receive_status( server_status ) == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    if( send_status( client_status ) == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    return server_status;
}

int Condor_Auth_SSL :: server_share_status( int server_status )
{
    int client_status;
    if( send_status( server_status ) == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    if( receive_status( client_status ) == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    return client_status;
}

int Condor_Auth_SSL :: send_message( int status, char *buf, int len )
{

    char *send = buf;

    dprintf(D_SECURITY, "Send message (%d).\n", status );
    mySock_ ->encode( );
    if( !(mySock_ ->code( status ))
        || !(mySock_ ->code( len ))
        || !(len == (mySock_ ->put_bytes( send, len )))
        || !(mySock_ ->end_of_message( )) ) {
        ouch( "Error communicating with peer.\n" );
        return AUTH_SSL_ERROR;
    }
    return AUTH_SSL_A_OK;
}

int Condor_Auth_SSL :: receive_message( int &status, int &len, char *buf )
{
    ouch("Receive message.\n");
    mySock_ ->decode( );
    if( !(mySock_ ->code( status ))
        || !(mySock_ ->code( len ))
        || !(len == (mySock_ ->get_bytes( buf, len )))
        || !(mySock_ ->end_of_message( )) ) {
        ouch( "Error communicating with peer.\n" );
        return AUTH_SSL_ERROR;
    }
    dprintf(D_SECURITY,"Received message (%d).\n", status );
    return AUTH_SSL_A_OK;
}

int Condor_Auth_SSL :: server_receive_message( int server_status, char *buf, BIO *conn_in, BIO *conn_out )
{
    int client_status;
    int len;
    int rv;
    int written;
    if( receive_message( client_status, len, buf ) == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
//    if( client_status == AUTH_SSL_SENDING) {
        if( len > 0 ) {
            written = 0;
            while( written < len ) {
                rv =  BIO_write( conn_in, buf, len );
                if( rv <= 0 ) {
                    ouch( "Couldn't write connection data into bio\n" );
                    return AUTH_SSL_ERROR;
                    break;
                }
                written += rv;
            }
        }
//    }
    return client_status;
}
int Condor_Auth_SSL :: server_send_message( int server_status, char *buf, BIO *conn_in, BIO *conn_out )
{
    int len;
    // Read from server's conn_out into buffer to send to client.
    buf[0] = 0; // just in case we don't read anything
    len = BIO_read( conn_out, buf, AUTH_SSL_BUF_SIZE );
    if (len < 0) {
        len = 0;
    }
    if( send_message( server_status, buf, len ) == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    return AUTH_SSL_A_OK;
}


int Condor_Auth_SSL :: server_exchange_messages( int server_status, char *buf, BIO *conn_in, BIO *conn_out )
{
    ouch("Server exchange messages.\n");
    if(server_send_message( server_status, buf, conn_in, conn_out )
       == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    return server_receive_message( server_status, buf, conn_in, conn_out );
}

int Condor_Auth_SSL :: client_send_message( int client_status, char *buf, BIO *conn_in, BIO *conn_out )
{
    int len = 0;
    buf[0] = 0; // just in case we don't read anything
    len = BIO_read( conn_out, buf, AUTH_SSL_BUF_SIZE );
    if(len < 0) {
        len = 0;
    }
    if( send_message( client_status, buf, len ) == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    return AUTH_SSL_A_OK;
}
int Condor_Auth_SSL :: client_receive_message( int client_status, char *buf, BIO *conn_in, BIO *conn_out )
{
    int server_status;
    int len = 0;
    int rv;
    int written;
    if( receive_message( server_status, len, buf ) == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }

    if( len > 0 ) {
        written = 0;
        while( written < len ) {
            rv =  BIO_write( conn_in, buf, len );
            if( rv <= 0 ) {
                ouch( "Couldn't write connection data into bio\n" );
                return AUTH_SSL_ERROR;
                break;
            }
            written += rv;
        }
    }
    return server_status;   
}
int Condor_Auth_SSL :: client_exchange_messages( int client_status, char *buf, BIO *conn_in, BIO *conn_out )
{
    int server_status = AUTH_SSL_ERROR;
    ouch("Client exchange messages.\n");
    if(( server_status = client_receive_message(
             client_status, buf, conn_in, conn_out ))
       == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    if( client_send_message(
            client_status, buf, conn_in, conn_out )
        == AUTH_SSL_ERROR) {
        return AUTH_SSL_ERROR;
    }
    return server_status;
}


long Condor_Auth_SSL :: post_connection_check(SSL *ssl, int role)
{
	X509      *cert;
		/* These are removed, see below.
    X509_NAME *subj;
    char      data[256];
    int       extcount;
    int       ok = 0;
		*/
//    char      err_buf[500];
    ouch("post_connection_check.\n");
 
    /* Checking the return from SSL_get_peer_certificate here is not
     * strictly necessary.  With our example programs, it is not
     * possible for it to return NULL.  However, it is good form to
     * check the return since it can return NULL if the examples are
     * modified to enable anonymous ciphers or for the server to not
     * require a client certificate.
     * (Comment from book.  -Ian)
     */
    cert = SSL_get_peer_certificate(ssl);
    if( cert == NULL ) {
        dprintf(D_SECURITY,"SSL_get_peer_certificate returned null.\n" );
        goto err_occured;
    }
    dprintf(D_SECURITY,"SSL_get_peer_certificate returned data.\n" );

    /* What follows is working code based on examples from the book
       "Network Programming with OpenSSL."  The point of this code
       is to determine that the certificate is a valid host certificate
       for the server side.  Zach says that this will be done by
       the caller of the authenticate method, so we just ignore it.
       However, since it's more complicated than simply extracting
       the CN and comparing it to the host name, I'm leaving it in
       here so we can refer to it if needed.

       TODO: Does globus do this?  Do we need to do it?

       TODO: The caller is probably going to want the DN, we should
       extract it here.
       
       -Ian
       
    if(role == AUTH_SSL_ROLE_SERVER) {
        X509_free( cert );
        ouch("Server role: returning from post connection check.\n");
        return SSL_get_verify_result( ssl );
    } // else ROLE_CLIENT: check dns (arg 2) against cn
    if ((extcount = X509_get_ext_count(cert)) > 0) {
        int i;
        for (i = 0;  i < extcount;  i++) {
            const char              *extstr;
            X509_EXTENSION    *ext;
            ASN1_OBJECT *foo;

            ext = X509_get_ext(cert, i);
            foo = X509_EXTENSION_get_object(ext);
            int bar = OBJ_obj2nid(foo);
            extstr = OBJ_nid2sn(bar);
            
            if (!strcmp(extstr, "subjectAltName")) {
                int                  j;
                unsigned char        *data;
                STACK_OF(CONF_VALUE) *val;
                CONF_VALUE           *nval;
                X509V3_EXT_METHOD    *meth;
                void                 *ext_str = NULL;
 
                if (!(meth = X509V3_EXT_get(ext)))
                    break;
                data = ext->value->data;
#if (OPENSSL_VERSION_NUMBER > 0x00907000L)

                if (meth->it)
                    ext_str = ASN1_item_d2i(NULL, &data, ext->value->length,
                                            ASN1_ITEM_ptr(meth->it));
                else
                    ext_str = meth->d2i(NULL, &data, ext->value->length);
#else
                ext_str = meth->d2i(NULL, &data, ext->value->length);
#endif
                val = meth->i2v(meth, ext_str, NULL);
                for (j = 0;  j < sk_CONF_VALUE_num(val);  j++) {
                    nval = sk_CONF_VALUE_value(val, j);
                    if (!strcmp(nval->name, "DNS")
                        && !strcmp(nval->value, host)) {
                        ok = 1;
                        break;
                    }
                }
            }
            if (ok)
                break;
        }
    }
    
    if (!ok && (subj = X509_get_subject_name(cert)) &&
        X509_NAME_get_text_by_NID(subj, NID_commonName, data, 256) > 0)
    {
        data[255] = 0;
        dprintf(D_SECURITY, "Common Name: '%s'; host: '%s'\n", data, host);
        if (strcasecmp(data, host) != 0)
            goto err_occured;
    }
    if( cert != NULL ) {
        dprintf(D_SECURITY,"Client: Got non null cert.\n" );
    }
    */
    ouch("Returning SSL_get_verify_result.\n");
    
    X509_free(cert);
    return SSL_get_verify_result(ssl);
 
err_occured:
    if (cert)
        X509_free(cert);
    return X509_V_ERR_APPLICATION_VERIFICATION;
}

SSL_CTX *Condor_Auth_SSL :: setup_ssl_ctx( bool is_server )
{
    SSL_CTX *ctx       = NULL;
    char *cafile       = NULL;
    char *cadir        = NULL;
    char *certfile     = NULL;
    char *keyfile      = NULL;
    char *cipherlist   = NULL;
    priv_state priv;

		// Not sure where we want to get these things from but this
		// will do for now.
	if( is_server ) {
		cafile     = param( AUTH_SSL_SERVER_CAFILE_STR );
		cadir      = param( AUTH_SSL_SERVER_CADIR_STR );
		certfile   = param( AUTH_SSL_SERVER_CERTFILE_STR );
		keyfile    = param( AUTH_SSL_SERVER_KEYFILE_STR );
	} else {
		cafile     = param( AUTH_SSL_CLIENT_CAFILE_STR );
		cadir      = param( AUTH_SSL_CLIENT_CADIR_STR );
		certfile   = param( AUTH_SSL_CLIENT_CERTFILE_STR );
		keyfile    = param( AUTH_SSL_CLIENT_KEYFILE_STR );
	}		
	cipherlist = param( AUTH_SSL_CIPHERLIST_STR );
    if( cipherlist == NULL ) {
		cipherlist = strdup(AUTH_SSL_DEFAULT_CIPHERLIST);
    }
    if( !certfile || !keyfile ) {
        ouch( "Please specify path to server certificate and key\n" );
        dprintf(D_SECURITY, "in config file : '%s' and '%s'.\n",
                AUTH_SSL_SERVER_CERTFILE_STR, AUTH_SSL_SERVER_KEYFILE_STR );
		ctx = NULL;
        goto setup_server_ctx_err;
    }
    if(cafile)     dprintf( D_SECURITY, "CAFILE:     '%s'\n", cafile     );
    if(cadir)      dprintf( D_SECURITY, "CADIR:      '%s'\n", cadir      );
    if(certfile)   dprintf( D_SECURITY, "CERTFILE:   '%s'\n", certfile   );
    if(keyfile)    dprintf( D_SECURITY, "KEYFILE:    '%s'\n", keyfile    );
    if(cipherlist) dprintf( D_SECURITY, "CIPHERLIST: '%s'\n", cipherlist );
        
    ctx = SSL_CTX_new( SSLv23_method(  ) );
	if(!ctx) {
		ouch( "Error creating new SSL context.\n");
		goto setup_server_ctx_err;
	}

	// disable SSLv2.  it has vulnerabilities.
	SSL_CTX_set_options( ctx, SSL_OP_NO_SSLv2 );

    if( SSL_CTX_load_verify_locations( ctx, cafile, cadir ) != 1 ) {
        ouch( "Error loading CA file and/or directory\n" );
		goto setup_server_ctx_err;
    }
    if( SSL_CTX_use_certificate_chain_file( ctx, certfile ) != 1 ) {
        ouch( "Error loading certificate from file" );
        goto setup_server_ctx_err;
    }
    priv = set_root_priv();
    if( SSL_CTX_use_PrivateKey_file( ctx, keyfile, SSL_FILETYPE_PEM) != 1 ) {
        set_priv(priv);
        ouch( "Error loading private key from file" );
        goto setup_server_ctx_err;
    }
    set_priv(priv);
		// TODO where's this?
    SSL_CTX_set_verify( ctx, SSL_VERIFY_PEER, verify_callback ); 
    SSL_CTX_set_verify_depth( ctx, 4 ); // TODO arbitrary?
    SSL_CTX_set_options( ctx, SSL_OP_ALL|SSL_OP_NO_SSLv2 );
    if(SSL_CTX_set_cipher_list( ctx, cipherlist ) != 1 ) {
        ouch( "Error setting cipher list (no valid ciphers)\n" );
        goto setup_server_ctx_err;
    }
    if(cafile)          free(cafile);
    if(cadir)           free(cadir);
    if(certfile)        free(certfile);
    if(keyfile)         free(keyfile);
    if(cipherlist)      free(cipherlist);
    return ctx;
  setup_server_ctx_err:
    if(cafile)          free(cafile);
    if(cadir)           free(cadir);
    if(certfile)        free(certfile);
    if(keyfile)         free(keyfile);
    if(cipherlist)      free(cipherlist);
	if(ctx)		        SSL_CTX_free(ctx);
    return NULL;
}


 
#endif
