/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_crypt.h"
#include "condor_md.h"
#include "condor_random_num.h"
#ifdef HAVE_EXT_OPENSSL
#include <openssl/rand.h>              // SSLeay rand function
#endif
#include "condor_debug.h"


Condor_Crypt_Base :: Condor_Crypt_Base(Protocol prot, const KeyInfo& keyInfo)
    : keyInfo_ (keyInfo)
{
#ifdef HAVE_EXT_OPENSSL
    ASSERT(keyInfo_.getProtocol() == prot);
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
#ifdef HAVE_EXT_OPENSSL
    int size = inputLength % blockSize;
    return (inputLength + ((size == 0) ? blockSize : (blockSize - size)));
#else
    return -1;
#endif
}

Protocol Condor_Crypt_Base :: protocol()
{
#ifdef HAVE_EXT_OPENSSL
    return keyInfo_.getProtocol();
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
		for (int i = 0; i < size; i++) {
			buf[i] = get_random_int() & 0xFF;
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
#ifdef HAVE_EXT_OPENSSL
    return Condor_MD_MAC::computeOnce((unsigned char *)initialKey, strlen(initialKey));
#else 
    return 0;
#endif
}
