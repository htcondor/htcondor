/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

//----------------------------------------------------------------------
// Author: Hao Wang
// $id:$
//----------------------------------------------------------------------

#include "condor_cred_kerberos.h"
#include <krb5.h>                        // Kerberos API
#include "condor_debug.h"

//------------------------------------------
// Some predefined structures
//------------------------------------------

//----------------------------------------------------------------------
// class Condor_Kerberos
//----------------------------------------------------------------------
Condor_Kerberos :: Condor_Kerberos(ReliSock * sock)
  : Condor_Credential_B (CONDOR_CRED_KERBEROS),
    forwarded_          (TRUE),
    context_            (0),
    ccname_             (NULL)
{
  int retval;

  retval = receive_credential(sock);
  // Throw an exception if not successful
}

Condor_Kerberos :: Condor_Kerberos(const Condor_Kerberos& copy)
    : Condor_Credential_B (CONDOR_CRED_KERBEROS),
      forwarded_          (copy.forwarded_),
      context_            (copy.context_),
      ccname_             (0)
{
  // Let's copy it now
  ccname_ = new char[CONDOR_KRB_DIR_LENGTH];
  strcpy(ccname_, copy.ccname_);
}

//------------------------------------------
// Use this constructor for creating a
// credential object for user
//------------------------------------------
Condor_Kerberos :: Condor_Kerberos()
  : Condor_Credential_B (CONDOR_CRED_KERBEROS),
    forwarded_          (FALSE),
    context_            (0),
    ccname_             (NULL)
{
  krb5_error_code retval;
  krb5_ccache     ccdef;       // default credential cache for user
  krb5_creds      creds;
  krb5_creds      fwdcred;     // forwarding credential
  //------------------------------------------
  // If not forwared, let's try to create a new one for the user
  //------------------------------------------

  // First, get the default one
  retval = krb5_init_context(&context_);
  if (retval) {
    //condor_krb5_error(retval, STR_CONDOR_CRED_KRB_INIT);
    // throw exception!
  }
 
  // Let's copy it now
  ccname_ = new char[CONDOR_KRB_DIR_LENGTH];

  // Make a kerberos cache
  snprintf(ccname_, strlen(ccname_), "FILE:%s/%s", 
	   condor_cred_dir(), STR_CONDOR_KERBEROS_PREFIX, "userid", getpid());

  // Now, copy it.
}

Condor_Kerberos :: ~Condor_Kerberos()
{
  //------------------------------------------
  // Do some clean up
  //------------------------------------------
  if (context_) {
    krb5_free_context(context_);
  }
  
  if (ccname_) {
    delete ccname_;
  }
}

//------------------------------------------
// Storing credential locally for future use
//------------------------------------------
bool Condor_Kerberos :: store_credential(krb5_creds *creds)
{
  krb5_error_code   ret;
  krb5_ccache       ccache;
  char *            old      = NULL;
  char *            cred_dir = NULL;

  // Do we have one already? If so, use that name
  
  if (ccname_ == NULL) {
    // First, get the directory where we shall store the credentials
    cred_dir = condor_cred_dir();

    // Now, we have a cred_dir, let's make up the FILE name for Kerberos
    // NOTE: ccname_ should be of the format:
    //        FILE:/dir/credential_cache_name

    ccname_ = new char[CONDOR_KRB_DIR_LENGTH]; // USER MAX_PATH_LENGTH?
    snprintf(ccname_, strlen(ccname_), "FILE:%s/%s", 
	     cred_dir, STR_CONDOR_KERBEROS_PREFIX, "principal", getpid());
    dprintf(D_FULLDEBUG, ccname_);
  }
  else {
    // remove the old credential
    old = ccname_;
  }

  // Now, we have a ccname_, let's store it
  if (!(ret = krb5_cc_resolve(context_, ccname_, &ccache))) {
    if (!(ret = krb5_cc_initialize(context_, ccache, (creds->client)))) {
      ret = krb5_cc_store_cred(context_, ccache, creds);
    }
  }
  
  //ticket = xmalloc(strlen(ccname_) + 1);
  //(void) snprintf(ticket, strlen(ccname_) + 1, "%s", ccname_);
  
  if (ret) { // Failure
    delete ccname_;
    ccname_ = old;
  }
  else {  // Success, clean up the old one
    if (old) {
      //krb5_cc_destroy(context_, old);
    }
    delete old;
  }

  return (ret == 0);
}

bool Condor_Kerberos :: is_valid() const
{
  return TRUE;  // What the hack
}

//------------------------------------------
// Forwarding credential if possible
//------------------------------------------
//#ifdef KERBEROS_TGT_PASSING
bool Condor_Kerberos :: forward_credential(ReliSock * sock)
{
  //#ifdef KRB5
  char *remotehost;
  krb5_principal client;
  krb5_principal server;
  krb5_ccache ccache;
  krb5_data outbuf;
  krb5_error_code r;
  krb5_auth_context auth_context = 0;
  int type, msg;
  char server_name[512];
  
  //remotehost = (char *) get_canonical_hostname();
  memset(&outbuf, 0 , sizeof(outbuf));
  
  dprintf(D_FULLDEBUG, "Trying Kerberos V5 TGT passing.");
  
  if (!context_)
    {
      if ((r = krb5_init_context(&context_))) {
        dprintf(D_FULLDEBUG, "Unable to initialize context");
      }
      krb5_init_ets(context_);
    }

  //------------------------------------------
  // Check for connection context
  //------------------------------------------
  if (auth_context != NULL)
    {
      if ((r = krb5_auth_con_init(context_, &auth_context)))
        {
          dprintf(D_FULLDEBUG, 
		  "Kerberos V5: failed to init auth_context (%.100s)");
          return FALSE ;
        }

      krb5_auth_con_setflags(context_, auth_context,
                             KRB5_AUTH_CONTEXT_RET_TIME);
    }

  //------------------------------------------
  // Read the credential cache
  //------------------------------------------
  
  // First, let's find out where it was stored
  if (ccname_)
  if ((r = krb5_cc_resolve(context_, ccname_, &ccache)))
    {
      dprintf(D_FULLDEBUG,"Kerberos V5: could not get default ccache.");
      return FALSE ;
    }
  
    if ((r = krb5_cc_get_principal(context_, ccache, &client)))
      {
        dprintf(D_FULLDEBUG,"Kerberos V5: failure on principal (%.100s)");
        return FALSE ;
      }
    
    /* Somewhat of a hack here. We need to get the TGT for the
       clients realm. However if remotehost is in another realm
       krb5_fwd_tgt_creds will try to go to that realm to get
       the TGT, which will fail. So we create the server 
       principal and point it to clients realm. This way
       we pass over a TGT of the clients realm. */
    
    snprintf(server_name, sizeof(server_name), "host/%.100s@", remotehost);
    strncat(server_name, client->realm.data,client->realm.length);
    krb5_parse_name(context_,server_name, &server);
    server->type = KRB5_NT_SRV_HST;
    
    // Now forward the cache
    if ((r = krb5_fwd_tgt_creds(context_, auth_context, 0, client, 
                                server, ccache, 1, &outbuf)))
      {
        dprintf(D_FULLDEBUG,"Kerberos V5 krb5_fwd_tgt_creds failure (%.100s)");
        krb5_free_principal(context_, client);
        krb5_free_principal(context_, server);
        return FALSE ;
      }

    //Now, send it over
    sock->encode();
    msg = CONDOR_KERBEROS_FORWARD_TGT;
    sock->code( msg );
    sock->code( outbuf.length );
    sock->put_bytes((const void *)(outbuf.data), outbuf.length);
    sock->end_of_message();

    // Let's check to see if the other side got it
    sock->decode();
    sock->code(type);

    // clean up
    if (outbuf.data)
      free(outbuf.data);
    krb5_free_principal(context_, client);
    krb5_free_principal(context_, server);

    if (type == CONDOR_KERBEROS_RECEIVE_TGT) {
        dprintf(D_FULLDEBUG,"Kerberos V5 TGT passing was successful.");
        return TRUE;
    }
    else {
      dprintf(D_FULLDEBUG,"Kerberos V5 TGT passing failed.");
    }
    
    return FALSE;
    //#endif /* KRB5 */
}
//#endif /* KERBEROS_TGT_PASSING */

bool Condor_Kerberos :: receive_credential(ReliSock * sock)
{
  bool ret = FALSE;
  krb5_creds **     creds;
  krb5_error_code   retval;
  krb5_ccache       ccache = NULL;
  krb5_auth_context auth_context = NULL;
  extern char *ticket;
  static krb5_principal rcache_server = 0;
  static krb5_rcache rcache;
  krb5_address *local_addr, *remote_addr;
  int type, msgLength;
  krb5_data    krb5data;
  
  // First, check for message
  sock->decode();
  sock->code(type);

  if (type == CONDOR_KERBEROS_FORWARD_TGT) {
    // The other side sent us a TGT!, let's receive it
    sock->code(krb5data.length);
    sock->get_bytes((void *)krb5data.data, krb5data.length);

    if (auth_context != NULL)    {
      if (!rcache_server) {
        krb5_parse_name(context_,"condor", &rcache_server);
      }
      
      krb5_auth_con_init(context_, &auth_context);
      
      /* Set the addresses for local and remote systems, and replay
         cache */
      
      /* GDM : We need to establish the local addresses and remote addresses
         within auth_context, in order to to TGT forwarding. Normally
         when the authentication credentials are passed, these are 
         established then, however in SSH the forwarded TGT is 
         passed prior to authentication. (Needed by AFS) */
      
      krb5_auth_con_genaddrs(context_, auth_context, sock->get_file_desc(), 
                             KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR |
                             KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR);
      krb5_get_server_rcache(context_,
                             krb5_princ_component(context, rcache_server, 0),
                             &rcache);
      krb5_auth_con_setrcache( context_, auth_context, rcache);
      
    }
    
    if (retval=krb5_rd_cred(context_,auth_context, &krb5data,&creds,NULL))
      {
	dprintf(D_FULLDEBUG, "Error getting forwarded credential");
	sock->encode();
	type = CONDOR_KERBEROS_TGT_FAIL;
	sock->code( type );
	sock->end_of_message();
	return FALSE;
      }
    else{
      // Store the credential locally.
      store_credential(*creds);
      sock->encode();
      type = CONDOR_KERBEROS_RECEIVE_TGT;
      sock->code( type );   // Tell the other end
    }
  
  }
  return ret;
}

//------------------------------------------
// Try to print out some meaningful messages
//------------------------------------------
void Condor_Kerberos :: condor_krb5_error(int ret, char * message)
{
  dprintf(D_FULLDEBUG, message);
}  
//Condor_Credential_B& IPC_transfer_credential(int pid);
//Condor_Credential_B& IPC_receive_credential(int pid);









