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

#include "condor_dh.h"
#include "condor_debug.h"
#include "condor_config.h"
#include <openssl/pem.h>
#include <openssl/bn.h>

const char DH_CONFIG_FILE[] = "CONDOR_DH_CONFIG";

Condor_Diffie_Hellman :: Condor_Diffie_Hellman()
    : dh_     (0),
      secret_ (0),
      keySize_(0)
{
    initialize();
}

Condor_Diffie_Hellman :: ~Condor_Diffie_Hellman()
{
    if (dh_) {
        DH_free(dh_);
    }
    if (secret_) {
        free(secret_);
    }
    keySize_ = 0;

}

int Condor_Diffie_Hellman :: compute_shared_secret(const char * pk)
{
    // the input pk is assumed to be an encoded string representing
    // the binary data for the remote party's public key -- y (or x)
    // the local DH knows about g and x, now, it will compute
    // (g^x)^y, or (g^y)^x

    BIGNUM * remote_pubKey = NULL;

    if (BN_hex2bn(&remote_pubKey, pk) == 0) {
        dprintf(D_ALWAYS, "Unable to obtain remote public key\n");
        goto error;
    }

    if ((dh_ != NULL) && (remote_pubKey != NULL)) {

        secret_ = (unsigned char *) malloc(DH_size(dh_));

        // Now compute
        keySize_ = DH_compute_key(secret_, remote_pubKey, dh_);
        BN_clear_free(remote_pubKey);

        if (keySize_ == -1) {
            dprintf(D_ALWAYS, "Unable to compute shared secret\n");
            goto error;
        }
    }   
    else {
        goto error;
    }
    return 1;

 error:
    if (remote_pubKey) {
        BN_clear_free(remote_pubKey);
    }
    if (secret_) {
        free(secret_);
        secret_ = NULL;
    }
    return 0;
}
    
char * Condor_Diffie_Hellman :: getPublicKeyChar()
{
    // This will return g^x, x is the secret, encoded in HEX format
    if (dh_ && dh_->pub_key) {
        return BN_bn2hex(dh_->pub_key);
    }
    else {
        return NULL;
    }
}

BIGNUM * Condor_Diffie_Hellman::getPrime()
{
    if (dh_) {
        return dh_->p;
    }
    else {
        return 0;
    }
}

char * Condor_Diffie_Hellman :: getPrimeChar()
{
    if (dh_ && dh_->p) {
        return BN_bn2hex(dh_->p);
    }
    else {
        return NULL;
    }
}

BIGNUM * Condor_Diffie_Hellman :: getGenerator()
{
    if (dh_) {
        return dh_->g;
    }
    else {
        return 0;
    }
}

char * Condor_Diffie_Hellman :: getGeneratorChar()
{
    if (dh_ && dh_->g) {
        return BN_bn2hex(dh_->g);
    }
    else {
        return NULL;
    }
}

const unsigned char * Condor_Diffie_Hellman :: getSecret() const
{
    return secret_;
}

int Condor_Diffie_Hellman :: getSecretSize() const
{
    return keySize_;
}

int Condor_Diffie_Hellman :: initialize()
{
    // First, check the config file to find out where is the file
    // with all the parameters
    config();
    char * dh_config = param(DH_CONFIG_FILE);

    FILE * fp = 0;
    if ( dh_config ) {
        if ( (fp = fopen(dh_config, "r")) == NULL) {
            dprintf(D_ALWAYS, "Unable to open condor_dh_config file %s\n", dh_config);
            goto error;
        }

        dh_ = PEM_read_DHparams(fp, NULL, NULL, NULL);
        if (dh_ == NULL) {
            dprintf(D_ALWAYS, "Unable to read DH structure from the configuration file.\n");
            goto error;
        }

        // Now generate private key
        if (DH_generate_key(dh_) == 0) {
            dprintf(D_ALWAYS, "Unable to generate a private key \n");
            goto error;
        }
    }
    else {
        dprintf(D_ALWAYS, "The required configuration parameter CONDOR_DH_CONFIG is not specified in the condor configuration file!\n");
        goto error;
    }
    fclose(fp);
    free(dh_config);
    return 1;
 error:
    if (dh_) {
        DH_free(dh_);
        dh_ = 0;
    }
    if (dh_config) {
        free(dh_config);
    }
    if (fp) {
        fclose(fp);
    }
    return 0;
}
