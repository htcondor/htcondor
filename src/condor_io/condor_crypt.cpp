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
#include "condor_crypt.h"
#include "condor_md.h"
#include "condor_random_num.h"
#ifdef HAVE_EXT_OPENSSL
#include <openssl/rand.h>              // SSLeay rand function
#endif
#include "condor_debug.h"


Condor_Crypto_State::Condor_Crypto_State(Protocol proto, KeyInfo &key) {
    // zero everything;
    //
    reset();

#if !defined(SKIP_AUTHENTICATION)
    KeyInfo k(key);
    switch(proto) {
        case CONDOR_3DES: {
            // initialize ivsec ???
            
            // triple des requires a key of 8 x 3 = 24 bytes
            // so pad the key out to at least 24 bytes if needed
            unsigned char * keyData = k.getPaddedKeyData(24);
            ASSERT(keyData);
            
            DES_set_key((DES_cblock *)  keyData    , &keySchedule1_);
            DES_set_key((DES_cblock *) (keyData+8) , &keySchedule2_);
            DES_set_key((DES_cblock *) (keyData+16), &keySchedule3_);
            
            free(keyData);
            break;
        }
        case CONDOR_BLOWFISH: {
            BF_set_key(&key_, k.getKeyLength(), k.getKeyData());
            break;
        }
        default:
            // already zerod out above
            break;
    }
#endif
}

Condor_Crypto_State::Condor_Crypto_State() {
    EXCEPT("crypto regular con");
}

Condor_Crypto_State::Condor_Crypto_State(Condor_Crypto_State &c) {
    c.keytype = 0;
    EXCEPT("crypto copy con");
}

Condor_Crypto_State::~Condor_Crypto_State() {
}

void Condor_Crypto_State::reset() {
    // assign "empty" key (there's no clear() or reset() method on KeyInfo)
    KeyInfo k;
    keyInfo_ = k;

    keytype = 0;
    keyalgorithm.clear();
    
    keylen = 0;
    key = NULL;
    
    iveclen = 0;
    ivec = NULL;
    
    num = 0;
    
    additional_len = 0;
    additional = NULL;

    num_ = 0;
    bzero(ivec_, 8); // u char [8]

    bzero(&key_, sizeof(key_));

    bzero(&keySchedule1_, sizeof(keySchedule1_));
    bzero(&keySchedule2_, sizeof(keySchedule2_));
    bzero(&keySchedule3_, sizeof(keySchedule3_));

    dprintf(D_ALWAYS, "ZKM: NEED TO RESET STATE FOR ALL TYPES\n");
}

/*
Condor_Crypt_Base :: Condor_Crypt_Base(Protocol prot, const KeyInfo& keyInfo)
{
#ifdef HAVE_EXT_OPENSSL
    EXCEPT("ZKM: ERROR: base con key");

    if((prot = keyInfo.getProtocol())) {}
    //state.keyInfo_ = keyInfo;
    dprintf(D_ALWAYS, "ZKM: ERROR: accessed internal state!!!\n");
    //ASSERT(state.keyInfo_.getProtocol() == prot);
#endif
}
*/

Condor_Crypt_Base :: Condor_Crypt_Base()
{
    dprintf(D_ALWAYS, "ZKM: Constructed Crypto Object.\n");
}

Condor_Crypt_Base :: ~Condor_Crypt_Base()
{
}

int Condor_Crypt_Base :: encryptedSize(int inputLength, int blockSize)
{
#ifdef HAVE_EXT_OPENSSL
    int size = inputLength % blockSize;
    return (inputLength + ((size == 0) ? blockSize : (blockSize - size)));
#else
    return -1;
#endif
}

Protocol Condor_Crypt_Base :: protocol()
{
    EXCEPT("ZKM: ERROR: accessed internal state!!!");
#ifdef HAVE_EXT_OPENSSL
    dprintf(D_ALWAYS, "ZKM: ERROR: accessed internal state!!!\n");
    //return state.keyInfo_.getProtocol();
#else
    return (Protocol)0;
#endif
}

unsigned char * Condor_Crypt_Base :: randomKey(int length)
{
    unsigned char * key = (unsigned char *)(malloc(length));

	memset(key, 0, length);

#ifdef HAVE_EXT_OPENSSL
	static bool already_seeded = false;
    int size = 128;
    if( ! already_seeded ) {
        unsigned char * buf = (unsigned char *) malloc(size);
        ASSERT(buf);
		// Note that RAND_seed does not seed, but rather simply
		// adds entropy to the pool that is initialized with /dev/urandom
		// (actually, this could potentially help in the case where HTCondor
		// is running on a system without /dev/urandom; seems ... unlikely for
		// Linux!).
		//
		// As this only helps the pool, we are OK with calling the 'insecure'
		// variant here.
		for (int i = 0; i < size; i++) {
			buf[i] = get_random_int_insecure() & 0xFF;
		}

        RAND_seed(buf, size);
        free(buf);
		already_seeded = true;
    }

    RAND_bytes(key, length);
#else
    // use condor_util_lib/get_random.c
    int r, s, size = sizeof(r);
    unsigned char * tmp = key;
    for (s = 0; s < length; s+=size, tmp+=size) {
        r = get_random_int_insecure();
        memcpy(tmp, &r, size);
    }
    if (length > s) {
        r = get_random_int_insecure();
        memcpy(tmp, &r, length - s);
    }
#endif
    return key;
}

char *Condor_Crypt_Base::randomHexKey(int length)
{
	unsigned char *bytes = randomKey(length);
	char *hex = (char *)malloc(length*2+1);
	ASSERT( hex );
	int i;
	for(i=0; i<length; i++) {
		sprintf(hex+i*2,"%02x",bytes[i]);
	}
	free(bytes);
	return hex;
}

unsigned char * Condor_Crypt_Base :: oneWayHashKey(const char * initialKey)
{
#ifdef HAVE_EXT_OPENSSL
    return Condor_MD_MAC::computeOnce((const unsigned char *)initialKey, strlen(initialKey));
#else 
    return NULL;
#endif
}
