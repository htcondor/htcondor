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

#include "condor_common.h"
#include "condor_crypt.h"
#include "condor_md.h"
#include "condor_random_num.h"
#if defined(CONDOR_ENCRYPTION)
#include <openssl/rand.h>              // SSLeay rand function
#endif
#include "condor_debug.h"


Condor_Crypt_Base :: Condor_Crypt_Base(Protocol protocol, const KeyInfo& keyInfo)
    : keyInfo_ (keyInfo)
{
#if defined(CONDOR_ENCRYPTION)
    ASSERT(keyInfo_.getProtocol() == protocol);
#endif
}

Condor_Crypt_Base :: Condor_Crypt_Base()
    : keyInfo_ ()
{
}

Condor_Crypt_Base :: ~Condor_Crypt_Base()
{
}

int Condor_Crypt_Base :: encryptedSize(int inputLength, int blockSize)
{
#if defined(CONDOR_ENCRYPTION)
    int size = inputLength % blockSize;
    return (inputLength + ((size == 0) ? blockSize : (blockSize - size)));
#else
    return -1;
#endif
}

Protocol Condor_Crypt_Base :: protocol()
{
#if defined(CONDOR_ENCRYPTION)
    return keyInfo_.getProtocol();
#else
    return (Protocol)0;
#endif
}

unsigned char * Condor_Crypt_Base :: randomKey(int length)
{
    unsigned char * key = (unsigned char *)(malloc(length));

#if defined(CONDOR_ENCRYPTION)
    char * file = 0;
    int size = 4096;
    if (count_ == 0) {
        unsigned char * buf = (unsigned char *) malloc(size);

        RAND_seed(buf, size);

        free(buf);
    }
    else {
        count_++;
    }

    RAND_bytes(key, length);
#else
    // use condor_util_lib/get_random.c
    int r, s, size = sizeof(r);
    unsigned char * tmp = key;
    for (s = 0; s < length; s+=size, tmp+=size) {
        r = get_random_int();
        memcpy(tmp, &r, size);
    }
    if (length > s) {
        r = get_random_int();
        memcpy(tmp, &r, length - s);
    }
#endif
    return key;
}

unsigned char * Condor_Crypt_Base :: oneWayHashKey(const char * initialKey)
{
#if defined(CONDOR_ENCRYPT)
    return Condor_MD_MAC::computeOnce((unsigned char *)initialKey, strlen(initialKey));
#else 
    return 0;
#endif
}

int Condor_Crypt_Base :: count_ = 0;

