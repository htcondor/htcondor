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

