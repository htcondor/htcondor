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
#include "condor_md.h"

#if defined(CONDOR_MD)
#include <openssl/md5.h>
#endif

class MD_Context {
public:
#if defined(CONDOR_MD)
    MD5_CTX          md5_;     // MD5 context
#endif
};

Condor_MD_MAC::Condor_MD_MAC()
    : isMAC_   (false),
      context_ (new MD_Context()),
      key_     (0)
{
    init();
}
    
Condor_MD_MAC::Condor_MD_MAC(KeyInfo * key)
    : isMAC_   (true),
      context_ (new MD_Context()),
      key_     (0)
{
    key_ = new KeyInfo(*key);
    init();
}

Condor_MD_MAC :: ~Condor_MD_MAC()
{
    delete key_;
    delete context_;
}

void Condor_MD_MAC :: init()
{
#if defined(CONDOR_MD)
    MD5_Init(&(context_->md5_));
    
    if (key_) {
        addMD(key_->getKeyData(), key_->getKeyLength());
    }
#endif
}

unsigned char * Condor_MD_MAC::computeOnce(unsigned char * buffer, unsigned long length)
{
#if defined(CONDOR_MD)
    unsigned char * md = (unsigned char *) malloc(MAC_SIZE);

    return MD5(buffer, (unsigned long) length, md);
#else
    return NULL;
#endif
}

unsigned char * Condor_MD_MAC::computeOnce(unsigned char * buffer, 
                                           unsigned long   length, 
                                           KeyInfo       * key)
{
#if defined(CONDOR_MD)
    unsigned char * md = (unsigned char *) malloc(MAC_SIZE);
    MD5_CTX  context;

    MD5_Init(&context);

    MD5_Update(&context, key->getKeyData(), key->getKeyLength());

    MD5_Update(&context, buffer, length);

    MD5_Final(md, &context);

    return md;
#else
    return NULL;
#endif
}

bool Condor_MD_MAC::verifyMD(unsigned char * md, 
                             unsigned char * buffer, 
                             unsigned long   length)
{
#if defined(CONDOR_MD)
    unsigned char * myMd = Condor_MD_MAC::computeOnce(buffer, length);
    bool match = (memcmp(md, myMd, MAC_SIZE) == 0);
    free(myMd);
    return match;
#else
    return true;
#endif
}

bool Condor_MD_MAC::verifyMD(unsigned char * md, 
                             unsigned char * buffer, 
                             unsigned long   length, 
                             KeyInfo       * key)
{
#if defined(CONDOR_MD)
   unsigned char * myMd = Condor_MD_MAC::computeOnce(buffer, length, key); 
   bool match = (memcmp(md, myMd, MAC_SIZE) == 0);
   free(myMd);
   return match;
#else
   return true;
#endif
}
  
void Condor_MD_MAC::addMD(const unsigned char * buffer, unsigned long length)
{
#if defined(CONDOR_MD)
   MD5_Update(&(context_->md5_), buffer, length); 
#endif
}

unsigned char * Condor_MD_MAC::computeMD()
{
#if defined(CONDOR_MD)
    unsigned char * md = (unsigned char *) malloc(MAC_SIZE);
    
    MD5_Final(md, &(context_->md5_));

    init(); // reinitialize for the next round

    return md;
#else 
    return NULL;
#endif
}

bool Condor_MD_MAC :: verifyMD(unsigned char * md)
{
#if defined(CONDOR_MD)
    unsigned char * md2 = computeMD();
    bool match = (memcmp(md, md2, MAC_SIZE) == 0);
    free(md2);
    return match;
#else
    return true;
#endif
}

